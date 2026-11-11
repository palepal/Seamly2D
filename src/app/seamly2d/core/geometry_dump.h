//-----------------------------------------------------------------------------
//  @file   geometry_dump.h
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

#ifndef GEOMETRY_DUMP_H
#define GEOMETRY_DUMP_H

#include <QString>

class VAbstractPattern;
class VContainer;

bool writeGeometryDump(const QString &path, const VContainer *data, VAbstractPattern *doc);

#endif // GEOMETRY_DUMP_H
