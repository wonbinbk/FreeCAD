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

#include "GeoFeatureGroupExtension.h"
#include "Link.h"

using namespace App;

EXTENSION_PROPERTY_SOURCE(App::LinkExtension, App::DocumentObjectExtension)

LinkExtension::LinkExtension(void)
    :propLink(0),propPlacement(0)
{
    initExtensionType(LinkExtension::getExtensionClassTypeId());

    // Note: Because PropertyView will merge linked object's properties into
    // ours, we set group name as ' Link' with a leading space to try to make
    // our group before others
    EXTENSION_ADD_PROPERTY_TYPE(LinkTransform, (false), " Link", Prop_None,
            "Link child placement. If false, the child object's placement is ignored.");
    EXTENSION_ADD_PROPERTY_TYPE(LinkScale,(Base::Vector3d(1.0,1.0,1.0))," Link",Prop_None,
            "Scale factor for view provider. It does not actually scale the geometry data.");
    EXTENSION_ADD_PROPERTY_TYPE(LinkPlacement,(Base::Placement())," Link",Prop_None,
            "The placement of this link. If LinkTransform is 'true', then the final\n"
            "placement is the composite of this and the child's placement.");
    EXTENSION_ADD_PROPERTY_TYPE(LinkRecomputed, (false), " Link", 
            PropertyType(Prop_Output|Prop_Transient|Prop_Hidden),0);
}

LinkExtension::~LinkExtension(void)
{
}

void LinkExtension::setProperties(Property *propLink, PropertyPlacement *propPlacement) {
    if(!propLink || (!propLink->isDerivedFrom(PropertyLink::getClassTypeId()) &&
                    !propLink->isDerivedFrom(PropertyLinkSub::getClassTypeId())))
        throw Base::RuntimeError("App::Link: invalid link property");
    this->propLink = propLink;
    this->propPlacement = propPlacement;
}

App::DocumentObjectExecReturn *LinkExtension::extensionExecute(void) {
    // The actual value is not important, just to notify view provider that
    // the link (in fact, its dependents, i.e. linked ones) have recomputed.
    LinkRecomputed.touch();
    return 0;
}

DocumentObject *LinkExtension::getLink() const{
    if(!propLink) throw Base::RuntimeError("App::Link: no link property");
    if(propLink->isDerivedFrom(PropertyLink::getClassTypeId()))
        return static_cast<const PropertyLink*>(propLink)->getValue();
    return dynamic_cast<const PropertyLinkSub*>(propLink)->getValue();
}

DocumentObject *LinkExtension::extensionGetSubObject(const char *element,
        const char **subname, PyObject **pyObj, Base::Matrix4D *mat, bool transform) const 
{
    auto object = getLink();
    if(!object) return nullptr;

    if(mat) {
        if(transform)
            *mat *= LinkPlacement.getValue().toMatrix();
        Base::Matrix4D s;
        s.scale(LinkScale.getValue());
        *mat *= s;
    }
    if(pyObj || (element && strchr(element,'.')))
        return object->getSubObject(element,subname,pyObj,mat,LinkTransform.getValue());

    Base::Matrix4D matNext;
    const char *nextsub = 0;
    auto ret = object->getSubObject(element,&nextsub,0, &matNext, LinkTransform.getValue());
    if(ret) {
        if(subname) *subname = nextsub;
        // do not resolve the last link object
        if(nextsub == element) {
            auto ext = getExtendedContainer();
            if(!ext || !ext->isDerivedFrom(DocumentObject::getClassTypeId()))
                throw Base::RuntimeError("Link: container not derived from document object");
            auto obj = static_cast<const DocumentObject *>(ext);
            return const_cast<DocumentObject*>(obj);
        }else if(mat)
            *mat *= matNext;
    }
    return ret;
}

DocumentObject *LinkExtension::getLinkedObjectExt(bool recurse, Base::Matrix4D *mat, bool transform)
{
    auto object = getLink();

    if(mat) {
        if(transform)
            *mat *= LinkPlacement.getValue().toMatrix();
        Base::Matrix4D s;
        s.scale(LinkScale.getValue());
        *mat *= s;
    }

    if(!object) return 0;
    if(!recurse) return object;
    return object->getLinkedObject(recurse,mat,LinkTransform.getValue());
}

void LinkExtension::extensionOnChanged(const Property *prop) {
    if(propPlacement) {
        if (prop == &LinkPlacement) {
            if(!LinkPlacement.testStatus(App::Property::User3)) {
                // prevent recuse
                propPlacement->setStatus(App::Property::User3,true);
                propPlacement->setValue(LinkPlacement.getValue());
                propPlacement->setStatus(App::Property::User3,false);
            }
        } else if(prop == propPlacement) {
            if(!propPlacement->testStatus(App::Property::User3)) {
                LinkPlacement.setStatus(App::Property::User3,true);
                LinkPlacement.setValue(propPlacement->getValue());
                LinkPlacement.setStatus(App::Property::User3,false);
            }
        }
    }
}

//-------------------------------------------------------------------------------

PROPERTY_SOURCE_WITH_EXTENSIONS(App::Link, App::DocumentObject)

Link::Link() {
    ADD_PROPERTY_TYPE(LinkedObject, (0), " Link", Prop_None, "Linked object");
    ADD_PROPERTY_TYPE(Placement,(Base::Placement())," Link",
            PropertyType(Prop_Output|Prop_Transient|Prop_Hidden),0);
    LinkExtension::setProperties(&LinkedObject, &Placement);
    LinkExtension::initExtension(this);
}

DocumentObject *Link::getLinkedObject(bool recurse, Base::Matrix4D *mat, bool transform)
{
    return getLinkedObjectExt(recurse,mat,transform);
}

//-------------------------------------------------------------------------------

PROPERTY_SOURCE_WITH_EXTENSIONS(App::LinkSub, App::DocumentObject)

LinkSub::LinkSub() {
    ADD_PROPERTY_TYPE(LinkedSubs, (0), " Link", Prop_None, "Linked object with sub elements");
    ADD_PROPERTY_TYPE(Placement,(Base::Placement())," Link",
            PropertyType(Prop_Output|Prop_Transient|Prop_Hidden),0);
    LinkExtension::setProperties(&LinkedSubs, &Placement);
    LinkExtension::initExtension(this);
    LinkTransform.setValue(true);
}

DocumentObject *LinkSub::getLinkedObject(bool recurse, Base::Matrix4D *mat, bool transform)
{
    return getLinkedObjectExt(recurse,mat,transform);
}
//--------------------------------------------------------------------------------

// namespace App {
// /// @cond DOXERR
// PROPERTY_SOURCE_TEMPLATE(LinkPython, Part::Feature)
// template<> const char* LinkPython::getViewProviderName(void) const {
//     return "App::ViewProviderLinkPython";
// }
// template<> PyObject* LinkPython::getPyObject(void) {
//     if (PythonObject.is(Py::_None())) {
//         // ref counter is set to 1
//         PythonObject = Py::Object(new FeaturePythonPyT<LinkPy>(this),true);
//     }
//     return Py::new_reference_to(PythonObject);
// }
// /// @endcond
//
// // explicit template instantiation
// template class AppExport FeaturePythonT<Link>;
// }

