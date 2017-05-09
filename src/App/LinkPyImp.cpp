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

#include <Base/GeometryPyCXX.h>
#include <Base/PlacementPy.h>
#include "Link.h"
#include "LinkPy.h"
#include "LinkExtensionPy.h"
#include "LinkPy.cpp"

using namespace App;

// returns a string which represent the object e.g. when printed in python
std::string LinkPy::representation(void) const
{
    return static_cast<LinkExtensionPy*>(getLinkPtr()->getExtensionPyObject())->representation();
}

void LinkPy::setPlacement(Py::Object arg) {
    PyObject* p = arg.ptr();
    if (PyObject_TypeCheck(p, &(Base::PlacementPy::Type))) {
        Base::Placement* trf = static_cast<Base::PlacementPy*>(p)->getPlacementPtr();
        getLinkPtr()->LinkPlacement.setValue(*trf);
    }
    else {
        std::string error = std::string("type must be 'Placement', not ");
        error += p->ob_type->tp_name;
        throw Py::TypeError(error);
    }
}

Py::Object LinkPy::getPlacement() const{
    return Py::Placement(getLinkPtr()->LinkPlacement.getValue());
}

PyObject *LinkPy::getCustomAttributes(const char* /*attr*/) const
{
    return 0;
}

int LinkPy::setCustomAttributes(const char* /*attr*/, PyObject* /*obj*/)
{
    return 0; 
}
