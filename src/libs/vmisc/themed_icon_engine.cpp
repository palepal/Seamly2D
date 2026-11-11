//-----------------------------------------------------------------------------
//  @file   themed_icon_engine.cpp
//  @author palepal
//  @date   1 Nov, 2026
//
//  @copyright
//  Copyright (C) 2026 palepal
//  https://github.com/palepal/Cutalyst
//
//  @brief
//  This program is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program. If not, see <http://www.gnu.org/licenses/>.
//-----------------------------------------------------------------------------

#include "themed_icon_engine.h"

#include "def.h"

#include <QFile>
#include <QImage>
#include <QPainter>
#include <QPixmap>
#include <QPixmapCache>
#include <QRegularExpression>
#include <QSvgRenderer>

//---------------------------------------------------------------------------------------------------------------------
/// @brief ThemedIconEngine read the icon source.
/// @param fileName path of the .tsvg file, usually a resource.
ThemedIconEngine::ThemedIconEngine(const QString &fileName)
{
    QFile file(fileName);
    if (file.open(QIODevice::ReadOnly))
    {
        m_svg = file.readAll();
    }
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief paint draw the icon into a rectangle.
/// @param painter painter to draw with.
/// @param rect target rectangle.
/// @param mode icon mode.
/// @param state icon state.
void ThemedIconEngine::paint(QPainter *painter, const QRect &rect, QIcon::Mode mode, QIcon::State state)
{
    const qreal scale = painter->device() != nullptr ? painter->device()->devicePixelRatioF() : 1.0;
    painter->drawPixmap(rect, scaledPixmap(rect.size(), mode, state, scale));
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief pixmap icon at device independent size 1:1.
/// @param size size of the pixmap.
/// @param mode icon mode.
/// @param state icon state.
/// @return rendered pixmap.
QPixmap ThemedIconEngine::pixmap(const QSize &size, QIcon::Mode mode, QIcon::State state)
{
    return scaledPixmap(size, mode, state, 1.0);
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief scaledPixmap render the icon in the theme colors for its mode.
/// @param size size in device independent pixels.
/// @param mode icon mode: normal, active (hover), selected or disabled.
/// @param state icon state.
/// @param scale device pixel ratio.
/// @return rendered pixmap, cached per size, scale and colors.
QPixmap ThemedIconEngine::scaledPixmap(const QSize &size, QIcon::Mode mode, QIcon::State state, qreal scale)
{
    Q_UNUSED(state)
    // A collapsed widget asks for an empty icon. Painting on a null image warns, and the warning popup opened mid
    // repaint crashes Qt's macOS backing store
    if ((size * scale).isEmpty())
    {
        return QPixmap();
    }

    const QString line = themeIconColor(static_cast<int>(mode)).name();
    const QString accent = themeAccentColor().name();
    const QString background = themeBackgroundColor().name();

    const QString cacheKey = QStringLiteral("tsvg_%1_%2x%3_%4_%5_%6_%7")
                                 .arg(qHash(m_svg)).arg(size.width()).arg(size.height()).arg(scale)
                                 .arg(line, accent, background);
    QPixmap result;
    if (QPixmapCache::find(cacheKey, &result))
    {
        return result;
    }

    // Spectrum icons name their color as a CSS variable, LibreCAD icons use black lines, a green accent and white
    // fills. Qt's SVG renderer understands neither variables nor a theme, so the colors are written in here
    QString svg = QString::fromUtf8(m_svg);
    static const QRegularExpression variable(QStringLiteral("var\\(--icon(Primary|Fill)[^)]*\\)"));
    static const QRegularExpression black(QStringLiteral("#000(000)?\\b|\\bblack\\b"),
                                          QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression green(QStringLiteral("#00ff7f\\b"), QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression white(QStringLiteral("#fff(fff)?\\b"), QRegularExpression::CaseInsensitiveOption);
    svg.replace(variable, line);
    svg.replace(black, line);
    svg.replace(green, accent);
    svg.replace(white, background);

    QImage image(size * scale, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QSvgRenderer renderer(svg.toUtf8());
    renderer.setAspectRatioMode(Qt::KeepAspectRatio);
    QPainter painter(&image);
    renderer.render(&painter, QRectF(QPointF(0, 0), QSizeF(image.size())));
    painter.end();

    result = QPixmap::fromImage(image);
    result.setDevicePixelRatio(scale);
    QPixmapCache::insert(cacheKey, result);
    return result;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief actualSize the icon scales to any size.
/// @param size requested size.
/// @param mode icon mode.
/// @param state icon state.
/// @return the requested size.
QSize ThemedIconEngine::actualSize(const QSize &size, QIcon::Mode mode, QIcon::State state)
{
    Q_UNUSED(mode)
    Q_UNUSED(state)
    return size;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief clone copy of this engine.
/// @return new engine with the same source.
QIconEngine *ThemedIconEngine::clone() const
{
    return new ThemedIconEngine(*this);
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief key name of the engine.
/// @return engine key.
QString ThemedIconEngine::key() const
{
    return QStringLiteral("ThemedIconEngine");
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief create engine for a .tsvg file, called by QIcon for that suffix.
/// @param fileName path of the icon.
/// @return new engine.
QIconEngine *ThemedIconEnginePlugin::create(const QString &fileName)
{
    return new ThemedIconEngine(fileName);
}
