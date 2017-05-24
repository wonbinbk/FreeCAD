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
PROPERTY_SOURCE_WITH_EXTENSIONS(Part::LinkBase, Part::Feature)

LinkBase::LinkBase() {
    ADD_PROPERTY_TYPE(LinkShape,(true)," Link",App::Prop_None,"Enable this to generate TopoShape from linked object");
    App::LinkExtension::initExtension(this);
    Placement.setStatus(App::Property::Status::Hidden, true);
    Shape.setStatus(App::Property::Immutable,true);
    Shape.setStatus(App::Property::Transient,true);
}

App::DocumentObjectExecReturn *LinkBase::execute(void) {
    auto ret = buildShape(false);
    if(ret) return ret;
    return Part::Feature::execute();
}

App::DocumentObject *LinkBase::getLinkedObject(bool recurse, Base::Matrix4D *mat, bool transform)
{
    return getLinkedObjectExt(recurse,mat,transform);
}

App::DocumentObject *LinkBase::getSubObject(const char *element, const char **subname, 
        PyObject **pyObj, Base::Matrix4D *mat, bool transform) const
{
    // bypass Part::Feature's handling
    return DocumentObject::getSubObject(element,subname,pyObj,mat,transform);
}

void LinkBase::onChanged(const App::Property* prop)
{
    if(prop == &Placement) {
        // bypass Part::Feature handling
        GeoFeature::onChanged(prop);
    }else
        Feature::onChanged(prop);
}

void LinkBase::onDocumentRestored() {
    buildShape(true);
}

//---------------------------------------------------------------------
PROPERTY_SOURCE_WITH_EXTENSIONS(Part::Link, Part::LinkBase)

Link::Link() {
    ADD_PROPERTY_TYPE(LinkedObject, (0), " Link", App::Prop_None, "Linked object");
    App::LinkExtension::setProperties(&LinkedObject, &Placement);
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

    auto prop = linked->getPropertyByName("Shape");
    if(!prop || !prop->isDerivedFrom(PropertyPartShape::getClassTypeId()))
        EXEC_RET("No shape property. Please turn off LinkShape.");
    TopoShape shape(static_cast<PropertyPartShape*>(prop)->getValue().Located(TopLoc_Location()));
    if(shape.isNull())
        EXEC_RET("No shape found");
    shape.transformShape(mat,false,true);
    Shape.setValue(shape);
    return 0;
}


//---------------------------------------------------------------------
PROPERTY_SOURCE_WITH_EXTENSIONS(Part::LinkSub, Part::LinkBase)

LinkSub::LinkSub() {
    ADD_PROPERTY_TYPE(LinkedSubs, (0), " Link", App::Prop_None, "Linked sub objects");
    App::LinkExtension::setProperties(&LinkedSubs, &Placement);

    LinkTransform.setValue(true);
}

App::DocumentObjectExecReturn *LinkSub::buildShape(bool silent) {
    if(!LinkShape.getValue()) return 0;

    Base::Matrix4D mat;
    auto obj = getLinkedObjectExt(true,&mat,true);
    if(!obj)
        EXEC_RET("no object linked");

    auto subs = LinkedSubs.getSubValues();
    if(subs.empty())
        EXEC_RET("no sub object linked");

    BRep_Builder builder;
    TopoDS_Compound compound;
    builder.MakeCompound(compound);
    int count = 0;

    bool transform = LinkTransform.getValue();
    for(const auto &sub : subs) {
        const char *element = 0;
        Base::Matrix4D m(mat);
        auto sobj = obj->getSubObject(sub.c_str(),&element,0,&m,transform);
        if(!sobj || !sobj->getNameInDocument()) {
            FC_WARN("skip invalid object " << sobj->getNameInDocument() << '.' << sub.c_str());
            continue;
        }

        auto prop = sobj->getPropertyByName("Shape");
        if(!prop || !prop->isDerivedFrom(PropertyPartShape::getClassTypeId())) {
            FC_WARN("no shape found in " << sobj->getNameInDocument() << '.' << sub.c_str());
            continue;
        }

        TopoShape s(static_cast<PropertyPartShape*>(prop)->getValue().Located(TopLoc_Location()));
        if(s.isNull()) continue;
        if(element && *element) {
            try {
                s = s.getSubShape(element);
            }catch(Standard_Failure &e) {
                FC_WARN("OCCT exception on getting " << sobj->getNameInDocument() << '.' << sub.c_str());
                continue;
            }
        }
        s.transformShape(m,false,true);
        builder.Add(compound,s.getShape());
        ++count;
    }

    if(count==0)
        EXEC_RET("No shape found");
    Shape.setValue(compound);
    return 0;
}

