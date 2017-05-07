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
#include "LinkExtensionPy.h"

using namespace App;

EXTENSION_PROPERTY_SOURCE(App::LinkExtension, App::DocumentObjectExtension)

LinkExtension::LinkExtension(void)
{
    initExtensionType(LinkExtension::getExtensionClassTypeId());

    // Note: Because PropertyView will merge linked object's properties into
    // ours, we set group name as ' Link' with a leading space to try to make
    // our group before others
    EXTENSION_ADD_PROPERTY_TYPE(LinkedObject, (0), " Link", Prop_None, "Linked object");
    // EXTENSION_ADD_PROPERTY_TYPE(LinkMoveChild, (true), " Link", Prop_None, "Remove child object(s) from root");
    EXTENSION_ADD_PROPERTY_TYPE(LinkTransform, (false), " Link", Prop_None,
            "Link child placement. If false, the child object's placement is ignored.");
    EXTENSION_ADD_PROPERTY_TYPE(LinkScale,(Base::Vector3d(1.0,1.0,1.0))," Link",Prop_None,
            "Scale factor for view provider. It does not actually scale the geometry data.");
    EXTENSION_ADD_PROPERTY_TYPE(LinkPlacement,(Base::Placement())," Link",Prop_None,
            "The placement of this link. If LinkTransform is 'true', then the final\n"
            "placement is the composite of this and the child's placement.");
}

LinkExtension::~LinkExtension(void)
{
}

PyObject *LinkExtension::extensionGetPySubObjects(const char *element,
                            const Base::Matrix4D &mat, bool transform) const 
{
    auto object = LinkedObject.getValue();
    if(!object) return 0;

    Base::Matrix4D _mat;
    if(transform) {
        _mat = mat * LinkPlacement.getValue().toMatrix();
        Base::Matrix4D s;
        s.scale(LinkScale.getValue());
        _mat *= s;
    }
    const Base::Matrix4D &matNext = transform?_mat:mat;
    return object->getPySubObject(element,matNext,LinkTransform.getValue());
}

DocumentObject *LinkExtension::getLinkedObjectExt(bool recurse, Base::Matrix4D *mat, bool transform)
{
    auto object = LinkedObject.getValue();

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


PyObject *LinkExtension::getExtensionPyObject(void) {
    if (ExtensionPythonObject.is(Py::_None())){
        auto ext = new LinkExtensionPy(this);
        ExtensionPythonObject = Py::Object(ext,true);
    }
    return Py::new_reference_to(ExtensionPythonObject);
}



//-------------------------------------------------------------------------------

PROPERTY_SOURCE_WITH_EXTENSIONS(App::Link, App::DocumentObject)

Link::Link() {
    LinkExtension::initExtension(this);
}

DocumentObject *Link::getLinkedObject(bool recurse, Base::Matrix4D *mat, bool transform)
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

