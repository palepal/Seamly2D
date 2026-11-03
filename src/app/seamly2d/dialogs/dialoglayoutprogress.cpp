/***************************************************************************
 *                                                                         *
 *   Copyright (C) 2017  Seamly, LLC                                       *
 *                                                                         *
 *   https://github.com/fashionfreedom/seamly2d                             *
 *                                                                         *
 ***************************************************************************
 **
 **  Seamly2D is free software: you can redistribute it and/or modify
 **  it under the terms of the GNU General Public License as published by
 **  the Free Software Foundation, either version 3 of the License, or
 **  (at your option) any later version.
 **
 **  Seamly2D is distributed in the hope that it will be useful,
 **  but WITHOUT ANY WARRANTY; without even the implied warranty of
 **  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 **  GNU General Public License for more details.
 **
 **  You should have received a copy of the GNU General Public License
 **  along with Seamly2D.  If not, see <http://www.gnu.org/licenses/>.
 **
 **************************************************************************

 ************************************************************************
 **
 **  @file   dialoglayoutprogress.cpp
 **  @author Roman Telezhynskyi <dismine(at)gmail.com>
 **  @date   14 1, 2015
 **
 **  @brief
 **  @copyright
 **  This source code is part of the Valentine project, a pattern making
 **  program, whose allow create and modeling patterns of clothing.
 **  Copyright (C) 2013-2015 Seamly2D project
 **  <https://github.com/fashionfreedom/seamly2d> All Rights Reserved.
 **
 **  Seamly2D is free software: you can redistribute it and/or modify
 **  it under the terms of the GNU General Public License as published by
 **  the Free Software Foundation, either version 3 of the License, or
 **  (at your option) any later version.
 **
 **  Seamly2D is distributed in the hope that it will be useful,
 **  but WITHOUT ANY WARRANTY; without even the implied warranty of
 **  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 **  GNU General Public License for more details.
 **
 **  You should have received a copy of the GNU General Public License
 **  along with Seamly2D.  If not, see <http://www.gnu.org/licenses/>.
 **
 *************************************************************************/

#include "dialoglayoutprogress.h"
#include "ui_dialoglayoutprogress.h"
#include "../options.h"
#include "../core/application_2d.h"

#include <QMessageBox>
#include <QPushButton>
#include <QFile>
#include <QMovie>
#include <QPainter>
#include <QRegularExpression>
#include <QSvgRenderer>
#include <QtDebug>

//---------------------------------------------------------------------------------------------------------------------
DialogLayoutProgress::DialogLayoutProgress(int count, QWidget *parent)
    :QDialog(parent), ui(new Ui::DialogLayoutProgress), maxCount(count), movie(nullptr), isInitialized(false)
{
    ui->setupUi(this);

    qApp->Seamly2DSettings()->getOsSeparator() ? setLocale(QLocale()) : setLocale(QLocale::c());

    ui->progressBar->setMaximum(maxCount);
    ui->progressBar->setValue(0);

    ui->labelMessage->setText(tr("Arranged workpieces: %1 from %2").arg(0).arg(count));

    movie = new QMovie("://icon/16x16/progress.gif");
    ui->labelProgress->setMovie (movie);
    movie->start ();

    QPushButton *bCancel = ui->buttonBox->button(QDialogButtonBox::Cancel);
    SCASSERT(bCancel != nullptr)
    connect(bCancel, &QPushButton::clicked, this, [this](){emit Abort();});

    this->setWindowFlags(Qt::Dialog | Qt::WindowTitleHint | Qt::CustomizeWindowHint);
}

//---------------------------------------------------------------------------------------------------------------------
DialogLayoutProgress::~DialogLayoutProgress()
{
    delete ui;
    delete movie;
}

//---------------------------------------------------------------------------------------------------------------------
void DialogLayoutProgress::Start()
{
    show();
}

