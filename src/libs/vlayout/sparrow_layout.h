//-----------------------------------------------------------------------------
//  @file   sparrow_layout.h
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

#ifndef SPARROW_LAYOUT_H
#define SPARROW_LAYOUT_H

#include <QString>
#include <QVector>
#include <atomic>
#include <functional>

#include "vlayoutpiece.h"

namespace SparrowLayout
{
    QString program();
    bool    arrange(QVector<VLayoutPiece> &pieces, qreal width, int timeLimit, const std::atomic_bool &stop,
                    const std::function<void(int, qreal, const QString &)> &progress, qreal &length);
}

#endif // SPARROW_LAYOUT_H
