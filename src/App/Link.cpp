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

#include <boost/preprocessor/stringize.hpp>
#include "Application.h"
#include "Document.h"
#include "GeoFeatureGroupExtension.h"
#include "Link.h"
#include "LinkBaseExtensionPy.h"
#include <Base/Console.h>

FC_LOG_LEVEL_INIT("App::Link", true,true)

using namespace App;

EXTENSION_PROPERTY_SOURCE(App::LinkBaseExtension, App::DocumentObjectExtension)

LinkBaseExtension::LinkBaseExtension(void)
{
    initExtensionType(LinkBaseExtension::getExtensionClassTypeId());
    props.resize(PropMax,0);
}

LinkBaseExtension::~LinkBaseExtension()
{
}

PyObject* LinkBaseExtension::getExtensionPyObject(void) {
    if (ExtensionPythonObject.is(Py::_None())){
        // ref counter is set to 1
        ExtensionPythonObject = Py::Object(new LinkBaseExtensionPy(this),true);
    }
    return Py::new_reference_to(ExtensionPythonObject);
}

const std::vector<LinkBaseExtension::PropInfo> &LinkBaseExtension::getPropertyInfo() const {
    static std::vector<LinkBaseExtension::PropInfo> PropsInfo;
    if(PropsInfo.empty()) {
        BOOST_PP_SEQ_FOR_EACH(LINK_PROP_INFO,PropsInfo,LINK_PARAMS);
    }
    return PropsInfo;
}

LinkBaseExtension::PropInfoMap LinkBaseExtension::getPropertyInfoMap() const {
    const auto &infos = getPropertyInfo();
    PropInfoMap ret;
    for(const auto &info : infos) 
        ret[info.name] = info;
    return ret;
}

void LinkBaseExtension::setProperty(int idx, Property *prop) {
    const auto &infos = getPropertyInfo();
    if(idx<0 || idx>=(int)infos.size())
        throw Base::RuntimeError("App::LinkBaseExtension: property index out of range");
    if(!prop->isDerivedFrom(infos[idx].type)) {
        std::ostringstream str;
        str << "App::LinkBaseExtension: expected property type '" << 
            infos[idx].type.getName() << "', instead of '" << 
            prop->getClassTypeId().getName() << "'";
        throw Base::TypeError(str.str().c_str());
    }
    props[idx] = prop;

    if(FC_LOG_INSTANCE.isEnabled(FC_LOGLEVEL_LOG)) {
        const char *propName;
        if(!prop) 
            propName = "<null>";
        else if(prop->getContainer())
            propName = prop->getName();
        else 
            propName = extensionGetPropertyName(prop);
        if(!propName) 
            propName = "?";
        FC_LOG("set property " << infos[idx].name << ": " << propName);
    }

    if(idx == PropPlacement || idx == PropLinkPlacement) {
        auto propPlacement = getPlacementProperty();
        auto propLinkPlacement = getLinkPlacementProperty();
        if(propPlacement && propLinkPlacement) {
            propPlacement->setStatus(Property::Hidden,true);
            propPlacement->setStatus(Property::Transient,true);
        }
    }
}

App::DocumentObjectExecReturn *LinkBaseExtension::extensionExecute(void) {
    // The actual value of LinkRecompouted is not important, just to notify view
    // provider that the link (in fact, its dependents, i.e. linked ones) have
    // recomputed.
    auto prop = getLinkRecomputedProperty();
    if(prop) prop->touch();
    return inherited::extensionExecute();
}

const DocumentObject *LinkBaseExtension::getContainer() const {
    auto ext = getExtendedContainer();
    if(!ext || !ext->isDerivedFrom(DocumentObject::getClassTypeId()))
        throw Base::RuntimeError("Link: container not derived from document object");
    return static_cast<const DocumentObject *>(ext);
}

DocumentObject *LinkBaseExtension::getContainer(){
    auto ext = getExtendedContainer();
    if(!ext || !ext->isDerivedFrom(DocumentObject::getClassTypeId()))
        throw Base::RuntimeError("Link: container not derived from document object");
    return static_cast<DocumentObject *>(ext);
}

DocumentObject *LinkBaseExtension::getLink(int depth) const{
    checkDepth(depth);
    if(getLinkedSubsProperty())
        return getLinkedSubs();
    if(getLinkedObjectProperty())
        return getLinkedObject();
    return 0;
}

