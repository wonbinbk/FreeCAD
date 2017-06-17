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

#ifndef PART_LINK_H
#define PART_LINK_H

#include <App/Link.h>
#include "PartFeature.h"

namespace Part {

class LinkBaseExtension : public App::LinkBaseExtension
{
    EXTENSION_PROPERTY_HEADER(Part::LinkBaseExtension);
    typedef App::LinkBaseExtension inherited;
public:
    LinkBaseExtension();
    const std::vector<PropInfo> &getPropertyInfo() const override;

#define LINK_PARAM_SHAPE(...) \
    (Shape, TopoDS_Shape, Part::PropertyPartShape, TopoDS_Shape(), 0, ##__VA_ARGS__)

#define LINK_PARAM_LINK_SHAPE(...) \
    (LinkShape, bool, App::PropertyBool, true, \
     "Automatically build the TopoShape from the linked object", ##__VA_ARGS__)

#define LINK_PARAM_FUSE(...) \
    (Fuse, bool, App::PropertyBool, false, "Set to true to fuse the linked shape", ##__VA_ARGS__)

#define LINK_PARAMS_PART \
    LINK_PARAM(SHAPE)\
    LINK_PARAM(LINK_SHAPE)\
    LINK_PARAM(FUSE)

    enum PartPropIndex {
        PropShape = PropMax,
        PropLinkShape,
        PropFuse,
        PropPartMax,
    };

    BOOST_PP_SEQ_FOR_EACH(LINK_PROP_GET,_,LINK_PARAMS_PART)

    void setProperty(int idx, App::Property *prop) override;

    App::DocumentObjectExecReturn *extensionExecute(void) override;

    void extensionOnDocumentRestored() override;

    bool extensionGetSubObject(App::DocumentObject *&ret, const char *element, const char **subname, 
            PyObject **pyObj, Base::Matrix4D *mat, bool transform, int depth) const override;

protected:
    App::DocumentObjectExecReturn *buildShape(bool);
    TopoDS_Shape combineElements(const TopoDS_Shape &shape);
    TopoDS_Shape getLinkedShape(const char *element=0);
};

//////////////////////////////////////////////////////////////////////////////////////////////
typedef App::ExtensionPythonT<LinkBaseExtension> LinkBaseExtensionPython;
//////////////////////////////////////////////////////////////////////////////////////////////

class LinkExtension : public Part::LinkBaseExtension
{
    EXTENSION_PROPERTY_HEADER(Part::LinkExtension);
    typedef Part::LinkBaseExtension inherited;
public:
    LinkExtension();
    ~LinkExtension();

#define LINK_PARAMS_PART_EXT \
    _LINK_PARAMS_EXT \
    LINK_PARAM_EXT(LINK_SHAPE)\
    LINK_PARAM_EXT(FUSE)

    LINK_PROPS_DEFINE(LINK_PARAMS_PART_EXT)

    void extensionOnDocumentRestored() override {
        LINK_PROPS_SET(LINK_PARAMS_PART_EXT);
        inherited::extensionOnDocumentRestored();
    }
};

//////////////////////////////////////////////////////////////////////////////////////////////
typedef App::ExtensionPythonT<LinkExtension> LinkExtensionPython;
//////////////////////////////////////////////////////////////////////////////////////////////

class Link : public Part::Feature, public Part::LinkExtension
{
    PROPERTY_HEADER_WITH_EXTENSIONS(Part::Feature);
    typedef Part::Feature inherited;

public:
#define LINK_PARAMS_PART_LINK \
    LINK_PARAM_EXT_TYPE(OBJECT, App::PropertyXLink)

    LINK_PROPS_DEFINE(LINK_PARAMS_PART_LINK)

    Link();

    const char* getViewProviderName(void) const override {
        return "PartGui::ViewProviderPartLink";
    }

    void onDocumentRestored() override {
        LINK_PROPS_SET(LINK_PARAMS_PART_LINK);
        inherited::onDocumentRestored();
    }
};

typedef App::FeaturePythonT<Link> LinkPython;

}

#endif

