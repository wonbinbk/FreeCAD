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
#include <Gui/BitmapFactory.h>
#include "ViewProviderPartLink.h"

using namespace PartGui;

PROPERTY_SOURCE(PartGui::ViewProviderPartLink, Gui::ViewProviderLink)

ViewProviderPartLink::ViewProviderPartLink()
{
}

QIcon ViewProviderPartLink::getIconDefault() const {
    const char *pixmap;
    if(hasElements())
        pixmap = "links";
    else if(linkType == LinkTypeSubs)
        pixmap = "Tree_Part_LinkSub";
    else
        pixmap = "Tree_Part_Link";
    return Gui::BitmapFactory().pixmap(pixmap);
}

QPixmap ViewProviderPartLink::getOverlayPixmap() const
{
    static QPixmap px[6];
    if(px[0].isNull()) {
        // right top pointing arrow for normal link
        const char * const feature_link_xpm[]={
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
        px[0] = QPixmap(feature_link_xpm);

        // left top pointing arrow for xlink
        const char * const feature_xlink_xpm[]={
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
        px[1] = QPixmap(feature_xlink_xpm);

        // double arrow for link subs
        const char * const feature_linksub_xpm[]={
            "8 8 3 1",
            ". c None",
            "# c #000000",
            "a c #aaf254",
            "########",
            "##aaaaa#",
            "######a#",
            "aaaaa#a#",
            "##aaa#a#",
            "#aa#a#a#",
            "aa##a###",
            "########"};
        px[2] = QPixmap(feature_linksub_xpm);

        const char * const feature_links_xpm[]={
            "8 8 3 1",
            ". c None",
            "# c #000000",
            "a c #b0e0e6",
            "########",
            "##aaaaa#",
            "####aaa#",
            "###aaaa#",
            "##aaa#a#",
            "#aaa##a#",
            "#aa#####",
            "########"};
        px[3] = QPixmap(feature_links_xpm);

        const char * const feature_xlinks_xpm[]={
            "8 8 3 1",
            ". c None",
            "# c #000000",
            "a c #b0e0e6",
            "########",
            "#aaaaa##",
            "#aaa####",
            "#aaaa###",
            "#a#aaa##",
            "#a##aaa#",
            "#####aa#",
            "########"};
        px[4] = QPixmap(feature_xlinks_xpm);

        const char * const feature_linksubs_xpm[]={
            "8 8 3 1",
            ". c None",
            "# c #000000",
            "a c #aaf254",
            "########",
            "##aaaaa#",
            "######a#",
            "aaaaa#a#",
            "##aaa#a#",
            "#aa#a#a#",
            "aa##a###",
            "########"};
        px[5] = QPixmap(feature_linksubs_xpm);
    }
    int index = 0;
    if(linkType == LinkTypeSubs) 
        index = 2;
    else if(linkType == LinkTypeX)
        index = 1;
    if(handle.getSize())
        index += 3;
    return px[index];
}
