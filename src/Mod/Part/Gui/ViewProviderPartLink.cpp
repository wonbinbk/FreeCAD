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
    else if(hasLinkSubs())
        pixmap = "Tree_Part_LinkSub";
    else
        pixmap = "Tree_Part_Link";
    return Gui::BitmapFactory().pixmap(pixmap);
}

QPixmap ViewProviderPartLink::getOverlayPixmap() const
{
#define LINK_ICON_COUNT 4
    static QPixmap px[LINK_ICON_COUNT];
    static QPixmap px2[LINK_ICON_COUNT];

    if(px[0].isNull()) {
        int i = 0;
        const char **xpm[LINK_ICON_COUNT];

        // right top pointing arrow for normal link
        const char *xpm_link[] = 
           {"8 8 3 1",
            ". c None",
            "# c #000000",
            "a c #ffffff",
            "########",
            "##aaaaa#",
            "####aaa#",
            "###aaaa#",
            "##aaa#a#",
            "#aaa##a#",
            "#aa#####",
            "########"};
        xpm[i++] = xpm_link;

        // left top pointing arrow for xlink
        const char *xpm_xlink[] =
           {"8 8 3 1",
            ". c None",
            "# c #000000",
            "a c #ffffff",
            "########",
            "#aaaaa##",
            "#aaa####",
            "#aaaa###",
            "#a#aaa##",
            "#a##aaa#",
            "#####aa#",
            "########"};
        xpm[i++] = xpm_xlink;

        // double right arrow for link subs
        const char *xpm_linksub[] = 
           {"8 8 3 1",
            ". c None",
            "# c #000000",
            "a c #ffffff",
            "########",
            "##aaaaa#",
            "######a#",
            "aaaaa#a#",
            "##aaa#a#",
            "#aa#a#a#",
            "aa##a###",
            "########"};
        xpm[i++] = xpm_linksub;

        // double left arrow for xlink subs
        const char *xpm_xlinksub[] =
           {"8 8 3 1",
            ". c None",
            "# c #000000",
            "a c #ffffff",
            "########",
            "#aaaaa##",
            "#a######",
            "#a#aaaaa",
            "#a#aaa##",
            "#a#a#aa#",
            "###a##aa",
            "########"};
        xpm[i++] = xpm_xlinksub;
        for(int i=0;i<LINK_ICON_COUNT;++i){
            const char *replace_color = "a c #aaf254";
            xpm[i][3] = replace_color;
            px[i] = QPixmap(xpm[i]);

            const char *replace_color2 = "a c #ffd700";
            xpm[i][3] = replace_color2;
            px2[i] = QPixmap(xpm[i]);
        }
    }
    int index;
    switch(linkType) {
    case LinkTypeX:
        index = 1;
        break;
    case LinkTypeSubs:
        index = 2;
        break;
    case LinkTypeXSubs:
        index = 3;
        break;
    default:
        index = 0;
    }
    return hasElements()?px2[index]:px[index];
}
