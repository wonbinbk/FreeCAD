/***************************************************************************
 *   Copyright (c) Juergen Riegel          (juergen.riegel@web.de) 2014    *
 *   Copyright (c) Alexander Golubev (Fat-Zer) <fatzer2@gmail.com> 2015    *
 *                                                                         *
 *   This file is part of the FreeCAD CAx development system.              *
 *                                                                         *
 *   This library is free software; you can redistribute it and/or         *
 *   modify it under the terms of the GNU Library General Public           *
 *   License as published by the Free Software Foundation; either          *
 *   version 2 of the License, or (at your option) any later version.      *
 *                                                                         *
 *   This library  is distributed in the hope that it will be useful,      *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU Library General Public License for more details.                  *
 *                                                                         *
 *   You should have received a copy of the GNU Library General Public     *
 *   License along with this library; see the file COPYING.LIB. If not,    *
 *   write to the Free Software Foundation, Inc., 59 Temple Place,         *
 *   Suite 330, Boston, MA  02111-1307, USA                                *
 *                                                                         *
 ***************************************************************************/


#include "PreCompiled.h"

#ifndef _PreComp_
#endif

#include <App/Document.h>

#include "GeoFeatureGroupExtension.h"
//#include "GeoFeatureGroupPy.h"
//#include "FeaturePythonPyImp.h"

using namespace App;


EXTENSION_PROPERTY_SOURCE(App::GeoFeatureGroupExtension, App::GroupExtension)


//===========================================================================
// Feature
//===========================================================================

GeoFeatureGroupExtension::GeoFeatureGroupExtension(void)
{
    initExtensionType(GeoFeatureGroupExtension::getExtensionClassTypeId());
}

GeoFeatureGroupExtension::~GeoFeatureGroupExtension(void)
{
}

void GeoFeatureGroupExtension::initExtension(ExtensionContainer* obj) {
    
    if(!obj->isDerivedFrom(App::GeoFeature::getClassTypeId()))
        throw Base::RuntimeError("GeoFeatureGroupExtension can only be applied to GeoFeatures");
        
    App::GroupExtension::initExtension(obj);
}

PropertyPlacement& GeoFeatureGroupExtension::placement() {

    if(!getExtendedContainer())
        throw Base::RuntimeError("GeoFeatureGroupExtension was not applied to GeoFeature");
    
    return static_cast<App::GeoFeature*>(getExtendedContainer())->Placement;
}


void GeoFeatureGroupExtension::transformPlacement(const Base::Placement &transform)
{
    // NOTE: Keep in sync with APP::GeoFeature
    Base::Placement plm = this->placement().getValue();
    plm = transform * plm;
    this->placement().setValue(plm);
}

std::vector<App::DocumentObject*> GeoFeatureGroupExtension::getGeoSubObjects () const {
    const auto & objs = Group.getValues();

    std::set<const App::GroupExtension*> processedGroups;
    std::set<App::DocumentObject*> rvSet;
    std::set<App::DocumentObject*> curSearchSet (objs.begin(), objs.end());

    processedGroups.insert ( this );

    while ( !curSearchSet.empty() ) {
        rvSet.insert ( curSearchSet.begin (), curSearchSet.end () );

        std::set<App::DocumentObject*> nextSearchSet;
        for ( auto obj: curSearchSet) {
            if ( isNonGeoGroup (obj) ) {
                const App::DocumentObjectGroup *grp = static_cast<const App::DocumentObjectGroup *> (obj);
                // Check if we havent already processed the element may happen in case of nontree structure
                // Note: if the condition is false this generally indicates malformed structure
                if ( processedGroups.find (grp) == processedGroups.end() ) {
                    processedGroups.insert ( grp );
                    const auto & objs = grp->Group.getValues();
                    nextSearchSet.insert (objs.begin(), objs.end());
                }
            }
        }
        nextSearchSet.swap (curSearchSet);
    }

    return std::vector<App::DocumentObject*> ( rvSet.begin(), rvSet.end() );
}

bool GeoFeatureGroupExtension::geoHasObject (const DocumentObject* obj) const {
    const auto & objs = Group.getValues();

    if (!obj) {
        return false;
    }

    std::set<const App::GroupExtension*> processedGroups;
    std::set<const App::DocumentObject*> curSearchSet (objs.begin(), objs.end());

    processedGroups.insert ( this );

    while ( !curSearchSet.empty() ) {
        if ( curSearchSet.find (obj) != curSearchSet.end() ) {
            return true;
        }
        std::set<const App::DocumentObject*> nextSearchSet;
        for ( auto obj: curSearchSet) {
            if ( isNonGeoGroup (obj) ) {
                const App::DocumentObjectGroup *grp = static_cast<const App::DocumentObjectGroup *> (obj);
                if ( processedGroups.find (grp) == processedGroups.end() ) {
                    processedGroups.insert ( grp );
                    const auto & objs = grp->Group.getValues();
                    nextSearchSet.insert (objs.begin(), objs.end());
                }
            }
        }
        nextSearchSet.swap (curSearchSet);
    }
    return false;
}

DocumentObject* GeoFeatureGroupExtension::getGroupOfObject(const DocumentObject* obj, bool indirect)
{
    const Document* doc = obj->getDocument();
    std::vector<DocumentObject*> grps = doc->getObjectsWithExtension(GeoFeatureGroupExtension::getExtensionClassTypeId());
    for (std::vector<DocumentObject*>::const_iterator it = grps.begin(); it != grps.end(); ++it) {
        GeoFeatureGroupExtension* grp = (*it)->getExtensionByType<GeoFeatureGroupExtension>();
        if ( indirect ) {
            if (grp->geoHasObject(obj)) {
                return dynamic_cast<App::DocumentObject*>(grp);
            }
        } else {
            if (grp->hasObject(obj)) {
                return dynamic_cast<App::DocumentObject*>(grp);
            }
        }
    }

    return 0;
}


PyObject *GeoFeatureGroupExtension::extensionGetPySubObjects(
    const char *element, const Base::Matrix4D &mat, bool transform) const 
{
    if(!element || *element==0) return 0;

    std::vector<PyObject *> ret;
    Base::Matrix4D _mat;
    if(transform) 
        _mat = mat*const_cast<GeoFeatureGroupExtension*>(this)->placement().getValue().toMatrix();
    const Base::Matrix4D &matNext = transform?_mat:mat;

    auto children = getGeoSubObjects();
    std::string _name;
    const char *name = element;
    const char *next = 0;
    next = strchr(element,'.');
    if(next) {
        _name = std::string(element,next-element);
        name = _name.c_str();
        ++next;
    }
    for(auto child : children) {
        if(!child || !child->getNameInDocument() || 
            strcmp(name,child->getNameInDocument())!=0)
            continue;
        return child->getPySubObject(next,matNext,true);
    }
    return 0;
}

// Python feature ---------------------------------------------------------

namespace App {
EXTENSION_PROPERTY_SOURCE_TEMPLATE(App::GeoFeatureGroupExtensionPython, App::GeoFeatureGroupExtension)

// explicit template instantiation
template class AppExport ExtensionPythonT<GroupExtensionPythonT<GeoFeatureGroupExtension>>;
}
