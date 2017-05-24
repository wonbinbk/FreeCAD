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
#include "ViewProviderDocumentObject.h"
#include "ViewProviderExtension.h"

namespace Gui {

class SoFCSelectionRoot;

class LinkInfo;
typedef boost::intrusive_ptr<LinkInfo> LinkInfoPtr;

class GuiExport ViewProviderLinkObserver: public ViewProviderExtension {
    EXTENSION_TYPESYSTEM_HEADER_WITH_OVERRIDE();
public:
    ViewProviderLinkObserver();
    void extensionOnDeleting() override;
    void extensionOnChanged(const App::Property *) override;
    void extensionUpdateData(const App::Property*) override;
    void extensionHide(void) override;
    void extensionShow(void) override;
    void extensionFinishRestoring() override;
    void extensionGetLinks(std::vector<ViewProviderDocumentObject*> &) const override;
    void extensionGetNodeNames(Gui::Document *, QMap<SoNode*, QString> &) const override;

    LinkInfoPtr linkInfo;
};

class GuiExport LinkHandle : public Base::BaseClass {
    TYPESYSTEM_HEADER();
public:
    LinkHandle();
    ~LinkHandle();

    void unlink(LinkInfoPtr link);
    bool isLinked() const;

    SoSeparator *getLinkRoot() const {return pcLinkRoot;}

    QIcon getLinkedIcon(QPixmap overlay) const;

    virtual void onLinkUpdate();
    virtual void onLinkedIconChange(LinkInfoPtr link);

    std::string getSubName(LinkInfoPtr);

    void setLink(App::DocumentObject *obj, bool reorder = false,
            const std::vector<std::string> &subs = std::vector<std::string>());

    void setMaterial(const App::Material *material);

    static void setTransform(SoTransform *pcTransform, const Base::Matrix4D &mat);

    enum SnapshotType {
        //three type of snapshot to override linked root node:
        
        //override transform and visibility
        SnapshotTransform = 0,
        //override visibility
        SnapshotVisible = 1,
        //override none (for child objects of a container)
        SnapshotChild = 2,

        SnapshotMax,

        //special type for subelement linking
        SnapshotContainer = -1,
        // subelement linking with transform override
        SnapshotContainerTransform = -2,
    };
    void setNodeType(SnapshotType type);

    void setVisibility(bool visible);
    bool getVisibility() const {return visible;}

    ViewProviderDocumentObject *linkGetLinkedView(bool recursive, int depth=0) const;
    ViewProviderDocumentObject *linkGetElementView(const char *, const char **) const;
    bool linkGetDetailPath(const char *, SoFullPath *, SoDetail *&) const;
    bool linkGetElementPicked(const SoPickedPoint *, std::string &) const;

    ViewProviderDocumentObject *getOwner() const {return owner;}
    void setOwner(ViewProviderDocumentObject *vpd) {owner=vpd;}

    friend class LinkInfo;

protected:
    ViewProviderDocumentObject *owner;
    SoSeparator *pcLinkRoot;
    SoMaterial  *pcMaterial;
    SnapshotType nodeType;
    LinkInfoPtr linkInfo;
    struct SubInfo {
        LinkHandle &handle;
        SoSeparator *pcNode;
        SoTransform *pcTransform;
        LinkInfoPtr link;
        std::set<std::string> elements;

        SubInfo(LinkHandle &handle);
        ~SubInfo();
        void unlink(bool reset=false);
        bool isLinked() const;
    };
    std::map<std::string, SubInfo> subInfo;
    bool visible;
};
    
class GuiExport ViewProviderLink : public ViewProviderDocumentObject
{
    PROPERTY_HEADER(Gui::ViewProviderLink);
    typedef ViewProviderDocumentObject inherited;

public:
    App::PropertyBool LinkUseMaterial;
    App::PropertyColor LinkShapeColor;
    App::PropertyPercent LinkTransparency;
    App::PropertyMaterial LinkShapeMaterial;
    App::PropertyBool Selectable;

    ViewProviderLink();
    virtual ~ViewProviderLink();

    void attach(App::DocumentObject *pcObj) override;

    bool isSelectable(void) const override {return Selectable.getValue();}

    bool useNewSelectionModel(void) const override {return true;}

    void updateData(const App::Property*) override;
    void onChanged(const App::Property* prop) override;
    std::vector<App::DocumentObject*> claimChildren(void) const override;
    bool getElementPicked(const SoPickedPoint *, std::string &) const override;
    SoDetail* getDetailPath(const char *, SoFullPath *, bool) const override;

    bool onDelete(const std::vector<std::string> &subNames) override;
    
    void finishRestoring() override;

    ViewProviderDocumentObject *getLinkedView(bool recursive, int depth=0) const override;

    ViewProviderDocumentObject *getElementView(const char *, const char **) const override;

    QIcon getIcon(void) const override;

    bool canDragObjects() const override;
    bool canDragObject(App::DocumentObject*) const override;
    void dragObject(App::DocumentObject*) override;
    bool canDropObjects() const override;
    bool canDropObject(App::DocumentObject*) const override;
    bool canDragAndDropObject(App::DocumentObject*) const override;
    void dropObjectEx(App::DocumentObject*, App::DocumentObject*, const char *) override;

    enum LinkConfig {
        HandleAll = -1,
        HandleDragDrop = 0,
        HandlePick = 1,
        HandleLinkedView = 2,
        HandleElementView = 3,
        HandleClaimChildren = 4,
    };
    void setConfig(LinkConfig bit, bool on);
    bool testConfig(LinkConfig bit) {return config.test((size_t)bit);}

    enum PropIndex {
        PropPlacement,
        PropObject,
        PropSubs,
        PropTransform,
        PropScale,
        PropRecomputed,
        PropMax,
    };
    virtual const char *propNameFromIndex(PropIndex index) const;
    virtual QPixmap getOverlayPixmap() const;

private:
    int propIndexFromName(const char *name) const;
protected:
    LinkHandle handle;
    std::bitset<32> config;
    bool linkTransform;
    bool xlink;
    bool sublink;
};

} //namespace Gui


#endif // GUI_VIEWPROVIDER_LINK_H
