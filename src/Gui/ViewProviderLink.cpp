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
# include <Inventor/nodes/SoTransform.h>
# include <Inventor/SoPickedPoint.h>
# include <Inventor/details/SoDetail.h>
# include <Inventor/misc/SoChildList.h>
# include <Inventor/nodes/SoMaterial.h>
#endif
#include <atomic>
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
#include "ViewProviderGeometryObject.h"
#include <App/GeoFeatureGroupExtension.h>
#include <SoFCUnifiedSelection.h>

FC_LOG_LEVEL_INIT("App::Link",true,true)

using namespace Gui;

////////////////////////////////////////////////////////////////////////////


#if 1
void appendPath(SoPath *path, SoNode *node) {
    if(path->getLength()) {
        SoNode * tail = path->getTail();
        const SoChildList * children = tail->getChildren();
        if(!children || children->find((void *)node)<0) {
            FC_ERR("coin error");
            return;
        }
    }
    path->append(node);
}
#else
#define appendPath(_path, _node) _path->append(_node)
#endif

class Gui::LinkInfo {

public:
    std::atomic<int> ref;
    std::atomic<int> vref; //visibility ref counter

    boost::signals::connection connChangeIcon;

    ViewProviderDocumentObject *pcLinked;
    std::multiset<LinkHandle*> links;

    typedef LinkInfoPtr Pointer;

    std::array<SoFCSelectionRoot*,LinkHandle::SnapshotMax> pcSnapshots;
    std::array<SoSwitch*,LinkHandle::SnapshotMax> pcSwitches;
    SoSwitch *pcLinkedSwitch;

    // for group type view providers
    SoGroup *pcChildGroup;
    typedef std::map<SoNode *, Pointer> NodeMap;
    NodeMap nodeMap;

    std::map<qint64, QIcon> iconMap;

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

    static Pointer get(App::DocumentObject *obj, LinkHandle *l=0) {
        return get(getView(obj),l);
    }

