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
# include <Inventor/nodes/SoSeparator.h>
# include <Inventor/nodes/SoSwitch.h>
# include <Inventor/nodes/SoTransform.h>
# include <Inventor/SoPickedPoint.h>
# include <Inventor/details/SoDetail.h>
#endif
#include <QApplication>
#include <QFileInfo>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/bind.hpp>
#include <Base/Console.h>
#include "Application.h"
#include "BitmapFactory.h"
#include "Document.h"
#include "Selection.h"
#include "MainWindow.h"
#include "ViewProviderLink.h"
#include "ViewProviderBuilder.h"
#include <SoFCUnifiedSelection.h>

FC_LOG_LEVEL_INIT("App::Link",true,true)

using namespace Gui;

////////////////////////////////////////////////////////////////////////////

class ViewProviderLink::LinkInfo {

public:
    int ref;

    int vref; //visibility ref counter

    boost::signals::connection connChangeIcon;

    ViewProviderDocumentObject *pcLinked;
    std::set<ViewProviderLink*> links;

    typedef ViewProviderLink::LinkInfoPtr Pointer;

    enum SnapshotType {
        //three type of root overrides:
        
        //override transform and visibility
        SnapshotTransform,
        //override visibility
        SnapshotVisible,
        //override none (for child objects of a container)
        SnapshotContainer,
    };
    std::array<SoFCSelectionRoot*,3> pcSnapshots;
    std::array<SoSwitch*,3> pcSwitches;
    SoSwitch *pcLinkedSwitch;

    // for group type view providers
    SoGroup *pcChildGroup;
    typedef std::map<SoNode *, Pointer> NodeMap;
    NodeMap nodeMap;

    std::map<const void *, QIcon> iconMap;

    static ViewProviderDocumentObject *getView(App::DocumentObject *obj) {
        if(obj && obj->getNameInDocument()) {
            Document *pDoc = Application::Instance->getDocument(obj->getDocument());
            if(pDoc) {
                ViewProvider *vp = pDoc->getViewProvider(obj);
                if(vp && vp->isDerivedFrom(ViewProviderDocumentObject::getClassTypeId()))
                    return static_cast<ViewProviderDocumentObject*>(vp);
            }
        }
        return 0;
    }

    static Pointer get(App::DocumentObject *obj, ViewProviderLink *l=0) {
        return get(getView(obj),l);
    }

    static Pointer get(ViewProviderDocumentObject *vp, ViewProviderLink *l=0) {
        if(!vp) return Pointer();

        if(vp==l || vp->getLinkedView(true)==l)
            throw Base::Exception("cyclic links");

        auto ext = vp->getExtensionByType<ViewProviderLink::Observer>(true);
        if(!ext) {
            ext = new ViewProviderLink::Observer();
            ext->initExtension(vp);
        }
        if(!ext->linkInfo) {
            // extension can be created automatically when restored from document,
            // with an empty linkInfo. So we need to check here.
            ext->linkInfo = Pointer(new LinkInfo(vp));
        }
        if(l) {
            ext->linkInfo->links.insert(l);
            if(l->visible)
                ext->linkInfo->setVisible(true);
        }
        return ext->linkInfo;
    }

    LinkInfo(ViewProviderDocumentObject *vp)
        :ref(0),vref(0),pcLinked(vp),pcLinkedSwitch(0),pcChildGroup(0) 
    {
        FC_TRACE("new link to " << pcLinked->getObject()->getNameInDocument());
        connChangeIcon = vp->signalChangeIcon.connect(
                boost::bind(&LinkInfo::slotChangeIcon,this));
        pcSnapshots.fill(0);
        pcSwitches.fill(0);
        slotChangeIcon();
        update();
    }

    ~LinkInfo() {
        clear();
    }

    void setVisible(bool visible) {
        if(visible) {
            if(++vref == 1)
                update();
        }else if(vref>0)
            --vref;
        else
            FC_WARN("visibility ref count error");
    }

