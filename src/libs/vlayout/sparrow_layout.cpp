//-----------------------------------------------------------------------------
//  @file   sparrow_layout.cpp
//  @author palepal
//  @date   3 Nov, 2026
//
//  @copyright
//  Copyright (C) 2026 Seamly, LLC
//  https://github.com/fashionfreedom/seamly2d
//
//  @brief
//  Seamly2D is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  Seamly2D is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with Seamly2D. If not, see <http://www.gnu.org/licenses/>.
//-----------------------------------------------------------------------------

#include "sparrow_layout.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLineF>
#include <QProcess>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QTimer>
#include <QtDebug>

#ifdef Q_OS_UNIX
#include <csignal>
#endif

namespace
{
//---------------------------------------------------------------------------------------------------------------------
/// @brief instanceItem describes a piece for Sparrow.
///
/// Sparrow's strip grows along x with a fixed height, the sheet grows along y with a fixed width, so x and y swap.
/// @param piece layout piece, turned to follow its grainline.
/// @param id item id.
/// @return Sparrow item.
QJsonObject instanceItem(const VLayoutPiece &piece, int id)
{
    // ponytail: grainline pieces turn 0/180 even for one-way arrows (nap), add 0 only when nap matters
    const bool grainline = piece.getGrainline().size() >= 2;
    const QJsonArray angles = grainline ? QJsonArray{0, 180} : QJsonArray{0, 90, 180, 270};

    QJsonArray outline;
    QPointF previous;
    for (const QPointF &point : piece.getLayoutAllowancePoints())
    {
        if (outline.isEmpty() || QLineF(previous, point).length() > 0.01)
        {
            outline.append(QJsonArray{point.y(), point.x()});
            previous = point;
        }
    }

    const QJsonObject rotation{{"mode", "discrete"}, {"angles", angles}};
    return QJsonObject{{"id", id},
                       {"demand", 1},
                       {"orientation", QJsonObject{{"rotation", rotation}}},
                       {"shape", QJsonObject{{"type", "simple_polygon"}, {"data", outline}}}};
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief readFrame reads where the pieces are now from Sparrow's live SVG.
///
/// Reading it needs no SVG parser: QtSvg warns on a half-written file, and warnings are popups in GUI mode.
/// @param path live SVG file, written on every step of the optimization.
/// @param last content of the previous frame.
/// @param count number of pieces.
/// @param frame gets the sheet length and placements if the file is complete and has changed.
void readFrame(const QString &path, QByteArray &last, int count, SparrowFrame &frame)
{
    QFile file(path);
    const QByteArray svg = file.open(QIODevice::ReadOnly) ? file.readAll().trimmed() : QByteArray();
    if (!svg.endsWith("</svg>") || svg == last)
    {
        return;
    }
    last = svg;

    static const QRegularExpression sheet(QStringLiteral("<path d=\"M0,0 L([\\d.]+),0"));
    static const QRegularExpression item(QStringLiteral("<use href=\"#item_0_(\\d+)\" transform=\""
                                                        "translate\\(([-\\d.e]+) ([-\\d.e]+)\\), "
                                                        "rotate\\(([-\\d.e]+)\\)\""));
    const QString text = QString::fromUtf8(svg);
    frame.sheet_length = sheet.match(text).captured(1).toDouble();
    frame.placements.resize(count);

    QRegularExpressionMatchIterator items = item.globalMatch(text);
    while (items.hasNext())
    {
        const QRegularExpressionMatch match = items.next();
        const int id = match.captured(1).toInt();
        if (id < count)
        {
            // same axes swap as the final placement in arrange
            frame.placements[id] = QTransform().rotate(-match.captured(4).toDouble())
                                   * QTransform::fromTranslate(match.captured(3).toDouble(),
                                                               match.captured(2).toDouble());
        }
    }
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief interrupt asks Sparrow to stop and write the best layout so far.
/// @param process running Sparrow.
void interrupt(QProcess &process)
{
#ifdef Q_OS_UNIX
    ::kill(static_cast<pid_t>(process.processId()), SIGINT);
#else
    // ponytail: Windows has no Ctrl+C for a child without a console, so stopping drops the layout
    process.kill();
#endif
}
} // namespace

//---------------------------------------------------------------------------------------------------------------------
/// @brief program the Sparrow executable next to the application.
/// @return path, empty if it is missing.
QString SparrowLayout::program()
{
#ifdef Q_OS_WIN
    const QString path = QCoreApplication::applicationDirPath() + QStringLiteral("/sparrow.exe");
#else
    const QString path = QCoreApplication::applicationDirPath() + QStringLiteral("/sparrow");
#endif
    return QFileInfo(path).isExecutable() ? path : QString();
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief alignGrainlines turns the pieces so their grainlines run along the sheet length (vertical).
/// @param pieces layout pieces.
void SparrowLayout::alignGrainlines(QVector<VLayoutPiece> &pieces)
{
    for (VLayoutPiece &piece : pieces)
    {
        const QVector<QPointF> grainline = piece.getGrainline();
        if (grainline.size() >= 2)
        {
            piece.Rotate(grainline.first(), 90 - QLineF(grainline.first(), grainline.last()).angle());
        }
    }
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief arrange places pieces in a strip of fixed width with Sparrow and moves them to their places.
///
/// Errors are reported with qCritical.
/// @param pieces layout pieces with aligned grainlines, moved in place.
/// @param width strip width in pixels.
/// @param time_limit optimization time in seconds.
/// @param stop set to finish early with the best layout so far.
/// @param progress called about 10 times a second.
/// @param length strip length in pixels.
/// @return true if the pieces were placed.
bool SparrowLayout::arrange(QVector<VLayoutPiece> &pieces, qreal width, int time_limit, const std::atomic_bool &stop,
                            const std::function<void(const SparrowFrame &)> &progress, qreal &length)
{
    const QString sparrow = program();
    QTemporaryDir dir;
    if (sparrow.isEmpty() || !dir.isValid())
    {
        qCritical() << QCoreApplication::translate("SparrowLayout", "The Sparrow layout engine is not installed.");
        return false;
    }

    QJsonArray items;
    for (int i = 0; i < pieces.size(); ++i)
    {
        items.append(instanceItem(pieces.at(i), i));
    }
    QFile instance(dir.filePath(QStringLiteral("seamly.json")));
    if (!instance.open(QIODevice::WriteOnly))
    {
        qCritical() << QCoreApplication::translate("SparrowLayout", "Could not write the Sparrow input file.");
        return false;
    }
    const QJsonObject input{{"name", "seamly"}, {"strip_height", width}, {"items", items}};
    instance.write(QJsonDocument(input).toJson(QJsonDocument::Compact));
    instance.close();

    QProcess process;
    process.setWorkingDirectory(dir.path());
    process.setProcessChannelMode(QProcess::MergedChannels);

    // the best length so far: "feasible solution found! (width: 909.1" and "success at 0.1% (681.3"
    const QRegularExpression best(QStringLiteral("(?:width: |success at [\\d.]+% \\()([\\d.]+)"));
    qreal best_length = 0;
    QString last_line;
    QObject::connect(&process, &QProcess::readyRead, [&]()
    {
        while (process.canReadLine())
        {
            last_line = QString::fromUtf8(process.readLine()).trimmed();
            const QRegularExpressionMatch match = best.match(last_line);
            if (match.hasMatch() && !last_line.contains(QLatin1String("[SEP]")))
            {
                best_length = match.captured(1).toDouble();
            }
        }
    });

    QEventLoop loop;
    QObject::connect(&process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                     &loop, &QEventLoop::quit);
    QObject::connect(&process, &QProcess::errorOccurred, &loop, &QEventLoop::quit);

    // written by sparrow's live_svg feature on every step, overlapping pieces included
    const QString live = dir.filePath(QStringLiteral("data/live/.live_solution.svg"));
    QByteArray last_frame;

    QElapsedTimer clock;
    QTimer timer;
    bool interrupted = false;
    QObject::connect(&timer, &QTimer::timeout, [&]()
    {
        SparrowFrame frame;
        frame.elapsed = static_cast<int>(clock.elapsed());
        frame.best_length = best_length;
        readFrame(live, last_frame, pieces.size(), frame);
        progress(frame);
        if (stop.load() && !interrupted)
        {
            interrupted = true;
            interrupt(process);
        }
    });

    clock.start();
    timer.start(100);
    process.start(sparrow, {QStringLiteral("-i"), instance.fileName(), QStringLiteral("-t"),
                            QString::number(time_limit)});
    if (process.state() != QProcess::NotRunning)
    {
        loop.exec();
    }
    timer.stop();

    QFile output(dir.filePath(QStringLiteral("output/final_seamly.json")));
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0 || !output.open(QIODevice::ReadOnly))
    {
        if (!interrupted)
        {
            qCritical() << QCoreApplication::translate("SparrowLayout", "The Sparrow layout engine failed: %1")
                           .arg(process.error() == QProcess::FailedToStart ? process.errorString() : last_line);
        }
        return false;
    }

    const QJsonObject solution = QJsonDocument::fromJson(output.readAll()).object().value("solution").toObject();
    const QJsonArray placed = solution.value("layout").toObject().value("placed_items").toArray();
    if (placed.size() != pieces.size())
    {
        qCritical() << QCoreApplication::translate("SparrowLayout",
                                                   "The Sparrow layout engine did not place every piece.");
        return false;
    }

    // Sparrow turns by r and moves by (tx, ty) in swapped axes: in Seamly that turns by -r and moves by (ty, tx)
    for (const QJsonValue &value : placed)
    {
        const QJsonObject transformation = value.toObject().value("transformation").toObject();
        const QJsonArray translation = transformation.value("translation").toArray();
        VLayoutPiece &piece = pieces[value.toObject().value("item_id").toInt()];
        piece.Rotate(QPointF(), transformation.value("rotation").toDouble());
        piece.Translate(translation.at(1).toDouble(), translation.at(0).toDouble());
    }
    length = solution.value("strip_width").toDouble();
    return true;
}