    static Pointer get(ViewProviderDocumentObject *vp, LinkHandle *l=0) {
        if(!vp) return Pointer();

        if(l && l->getOwner()) {
            ViewProviderDocumentObject *linked;
            auto owner = l->getOwner();
            if(vp==owner || (linked=vp->getLinkedView(true))==owner)
                throw Base::RuntimeError("Link: cyclic links");

            if(linked && linked->getObject()) {
                //TODO: check if it is slow here
                auto group = linked->getObject()->getExtensionByType<App::GeoFeatureGroupExtension>(true);
                std::vector<App::GeoFeatureGroupExtension*> groups;
                if(group) groups.push_back(group);
                while(groups.size()) {
                    auto group = groups.back();
                    groups.pop_back();
                    for(auto child : group->Group.getValues()) {
                        if(child == owner->getObject())
                            throw Base::RuntimeError("Link: cyclic group links");
                        child = child->getLinkedObject(true);
                        if(!child) continue;
                        auto subgroup = child->getExtensionByType<App::GeoFeatureGroupExtension>(true);
                        if(subgroup) groups.push_back(subgroup);
                    }
                }
            }
        }

        auto ext = vp->getExtensionByType<ViewProviderLinkObserver>(true);
        if(!ext) {
            ext = new ViewProviderLinkObserver();
            ext->initExtension(vp);
        }
        if(!ext->linkInfo) {
            // extension can be created automatically when restored from document,
            // with an empty linkInfo. So we need to check here.
            ext->linkInfo = Pointer(new LinkInfo(vp));
            ext->linkInfo->update();
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

    void remove(LinkHandle *l) {
        auto it = links.find(l);
        if(it!=links.end()) {
            if(l->linkInfo.get()!=this || l->getVisibility()) 
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
        std::multiset<LinkHandle*> linksTmp;
        linksTmp.swap(links);
        auto me = LinkInfoPtr(this);
        for(auto link : linksTmp)
            link->unlink(me);
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
            if((index<0 && i==LinkHandle::SnapshotChild) || !count)
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

    SoSeparator *getSnapshot(int type, bool update=false) {
        if(type<0 || type>=LinkHandle::SnapshotMax)
            return 0;

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
        pcModeSwitch->whichChild = -1;
        pcModeSwitch->removeAllChildren();

        auto childRoot = pcLinked->getChildRoot();

        for(int i=0,count=root->getNumChildren();i<count;++i) {
            SoNode *node = root->getChild(i);
            if(type==LinkHandle::SnapshotTransform && 
               node->isOfType(SoTransform::getClassTypeId()))
                continue;
            if(!node->isOfType(SoSwitch::getClassTypeId())) {
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
            for(int i=0,count=pcLinkedSwitch->getNumChildren();i<count;++i) {
                auto child = pcLinkedSwitch->getChild(i);
                if(pcChildGroup && child==childRoot)
                    pcModeSwitch->addChild(pcChildGroup);
                else
                    pcModeSwitch->addChild(child);
            }
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
                SoNode *node = info->getSnapshot(LinkHandle::SnapshotChild);
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
                getSnapshot(i,true);
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

        if(!pcChildGroup || !subname || *subname==0)
            return pcLinked->getElementView(subname,psubname);

        for(auto &v : nodeMap) {
            auto *vp = v.second->getElementView(true,subname,psubname);
            if(vp) return vp;
        }
        return 0;
    }

    bool getElementPicked(bool addname, int type, 
            const SoPickedPoint *pp, std::stringstream &str) const 
    {
        if(!pp || !isLinked())
            return false;

        if(addname) 
            str << getLinkedName() <<'.';
        
        auto pcSwitch = pcSwitches[type];
        if(pcChildGroup && pcSwitch && 
            pcSwitch->getChild(pcSwitch->whichChild.getValue())==pcChildGroup)
        {
            SoPath *path = pp->getPath();
            int index = path->findNode(pcChildGroup);
            if(index<=0) return false;
            auto it = nodeMap.find(path->getNode(index+1));
            if(it==nodeMap.end()) return false;
            return it->second->getElementPicked(true,LinkHandle::SnapshotChild,pp,str);
        }else{
            std::string subname;
            if(!pcLinked->getElementPicked(pp,subname))
                return false;
            str<<subname;
        }
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
            SoDetail *&det, SoFullPath *path) const 
    {
        if(!isLinked()) return false;

        if(checkname) {
            subname = checkSubname(pcLinked->getObject(),subname);
            if(!subname) return false;
        }

        if(path) {
            appendPath(path,pcSnapshots[type]);
            appendPath(path, pcSwitches[type]);
        }
        if(*subname == 0) return true;

        auto pcSwitch = pcSwitches[type];
        if(!pcChildGroup || !pcSwitch ||
            pcSwitch->getChild(pcSwitch->whichChild.getValue())!=pcChildGroup)
        {
            det = pcLinked->getDetailPath(subname,path,false);
            return true;
        }
        if(path){
            appendPath(path,pcChildGroup);
            if(pcLinked->getChildRoot())
                type = LinkHandle::SnapshotChild;
            else
                type = LinkHandle::SnapshotVisible;
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
        auto me = LinkInfoPtr(this);
        for(auto link : links) 
            link->onLinkedIconChange(me);
    }

    QIcon getIcon(QPixmap px) {
        static int iconSize = -1;
        if(iconSize < 0) 
            iconSize = QApplication::style()->standardPixmap(QStyle::SP_DirClosedIcon).width();

        if(px.isNull()) 
            return pcLinked->getIcon();
        QIcon &iconLink = iconMap[px.cacheKey()];
        if(iconLink.isNull()) {
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

EXTENSION_TYPESYSTEM_SOURCE(Gui::ViewProviderLinkObserver,Gui::ViewProviderExtension);

ViewProviderLinkObserver::ViewProviderLinkObserver() {
    // TODO: any better way to get deleted automatically?
    m_isPythonExtension = true;
    initExtensionType(ViewProviderLinkObserver::getExtensionClassTypeId());
}

void ViewProviderLinkObserver::extensionOnDeleting() {
    if(linkInfo) linkInfo->onDelete();
}

void ViewProviderLinkObserver::extensionShow() {
    if(linkInfo) linkInfo->updateSwitch("Visibility");
}

void ViewProviderLinkObserver::extensionHide() {
    if(linkInfo) linkInfo->updateSwitch("Visibility");
}

void ViewProviderLinkObserver::extensionOnChanged(const App::Property *prop) {
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

void ViewProviderLinkObserver::extensionUpdateData(const App::Property *prop) {
    if(linkInfo) linkInfo->update(prop);
}

void ViewProviderLinkObserver::extensionFinishRestoring() {
    if(linkInfo) {
        FC_TRACE("linked finish restoing");
        linkInfo->update();
    }
}

void ViewProviderLinkObserver::extensionGetLinks(std::vector<ViewProviderDocumentObject*> &links) const {
    if(!linkInfo) return;
    std::set<ViewProviderDocumentObject*> _links;
    for(auto link : linkInfo->links) {
        if(link->getOwner())
            _links.insert(link->getOwner());
    }
    links.reserve(links.size()+_links.size());
    for(auto vp : _links)
        links.push_back(vp);
}

void ViewProviderLinkObserver::extensionGetNodeNames(
        Document *doc, QMap<SoNode*, QString> &nodeNames) const 
{
    if(linkInfo) linkInfo->getNodeNames(doc,nodeNames);
}

///////////////////////////////////////////////////////////////////////////////////

TYPESYSTEM_SOURCE(Gui::LinkHandle,Base::BaseClass);

LinkHandle::LinkHandle()
    :owner(0),pcLinkRoot(0),pcMaterial(0),nodeType(SnapshotTransform),visible(false)
{
    pcLinkRoot = new SoFCSelectionRoot;
    pcLinkRoot->ref();
}

LinkHandle::~LinkHandle() {
    unlink(LinkInfoPtr());
    pcLinkRoot->unref();
    if(pcMaterial) 
        pcMaterial->unref();
}

void LinkHandle::setMaterial(const App::Material *material) {
    if(material) {
        if(!pcMaterial) {
            pcMaterial = new SoMaterial;
            pcMaterial->ref();
            pcMaterial->setOverride(true);
            pcLinkRoot->insertChild(pcMaterial,0);
        }
        const App::Material &Mat = *material;
        pcMaterial->ambientColor.setValue(Mat.ambientColor.r,Mat.ambientColor.g,Mat.ambientColor.b);
        pcMaterial->specularColor.setValue(Mat.specularColor.r,Mat.specularColor.g,Mat.specularColor.b);
        pcMaterial->emissiveColor.setValue(Mat.emissiveColor.r,Mat.emissiveColor.g,Mat.emissiveColor.b);
        pcMaterial->shininess.setValue(Mat.shininess);
        pcMaterial->diffuseColor.setValue(Mat.diffuseColor.r,Mat.diffuseColor.g,Mat.diffuseColor.b);
        pcMaterial->transparency.setValue(Mat.transparency);
    }else if(pcMaterial) {
        pcLinkRoot->removeChild(pcMaterial);
        pcMaterial->unref();
        pcMaterial = 0;
    }
}

void LinkHandle::setLink(App::DocumentObject *obj, bool reorder,
        const std::vector<std::string> &subs) 
{
    if(!isLinked() || linkInfo->pcLinked->getObject()!=obj) {
        unlink(LinkInfoPtr());
        onLinkedIconChange(LinkInfoPtr());

        linkInfo = LinkInfo::get(obj,this);
        if(!linkInfo) return;

        linkInfo->links.insert(this);
        if(getVisibility())
            linkInfo->setVisible(true);

        if(reorder && owner)
            owner->getDocument()->reorderViewProviders(owner,linkInfo->pcLinked);
    }
    for(const auto &sub : subs) 
        subInfo.insert(std::make_pair(sub,SubInfo(*this)));
    onLinkUpdate();
}

void LinkHandle::setTransform(SoTransform *pcTransform, const Base::Matrix4D &mat) {

    // extract scale factor from colum vector length
    double sx = Base::Vector3d(mat[0][0],mat[1][0],mat[2][0]).Sqr();
    double sy = Base::Vector3d(mat[0][1],mat[1][1],mat[2][1]).Sqr();
    double sz = Base::Vector3d(mat[0][2],mat[1][2],mat[2][2]).Sqr();
    bool bx,by,bz;
    if((bx=fabs(sx-1.0)>=1e-10))
        sx = sqrt(sx);
    else
        sx = 1.0;
    if((by=fabs(sy-1.0)>=1e-10))
        sy = sqrt(sy);
    else
        sy = 1.0;
    if((bz=fabs(sz-1.0)>=1e-10))
        sz = sqrt(sz);
    else
        sz = 1.0;
    // TODO: how to deal with negative scale?
    pcTransform->scaleFactor.setValue(sx,sy,sz);

    Base::Matrix4D matRotate;
    if(bx) {
        matRotate[0][0] = mat[0][0]/sx;
        matRotate[1][0] = mat[1][0]/sx;
        matRotate[2][0] = mat[2][0]/sx;
    }else{
        matRotate[0][0] = mat[0][0];
        matRotate[1][0] = mat[1][0];
        matRotate[2][0] = mat[2][0];
    }
    if(by) {
        matRotate[0][1] = mat[0][1]/sy;
        matRotate[1][1] = mat[1][1]/sy;
        matRotate[2][1] = mat[2][1]/sy;
    }else{
        matRotate[0][1] = mat[0][1];
        matRotate[1][1] = mat[1][1];
        matRotate[2][1] = mat[2][1];
    }
    if(bz) {
        matRotate[0][2] = mat[0][2]/sz;
        matRotate[1][2] = mat[1][2]/sz;
        matRotate[2][2] = mat[2][2]/sz;
    }else{
        matRotate[0][2] = mat[0][2];
        matRotate[1][2] = mat[1][2];
        matRotate[2][2] = mat[2][2];
    }

    Base::Rotation rot(matRotate);
    pcTransform->rotation.setValue(rot[0],rot[1],rot[2],rot[3]);
    pcTransform->translation.setValue(mat[0][3],mat[1][3],mat[2][3]);
    pcTransform->center.setValue(0.0f,0.0f,0.0f);
}

void LinkHandle::setNodeType(SnapshotType type) {
    if(nodeType==type) return;
    if(type>=SnapshotMax || 
       (type<0 && type!=SnapshotContainer && type!=SnapshotContainerTransform))
        throw Base::ValueError("Link: invalid node type");
    nodeType = type;
    onLinkUpdate();
}

void LinkHandle::onLinkedIconChange(LinkInfoPtr link) {
    if(owner && link==linkInfo) owner->signalChangeIcon();
}

LinkHandle::SubInfo::SubInfo(LinkHandle &handle)
    :handle(handle),pcNode(0),pcTransform(0)
{}

LinkHandle::SubInfo::~SubInfo() {
    unlink(true);
}

void LinkHandle::SubInfo::unlink(bool reset) {
    if(link) {
        link->remove(&handle);
        link.reset();
    }
    if(reset) {
        if(pcNode){
            auto root = handle.getLinkRoot();
            if(root) {
                int idx = root->findChild(pcNode);
                if(idx>=0)
                    root->removeChild(idx);
            }
            pcNode->unref();
            pcNode = 0;
        }
        if(pcTransform) {
            pcTransform->unref();
            pcTransform = 0;
        }
    }else if(pcNode) {
        pcNode->removeAllChildren();
        pcNode->addChild(pcTransform);
    }
}

bool LinkHandle::SubInfo::isLinked() const {
    return link && link->isLinked();
}

void LinkHandle::onLinkUpdate() {
    if(!isLinked())
        return;

    if(owner && owner->isRestoring()) {
        FC_TRACE("restoring '" << owner->getObject()->getNameInDocument() << "'");
        return;
    }

    // TODO: is it a good idea to clear any selection here?
    static_cast<SoFCSelectionRoot*>(pcLinkRoot)->resetContext();

    if(nodeType >= 0) {
        pcLinkRoot->removeAllChildren();
        if(pcMaterial) 
            pcLinkRoot->addChild(pcMaterial);
        auto node = linkInfo->getSnapshot(nodeType);
        if(node) 
            pcLinkRoot->addChild(node);
        return;
    }

    // rebuild link sub objects tree

    auto obj = linkInfo->pcLinked->getObject();
    for(auto it=subInfo.begin(),itNext=it;it!=subInfo.end();it=itNext) {
        ++itNext;
        Base::Matrix4D mat;
        const char *nextsub = 0;
        App::DocumentObject *sobj = obj->getSubObject(it->first.c_str(), 
                &nextsub, 0, &mat, nodeType==SnapshotContainer);
        if(!sobj) {
            it->second.unlink();
            continue;
        }

        // group all subelement of the same object together
        if(nextsub && *nextsub!=0) {
            std::string next;
            if(nextsub!=it->first.c_str())
                next = it->first.substr(0,nextsub-1-it->first.c_str());
            auto itEntry = subInfo.find(next);
            if(itEntry == subInfo.end()) {
                auto ret = subInfo.insert(std::make_pair(next,SubInfo(*this)));
                subInfo.erase(it);
                it = ret.first;
                it->second.elements.insert(nextsub);
            } else {
                itEntry->second.elements.insert(nextsub);
                subInfo.erase(it);
                //subInfo is a std::map, which means it is order by lexical string
                //order. So if the entry exists than it must have already been
                //visited, so just continue
                continue;
            }
        }

        auto &sub = it->second;
        if(!sub.isLinked() || sub.link->pcLinked->getObject()!=sobj) {
            sub.unlink();
            sub.link = LinkInfo::get(sobj,this);
            if(!sub.link) continue;
            sub.link->links.insert(this);
            sub.link->setVisible(true);
            if(!sub.pcNode) {
                sub.pcNode = new SoFCSelectionRoot;
                sub.pcNode->ref();
                sub.pcTransform = new SoTransform;
                sub.pcTransform->ref();
                sub.pcNode->addChild(sub.pcTransform);
                sub.pcNode->addChild(sub.link->getSnapshot(SnapshotTransform));
                pcLinkRoot->addChild(sub.pcNode);
            }
        }
        setTransform(sub.pcTransform,mat);
    }

    // now rebuild the subelement, using SoSelectionElementAction
    // marked as 'secondary'

    SoSelectionElementAction action(SoSelectionElementAction::None,true);
    action.apply(pcLinkRoot);

    SoFullPath *path = 0;
    for(auto &v : subInfo) {
        auto &sub = v.second;
        if(!sub.isLinked() || !sub.pcNode || sub.elements.empty()) 
            continue;
        if(!path) {
           path = static_cast<SoFullPath*>(new SoPath(10));
           path->ref();
           appendPath(path,pcLinkRoot);
        }
        path->truncate(1);
        appendPath(path,sub.pcNode);
        if(path->getLength()!=2) {
            if(FC_LOG_INSTANCE.isEnabled(FC_LOGLEVEL_LOG))
                FC_WARN("node path error");
            continue;
        }

        SoSelectionElementAction action(SoSelectionElementAction::Append,true);
        for(const auto &element : sub.elements) {
            path->truncate(2);
            SoDetail *det = 0;
            if(!sub.link->getDetail(false,SnapshotTransform,element.c_str(),det,path))
                continue;
            action.setElement(det);
            action.apply(path);
            delete det;
        }
    }
    if(path) path->unref();
}

void LinkHandle::setVisibility(bool visible) {
    if(this->visible != visible) {
        this->visible = visible;
        if(linkInfo) 
            linkInfo->setVisible(visible);
    }
}

bool LinkHandle::linkGetElementPicked(const SoPickedPoint *pp, std::string &subname) const {
    if(!isLinked()) return false;

    std::stringstream str;
    if(nodeType >= 0) {
        if(linkInfo->getElementPicked(false,nodeType,pp,str)) {
            subname = str.str();
            return true;
        }
        return false;
    }
    auto path = pp->getPath();
    auto idx = path->findNode(pcLinkRoot);
    if(idx<0 || idx+1>=path->getLength()) return false;
    auto node = path->getNode(idx+1);
    for(auto &v : subInfo) {
        auto &sub = v.second;
        if(node != sub.pcNode) continue;
        if(!sub.link->getElementPicked(false,SnapshotTransform,pp,str))
            return false;
        const std::string &element = str.str();
        if(sub.elements.size() && element.size() && 
            sub.elements.find(element)==sub.elements.end())
            return false;
        if(element.empty())
            subname = v.first;
        else if(v.first.size()) 
            subname = v.first + '.' + element;
        else
            subname = element;
        return true;
    }
    return false;
}

bool LinkHandle::isLinked() const {
    return linkInfo && linkInfo->isLinked();
}

ViewProviderDocumentObject *LinkHandle::linkGetLinkedView(bool recursive, int depth) const{
    static int s_limit;
    if(!s_limit) {
        ParameterGrp::handle hGrp = App::GetApplication().GetParameterGroupByPath(
                "User parameter:BaseApp/Preferences/Link");
        s_limit = hGrp->GetInt("Depth",100);
    }
    if(isLinked()) {
        if(!recursive) return linkInfo->pcLinked;
        if(depth >= s_limit) 
            throw Base::RuntimeError("Link: Recursion limit reached."
                   "Please check for cyclic reference, "
                   "or change setting at BaseApp/Preferences/Link/Depth.");
        return linkInfo->pcLinked->getLinkedView(true,++depth);
    }
    return 0;
}

bool LinkHandle::linkGetDetailPath(const char *subname, SoFullPath *path, SoDetail *&det) const {
    if(!isLinked()) return false;
    if(!subname || *subname==0) return true;
    auto len = path->getLength();
    appendPath(path,pcLinkRoot);
    if(nodeType >= 0) {
        if(linkInfo->getDetail(false,nodeType,subname,det,path))
            return true;
    }else {
        for(auto &v : subInfo) {
            auto &sub = v.second;
            if(!sub.isLinked() || !sub.pcNode || 
               !boost::algorithm::starts_with(subname,v.first)) 
                continue;
            const char *nextsub = subname+v.first.size();
            if(*nextsub == '.') 
                ++nextsub;
            appendPath(path,sub.pcNode);
            len = path->getLength();
            if(sub.link->getDetail(false,SnapshotTransform,nextsub,det,path))
                return true;
            break;
        }
    }
    path->truncate(len);
    return false;
}

void LinkHandle::unlink(LinkInfoPtr link) {
    if(!link || link==linkInfo) {
        if(linkInfo) {
            linkInfo->remove(this);
            linkInfo.reset();
        }
        if(pcLinkRoot) {
            pcLinkRoot->removeAllChildren();
            if(pcMaterial) 
                pcLinkRoot->addChild(pcMaterial);
        }
        subInfo.clear();
        return;
    }
    if(link) {
        for(auto &v : subInfo) {
            auto &sub = v.second;
            if(sub.link == link) {
                sub.unlink();
                break;
            }
        }
    }
}

ViewProviderDocumentObject *LinkHandle::linkGetElementView(
        const char *element, const char **subname) const 
{
    if(!isLinked() || !linkInfo->pcChildGroup || !element || *element==0) 
        return nullptr;
    return linkInfo->getElementView(false,element,subname);
}

QIcon LinkHandle::getLinkedIcon(QPixmap px) const {
    if(!isLinked()) return QIcon();
    return linkInfo->getIcon(px);
}

///////////////////////////////////////////////////////////////////////////////////

PROPERTY_SOURCE(Gui::ViewProviderLink, Gui::ViewProviderDocumentObject)

static Base::Type PropTypes[ViewProviderLink::PropMax];

ViewProviderLink::ViewProviderLink()
    :linkTransform(false),xlink(false),sublink(false)
{
    ParameterGrp::handle hGrp = App::GetApplication().GetParameterGroupByPath("User parameter:BaseApp/Preferences/View");
    unsigned long shcol = hGrp->GetUnsigned("DefaultShapeColor",3435973887UL); // light gray (204,204,204)
    float r,g,b;
    r = ((shcol >> 24) & 0xff) / 255.0; g = ((shcol >> 16) & 0xff) / 255.0; b = ((shcol >> 8) & 0xff) / 255.0;

    ADD_PROPERTY_TYPE(Selectable, (true), " Link", App::Prop_None, 0);

    ADD_PROPERTY_TYPE(LinkUseMaterial, (false), " Link", App::Prop_None, "Override linked object's material");

    ADD_PROPERTY_TYPE(LinkShapeColor, (r,g,b), " Link", App::Prop_None, 0);
    ADD_PROPERTY_TYPE(LinkTransparency, (0), " Link", App::Prop_None, 0);
    App::Material mat(App::Material::DEFAULT);
    ADD_PROPERTY_TYPE(LinkShapeMaterial, (mat), " Link", App::Prop_None, 0);

    sPixmap = "link";
    DisplayMode.setStatus(App::Property::Status::Hidden, true);

    if(PropTypes[0].isBad()) {
        PropTypes[PropPlacement] = App::PropertyPlacement::getClassTypeId();
        PropTypes[PropObject] = App::PropertyLink::getClassTypeId();
        PropTypes[PropSubs] = App::PropertyLinkSub::getClassTypeId();
        PropTypes[PropTransform] = App::PropertyBool::getClassTypeId();
        PropTypes[PropScale] = App::PropertyVector::getClassTypeId();
        PropTypes[PropRecomputed] = App::PropertyBool::getClassTypeId();
    }
    handle.setOwner(this);
}

ViewProviderLink::~ViewProviderLink()
{
}

void ViewProviderLink::attach(App::DocumentObject *pcObj) {
    inherited::attach(pcObj);

    addDisplayMaskMode(handle.getLinkRoot(),"Link");
    setDisplayMaskMode("Link");

    std::map<std::string,App::Property*> pmap;
    pcObj->getPropertyMap(pmap);

    auto it = pmap.end();
    const char *name = 0;

#define HAS_PROP(_type) \
    ((name=propNameFromIndex(Prop##_type)) && \
     (it=pmap.find(name))!=pmap.end() && \
     it->second->isDerivedFrom(PropTypes[Prop##_type]))

    bool has_obj = HAS_PROP(Object);
    sublink = HAS_PROP(Subs);
    if(!has_obj && !sublink) 
        throw Base::RuntimeError("Link: no link property");

    if(has_obj && sublink)
        throw Base::RuntimeError("Link: too many link property");

    if(sublink)
        sPixmap = "linksub";

    if(sublink)
        handle.setNodeType(linkTransform?LinkHandle::SnapshotContainer:
                LinkHandle::SnapshotContainerTransform);
    else
        handle.setNodeType(linkTransform?LinkHandle::SnapshotVisible:
                LinkHandle::SnapshotTransform);
}

QIcon ViewProviderLink::getIcon() const {
    auto icon = handle.getLinkedIcon(getOverlayPixmap());
    if(icon.isNull())
        return Gui::BitmapFactory().pixmap(sPixmap);
    return icon;
}

QPixmap ViewProviderLink::getOverlayPixmap() const {
    static QPixmap px[3];

    if(px[0].isNull()) {
        // right top pointing arrow for normal link
        const char * const feature_link_xpm[]={
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
        px[0] = QPixmap(feature_link_xpm);

        // left top pointing arrow for xlink
        const char * const feature_xlink_xpm[]={
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
        px[1] = QPixmap(feature_xlink_xpm);

        // double arrow for link subs
        const char * const feature_linksub_xpm[]={
            "8 8 3 1",
            ". c None",
            "# c #000000",
            "a c #ffffff",
            "########",
            "##aaaaa#",
            "######a#",
            "##aaa#a#",
            "###aa#a#",
            "##a#a#a#",
            "#a######",
            "########"};
        px[2] = QPixmap(feature_linksub_xpm);
    }
    if(sublink) return px[2];
    return px[xlink?1:0];
}

const char *ViewProviderLink::propNameFromIndex(PropIndex index) const {
    static const char *s_names[] = {
        "LinkPlacement",
        "LinkedObject",
        "LinkedSubs",
        "LinkTransform",
        "LinkScale",
        "LinkRecomputed",
        0,
    };
    if(index<0 || index>PropMax)
        index = PropMax;
    return s_names[index];
}

int ViewProviderLink::propIndexFromName(const char *name) const {
    static std::map<Base::Type, std::map<std::string,int> > propMap;
    if(!name || *name==0) return PropMax;
    auto &props = propMap[getTypeId()];
    if(props.empty()) {
        for(int i=0;i<PropMax;++i) {
            auto n = propNameFromIndex(static_cast<PropIndex>(i));
            if(n) props[n] = i;
        }
        if(props.empty()) 
            throw Base::RuntimeError("Link: no properties configuration");
    }
    auto it = props.find(name);
    if(it == props.end()) return PropMax;
    return it->second;
}

void ViewProviderLink::setConfig(LinkConfig bit, bool on) {
    if(bit != HandleAll)
        config.set((size_t)bit,on);
    else if(on)
        config.set();
    else
        config.reset();
}

bool ViewProviderLink::onDelete(const std::vector<std::string> &subs) {
    if(inherited::onDelete(subs)) {
        handle.unlink(LinkInfoPtr());
        return true;
    }
    return false;
}

void ViewProviderLink::onChanged(const App::Property* prop) {
    if(prop == &Visibility) 
        handle.setVisibility(Visibility.getValue());
    else if (prop == &LinkUseMaterial) {
        if(LinkUseMaterial.getValue())
            handle.setMaterial(&LinkShapeMaterial.getValue());
        else
            handle.setMaterial(0);
    }else if (prop == &LinkShapeColor) {
        const App::Color& c = LinkShapeColor.getValue();
        if (c != LinkShapeMaterial.getValue().diffuseColor) 
            LinkShapeMaterial.setDiffuseColor(c);
    }
    else if (prop == &LinkTransparency) {
        const App::Material& Mat = LinkShapeMaterial.getValue();
        long value = (long)(100*Mat.transparency);
        if (value != LinkTransparency.getValue()) {
            float trans = LinkTransparency.getValue()/100.0f;
            LinkShapeMaterial.setTransparency(trans);
        }
    }
    else if (prop == &LinkShapeMaterial) {
        const App::Material& Mat = LinkShapeMaterial.getValue();
        if(LinkUseMaterial.getValue())
            handle.setMaterial(&LinkShapeMaterial.getValue());
        long value = (long)(100*Mat.transparency);
        if (value != LinkTransparency.getValue())
            LinkTransparency.setValue(value);
        const App::Color& color = Mat.diffuseColor;
        if (color != LinkShapeColor.getValue())
            LinkShapeColor.setValue(Mat.diffuseColor);

    }
    inherited::onChanged(prop);
}

void ViewProviderLink::updateData(const App::Property *prop) {
    int index = propIndexFromName(prop->getName());
    if(index == PropMax || !prop->isDerivedFrom(PropTypes[index])) return;
    switch(index) {
    case PropRecomputed:
        if(sublink)
            handle.onLinkUpdate();
        break;
    case PropScale: {
        const Base::Vector3d &v = 
            static_cast<const App::PropertyVector*>(prop)->getValue();
        pcTransform->scaleFactor.setValue(v.x,v.y,v.z);
        break;
    } case PropPlacement: {
        auto v = pcTransform->scaleFactor.getValue();
        ViewProviderGeometryObject::updateTransform(
            static_cast<const App::PropertyPlacement*>(prop)->getValue(), pcTransform);
        pcTransform->scaleFactor.setValue(v);
        break;
    } case PropObject: {
        xlink = prop->isDerivedFrom(App::PropertyXLink::getClassTypeId()) &&
                static_cast<const App::PropertyXLink*>(prop)->getDocumentPath();
        handle.setLink(static_cast<const App::PropertyLink*>(prop)->getValue());
        break;
    } case PropSubs: {
        auto propLink = static_cast<const App::PropertyLinkSub*>(prop);
        handle.setLink(propLink->getValue(),true,propLink->getSubValues());
        break;
    } case PropTransform: {
        if(linkTransform != static_cast<const App::PropertyBool*>(prop)->getValue()) {
            linkTransform = !linkTransform;
            if(sublink)
                handle.setNodeType(linkTransform?LinkHandle::SnapshotContainer:
                        LinkHandle::SnapshotContainerTransform);
            else
                handle.setNodeType(linkTransform?LinkHandle::SnapshotVisible:
                        LinkHandle::SnapshotTransform);
        }
        break;
    } default:
        break;
    }
    inherited::updateData(prop);
}

void ViewProviderLink::finishRestoring() {
    FC_TRACE("finish restoring");
    handle.onLinkUpdate();
}

std::vector<App::DocumentObject*> ViewProviderLink::claimChildren(void) const {
    if(!sublink) {
        auto linked = getLinkedView(true);
        if(linked!=this)
            return linked->claimChildren();
    }
    return std::vector<App::DocumentObject*>();
}

bool ViewProviderLink::canDragObject(App::DocumentObject* obj) const {
    auto linked = getLinkedView(true);
    if(linked!=this)
        return linked->canDragObject(obj);
    return false;
}

bool ViewProviderLink::canDragObjects() const {
    auto linked = getLinkedView(true);
    if(linked!=this)
        return linked->canDragObjects();
    return false;
}

void ViewProviderLink::dragObject(App::DocumentObject* obj) {
    auto linked = getLinkedView(true);
    if(linked!=this)
        linked->dragObject(obj);
}

bool ViewProviderLink::canDropObject(App::DocumentObject* obj) const {
    if(sublink || !handle.isLinked()) return true;
    auto linked = getLinkedView(true);
    if(linked!=this)
        return linked->canDropObject(obj);
    return true;
}

bool ViewProviderLink::canDropObjects() const {
    if(sublink || !handle.isLinked()) return true;
    auto linked = getLinkedView(true);
    if(linked!=this)
        return linked->canDropObjects();
    return true;
}

void ViewProviderLink::dropObjectEx(App::DocumentObject* obj, 
        App::DocumentObject *owner, const char *subname) 
{
    if(sublink && owner) {
        std::map<std::string,App::Property*> pmap;
        getObject()->getPropertyMap(pmap);
        auto it = pmap.end();
        const char *name = 0;
        if(!HAS_PROP(Subs))
            throw Base::RuntimeError("Link: no link property");
        std::vector<std::string> subs;
        if(subname && *subname) subs.push_back(subname);
        static_cast<App::PropertyLinkSub*>(it->second)->setValue(owner,subs);
    }

    auto linked = getLinkedView(true);
    if(linked!=this) {
        linked->dropObject(obj);
        return;
    }
    std::map<std::string,App::Property*> pmap;
    getObject()->getPropertyMap(pmap);
    auto it = pmap.end();
    const char *name = 0;
    if(!HAS_PROP(Object))
        throw Base::RuntimeError("Link: no link property");
    static_cast<App::PropertyLink*>(it->second)->setValue(obj);
}

bool ViewProviderLink::canDragAndDropObject(App::DocumentObject* obj) const {
    if(!sublink || !handle.isLinked()) return false;
    auto linked = getLinkedView(true);
    if(linked!=this) 
        return linked->canDragAndDropObject(obj);
    return false;
}

bool ViewProviderLink::getElementPicked(const SoPickedPoint *pp, std::string &subname) const {
    return handle.linkGetElementPicked(pp,subname);
}

ViewProviderDocumentObject *ViewProviderLink::getLinkedView(bool recursive, int depth) const{
    auto ret = handle.linkGetLinkedView(recursive,depth);
    if(ret) return ret;
    return const_cast<ViewProviderLink*>(this);
}

SoDetail* ViewProviderLink::getDetailPath(
        const char *subname, SoFullPath *pPath, bool append) const 
{
    auto len = pPath->getLength();
    if(append) {
        appendPath(pPath,pcRoot);
        appendPath(pPath,pcModeSwitch);
    }
    SoDetail *det = 0;
    if(handle.linkGetDetailPath(subname,pPath,det))
        return det;
    pPath->truncate(len);
    return 0;
}

ViewProviderDocumentObject *ViewProviderLink::getElementView(
        const char *element, const char **subname) const
{
    return handle.linkGetElementView(element,subname);
}
