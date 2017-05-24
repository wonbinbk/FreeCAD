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

    PropertyPlacement LinkPlacement;
    PropertyVector LinkScale;
    PropertyBool LinkTransform;
    PropertyBool LinkRecomputed;

    DocumentObject *extensionGetSubObject(const char *element, 
            const char **subname, PyObject **pyObj, 
            Base::Matrix4D *mat, bool transform) const override;

    virtual App::DocumentObjectExecReturn *extensionExecute(void) override;
    virtual void extensionOnChanged(const Property* p) override;

    void setProperties(Property *link, PropertyPlacement *pla);

protected:
    DocumentObject *getLink() const;
    DocumentObject *getLinkedObjectExt(bool recurse, Base::Matrix4D *mat, bool transform);

private:
    Property *propLink;
    PropertyPlacement *propPlacement;
};

class AppExport Link : public App::DocumentObject, public App::LinkExtension
{
    PROPERTY_HEADER_WITH_EXTENSIONS(App::Link);
public:
    PropertyPlacement Placement;
    PropertyXLink LinkedObject;

    Link(void);

    const char* getViewProviderName(void) const override{
        return "Gui::ViewProviderLink";
    }

    DocumentObject *getLinkedObject(bool recurse, Base::Matrix4D *mat, bool transform) override;
};

class AppExport LinkSub : public App::DocumentObject, public App::LinkExtension
{
    PROPERTY_HEADER_WITH_EXTENSIONS(App::LinkSub);
public:
    PropertyPlacement Placement;
    PropertyLinkSub LinkedSubs;

    LinkSub(void);

    const char* getViewProviderName(void) const override{
        return "Gui::ViewProviderLink";
    }

    DocumentObject *getLinkedObject(bool recurse, Base::Matrix4D *mat, bool transform) override;
};


// typedef FeaturePythonT<Link> LinkPython;

} //namespace App


#endif // APP_LINK_H
