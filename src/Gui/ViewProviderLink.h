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


#ifndef GUI_VIEWPROVIDER_LINK_H
#define GUI_VIEWPROVIDER_LINK_H

#include <boost/intrusive_ptr.hpp>
#include <App/Link.h>
#include "ViewProviderPythonFeature.h"
#include "ViewProviderDocumentObject.h"
#include "ViewProviderExtension.h"

namespace Gui {

class GuiExport ViewProviderLink : public ViewProviderDocumentObject
{
    PROPERTY_HEADER(Gui::ViewProviderLink);
    typedef ViewProviderDocumentObject inherited;

public:
    ViewProviderLink();
    virtual ~ViewProviderLink();

    void attach(App::DocumentObject *pcObj) override;
    void updateData(const App::Property*) override;
    void onChanged(const App::Property* prop) override;
    std::vector<App::DocumentObject*> claimChildren(void) const override;

    bool useNewSelectionModel(void) const override;
    std::string getElementPicked(const SoPickedPoint *) const override;
    SoDetail* getDetail(const char* subelement) const override;
    SoDetail* getDetailPath(const char *subelement, SoFullPath **) const override;

    bool onDelete(const std::vector<std::string> &subNames) override;
    
    bool canRemoveChildrenFromRoot() const override {
        return moveChildFromRoot;
    }

    ViewProviderDocumentObject *getElementView(
            const char *element, const char **subname) override;

    ViewProviderDocumentObject *getLinkedView(bool recursive=true) override;

    QIcon getIcon(void) const override;

    void getNodeNames(QMap<SoNode*, QString> &nodeNames) const;

    class LinkInfo;
    friend class LinkInfo;
    typedef boost::intrusive_ptr<LinkInfo> LinkInfoPtr;

    class Observer: public ViewProviderExtension {
        EXTENSION_TYPESYSTEM_HEADER_WITH_OVERRIDE();
    public:
        Observer();
        void extensionOnDeleting() override;
        void extensionOnChanged(const App::Property *) override;
        void extensionUpdateData(const App::Property*) override;
        void extensionHide(void) override;
        void extensionShow(void) override;

        LinkInfoPtr linkInfo;
    };

    class DocInfo;
    friend class DocInfo;
    typedef std::shared_ptr<DocInfo> DocInfoPtr;

    enum PropName {
        PropNamePlacement,
        PropNameObject,
        PropNameFile,
        PropNameObjectName,
        PropNameMoveChild,
        PropNameTransform,
        PropNameScale,
        PropNameMax,
    };
    typedef std::map<std::string,PropName> PropNameMap;
    typedef std::array<std::string,ViewProviderLink::PropNameMax> PropNames;

    // ViewProviderLink will keep the passed 'conf'. DO NOT modify afterwards.
    void setPropertyNames(std::shared_ptr<PropNameMap> conf);

protected:
    void unlink(bool unlinkDoc=false);
    void findLink(bool touch=false);
    bool findLink(const App::PropertyString *prop);
    bool findLink(const App::PropertyLink *prop);
    bool updateLink(App::DocumentObject *pcLinkedObj);
    void setup();
    void checkProperty(const App::Property *prop, bool fromObject);

    std::shared_ptr<PropNameMap> propName2Type;

    std::shared_ptr<PropNames> propType2Name;

    LinkInfoPtr linkInfo;
    DocInfoPtr docInfo;

    bool moveChildFromRoot;
    bool linkTransform;
};

typedef Gui::ViewProviderPythonFeatureT<ViewProviderLink> ViewProviderLinkPython;

} //namespace Gui


#endif // GUI_VIEWPROVIDER_LINK_H
