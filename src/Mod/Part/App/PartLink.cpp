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
# include <BRep_Builder.hxx>
# include <TopoDS_Compound.hxx>
#endif

#include <Base/Console.h>
#include <App/GeoFeatureGroupExtension.h>
#include "PartLink.h"

FC_LOG_LEVEL_INIT("PartLink", false, true);

using namespace Part;

//---------------------------------------------------------------------
PROPERTY_SOURCE_WITH_EXTENSIONS(Part::Link, Part::Feature)

Link::Link() {
    ADD_PROPERTY_TYPE(LinkShape,(true)," Link",App::Prop_None,"Enable this to generate TopoShape from linked object");
    App::LinkExtension::initExtension(this);
    Placement.setStatus(App::Property::Status::Hidden, true);
    Shape.setStatus(App::Property::Immutable,true);
    Shape.setStatus(App::Property::Transient,true);
}

App::DocumentObjectExecReturn *Link::execute(void) {
    return buildShape(false);
}


App::DocumentObject *Link::getLinkedObject(bool recurse, Base::Matrix4D *mat, bool transform)
{
    return getLinkedObjectExt(recurse,mat,transform);
}


App::DocumentObjectExecReturn *Link::buildShape(bool silent) {
    if(!LinkShape.getValue()) return 0;

#define EXEC_RET(_txt) do{\
        if(silent) return nullptr;\
        return new App::DocumentObjectExecReturn(_txt);\
    }while(0)

    Base::Matrix4D mat;
    auto linked = getLinkedObject(true, &mat, false);
    if(!linked || linked==this)
        EXEC_RET("no object linked");

    FC_LOG("update part link " << getNameInDocument() << ": " << mat.analyse());

    auto ext = linked->getExtensionByType<App::GeoFeatureGroupExtension>(true);
    std::vector<App::DocumentObject *> children;
    if(!ext) {
        auto prop = linked->getPropertyByName("Shape");
        TopoDS_Shape shape;
        if(prop && prop->isDerivedFrom(PropertyPartShape::getClassTypeId()))
            shape = static_cast<PropertyPartShape*>(prop)->getValue();
        if(shape.IsNull())
            EXEC_RET("No shape found");
        shape.Location(TopLoc_Location());
        Shape.setValue(TopoShape(shape).transformGShape(mat));
        return 0;
    }
    // Unfortunately, we can't support shape linking GeoFeatureGroup yet, because its
    // compound shape apperance depends on the visibility property of its children's 
    // view provider. It should have been stored inside GeoFeatureGroup somehow
#if 1
    EXEC_RET("GeoFeatureGroup shape linking is not supported! Please set LinkShape to False.");
#else
    children = ext->getGeoSubObjects();

    BRep_Builder builder;
    TopoDS_Compound compound;
    builder.MakeCompound(compound);
    int count = 0;
    for(auto child : children) {
        if(!child || !child->getNameInDocument())
            continue;
        Base::Matrix4D cmat;
        child = child->getLinkedObject(true,&cmat,true);
        if(!child || !child->getNameInDocument())
            continue;

        auto prop = child->getPropertyByName("Shape");
        if(!prop || !prop->isDerivedFrom(PropertyPartShape::getClassTypeId()))
            continue;

        TopoDS_Shape s = static_cast<PropertyPartShape*>(prop)->getValue();
        if(s.IsNull()) continue;

        builder.Add(compound,TopoShape(s).transformGShape(cmat));
        ++count;
    }

    if(count==0)
        EXEC_RET("No shape found");
    Shape.setValue(TopoShape(compound).transformGShape(mat));
    return 0;
#endif
}

App::DocumentObject *Link::getSubObject(const char *element, const char **subname, 
        PyObject **pyObj, Base::Matrix4D *mat, bool transform) const
{
    // bypass Part::Feature's handling
    return DocumentObject::getSubObject(element,subname,pyObj,mat,transform);
}

void Link::onChanged(const App::Property* prop)
{
    if (prop == &LinkPlacement) {
        if(!LinkPlacement.testStatus(App::Property::User3)) {
            // prevent recuse
            Placement.setStatus(App::Property::User3,true);
            Placement.setValue(LinkPlacement.getValue());
            Placement.setStatus(App::Property::User3,false);
        }
    } else if(prop == &Placement) {
        if(!Placement.testStatus(App::Property::User3)) {
            LinkPlacement.setStatus(App::Property::User3,true);
            LinkPlacement.setValue(Placement.getValue());
            LinkPlacement.setStatus(App::Property::User3,false);
        }
        // bypass Part::Feature handling
        GeoFeature::onChanged(prop);
        return;
    }
    
    Feature::onChanged(prop);
}

void Link::onDocumentRestored() {
    buildShape(true);
}