    void getNodeNames(Document *pDocument, QMap<SoNode*, QString> &nodeNames) const {
        if(!isLinked()) return;
        QString label;
        if(pDocument != pcLinked->getDocument())
            label = QString::fromUtf8(getDocName());
        label += QString::fromAscii("->") + 
            QString::fromUtf8(pcLinked->getObject()->Label.getValue());
        for(auto node : pcSnapshots)
            nodeNames[node] = label;
        for(auto v : nodeMap) 
            v.second->getNodeNames(pDocument,nodeNames);
    }

    bool checkName(const char *name) const {
        return isLinked() && strcmp(name,getLinkedName())==0;
    }

    void remove(ViewProviderLink *l) {
        auto it = links.find(l);
        if(it!=links.end()) {
            if(l->visible) 
                setVisible(false);
            links.erase(it);
        }
    }

    bool isLinked() const {
        return pcLinked && pcLinked->getObject() && 
           pcLinked->getObject()->getNameInDocument();
    }

    const char *getLinkedName() const {
        return pcLinked->getObject()->getNameInDocument();
    }

    const char *getDocName() const {
        return pcLinked->getDocument()->getDocument()->getName();
    }

    void clear() {
        for(auto &node : pcSnapshots) {
            if(node) {
                node->removeAllChildren();
                node->resetContext();
                node->unref();
                node = 0;
            }
        }
        for(auto &node : pcSwitches) {
            if(node) {
                node->removeAllChildren();
                node->unref();
                node = 0;
            }
        }
        if(pcLinkedSwitch) {
            pcLinkedSwitch->unref();
            pcLinkedSwitch = 0;
        }
        if(pcChildGroup) {
            pcChildGroup->removeAllChildren();
            pcChildGroup->unref();
            pcChildGroup = 0;
        }
    }

    void onDelete() {
        std::set<ViewProviderLink*> linksTmp;
        linksTmp.swap(links);
        for(ViewProviderLink *link : linksTmp)
            link->unlink();
        clear();
        pcLinked = 0;
        vref = 0;
    }

    void updateSwitch(const char *propName = 0) {
        if(!isLinked() || !pcLinkedSwitch) return;
        if(propName)
            FC_TRACE(getLinkedName() << " display changed " << propName);
        int index = pcLinkedSwitch->whichChild.getValue();
        for(size_t i=0;i<pcSwitches.size();++i) {
            if(!pcSwitches[i]) 
                continue;
            int count = pcSwitches[i]->getNumChildren();
            if((index<0 && i==SnapshotContainer) || !count)
                pcSwitches[i]->whichChild = -1;
            else if(count>pcLinked->getDefaultMode())
                pcSwitches[i]->whichChild = pcLinked->getDefaultMode();
            else
                pcSwitches[i]->whichChild = 0;
        }
    }

    friend void intrusive_ptr_add_ref(LinkInfo *px){
        ++px->ref;
    }

    friend void intrusive_ptr_release(LinkInfo *px){
        int r = --px->ref;
        assert(r>=0);
        if(r==0) 
            delete px;
        else if(r==1) 
            px->clear();
    }

    SoSeparator *getSnapshot(SnapshotType type, bool update=false) {

        SoSeparator *root;
        if(!isLinked() || !(root=pcLinked->getRoot())) 
            return 0;

        auto &pcSnapshot = pcSnapshots[type];
        auto &pcModeSwitch = pcSwitches[type];
        if(pcSnapshot) {
            if(!update) return pcSnapshot;
        }else{
            pcSnapshot = new SoFCSelectionRoot;
            pcSnapshot->ref();
            pcModeSwitch = new SoSwitch;
            pcModeSwitch->ref();
        }

        if(pcLinkedSwitch) {
            pcLinkedSwitch->unref();
            pcLinkedSwitch = 0;
        }

        pcSnapshot->removeAllChildren();
        if(!Gui::Selection().hasSelection() &&
           !Gui::Selection().getPreselection().pDocName)
        {
            pcSnapshot->resetContext();
        }
        pcModeSwitch->whichChild = -1;
        pcModeSwitch->removeAllChildren();

        for(int i=0,count=root->getNumChildren();i<count;++i) {
            SoNode *node = root->getChild(i);
            if(type==SnapshotTransform && node->getTypeId()==SoTransform::getClassTypeId())
                continue;
            if(node->getTypeId() != SoSwitch::getClassTypeId()) {
                pcSnapshot->addChild(node);
                continue;
            }
            if(pcLinkedSwitch) {
                FC_WARN(getLinkedName() << " more than one switch node");
                pcSnapshot->addChild(node);
                continue;
            }
            pcLinkedSwitch = static_cast<SoSwitch*>(node);
            pcLinkedSwitch->ref();

            pcSnapshot->addChild(pcModeSwitch);
            if(pcChildGroup) {
                pcModeSwitch->addChild(pcChildGroup);
                continue;
            }
            for(int i=0,count=pcLinkedSwitch->getNumChildren();i<count;++i)
                pcModeSwitch->addChild(pcLinkedSwitch->getChild(i));
        }
        updateSwitch();
        return pcSnapshot;
    }

