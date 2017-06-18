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
# include <TopoDS_Iterator.hxx>
#endif

#include <Base/Console.h>
#include <App/GeoFeatureGroupExtension.h>
#include "PartLink.h"
#include "TopoShapePy.h"

FC_LOG_LEVEL_INIT("PartLink", false, true);

using namespace Part;

//---------------------------------------------------------------------
EXTENSION_PROPERTY_SOURCE(Part::LinkBaseExtension, App::LinkBaseExtension)

LinkBaseExtension::LinkBaseExtension() {
    initExtensionType(LinkBaseExtension::getExtensionClassTypeId());
    props.resize(PropPartMax,0);
}

const std::vector<LinkBaseExtension::PropInfo> &LinkBaseExtension::getPropertyInfo() const {
    static std::vector<LinkBaseExtension::PropInfo> PropsInfo;
    if(PropsInfo.empty()) {
        PropsInfo = inherited::getPropertyInfo();
        BOOST_PP_SEQ_FOR_EACH(LINK_PROP_INFO,PropsInfo,LINK_PARAMS_PART);
    }
    return PropsInfo;
}

App::DocumentObjectExecReturn *LinkBaseExtension::extensionExecute(void) {
    auto ret = inherited::extensionExecute();
    if(ret) return ret;
    return buildShape(false);
}

bool LinkBaseExtension::extensionGetSubObject(App::DocumentObject *&ret, const char *element, const char **subname, 
        PyObject **pyObj, Base::Matrix4D *mat, bool transform, int depth) const
{
    inherited::extensionGetSubObject(ret,element,subname,pyObj,mat,transform,depth);
    // return true to bypass Part::Feature::getSubObject handling
    return true;
}

void LinkBaseExtension::extensionOnDocumentRestored() {
    inherited::extensionOnDocumentRestored();
    buildShape(true);
}

#define EXCEPTION_ELEMENT_NAME(_element) \
        const char *dot = "";\
        const char *_sub = _element;\
        if(!_sub)\
            _sub = "";\
        else if(_sub[0])\
            dot = ".";\
        auto owner = getContainer();\
        const char *name=owner?owner->getNameInDocument():0;\
        if(!name) name ="?";

#define CATCH_EXCEPTION(_element,_msg) \
    catch(Standard_Failure &e) {\
        EXCEPTION_ELEMENT_NAME(_element);\
        Standard_CString msg = e.GetMessageString();\
        FC_ERR("OCCT exception on " _msg << " " <<  name << dot << _sub << ": "\
            << typeid(e).name() << " " << msg);\
    } catch(Base::Exception &e) {\
        EXCEPTION_ELEMENT_NAME(_element);\
        FC_ERR("exception on " _msg << " " <<  name << dot << _sub << ": " << e.what());\
    }

TopoDS_Shape LinkBaseExtension::combineElements(const TopoDS_Shape &shape) {
    if(shape.IsNull()) return shape;

    BRep_Builder builder;
    TopoDS_Compound compound;
    builder.MakeCompound(compound);

    // Apply scale but no placement, because Part::Feature will set the placement
    // on its Shape property
    Base::Matrix4D mat;
    mat.scale(getScale());

    int shapeCount = 0;
    int elementCount = getElementCount();
    if(elementCount) {
        auto visibilities = getVisibilityList();
        for(int i=0;i<elementCount;++i) {
            if(i<(int)visibilities.size() && !visibilities[i])
                continue;
            char element[64];
            snprintf(element,sizeof(element),"%d",i);
            Base::Matrix4D m(mat);
            App::DocumentObject *ret = 0;
            try {
                // obtain the element transformation
                extensionGetSubObject(ret,element,0,0,&m,false,0);
                if(!ret) continue;
                TopoShape s(shape);
                s.transformShape(m,false,true);
                builder.Add(compound,s.getShape());
                ++shapeCount;
            }CATCH_EXCEPTION(element,"getting element")
        }
    }else{
        // Event if we don't have elements, we still make a compound to hide
        // the child shapes placement, and leave only our own placement on the
        // output property shape (which is not set here, but by
        // Part::Feature::onChanged())
        try {
            TopoShape s(shape);
            s.transformShape(mat,false,true);
            builder.Add(compound,s.getShape());
            shapeCount = 1;
        }CATCH_EXCEPTION(0,"transforming");
    }
    
    if(!shapeCount)
        return TopoDS_Shape();
    if(!getFuse()) 
        return compound;

    std::vector<TopoDS_Shape> shapes;
    for(TopoDS_Iterator it(shape);it.More();it.Next()) {
        const TopoDS_Shape& aChild = it.Value();
        if(aChild.IsNull()) continue;
        shapes.push_back(aChild);
    }
    if(shapes.size()<=1)
        return shape;
    TopoShape s(shapes.back());
    shapes.pop_back();
    try {
        return s.fuse(shapes);
    }CATCH_EXCEPTION(0,"fuse")
    return TopoDS_Shape();
}

