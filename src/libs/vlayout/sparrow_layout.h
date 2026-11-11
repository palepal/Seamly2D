//-----------------------------------------------------------------------------
//  @file   sparrow_layout.h
//  @author palepal
//  @date   3 Nov, 2026
//
//  @copyright
//  Copyright (C) 2026 palepal
//  https://github.com/palepal/Seamly2D
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

#ifndef SPARROW_LAYOUT_H
#define SPARROW_LAYOUT_H

#include <QString>
#include <QTransform>
#include <QVector>
#include <atomic>
#include <functional>

#include "vlayoutpiece.h"

struct SparrowFrame
{
    int                 elapsed = 0;      // milliseconds
    qreal               best_length = 0;  // best layout so far in pixels, 0 before the first one
    qreal               sheet_length = 0; // sheet length Sparrow is working on now, pieces may overlap on it
    QVector<QTransform> placements;       // for each piece, empty if nothing moved
};

namespace SparrowLayout
{
    QString program();
    void    alignGrainlines(QVector<VLayoutPiece> &pieces);
    bool    arrange(QVector<VLayoutPiece> &pieces, qreal width, int time_limit, const std::atomic_bool &stop,
                    const std::function<void(const SparrowFrame &)> &progress, qreal &length);
}

#endif // SPARROW_LAYOUT_H
