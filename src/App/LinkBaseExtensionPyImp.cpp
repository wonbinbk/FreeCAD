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
# include <sstream>
#endif

#include "LinkBaseExtensionPy.h"
#include "LinkBaseExtensionPy.cpp"

using namespace App;

// returns a string which represent the object e.g. when printed in python
std::string LinkBaseExtensionPy::representation(void) const
{
    std::ostringstream str;
    str << "<" << getLinkBaseExtensionPtr()->getExtensionClassTypeId().getName() << ">";
    return str.str();
}

typedef std::map<std::string, std::pair<int,Property*> > PropTmpMap;
typedef std::map<std::string, Property*> PropMap;

static bool getProperty(PropTmpMap &props, const LinkBaseExtension::PropInfoMap &infoMap, 
        const PropMap &propMap, PyObject *key, PyObject *value) 
{
    std::ostringstream str;
    if(!PyString_Check(key)) {
        PyErr_SetString(PyExc_TypeError, "key must be string");
        return false;
    }
    const char *keyStr = PyString_AsString(key);
    auto it = infoMap.find(keyStr);
    if(it == infoMap.end()){
        str << "unknown key '" << keyStr << "'";
        PyErr_SetString(PyExc_KeyError, str.str().c_str());
        return false;
    }

    const char *valStr;
    if(key == value) 
        valStr = keyStr;
    else {
        if(!PyString_Check(value)) {
            PyErr_SetString(PyExc_TypeError, "value must be string");
            return false;
        }
        valStr = PyString_AsString(value);
    }

    auto pIt = propMap.find(valStr);
    if(pIt == propMap.end()) {
        str << "cannot find property '" << valStr << "'";
        PyErr_SetString(PyExc_ValueError, str.str().c_str());
        return false;
    }
    auto &info = it->second;
    App::Property *prop = pIt->second;
    if(!prop->isDerivedFrom(info.type)) {
        str << "expect property '" << keyStr << "(" << valStr 
            << ") to be derived from '" << info.type.getName() 
            << "', instead of '" << prop->getTypeId().getName() << "'";
        PyErr_SetString(PyExc_TypeError, str.str().c_str());
    }
    props[keyStr] = std::make_pair(info.index,prop);
    return true;
}

PyObject* LinkBaseExtensionPy::configLinkProperty(PyObject *args, PyObject *keywds) {
    auto ext = getLinkBaseExtensionPtr();
    const auto &info = ext->getPropertyInfoMap();

    PropMap propMap;
    ext->getExtendedContainer()->getPropertyMap(propMap);

    PropTmpMap props;

    if(PyTuple_Check(args)) {
        for(Py_ssize_t pos=0;pos<PyTuple_GET_SIZE(args);++pos) {
            auto key = PyTuple_GET_ITEM(args,pos);
            if(!getProperty(props,info,propMap,key,key))
                return 0;
        }
    }
    if(PyDict_Check(keywds)) {
        PyObject *key, *value;
        Py_ssize_t pos = 0;
        while (PyDict_Next(keywds, &pos, &key, &value)) {
            if(!getProperty(props,info,propMap,key,value))
                return 0;
        }
    }
    for(auto &v : props) 
        ext->setProperty(v.second.first,v.second.second);
    Py_Return;
}

PyObject* LinkBaseExtensionPy::getLinkPropertyInfo(PyObject *args)
{
    auto ext = getLinkBaseExtensionPtr();

    const auto &infos = ext->getPropertyInfo();

    if(PyArg_ParseTuple(args,"")) {
        Py::Tuple ret(infos.size());
        int i=0;
        for(const auto &info : infos) {
            ret.setItem(i,Py::TupleN(Py::String(info.name), 
                    Py::String(info.type.getName()),Py::String(info.doc)));
        }
        return Py::new_reference_to(ret);
    }

    short index = 0;
    if(PyArg_ParseTuple(args,"h",&index)) {
        if(index<0 || index>=(int)infos.size()) {
            PyErr_SetString(PyExc_ValueError, "index out of range");
            return 0;
        }
        Py::TupleN ret(Py::String(infos[index].name),
                Py::String(infos[index].type.getName()),Py::String(infos[index].doc));
        return Py::new_reference_to(ret);
    }

    char *name;
    if(PyArg_ParseTuple(args,"s",&name)) {
        for(int i=0;i<(int)infos.size();++i) {
            if(strcmp(infos[i].name,name)==0) {
                Py::TupleN ret(Py::String(infos[i].type.getName()),
                            Py::String(infos[i].doc));
                return Py::new_reference_to(ret);
            }
        }
        PyErr_SetString(PyExc_ValueError, "unknown property name");
        return 0;
    }

    PyErr_SetString(PyExc_ValueError, "invalid arguments");
    return 0;
}

PyObject *LinkBaseExtensionPy::getCustomAttributes(const char* /*attr*/) const
{
    return 0;
}

int LinkBaseExtensionPy::setCustomAttributes(const char* /*attr*/, PyObject * /*obj*/)
{
    return 0;
}