    void update(const App::Property *prop = nullptr) {
        if(!isLinked()) return;
        
        const char *propName = prop?prop->getName():"";
        if(pcLinked->isRestoring()) {
            FC_TRACE("restoring '" << getLinkedName() << "' " << propName);
            return;
        }

        if(vref)
            pcLinked->forceUpdate();

        if(!pcLinked->getChildRoot())
            FC_TRACE("update '" << getLinkedName() << "' " << propName);
        else{
            FC_TRACE("update group '" << getLinkedName() << "' " << propName);

            if(!pcChildGroup) {
                pcChildGroup = new SoGroup;
                pcChildGroup->ref();
            }else
                pcChildGroup->removeAllChildren();

            NodeMap nodeMap;

            for(auto child : pcLinked->claimChildren3D()) {
                Pointer info = get(child);
                if(!info) continue;
                SoNode *node = info->getSnapshot(SnapshotContainer);
                if(!node) continue;
                nodeMap[node] = info;
                pcChildGroup->addChild(node);
            }

            // Use swap instead of clear() here to avoid potential link
            // destruction
            this->nodeMap.swap(nodeMap);
        }

        for(size_t i=0;i<pcSnapshots.size();++i) 
            if(pcSnapshots[i]) 
                getSnapshot(static_cast<SnapshotType>(i),true);
    }

    ViewProviderDocumentObject *getElementView(
            bool checkname, const char *element, const char **psubname) const 
    {
        if(!isLinked()) return 0;

        const char *subname;
        if(checkname) {
            subname = checkSubname(pcLinked->getObject(),element);
            if(!subname) return 0;
        }else
            subname = element;

        if(!pcChildGroup) {
            if(pcLinked->isDerivedFrom(ViewProviderLink::getClassTypeId()))
                return pcLinked->getElementView(subname,psubname);
            if(psubname) *psubname = subname;
            return pcLinked;
        }
        for(auto &v : nodeMap) {
            auto *vp = v.second->getElementView(true,subname,psubname);
            if(vp) return vp;
        }
        return 0;
    }

    bool getElementPicked(bool addname, 
            const SoPickedPoint *pp, std::stringstream &str) const 
    {
        if(!pp || !isLinked())
            return false;

        if(addname) {
            // if(pcLinked->getDocument()!=link->getDocument())
            //     str << '*' << getDocName() << '*';
            str << getLinkedName() <<'.';
        }
        
        if(pcChildGroup) {
            SoPath *path = pp->getPath();
            int index = path->findNode(pcChildGroup);
            if(index<=0) return false;
            auto it = nodeMap.find(path->getNode(index+1));
            if(it==nodeMap.end()) return false;
            return it->second->getElementPicked(true,pp,str);
        }else
            str<<pcLinked->getElementPicked(pp);
        return true;
    }

    static const char *checkSubname(App::DocumentObject *obj, const char *subname) {
#define CHECK_NAME(_name,_end) do{\
            if(!_name) return 0;\
            const char *_n = _name;\
            for(;*subname && *_n; ++subname,++_n)\
                if(*subname != *_n) break;\
            if(*_n || (*subname!=0 && *subname!=_end))\
                    return 0;\
            if(*subname == _end) ++subname;\
        }while(0)

