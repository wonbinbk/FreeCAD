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

#ifndef APP_LINK_H
#define APP_LINK_H

#include "DocumentObject.h"
#include "PropertyLinks.h"
#include "PropertyGeo.h"

namespace App
{

class AppExport Link : public App::DocumentObject
{
    PROPERTY_HEADER(App::DocumentObject);

public:
    Link(void);
    virtual ~Link();

    virtual const char* getViewProviderName(void) const {
        return "Gui::ViewProviderLink";
    }

    PropertyLink LinkedObject;
    PropertyPlacement LinkPlacement;
    PropertyVector LinkScale;
    PropertyBool LinkMoveChild;
    PropertyBool LinkTransform;
};

class AppExport XLink : public App::DocumentObject
{
    PROPERTY_HEADER(App::DocumentObject);

public:
    XLink(void);
    virtual ~XLink();

    virtual const char* getViewProviderName(void) const {
        return "Gui::ViewProviderLink";
    }

    PropertyString LinkedFile;
    PropertyString LinkedObjectName;
    PropertyPlacement LinkPlacement;
    PropertyVector LinkScale;
    PropertyBool LinkTransform;
};

} //namespace App


#endif // APP_LINK_H
