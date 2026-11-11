//-----------------------------------------------------------------------------
//  @file   pieces_export.cpp
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

#include "pieces_export.h"

#include "../../libs/ifc/exception/vexceptionbadid.h"
#include "../../libs/qmuparser/qmuparsererror.h"
#include "../../libs/vgeometry/vabstractcurve.h"
#include "../../libs/vgeometry/vpointf.h"
#include "../../libs/vmisc/def.h"
#include "../../libs/vpatterndb/calculator.h"
#include "../../libs/vpatterndb/floatItemData/vgrainlinedata.h"
#include "../../libs/vpatterndb/floatItemData/vpiecelabeldata.h"
#include "../../libs/vpatterndb/vcontainer.h"
#include "../../libs/vpatterndb/vpiece.h"
#include "../../libs/vpatterndb/vpiecenode.h"
#include "../../libs/vpatterndb/vpiecepath.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLineF>
#include <QSaveFile>

#include <algorithm>
#include <cmath>

namespace
{
constexpr double on_outline_mm = 0.1;  // A node this close to the outline is on it.

//---------------------------------------------------------------------------------------------------------------------
/// @brief grainAngle the direction of a piece's grainline from bottom to top.
/// @param piece the piece.
/// @param pattern the pattern's data.
/// @param angle receives the angle in degrees, counter-clockwise, as QLineF measures it.
/// @return false if the piece has no usable grainline.
bool grainAngle(const VPiece &piece, const VContainer* pattern, double &angle)
{
    const VGrainlineData &grain = piece.GetGrainlineGeometry();
    if (grain.topAnchorPoint() != NULL_ID && grain.bottomAnchorPoint() != NULL_ID)
    {
        try
        {
            const auto top = pattern->GeometricObject<VPointF>(grain.topAnchorPoint());
            const auto bottom = pattern->GeometricObject<VPointF>(grain.bottomAnchorPoint());
            const QLineF line(static_cast<QPointF>(*bottom), static_cast<QPointF>(*top));
            if (line.length() > 0.0)
            {
                angle = line.angle();
                return true;
            }
        }
        catch (const VExceptionBadId &)
        {
            return false;
        }
    }

    if (grain.getRotation().isEmpty())
    {
        return false;
    }
    try
    {
        Calculator calculator;
        angle = calculator.EvalFormula(pattern->DataVariables(), grain.getRotation());
        return true;
    }
    catch (const qmu::QmuParserError &)
    {
        return false;
    }
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief toCloth a point from the draft's pixels with y down, to millimetres with y up and the grain along y.
/// @param point the point in pixels.
/// @param cosine cosine of the turn that brings the grain to y.
/// @param sine sine of the same turn.
/// @return the point in cloth coordinates.
QPointF toCloth(const QPointF &point, double cosine, double sine)
{
    const double x = FromPixel(point.x(), Unit::Mm);
    const double y = -FromPixel(point.y(), Unit::Mm);
    return QPointF(x * cosine - y * sine, x * sine + y * cosine);
}

} // namespace

//---------------------------------------------------------------------------------------------------------------------
/// @brief readGarmentPieces every piece in the layout as plain data: its outline, the named points on it, its notches,
/// lines and every point of the pattern, all turned so the grain is the piece's y. A grain is a line, not an arrow, so
/// the turn is never more than a quarter turn and a piece stays the way up it was drafted.
/// @param pattern the pattern's data.
/// @return the pieces, in the order of their ids so that the same pattern always gives the same result.
QVector<GarmentPiece> readGarmentPieces(const VContainer* pattern)
{
    const QHash<quint32, VPiece> *pieces = pattern->DataPieces();
    QVector<quint32> ids;
    ids.reserve(pieces->size());
    for (auto i = pieces->constBegin(); i != pieces->constEnd(); ++i)
    {
        if (i.value().isInLayout())
        {
            ids.append(i.key());
        }
    }
    std::sort(ids.begin(), ids.end());

    // Every named point once. A point used in a piece is a copy of the draft's with the same name, and the lowest id
    // is the draft's.
    const QHash<quint32, QSharedPointer<VGObject>> *objects = pattern->DataGObjects();
    QVector<quint32> object_ids;
    object_ids.reserve(objects->size());
    for (auto i = objects->constBegin(); i != objects->constEnd(); ++i)
    {
        if (i.value()->getType() == GOType::Point)
        {
            object_ids.append(i.key());
        }
    }
    std::sort(object_ids.begin(), object_ids.end());
    QVector<QPair<QString, QPointF>> named_points;
    QSet<QString> seen;
    for (const quint32 id : object_ids)
    {
        const QSharedPointer<VPointF> point = objects->value(id).staticCast<VPointF>();
        if (!point->name().isEmpty() && !seen.contains(point->name()))
        {
            seen.insert(point->name());
            named_points.append(qMakePair(point->name(), static_cast<QPointF>(*point)));
        }
    }

    QVector<GarmentPiece> out;
    for (const quint32 id : ids)
    {
        const VPiece piece = pieces->value(id);

        GarmentPiece garment_piece;
        garment_piece.name = piece.GetName();
        garment_piece.on_fold = piece.GetPatternPieceData().IsOnFold();
        garment_piece.quantity = piece.GetPatternPieceData().GetQuantity();

        double angle = 90.0;
        garment_piece.grain_known = grainAngle(piece, pattern, angle);
        double turn = 90.0 - angle;
        while (turn > 90.0)
        {
            turn -= 180.0;
        }
        while (turn <= -90.0)
        {
            turn += 180.0;
        }
        garment_piece.turn = qDegreesToRadians(turn);
        const double cosine = std::cos(garment_piece.turn);
        const double sine = std::sin(garment_piece.turn);

        for (const QPointF &point : piece.mainPathPoints(pattern))
        {
            const QPointF flat = toCloth(point, cosine, sine);
            if (garment_piece.outline.isEmpty() || QLineF(garment_piece.outline.last(), flat).length() > 1e-6)
            {
                garment_piece.outline.append(flat);
            }
        }
        if (garment_piece.outline.size() > 1
            && QLineF(garment_piece.outline.first(), garment_piece.outline.last()).length() <= 1e-6)
        {
            garment_piece.outline.removeLast();
        }

        // Walk the nodes along the outline, each search going on from the last, so a place the outline passes twice is
        // read at the right turn.
        QVector<double> arcs;
        arcs.reserve(garment_piece.outline.size() + 1);
        double walked = 0.0;
        for (int i = 0; i < garment_piece.outline.size(); ++i)
        {
            arcs.append(walked);
            walked += QLineF(garment_piece.outline.at(i),
                             garment_piece.outline.at((i + 1) % garment_piece.outline.size())).length();
        }
        int search_from = 0;
        const QVector<VPieceNode> nodes = piece.GetPath().getNodes();
        for (const VPieceNode &node : nodes)
        {
            if (node.isExcluded() || node.GetTypeTool() != Tool::NodePoint)
            {
                continue;
            }
            const QSharedPointer<VPointF> point = pattern->GeometricObject<VPointF>(node.GetId());
            const QPointF flat = toCloth(static_cast<QPointF>(*point), cosine, sine);
            const int count = garment_piece.outline.size();
            for (int k = 0; k < count; ++k)
            {
                const int i = (search_from + k) % count;
                const QPointF &a = garment_piece.outline.at(i);
                const QPointF &b = garment_piece.outline.at((i + 1) % count);
                const QPointF along = b - a;
                const double squared = QPointF::dotProduct(along, along);
                const double t = squared > 0.0 ? qBound(0.0, QPointF::dotProduct(flat - a, along) / squared, 1.0) : 0.0;
                if (QLineF(flat, a + along * t).length() < on_outline_mm)
                {
                    const double arc = arcs.at(i) + t * std::sqrt(squared);
                    if (!point->name().isEmpty())
                    {
                        garment_piece.nodes.append(qMakePair(point->name(), arc));
                    }
                    if (node.isNotch())
                    {
                        garment_piece.notches.append(arc);
                    }
                    search_from = i;
                    break;
                }
            }
        }

        for (const QPair<QString, QPointF> &named : named_points)
        {
            garment_piece.points.insert(named.first, toCloth(named.second, cosine, sine));
        }

        for (const quint32 path_id : piece.getInternalPaths())
        {
            const VPiecePath path = pattern->getPiecePath(path_id);
            if (path.getType() != PiecePathType::InternalPath)
            {
                continue;
            }
            GarmentLine line;
            line.name = path.getName();
            for (const QPointF &point : path.PathPoints(pattern))
            {
                line.points.append(toCloth(point, cosine, sine));
            }
            for (const VPointF &point : path.PathNodePoints(pattern))
            {
                line.nodes.append(point.name());
            }
            garment_piece.lines.append(line);
        }
        out.append(garment_piece);
    }
    return out;
}


//---------------------------------------------------------------------------------------------------------------------
/// @brief writeGarmentPieces pieces as a file for Cutalyst, which reads them with its own reader.
/// @param path where to write.
/// @param pieces the pieces.
/// @return false if the file can't be written.
bool writeGarmentPieces(const QString &path, const QVector<GarmentPiece> &pieces)
{
    const auto pointArray = [](const QVector<QPointF> &points)
    {
        QJsonArray array;
        for (const QPointF &point : points)
        {
            array.append(point.x());
            array.append(point.y());
        }
        return array;
    };
    QJsonArray list;
    for (const GarmentPiece &piece : pieces)
    {
        QJsonObject object;
        object[QLatin1String("name")] = piece.name;
        object[QLatin1String("outline")] = pointArray(piece.outline);
        QJsonArray nodes;
        for (const QPair<QString, double> &node : piece.nodes)
        {
            nodes.append(node.first);
            nodes.append(node.second);
        }
        object[QLatin1String("nodes")] = nodes;
        QJsonArray notches;
        for (const double notch : piece.notches)
        {
            notches.append(notch);
        }
        object[QLatin1String("notches")] = notches;
        QJsonObject points;
        for (auto i = piece.points.constBegin(); i != piece.points.constEnd(); ++i)
        {
            points[i.key()] = QJsonArray{i.value().x(), i.value().y()};
        }
        object[QLatin1String("points")] = points;
        QJsonArray lines;
        for (const GarmentLine &line : piece.lines)
        {
            lines.append(QJsonObject{{QLatin1String("name"), line.name},
                                     {QLatin1String("points"), pointArray(line.points)},
                                     {QLatin1String("nodes"), QJsonArray::fromStringList(line.nodes)}});
        }
        object[QLatin1String("lines")] = lines;
        object[QLatin1String("quantity")] = piece.quantity;
        object[QLatin1String("on_fold")] = piece.on_fold;
        object[QLatin1String("turn")] = piece.turn;
        object[QLatin1String("grain_known")] = piece.grain_known;
        list.append(object);
    }
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
    {
        return false;
    }
    file.write(QJsonDocument(list).toJson(QJsonDocument::Compact));
    return file.commit();
}