        if(subname[0] == '*') {
            ++subname;
            CHECK_NAME(obj->getDocument()->getName(),'*');
        }
        CHECK_NAME(obj->getNameInDocument(),'.');
        return subname;
    }

    bool getDetail(bool checkname, int type, const char* subname, 
            SoDetail *&det, SoFullPath **path) const 
    {
        if(!isLinked()) return false;

        if(checkname) {
            subname = checkSubname(pcLinked->getObject(),subname);
            if(!subname) return false;
        }

        if(path) {
            (*path)->append(pcSnapshots[type]);
            (*path)->append(pcSwitches[type]);
        }
        if(*subname == 0) return true;
        if(!pcChildGroup) {
            det = pcLinked->getDetailPath(subname,path);
            return true;
        }
        if(path){
            (*path)->append(pcChildGroup);
            if(pcLinked->isDerivedFrom(ViewProviderLink::getClassTypeId()))
                type = SnapshotVisible;
            else
                type = SnapshotContainer;
        }
        for(auto v : nodeMap) {
            if(v.second->getDetail(true,type,subname,det,path))
                return true;
        }
        return false;
    }

    void slotChangeIcon() {
        if(!isLinked()) return;
        iconMap.clear();
        for(auto link : links) 
            link->signalChangeIcon();
    }

    QIcon getIcon(const char * const * xpm) {
        static int iconSize = -1;
        if(iconSize < 0) 
            iconSize = QApplication::style()->standardPixmap(QStyle::SP_DirClosedIcon).width();

        QIcon &iconLink = iconMap[xpm];
        if(iconLink.isNull()) {
            QPixmap px(xpm);
            QIcon icon = pcLinked->getIcon();
            iconLink = QIcon();
            iconLink.addPixmap(BitmapFactory().merge(icon.pixmap(iconSize, iconSize, QIcon::Normal, QIcon::Off),
                px,BitmapFactoryInst::BottomLeft), QIcon::Normal, QIcon::Off);
            iconLink.addPixmap(BitmapFactory().merge(icon.pixmap(iconSize, iconSize, QIcon::Normal, QIcon::On ),
                px,BitmapFactoryInst::BottomLeft), QIcon::Normal, QIcon::On);
        }
        return iconLink;
    }
};

////////////////////////////////////////////////////////////////////////////////////

EXTENSION_TYPESYSTEM_SOURCE(Gui::ViewProviderLink::Observer,Gui::ViewProviderExtension);

ViewProviderLink::Observer::Observer() {
    // TODO: any better way to get deleted automatically?
    m_isPythonExtension = true;
    initExtensionType(ViewProviderLink::Observer::getExtensionClassTypeId());
}

void ViewProviderLink::Observer::extensionOnDeleting() {
    if(linkInfo) linkInfo->onDelete();
}

void ViewProviderLink::Observer::extensionShow() {
    if(linkInfo) linkInfo->updateSwitch("Visibility");
}

void ViewProviderLink::Observer::extensionHide() {
    if(linkInfo) linkInfo->updateSwitch("Visibility");
}

void ViewProviderLink::Observer::extensionOnChanged(const App::Property *prop) {
    if(!linkInfo) return;
    if(prop) {
        if(strcmp(prop->getName(),"Visibility")==0)
            return;
        if(strcmp(prop->getName(),"DisplayMode")==0) {
            linkInfo->updateSwitch("DisplayMode");
            return;
        }
    }
    linkInfo->update(prop);
}

void ViewProviderLink::Observer::extensionUpdateData(const App::Property *prop) {
    if(linkInfo) linkInfo->update(prop);
}

void ViewProviderLink::Observer::extensionFinishRestoring() {
    if(linkInfo) {
        FC_TRACE("linked finish restoing");
        linkInfo->update();
    }
}

void ViewProviderLink::Observer::extensionGetLinks(std::vector<ViewProviderDocumentObject*> &links) const {
    if(!linkInfo) return;
    links.reserve(links.size()+linkInfo->links.size());
    for(auto link : linkInfo->links)
        links.push_back(link);
}

///////////////////////////////////////////////////////////////////////////////////

PROPERTY_SOURCE(Gui::ViewProviderLink, Gui::ViewProviderDocumentObject)

