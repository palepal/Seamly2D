//-----------------------------------------------------------------------------
//  @file   macos_window.mm
//  @author palepal
//  @date   31 Oct, 2026
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

#include "macos_window.h"

#include <QColor>
#include <QWidget>

#import <AppKit/AppKit.h>

//---------------------------------------------------------------------------------------------------------------------
/// @brief setTitleBarColor paint the title bar of a window in the theme background instead of the system gray.
/// @param window top level widget.
/// @param color theme background.
void setTitleBarColor(QWidget *window, const QColor &color)
{
    if (window == nullptr)
    {
        return;
    }

    NSView *view = reinterpret_cast<NSView *>(window->winId());
    NSWindow *nsWindow = [view window];
    if (nsWindow == nil)
    {
        return;
    }

    // A transparent title bar shows the window background, which Qt leaves to us
    nsWindow.titlebarAppearsTransparent = YES;
    nsWindow.backgroundColor = [NSColor colorWithSRGBRed:color.redF()
                                                   green:color.greenF()
                                                    blue:color.blueF()
                                                   alpha:1.0];
}
