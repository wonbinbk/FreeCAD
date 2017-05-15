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

class Link : public Part::Feature, public App::LinkExtension
{
    PROPERTY_HEADER_WITH_EXTENSIONS(Part::Link);

public:

    App::PropertyBool LinkShape;

    Link();

    const char* getViewProviderName(void) const override {
        return "PartGui::ViewProviderPartLink";
    }

    App::DocumentObjectExecReturn *execute(void) override;

    void onChanged(const App::Property* prop) override;

    void onDocumentRestored() override;

    DocumentObject *getSubObject(const char *element, const char **subname, 
            PyObject **pyObj, Base::Matrix4D *mat, bool transform) const override;

    App::DocumentObject *getLinkedObject(bool recurse, Base::Matrix4D *mat, bool transform) override;

protected:
    App::DocumentObjectExecReturn *buildShape(bool silent);
};

}

#endif