static std::array<Base::Type,ViewProviderLink::PropNameMax> PropTypes;
static std::shared_ptr<ViewProviderLink::PropNameMap> PropConf;
std::map<std::shared_ptr<ViewProviderLink::PropNameMap>, 
    std::shared_ptr<ViewProviderLink::PropNames> > PropNameMaps;

ViewProviderLink::ViewProviderLink()
    :moveChildFromRoot(false),linkTransform(false),isXLink(false),visible(false)
{
    sPixmap = "link";

    DisplayMode.setStatus(App::Property::Status::Hidden, true);

    pcRoot->removeAllChildren();
    pcRoot->unref();
    pcRoot = new SoFCSelectionRoot();
    pcRoot->ref();
    pcRoot->addChild(pcTransform);
    pcRoot->addChild(pcModeSwitch);

    setDefaultMode(0);

    if(!PropConf) {
        PropConf = std::make_shared<PropNameMap>();
        auto &conf = *PropConf;
        conf["LinkPlacement"] = PropNamePlacement;
        conf["LinkedObject"] = PropNameObject;
        conf["LinkTransform"] = PropNameTransform;
        conf["LinkMoveChild"] = PropNameMoveChild;
        conf["LinkScale"] = PropNameScale;
        
        PropTypes[PropNamePlacement] = App::PropertyPlacement::getClassTypeId();
        PropTypes[PropNameObject] = App::PropertyLink::getClassTypeId();
        PropTypes[PropNameTransform] = App::PropertyBool::getClassTypeId();
        PropTypes[PropNameMoveChild] = App::PropertyBool::getClassTypeId();
        PropTypes[PropNameScale] = App::PropertyVector::getClassTypeId();
    }
    setPropertyNames(PropConf);
}

ViewProviderLink::~ViewProviderLink()
{
    unlink();
}

void ViewProviderLink::setPropertyNames(std::shared_ptr<PropNameMap> conf) {
    propName2Type = conf;
    if(!propName2Type)
        propName2Type = PropConf;
    auto &nmap = PropNameMaps[propName2Type];
    if(!nmap) {
        nmap = std::make_shared<PropNames>();
        for(const auto &v : *propName2Type)
            (*nmap)[v.second] = v.first;
    }
    propType2Name = nmap;
}