int LinkBaseExtension::getArrayIndex(const char *subname, const char **psubname) {
    if(!subname) return -1;
    if(*subname == 'i') ++subname;
    const char *dot = strchr(subname,'.');
    if(!dot) dot= subname+strlen(subname);
    if(dot == subname) return -1;
    int idx = 0;
    for(const char *c=subname;c!=dot;++c) {
        if(!isdigit(*c)) return -1;
        idx = idx*10 + *c -'0';
    }
    if(psubname) {
        if(*dot)
            *psubname = dot+1;
        else
            *psubname = dot;
    }
    return idx;
}

Base::Matrix4D LinkBaseExtension::getTransform(bool transform) const {
    Base::Matrix4D mat;
    if(transform) {
        if(getLinkPlacementProperty())
            mat = getLinkPlacement().toMatrix();
        else if(getPlacementProperty())
            mat = getPlacement().toMatrix();
    }
    if(getScaleProperty()) {
        Base::Matrix4D s;
        s.scale(getScale());
        mat *= s;
    }
    return mat;
}

bool LinkBaseExtension::extensionGetSubObject(DocumentObject *&ret, const char *element, 
        const char **subname, PyObject **pyObj, Base::Matrix4D *mat, bool transform, int depth) const 
{
    ret = 0;
    auto object = getLink(depth);
    if(!object) 
        return true;

    auto obj = getContainer();
    if(mat) *mat *= getTransform(transform);
    if(!element || !element[0]) {
        ret = const_cast<DocumentObject*>(obj);
        if(pyObj) *pyObj = ret->getPyObject();
        if(subname) *subname = element;
        return true;
    }
    DocumentObject *subObj = 0;
    int elementCount = getElementCount();
    if(elementCount) {
        int idx = getArrayIndex(element,&element);
        auto elementList = getElementListProperty();
        if(elementList && idx<0 && getShowElement()) {
            const char *dot = strchr(element,'.');
            if(dot) {
                subObj = elementList->find(std::string(element,dot-element).c_str(),&idx);
                element = dot+1;
            }else{
                subObj = elementList->find(element,&idx);
                element = 0;
            }
        }
        if(idx<0 || idx>=elementCount)
            return true;
        if(mat) {
            auto placementList = getPlacementListProperty();
            if(placementList && placementList->getSize()>idx)
                *mat *= (*placementList)[idx].toMatrix();
            auto scaleList = getScaleListProperty();
            if(scaleList && scaleList->getSize()>idx) {
                Base::Matrix4D s;
                s.scale((*scaleList)[idx]);
                *mat *= s;
            }
        }
    }
    if(pyObj || (element && strchr(element,'.'))) {
        ret = object->getSubObject(element,subname,pyObj,mat,getLinkTransform(),depth+1);
        return true;
    }

    Base::Matrix4D matNext;
    const char *nextsub = 0;
    ret = object->getSubObject(element,&nextsub,0, &matNext, getLinkTransform(),depth+1);
    if(ret) {
        if(subname) *subname = nextsub;
        // do not resolve the last link object
        if(nextsub == element)
            ret = const_cast<DocumentObject*>(obj);
        else if(mat)
            *mat *= matNext;
    }
    if(ret==obj && subObj)
        ret = subObj;
    return true;
}

void LinkBaseExtension::onExtendedUnsetupObject () {
    auto objs = getElementList();
    for(auto obj : objs) {
        if(!obj->isDeleting())
            obj->getDocument()->remObject(obj->getNameInDocument());
    }
}

void LinkBaseExtension::checkDepth(int depth) {
    static int s_limit;
    if(!s_limit) {
        ParameterGrp::handle hGrp = GetApplication().GetParameterGroupByPath(
                "User parameter:BaseApp/Preferences/Link");
        s_limit = hGrp->GetInt("Depth",100);
    }
    if(depth >= s_limit) 
        throw Base::RuntimeError("Link: Recursion limit reached."
                "Please check for cyclic reference, "
                "or change setting at BaseApp/Preferences/Link/Depth.");
}

bool LinkBaseExtension::extensionGetLinkedObject(DocumentObject *&ret, 
        bool recurse, Base::Matrix4D *mat, bool transform, int depth) const
{
    ret = getLink(depth);

    if(mat) *mat *= getTransform(transform);
    if(!ret) return false;
    if(!recurse) return ret;
    ret = ret->getLinkedObject(recurse,mat,getLinkTransform(),depth+1);
    return true;
}

void LinkBaseExtension::extensionOnChanged(const Property *prop) {
    auto parent = getContainer();
    if(parent && !parent->isRestoring() && prop && !prop->testStatus(Property::User3))
        update(parent,prop);
    inherited::extensionOnChanged(prop);
}

