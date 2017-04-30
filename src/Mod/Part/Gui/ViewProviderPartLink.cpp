/****************************************************************************
 *   Copyright (c) 2017 Zheng, Lei (realthunder) <realthunder.dev@gmail.com>*
 *                                                                          *
 *   This file is part of the FreeCAD CAx development system.               *
 *                                                                          *
 *   This library is free software; you can redistribute it and/or          *
 *   modify it under the terms of the GNU Library General Public            *
 *   License as published by the Free Software Foundation; either           *
 *   version 2 of the License, or (at your option) any later version.       *
 *                                                                          *
 *   This library  is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of         *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the          *
 *   GNU Library General Public License for more details.                   *
 *                                                                          *
 *   You should have received a copy of the GNU Library General Public      *
 *   License along with this library; see the file COPYING.LIB. If not,     *
 *   write to the Free Software Foundation, Inc., 59 Temple Place,          *
 *   Suite 330, Boston, MA  02111-1307, USA                                 *
 *                                                                          *
 ****************************************************************************/

#include "PreCompiled.h"
#include "ViewProviderPartLink.h"

using namespace PartGui;

PROPERTY_SOURCE(PartGui::ViewProviderPartLink, Gui::ViewProviderLink)

ViewProviderPartLink::ViewProviderPartLink()
{
    sPixmap = "Tree_Part_Link";
}

const char * const * ViewProviderPartLink::getOverlayPixmap(bool xlink) const
{
    // right top pointing arrow for normal link
    static const char * const feature_link_xpm[]={
        "8 8 3 1",
        ". c None",
        "# c #000000",
        "a c #aaf254",
        "########",
        "##aaaaa#",
        "####aaa#",
        "###aaaa#",
        "##aaa#a#",
        "#aaa##a#",
        "#aa#####",
        "########"};

    // left top pointing arrow for xlink
    static const char * const feature_xlink_xpm[]={
        "8 8 3 1",
        ". c None",
        "# c #000000",
        "a c #aaf254",
        "########",
        "#aaaaa##",
        "#aaa####",
        "#aaaa###",
        "#a#aaa##",
        "#a##aaa#",
        "#####aa#",
        "########"};

    return xlink?feature_xlink_xpm:feature_link_xpm;
}