void ViewProviderLink::attach(App::DocumentObject *pcObj) {
    std::map<std::string,App::Property*> pmap;
    pcObj->getPropertyMap(pmap);

    auto it = pmap.end();
    const std::string *name;

#define HAS_PROP(_type) \
    ((name=&propType2Name->at(PropName##_type)) && name->length() && \
     (it=pmap.find(*name))!=pmap.end() && \
     it->second->isDerivedFrom(PropTypes[PropName##_type]))

    if(!HAS_PROP(Object))
        FC_ERR("ViewProviderLink: Invalid document object");
    inherited::attach(pcObj);
}

bool ViewProviderLink::useNewSelectionModel(void) const {
    return true;
}

void ViewProviderLink::getNodeNames(QMap<SoNode*, QString> &nodeNames) const {
    if(linkInfo) linkInfo->getNodeNames(getDocument(),nodeNames);
}

std::string ViewProviderLink::getElementPicked(const SoPickedPoint *pp) const {
    std::stringstream str;
    if(linkInfo && linkInfo->getElementPicked(false,pp,str))
        return str.str();
    return std::string();
}

ViewProviderDocumentObject *ViewProviderLink::getLinkedView(bool recursive) {
    if(linkInfo && linkInfo->isLinked()) {
        if(!recursive) return linkInfo->pcLinked;
        return linkInfo->pcLinked->getLinkedView(true);
    }
    return this;
}

SoDetail* ViewProviderLink::getDetail(const char* subname) const {
    return getDetailPath(subname,0);
}

SoDetail* ViewProviderLink::getDetailPath(const char *subname, SoFullPath **path) const {
    if(!linkInfo) return 0;
    bool freepath = false;
    if(path && !*path) {
        *path = (SoFullPath*)(new SoPath(10));
        (*path)->ref();
        (*path)->append(pcRoot);
        (*path)->append(pcModeSwitch);
        freepath = true;
    }
    if(!subname || *subname==0) return 0;

    SoDetail *det = 0;
    if(!linkInfo->getDetail(false,
       linkTransform?LinkInfo::SnapshotVisible:LinkInfo::SnapshotTransform,subname,det,path)) 
    {
        if(freepath) {
            (*path)->unref();
            *path = 0;
        }
    }
    return det;
}

void ViewProviderLink::unlink() {
    if(linkInfo) {
        pcModeSwitch->whichChild = -1;
        pcModeSwitch->removeAllChildren();
        linkInfo->remove(this);
        linkInfo.reset();
    }
}

bool ViewProviderLink::onDelete(const std::vector<std::string> &svec) {
    if(!inherited::onDelete(svec)) return false;
    unlink();
    return true;
}

void ViewProviderLink::onChanged(const App::Property* prop) {
    if(prop == &Visibility) {
        if(visible!=Visibility.getValue()) {
            visible = !visible;
            if(linkInfo) 
                linkInfo->setVisible(visible);
        }
    }else
        checkProperty(prop,false);
    inherited::onChanged(prop);
}

bool ViewProviderLink::findLink(const App::PropertyLink *prop) {
    App::DocumentObject *pcLinkedObj = prop->getValue();
    if(linkInfo && linkInfo->pcLinked->getObject()==pcLinkedObj) 
        return false;
    isXLink = prop->isDerivedFrom(App::PropertyXLink::getClassTypeId()) &&
              static_cast<const App::PropertyXLink*>(prop)->getDocumentPath();
    unlink();
    linkInfo = LinkInfo::get(pcLinkedObj,this);
    setup();
    signalChangeIcon();
    return true;
}

void ViewProviderLink::setup() {
    if(!linkInfo || !getObject() || !getObject()->getNameInDocument()) {
        pcModeSwitch->whichChild = -1;
        pcModeSwitch->removeAllChildren();
        return;
    }

    if(isRestoring()) {
        FC_TRACE("restoring '" << getObject()->getNameInDocument() << "'");
        return;
    }

    pcModeSwitch->whichChild = -1;
    pcModeSwitch->removeAllChildren();

    FC_TRACE("setup '" << getObject()->getNameInDocument() << "'");

    SoNode *node = linkInfo->getSnapshot(
        linkTransform?LinkInfo::SnapshotVisible:LinkInfo::SnapshotTransform,false);
    if(node) {
        pcModeSwitch->addChild(node);
        if(Visibility.getValue())
            show();
    }
}

void ViewProviderLink::finishRestoring() {
    FC_TRACE("finish restoring");
    setup();
}

void ViewProviderLink::checkProperty(const App::Property *prop, bool fromObject) {

    FC_TRACE("check property '" << getObject()->getNameInDocument() << "': " << prop->getName());

    auto it = propName2Type->find(prop->getName());
    if(it!=propName2Type->end() && prop->isDerivedFrom(PropTypes[it->second])) {
        switch(it->second) {
        case PropNameScale: {
            const Base::Vector3d &v = 
                static_cast<const App::PropertyVector*>(prop)->getValue();
            pcTransform->scaleFactor.setValue(v.x,v.y,v.z);
            break;
        } case PropNameTransform:
            if(linkTransform != static_cast<const App::PropertyBool*>(prop)->getValue()) {
                linkTransform = !linkTransform;
                setup();
            }
            break;
        case PropNameObject:
            if(fromObject) 
                findLink(static_cast<const App::PropertyLink*>(prop));
            break;
        case PropNamePlacement:
            if(fromObject) {
                auto v = pcTransform->scaleFactor.getValue();
                ViewProviderGeometryObject::updateTransform(
                    static_cast<const App::PropertyPlacement*>(prop)->getValue(), pcTransform);
                pcTransform->scaleFactor.setValue(v);
            }
            break;

        case PropNameMoveChild: 
            moveChildFromRoot = static_cast<const App::PropertyBool*>(prop)->getValue();
            break;
        default:
            break;
        }
    }
}

void ViewProviderLink::updateData(const App::Property *prop) {
    if(getObject() && getObject()->getNameInDocument())
        checkProperty(prop,true);
    inherited::updateData(prop);
}

std::vector<App::DocumentObject*> ViewProviderLink::claimChildren(void) const
{
    std::vector<App::DocumentObject *> ret;
    if(linkInfo && linkInfo->isLinked())
        ret = linkInfo->pcLinked->claimChildren();
    return ret;
}

QIcon ViewProviderLink::getIcon() const{
    if(linkInfo && linkInfo->isLinked())
        return linkInfo->getIcon(getOverlayPixmap(isXLink));
    return Gui::BitmapFactory().pixmap(sPixmap);
}

ViewProviderDocumentObject *ViewProviderLink::getElementView(
        const char *element, const char **subname) 
{
    if(!element || *element==0) return this;
    if(!linkInfo) return 0;
    if(!linkInfo->pcChildGroup)
        return this;
    return linkInfo->getElementView(false,element,subname);
}

const char * const * ViewProviderLink::getOverlayPixmap(bool xlink) const{
    // right top pointing arrow for normal link
    static const char * const feature_link_xpm[]={
        "8 8 3 1",
        ". c None",
        "# c #000000",
        "a c #ffffff",
        "########",
        "##aaaaa#",
        "####aaa#",
        "###aaaa#",
        "##aaa#a#",
        "#aaa##a#",
        "#aa#####",
        "########"};

    // left bottom pointing arrow, not looking good?
    // static const char * const feature_xlink_xpm[]={
    //     "8 8 3 1",
    //     ". c None",
    //     "# c #000000",
    //     "a c #ffffff",
    //     "########",
    //     "#####aa#",
    //     "#a##aaa#",
    //     "#a#aaa##",
    //     "#aaaa###",
    //     "#aaa####",
    //     "#aaaaa##",
    //     "########"};

    // left top pointing arrow for xlink
    static const char * const feature_xlink_xpm[]={
        "8 8 3 1",
        ". c None",
        "# c #000000",
        "a c #ffffff",
        "########",
        "#aaaaa##",
        "#aaa####",
        "#aaaa###",
        "#a#aaa##",
        "#a##aaa#",
        "#####aa#",
        "########"};

    return xlink?feature_xlink_xpm:feature_link_xpm;
}

bool ViewProviderLink::canDragObject(App::DocumentObject* obj) const {
    if(!linkInfo || !linkInfo->isLinked()) return false;
    return linkInfo->pcLinked->canDragObject(obj);
}

bool ViewProviderLink::canDragObjects() const {
    if(!linkInfo || !linkInfo->isLinked()) return false;
    return linkInfo->pcLinked->canDragObjects();
}

void ViewProviderLink::dragObject(App::DocumentObject* obj) {
    if(!linkInfo || !linkInfo->isLinked())
        throw Base::RuntimeError("ViewProviderLink::dragObject: invalid link");
    linkInfo->pcLinked->dragObject(obj);
}

bool ViewProviderLink::canDropObject(App::DocumentObject* obj) const {
    if(!linkInfo || !linkInfo->isLinked()) return true;
    return linkInfo->pcLinked->canDropObject(obj);
}

bool ViewProviderLink::canDropObjects() const {
    if(!linkInfo || !linkInfo->isLinked()) return true;
    return linkInfo->pcLinked->canDropObjects();
}

void ViewProviderLink::dropObject(App::DocumentObject* obj) {
    if(linkInfo && linkInfo->isLinked()) {
        linkInfo->pcLinked->dropObject(obj);
        return;
    }
    std::map<std::string,App::Property*> pmap;
    getObject()->getPropertyMap(pmap);
    auto it = pmap.end();
    const std::string *name;
    if(!HAS_PROP(Object))
        throw Base::RuntimeError("ViewProviderLink::dropObject: no link property");
    static_cast<App::PropertyLink*>(it->second)->setValue(obj);
}

// Python object -----------------------------------------------------------------------

namespace Gui {
/// @cond DOXERR
PROPERTY_SOURCE_TEMPLATE(Gui::ViewProviderLinkPython, Gui::ViewProviderLink)
/// @endcond

// explicit template instantiation
template class GuiExport ViewProviderPythonFeatureT<Gui::ViewProviderLink>;
}