void LinkBaseExtension::update(App::DocumentObject *parent, const Property *prop) {
    if(!prop) return;
    if(prop == getLinkPlacementProperty() || prop == getPlacementProperty()) {
        auto src = getLinkPlacementProperty();
        auto dst = getPlacementProperty();
        if(src!=prop) std::swap(src,dst);
        if(src && dst) {
            dst->setStatus(Property::User3,true);
            dst->setValue(src->getValue());
            dst->setStatus(Property::User3,false);
        }
    }else if(prop == getElementCountProperty() || prop == getShowElementProperty()) {
        const auto &elementList = getElementListProperty();
        int elementCount = getElementCount();
        bool showElement = getShowElement();
        if(elementList) {
            if(elementCount<=0 || !showElement) {
                auto objs = elementList->getValues();
                elementList->setValues(std::vector<App::DocumentObject*>());
                for(auto obj : objs) 
                    obj->getDocument()->remObject(obj->getNameInDocument());
            }else if(showElement && elementCount>0) {
                const auto &placementList = getPlacementList();
                const auto &visibilityList = getVisibilityList();
                const auto &scaleList = getScaleList();
                if(elementCount>elementList->getSize()) {
                    std::string name = parent->getNameInDocument();
                    name += "_i";
                    name = parent->getDocument()->getUniqueObjectName(name.c_str());
                    if(name[name.size()-1] != 'i')
                        name += "_i";
                    auto offset = name.size();
                    int i = elementList->getSize();
                    elementList->setSize(elementCount);
                    for(;i<elementCount;++i) {
                        LinkElement *obj = new LinkElement;
                        if((int)placementList.size()>i) 
                            obj->Placement.setValue(placementList[i]);
                        if((int)scaleList.size()>i)
                            obj->Scale.setValue(scaleList[i]);
                        name.resize(offset);
                        name += std::to_string(i);
                        parent->getDocument()->addObject(obj,name.c_str());
                        elementList->set1Value(i,obj,i+1==elementCount);
                    }
                }else{
                    auto objs = elementList->getValues();
                    // NOTE setSize() won't touch() elementList. This is what we want. Or is it?
                    elementList->setSize(elementCount);
                    while((int)objs.size()>elementCount) {
                        parent->getDocument()->remObject(objs.back()->getNameInDocument());
                        objs.pop_back();
                    }
                }
            }
        }
    }else if(prop == getElementListProperty() ||
             prop == getPlacementListProperty() ||
             prop == getScaleListProperty())
    {
        int i;
        const auto &elementList = getElementList();
        const auto &placementList = getPlacementList();
        const auto &scaleList = getScaleList();
        for(i=0;i<(int)elementList.size();++i) {
#define GET_LINK_ELEMENT(_i,_list, _obj) \
            if(!(_list)[_i]->isDerivedFrom(LinkElement::getClassTypeId())) continue;\
            auto _obj = static_cast<LinkElement*>((_list)[_i])

            GET_LINK_ELEMENT(i,elementList,obj);
            obj->owner = this;
            obj->index = i;
            const auto &pla = (i<(int)placementList.size())?placementList[i]:Base::Placement();
            if(obj->Placement.getValue() != pla) {
                obj->Placement.setStatus(Property::User3,true);
                obj->Placement.setValue(pla);
                obj->Placement.setStatus(Property::User3,false);
            }
            const auto &scale = (i<(int)scaleList.size())?scaleList[i]:Base::Vector3d(1,1,1);
            if(obj->Scale.getValue() != scale) {
                obj->Scale.setStatus(Property::User3,true);
                obj->Scale.setValue(scale);
                obj->Scale.setStatus(Property::User3,false);
            }
        }
    }
}

void LinkBaseExtension::extensionOnDocumentRestored() {
    inherited::extensionOnDocumentRestored();
    auto parent = getContainer();
    if(parent) {
        if(getLinkPlacementProperty())
            update(parent,getLinkPlacementProperty());
        else
            update(parent,getPlacementProperty());
        update(parent,getElementListProperty());
    }
}

///////////////////////////////////////////////////////////////////////////////////////////

namespace App {
EXTENSION_PROPERTY_SOURCE_TEMPLATE(App::LinkBaseExtensionPython, App::LinkBaseExtension)

// explicit template instantiation
template class AppExport ExtensionPythonT<LinkBaseExtension>;

}

//////////////////////////////////////////////////////////////////////////////

