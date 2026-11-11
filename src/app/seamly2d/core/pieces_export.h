//-----------------------------------------------------------------------------
//  @file   pieces_export.h
//  @author palepal
//  @date   11 Nov, 2026
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

#ifndef PIECES_EXPORT_H
#define PIECES_EXPORT_H

#include <QHash>
#include <QPair>
#include <QPointF>
#include <QString>
#include <QStringList>
#include <QVector>

class VContainer;

// The pieces file is the documented format Cutalyst reads (--drape x.pieces.json). Lengths in millimetres, each
// piece in its own frame with y up and the grain along y.
struct GarmentLine
{
    QString          name;
    QVector<QPointF> points;
    QStringList      nodes;  // The named points it runs through, when it has them.
};

struct GarmentPiece
{
    QString                 name;
    QVector<QPointF>        outline;      // Dense and closed, not repeating its start.
    QVector<QPair<QString, double>> nodes; // Named points on the outline and their distance along it, in outline order.
    QVector<double>         notches;      // Distance along the outline of each notch.
    QHash<QString, QPointF> points;       // Every point of the pattern, in this piece's flat frame.
    QVector<GarmentLine>    lines;        // Internal lines.
    int                     quantity = 1;
    bool                    on_fold  = false;
    // Radians from the draft to this frame: drafted up is this frame turned back.
    double                  turn     = 0.0;
    bool                    grain_known = true;
};

QVector<GarmentPiece> readGarmentPieces(const VContainer* pattern);
bool                  writeGarmentPieces(const QString &path, const QVector<GarmentPiece> &pieces);

#endif // PIECES_EXPORT_H