//---------------------------------------------------------------------------------------------------------------------
void DialogLayoutProgress::Arranged(int count)
{
    ui->progressBar->setValue(count);
    ui->labelMessage->setText(tr("Arranged workpieces: %1 from %2").arg(count).arg(maxCount));
}

//---------------------------------------------------------------------------------------------------------------------
void DialogLayoutProgress::Error(const LayoutErrors &state)
{
    switch (state)
    {
        case LayoutErrors::NoError:
            return;
        case LayoutErrors::PrepareLayoutError:
            qCritical() << tr("Couldn't prepare data for creation layout");
            break;
        case LayoutErrors::EmptyPaperError:
            qCritical() << tr("One or more pattern pieces are bigger than the paper format you selected. Please select a bigger paper format.");
            break;
        case LayoutErrors::ProcessStoped:
        default:
            break;
    }

    done(QDialog::Rejected);
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief optimizing shows how far the Sparrow engine is and its current layout, Cancel becomes Finish now.
/// @param seconds elapsed time.
/// @param limit time limit in seconds.
/// @param length best layout length so far in pixels, 0 before the first layout.
/// @param preview SVG of Sparrow's current layout, the strip runs left to right.
void DialogLayoutProgress::optimizing(int seconds, int limit, qreal length, const QString &preview)
{
    ui->progressBar->setMaximum(limit);
    ui->progressBar->setValue(qMin(seconds, limit));
    ui->buttonBox->button(QDialogButtonBox::Cancel)->setText(tr("Finish now"));

    QString message = tr("Optimizing: %1 of %2 s").arg(seconds).arg(limit);
    if (length > 0)
    {
        const Unit unit = qApp->patternUnit();
        message += QStringLiteral(", ") + tr("length %1 %2")
            .arg(UnitConvertor(length, Unit::Px, unit), 0, 'f', 1).arg(UnitsToStr(unit, true));
    }
    ui->labelMessage->setText(message);

    if (ui->preview_Label->isHidden())
    {
        ui->preview_Label->show();
        if (isInitialized)
        {
            setMinimumSize(0, 0);
            setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
            adjustSize();
            setFixedSize(size());
        }
    }

    if (preview.isEmpty())
    {
        return;
    }

    // the file is missing at first and sparrow may be writing it: QtSvg would warn, and warnings are popups here
    QFile file(preview);
    const QByteArray svg = file.open(QIODevice::ReadOnly) ? file.readAll().trimmed() : QByteArray();
    if (!svg.endsWith("</svg>"))
    {
        return;
    }
    // drop sparrow's caption, its font lookup can warn too, the length is in the message already
    static const QRegularExpression caption(QStringLiteral("<text.*</text>"),
                                            QRegularExpression::DotMatchesEverythingOption
                                            | QRegularExpression::InvertedGreedinessOption);
    QSvgRenderer renderer(QString::fromUtf8(svg).remove(caption).toUtf8());
    if (!renderer.isValid())
    {
        return;
    }
    const qreal ratio = devicePixelRatioF();
    QSizeF frame = renderer.defaultSize();
    frame.scale(ui->preview_Label->contentsRect().size() * ratio, Qt::KeepAspectRatio);
    QPixmap pixmap(frame.toSize());
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    renderer.render(&painter);
    painter.end();
    pixmap.setDevicePixelRatio(ratio);
    ui->preview_Label->setPixmap(pixmap);
}

//---------------------------------------------------------------------------------------------------------------------
void DialogLayoutProgress::Finished()
{
    done(QDialog::Accepted);
}

//---------------------------------------------------------------------------------------------------------------------
void DialogLayoutProgress::showEvent(QShowEvent *event)
{
    QDialog::showEvent( event );
    if ( event->spontaneous() )
    {
        return;
    }

    if (isInitialized)
    {
        return;
    }
    // do your init stuff here

    setMaximumSize(size());
    setMinimumSize(size());

    isInitialized = true;//first show windows are held
}
