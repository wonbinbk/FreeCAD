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
#include "FeaturePython.h"
#include "PropertyLinks.h"
#include "DocumentObjectExtension.h"

namespace App
{

class AppExport LinkExtension : public App::DocumentObjectExtension
{
    EXTENSION_PROPERTY_HEADER(App::LinkExtension);

public:
    LinkExtension();
    virtual ~LinkExtension();

    PropertyXLink LinkedObject;
    PropertyPlacement LinkPlacement;
    PropertyVector LinkScale;
    // PropertyBool LinkMoveChild;
    PropertyBool LinkTransform;

    PyObject* getExtensionPyObject(void) override;

    PyObject *extensionGetPySubObject(const char *element, 
            const Base::Matrix4D &mat, bool transform) const override;

protected:
    DocumentObject *getLinkedObjectExt(bool recurse, Base::Matrix4D *mat, bool transform);
};

class AppExport Link : public App::DocumentObject, public App::LinkExtension
{
    PROPERTY_HEADER_WITH_EXTENSIONS(App::Link);
public:
    Link(void);

    const char* getViewProviderName(void) const override{
        return "Gui::ViewProviderLink";
    }

    DocumentObject *getLinkedObject(bool recurse, Base::Matrix4D *mat, bool transform) override;

    PyObject* getPyObject(void) override;
};

// typedef FeaturePythonT<Link> LinkPython;

} //namespace App


#endif // APP_LINK_H
