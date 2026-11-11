//-----------------------------------------------------------------------------
//  @file   geometry_dump.cpp
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

#include "geometry_dump.h"

#include "../../libs/ifc/xml/vabstractpattern.h"
#include "../../libs/vgeometry/varc.h"
#include "../../libs/vgeometry/vabstractcubicbezier.h"
#include "../../libs/vgeometry/vabstractcubicbezierpath.h"
#include "../../libs/vgeometry/vellipticalarc.h"
#include "../../libs/vgeometry/vpointf.h"
#include "../../libs/vgeometry/vspline.h"
#include "../../libs/vmisc/def.h"
#include "../../libs/vpatterndb/variables/vinternalvariable.h"
#include "../../libs/vpatterndb/vcontainer.h"
#include "../../libs/vpatterndb/vpiece.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

namespace
{
//---------------------------------------------------------------------------------------------------------------------
/// @brief varTypeName the name of a formula variable's kind.
/// @param type the kind.
/// @return its name.
QString varTypeName(VarType type)
{
    switch (type)
    {
        case VarType::Measurement:
            return QStringLiteral("measurement");
        case VarType::Variable:
            return QStringLiteral("variable");
        case VarType::LineLength:
            return QStringLiteral("lineLength");
        case VarType::CurveLength:
            return QStringLiteral("curveLength");
        case VarType::CurveCLength:
            return QStringLiteral("curveCLength");
        case VarType::LineAngle:
            return QStringLiteral("lineAngle");
        case VarType::CurveAngle:
            return QStringLiteral("curveAngle");
        case VarType::ArcRadius:
            return QStringLiteral("arcRadius");
        default:
            return QStringLiteral("unknown");
    }
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief curveTypeName the name of a curve's kind.
/// @param type the kind.
/// @return its name.
QString curveTypeName(GOType type)
{
    switch (type)
    {
        case GOType::Arc:
            return QStringLiteral("arc");
        case GOType::EllipticalArc:
            return QStringLiteral("ellipticalArc");
        case GOType::Spline:
            return QStringLiteral("spline");
        case GOType::SplinePath:
            return QStringLiteral("splinePath");
        case GOType::CubicBezier:
            return QStringLiteral("cubicBezier");
        case GOType::CubicBezierPath:
            return QStringLiteral("cubicBezierPath");
        default:
            return QStringLiteral("unknown");
    }
}
} // namespace

//---------------------------------------------------------------------------------------------------------------------
/// @brief writeGeometryDump write every evaluated point, curve, variable and piece of the pattern to a JSON file.
/// Lengths are in the pattern's unit, y down as Seamly2D stores it, angles in degrees.
/// @param path the JSON file.
/// @param data the evaluated pattern.
/// @param doc the pattern document, for the tool and draft block that made each object.
/// @return false if the file can't be written.
bool writeGeometryDump(const QString &path, const VContainer *data, VAbstractPattern *doc)
{
    const Unit unit = *data->GetPatternUnit();
    const auto len = [unit](qreal pixels)
    {
        return FromPixel(pixels, unit);
    };
    const auto xy = [len](const QPointF &point)
    {
        return QJsonArray{len(point.x()), len(point.y())};
    };
    const auto polyline = [xy](const QVector<QPointF> &points)
    {
        QJsonArray array;
        for (const QPointF &point : points)
        {
            array.append(xy(point));
        }
        return array;
    };
    const auto namedPoint = [xy](const VPointF &point)
    {
        return QJsonObject{{QStringLiteral("name"), point.name()}, {QStringLiteral("xy"), xy(point.toQPointF())}};
    };
    const auto bezier = [namedPoint](const VAbstractCubicBezier &curve)
    {
        return QJsonObject{{QStringLiteral("p1"), namedPoint(curve.GetP1())},
                           {QStringLiteral("p2"), namedPoint(curve.GetP2())},
                           {QStringLiteral("p3"), namedPoint(curve.GetP3())},
                           {QStringLiteral("p4"), namedPoint(curve.GetP4())}};
    };

    const QHash<quint32, QSharedPointer<VGObject>> *objects = data->DataGObjects();
    QList<quint32> ids = objects->keys();
    std::sort(ids.begin(), ids.end());

    QJsonArray points;
    QJsonArray curves;
    for (const quint32 id : ids)
    {
        const QSharedPointer<VGObject> &object = objects->value(id);
        if (object->getMode() == Draw::Modeling)
        {
            continue; // A piece's own copy of a draft object.
        }

        QJsonObject json{{QStringLiteral("id"), static_cast<qint64>(id)}, {QStringLiteral("name"), object->name()}};
        const QDomElement element = doc->elementById(id);
        if (!element.isNull())
        {
            json[QStringLiteral("element")] = element.tagName();
            json[QStringLiteral("tool")] = element.attribute(QStringLiteral("type"));
        }
        const QString block = doc->getToolDraftBlockName(id);
        if (!block.isEmpty())
        {
            json[QStringLiteral("block")] = block;
        }

        if (object->getType() == GOType::Point)
        {
            const auto point = object.staticCast<VPointF>();
            json[QStringLiteral("x")] = len(point->x());
            json[QStringLiteral("y")] = len(point->y());
            points.append(json);
            continue;
        }

        const auto curve = object.dynamicCast<VAbstractCurve>();
        if (curve.isNull())
        {
            continue;
        }
        json[QStringLiteral("type")] = curveTypeName(curve->getType());
        json[QStringLiteral("length")] = len(curve->GetLength());
        json[QStringLiteral("startAngle")] = curve->GetStartAngle();
        json[QStringLiteral("endAngle")] = curve->GetEndAngle();
        switch (curve->getType())
        {
            case GOType::Arc:
            {
                const auto arc = curve.staticCast<VArc>();
                json[QStringLiteral("center")] = namedPoint(arc->GetCenter());
                json[QStringLiteral("radius")] = len(arc->GetRadius());
                break;
            }
            case GOType::EllipticalArc:
            {
                const auto arc = curve.staticCast<VEllipticalArc>();
                json[QStringLiteral("center")] = namedPoint(arc->GetCenter());
                json[QStringLiteral("radius1")] = len(arc->GetRadius1());
                json[QStringLiteral("radius2")] = len(arc->GetRadius2());
                json[QStringLiteral("rotation")] = arc->getRotationAngle();
                break;
            }
            case GOType::Spline:
            case GOType::CubicBezier:
                json[QStringLiteral("segments")] = QJsonArray{bezier(*curve.staticCast<VAbstractCubicBezier>())};
                break;
            case GOType::SplinePath:
            case GOType::CubicBezierPath:
            {
                const auto path = curve.staticCast<VAbstractCubicBezierPath>();
                QJsonArray segments;
                for (qint32 i = 1; i <= path->CountSubSpl(); ++i)
                {
                    segments.append(bezier(path->GetSpline(i)));
                }
                json[QStringLiteral("segments")] = segments;
                break;
            }
            default:
                break;
        }
        json[QStringLiteral("points")] = polyline(curve->getPoints());
        curves.append(json);
    }

    const QHash<QString, QSharedPointer<VInternalVariable>> *variables = data->DataVariables();
    QStringList names = variables->keys();
    names.sort();
    QJsonArray variableList;
    for (const QString &name : names)
    {
        const VInternalVariable &variable = *variables->value(name);
        variableList.append(QJsonObject{{QStringLiteral("name"), name},
                                        {QStringLiteral("type"), varTypeName(variable.GetType())},
                                        {QStringLiteral("value"), variable.GetValue()}});
    }

    const QHash<quint32, VPiece> *pieces = data->DataPieces();
    QList<quint32> pieceIds = pieces->keys();
    std::sort(pieceIds.begin(), pieceIds.end());
    QJsonArray pieceList;
    for (const quint32 id : pieceIds)
    {
        const VPiece &piece = pieces->value(id);
        const QVector<QPointF> seamLine = piece.mainPathPoints(data);
        const QVector<QPointF> allowance = piece.seamAllowancePoints(data);
        const QVector<QPointF> &outline = allowance.isEmpty() ? seamLine : allowance;
        pieceList.append(QJsonObject{{QStringLiteral("id"), static_cast<qint64>(id)},
                                     {QStringLiteral("name"), piece.GetName()},
                                     {QStringLiteral("seamAllowance"), !allowance.isEmpty()},
                                     {QStringLiteral("seamAllowanceWidth"), piece.GetSAWidth()},
                                     {QStringLiteral("seamLine"), polyline(seamLine)},
                                     {QStringLiteral("outline"), polyline(outline)}});
    }

    const QJsonObject root{{QStringLiteral("unit"), UnitsToStr(unit)},
                           {QStringLiteral("draftBlock"), doc->getActiveDraftBlockName()},
                           {QStringLiteral("points"), points},
                           {QStringLiteral("curves"), curves},
                           {QStringLiteral("variables"), variableList},
                           {QStringLiteral("pieces"), pieceList}};

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
    {
        return false;
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    return file.commit();
}