TopoDS_Shape LinkBaseExtension::getLinkedShape(const char *subname)
{
    TopoDS_Shape shape;
    auto linked = getLinkedObject();
    if(!linked)
        return shape;
    try {
        Base::Matrix4D mat;
        PyObject *pyObj = 0;
        linked->getSubObject(subname,0,&pyObj,&mat,getLinkTransform());
        if(pyObj) {
            if(PyObject_TypeCheck(pyObj, &(TopoShapePy::Type)))
                shape = static_cast<TopoShapePy*>(pyObj)->getTopoShapePtr()->getShape();
            Py_DECREF(pyObj);
        }
    }CATCH_EXCEPTION(subname,"getting element")
    return shape;
}

App::DocumentObjectExecReturn *LinkBaseExtension::buildShape(bool silent) {
#define EXEC_RET(_txt) do{\
        if(silent) return nullptr;\
        return new App::DocumentObjectExecReturn(_txt);\
    }while(0)

    auto propShape = getShapeProperty();
    if(!propShape || !getLinkShape() || !getLinkedObject()) 
        return 0;

    TopoDS_Shape shape;
    auto subs = getSubList();
    if(subs.empty()) 
        shape = combineElements(getLinkedShape());
    else {
        BRep_Builder builder;
        TopoDS_Compound compound;
        builder.MakeCompound(compound);
        int count = 0;
        for(const auto &sub : subs) {
            TopoDS_Shape shape = getLinkedShape(sub.c_str());
            if(shape.IsNull()) continue;
            builder.Add(compound,shape);
            ++count;
        }
        if(count)
            shape = combineElements(compound);
    }
    shape = combineElements(getLinkedShape());
    if(shape.IsNull())
        EXEC_RET("No shape found");
    propShape->setValue(shape);
    return 0;
}

void LinkBaseExtension::setProperty(int idx, App::Property *prop) {
    inherited::setProperty(idx,prop);
    if(prop && idx == PropShape)
        prop->setStatus(App::Property::Transient,true);
}

//---------------------------------------------------------------------

namespace App {
EXTENSION_PROPERTY_SOURCE_TEMPLATE(Part::LinkBaseExtensionPython, Part::LinkBaseExtension)

// explicit template instantiation
template class PartExport App::ExtensionPythonT<Part::LinkBaseExtension>;

}

//---------------------------------------------------------------------

EXTENSION_PROPERTY_SOURCE(Part::LinkExtension, Part::LinkBaseExtension)

LinkExtension::LinkExtension() {
    initExtensionType(LinkExtension::getExtensionClassTypeId());

    LINK_PROPS_ADD_EXTENSION(LINK_PARAMS_PART_EXT);
}

LinkExtension::~LinkExtension()
{
}

//---------------------------------------------------------------------

namespace App {
EXTENSION_PROPERTY_SOURCE_TEMPLATE(Part::LinkExtensionPython, Part::LinkExtension)

// explicit template instantiation
template class PartExport App::ExtensionPythonT<Part::LinkExtension>;

}

//---------------------------------------------------------------------

PROPERTY_SOURCE_WITH_EXTENSIONS(Part::Link, Part::Feature)

Link::Link() {
    LinkExtension::initExtension(this);
    LINK_PROPS_ADD(LINK_PARAMS_PART_LINK);
    setProperty(PropPlacement, &Placement);
    setProperty(PropShape, &Shape);
}

namespace App {
PROPERTY_SOURCE_TEMPLATE(Part::LinkPython, Part::Feature)
template<> const char* Part::LinkPython::getViewProviderName(void) const {
    return "Gui::ViewProviderLinkPython";
}
template class PartExport FeaturePythonT<Part::Link>;
}

