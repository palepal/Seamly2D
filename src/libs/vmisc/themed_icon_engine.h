//-----------------------------------------------------------------------------
//  @file   themed_icon_engine.h
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

#ifndef THEMED_ICON_ENGINE_H
#define THEMED_ICON_ENGINE_H

#include <QByteArray>
#include <QIconEngine>
#include <QIconEnginePlugin>

// Draws a .tsvg icon (an SVG with the source colors below) in the colors of the current theme, at paint time, so
// icons follow theme changes and stay sharp at any size
class ThemedIconEngine : public QIconEngine
{
public:
    explicit ThemedIconEngine(const QString &fileName);

    void        paint(QPainter *painter, const QRect &rect, QIcon::Mode mode, QIcon::State state) override;
    QPixmap     pixmap(const QSize &size, QIcon::Mode mode, QIcon::State state) override;
    QPixmap     scaledPixmap(const QSize &size, QIcon::Mode mode, QIcon::State state, qreal scale) override;
    QSize       actualSize(const QSize &size, QIcon::Mode mode, QIcon::State state) override;
    QIconEngine *clone() const override;
    QString     key() const override;

private:
    QByteArray m_svg;
};

class ThemedIconEnginePlugin : public QIconEnginePlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID QIconEngineFactoryInterface_iid FILE "themed_icon_engine.json")

public:
    QIconEngine *create(const QString &fileName = QString()) override;
};

#endif // THEMED_ICON_ENGINE_H