EXTENSION_PROPERTY_SOURCE(App::LinkExtension, App::LinkBaseExtension)

LinkExtension::LinkExtension(void)
{
    initExtensionType(LinkExtension::getExtensionClassTypeId());

    LINK_PROPS_ADD_EXTENSION(LINK_PARAMS_EXT);

    static const PropertyIntegerConstraint::Constraints s_constraints = {0,INT_MAX,1};
    ElementCount.setConstraints(&s_constraints);
}

LinkExtension::~LinkExtension()
{
}

///////////////////////////////////////////////////////////////////////////////////////////

namespace App {
EXTENSION_PROPERTY_SOURCE_TEMPLATE(App::LinkExtensionPython, App::LinkExtension)

// explicit template instantiation
template class AppExport ExtensionPythonT<App::LinkExtension>;

}

///////////////////////////////////////////////////////////////////////////////////////////

PROPERTY_SOURCE_WITH_EXTENSIONS(App::Link, App::DocumentObject)

Link::Link() {
    LINK_PROPS_ADD(LINK_PARAMS_LINK);
    LinkExtension::initExtension(this);
}

namespace App {
PROPERTY_SOURCE_TEMPLATE(App::LinkPython, App::DocumentObject)
template<> const char* App::LinkPython::getViewProviderName(void) const {
    return "Gui::ViewProviderLinkPython";
}
template class AppExport FeaturePythonT<App::Link>;
}

//-------------------------------------------------------------------------------

PROPERTY_SOURCE_WITH_EXTENSIONS(App::LinkSub, App::DocumentObject)

LinkSub::LinkSub() {
    LINK_PROPS_ADD(LINK_PARAMS_LINKSUB);
    LinkExtension::initExtension(this);
    LinkTransform.setValue(true);
}

namespace App {
PROPERTY_SOURCE_TEMPLATE(App::LinkSubPython, App::DocumentObject)
template<> const char* App::LinkSubPython::getViewProviderName(void) const {
    return "Gui::ViewProviderLinkPython";
}
template class AppExport FeaturePythonT<App::LinkSub>;
}

//--------------------------------------------------------------------------------
PROPERTY_SOURCE(App::LinkElement, App::DocumentObject)

LinkElement::LinkElement():owner(0),index(-1)
{
    ADD_PROPERTY_TYPE(Placement,(Base::Placement())," Link",(PropertyType)(Prop_Transient|Prop_Output),0);
    ADD_PROPERTY_TYPE(Scale,(Base::Vector3d(1,1,1))," Link",(PropertyType)(Prop_Transient|Prop_Output),0);
}

void LinkElement::onChanged(const Property *prop) {
    if(!isRestoring()) {
        if(prop == &Placement) {
            if(owner && !Placement.testStatus(Property::User3)) {
                auto placementList = owner->getPlacementListProperty();
                if(placementList) {
                    placementList->setStatus(Property::User3,true);
                    if(placementList->getSize()<=index)
                        placementList->setSize(index+1);
                    placementList->set1Value(index,Placement.getValue(),true);
                    placementList->setStatus(Property::User3,false);
                }
            }
        }else if(prop == &Scale) {
            if(owner && !Scale.testStatus(Property::User3)) {
                auto scaleList = owner->getScaleListProperty();
                if(scaleList) {
                    scaleList->setStatus(Property::User3,true);
                    if(scaleList->getSize()<=index)
                        scaleList->setSize(index+1,Base::Vector3d(1,1,1));
                    scaleList->set1Value(index,Scale.getValue(),true);
                    scaleList->setStatus(Property::User3,false);
                }
            }
        }
    }
    DocumentObject::onChanged(prop);
}

DocumentObject *LinkElement::getSubObject(const char *element,
        const char **subname, PyObject **pyObj, Base::Matrix4D *mat, bool transform, int depth) const 
{
    if(!owner || !getNameInDocument()) return nullptr;
    std::string name(getNameInDocument());
    if(element && *element) {
        name += ".";
        name += element;
    }
    DocumentObject *ret = 0;
    owner->extensionGetSubObject(ret,name.c_str(),subname,pyObj,mat,transform,depth);
    return ret;
}

DocumentObject *LinkElement::getLinkedObject(bool recurse, Base::Matrix4D *mat, bool transform, int depth) const
{
    if(!owner || !getNameInDocument()) return nullptr;
    DocumentObject *ret = 0;
    owner->extensionGetLinkedObject(ret,recurse, mat, transform, depth);
    return ret;
}

