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

#ifndef _PreComp_
#endif

#include "Link.h"

using namespace App;

PROPERTY_SOURCE(App::Link, App::DocumentObject)

Link::Link(void)
{
    // Note: Because PropertyView will merge linked object's properties into
    // ours, we set group name as ' Link' with a leading space to try to make
    // our group before others
    ADD_PROPERTY_TYPE(LinkedObject, (0), " Link", Prop_None, "Linked object");
    ADD_PROPERTY_TYPE(LinkMoveChild, (false), " Link", Prop_None, 
            "Remove child object(s) from root");

#define COMMON_PROPERTY \
    ADD_PROPERTY_TYPE(LinkTransform, (false), " Link", Prop_None, \
            "Link child placement. If false, the child object's placement is ignored.");\
    ADD_PROPERTY_TYPE(LinkScale,(Base::Vector3d(1.0,1.0,1.0))," Link",Prop_None,\
            "Scale factor for view provider. It does not actually scale the geometry data.");\
    ADD_PROPERTY_TYPE(LinkPlacement,(Base::Placement())," Link",Prop_None,\
            "The placement of this link. If LinkTransform is 'true', then the final\n"\
            "placement is the composite of this and the child's placement.")

    COMMON_PROPERTY;
}

Link::~Link(void)
{
}

/////////////////////////////////////////////////////////

PROPERTY_SOURCE(App::XLink, App::DocumentObject)

XLink::XLink(void)
{
    ADD_PROPERTY_TYPE(LinkedFile,("")," Link", Prop_None, 
            "Linked document file path. It can be a relative path");
    ADD_PROPERTY_TYPE(LinkedObjectName,(""), " Link", Prop_None, 
            "Linked object name in the linked document");
    COMMON_PROPERTY;
}

XLink::~XLink(void)
{
}

