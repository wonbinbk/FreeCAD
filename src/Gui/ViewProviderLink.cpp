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
#endif
#include <QFileInfo>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/bind.hpp>
#include <Base/Console.h>
#include "Application.h"
#include "Document.h"
#include "Selection.h"
#include "MainWindow.h"
#include "ViewProviderLink.h"
#include "ViewProviderBuilder.h"
#include "SoFCSelection.h"
#include <SoFCUnifiedSelection.h>


using namespace Gui;

static std::map<std::string,ViewProviderLink::DocInfoPtr> DocInfoMap;

class ViewProviderLink::DocInfo : 
    public std::enable_shared_from_this<ViewProviderLink::DocInfo> 
{
public:
    boost::signals::connection connFinishRestoreDocument;
    boost::signals::connection connDeleteDocument;
    boost::signals::connection connSaveDocument;

    std::string filePath;
    App::Document *pcDoc;
    std::set<ViewProviderLink*> links;

    static DocInfoPtr get(const std::string &path, ViewProviderLink *l) {
        DocInfoPtr &info = DocInfoMap[path];
        if(!info) info = std::make_shared<DocInfo>(path);
        info->links.insert(l);
        return info;
    }

    static QString getFullPath(const char *p) {
        return QFileInfo(QString::fromUtf8(p)).canonicalFilePath();
    }

    QString getFullPath() const {
        return getFullPath(filePath.c_str());
    }

    DocInfo(const std::string &path)
        :filePath(path),pcDoc(0) 
    {
        App::Application &app = App::GetApplication();
        connFinishRestoreDocument = app.signalFinishRestoreDocument.connect(
            boost::bind(&DocInfo::slotFinishRestoreDocument,this,_1));
        connDeleteDocument = app.signalDeleteDocument.connect(
            boost::bind(&DocInfo::slotDeleteDocument,this,_1));
        connSaveDocument = app.signalSaveDocument.connect(
            boost::bind(&DocInfo::slotSaveDocument,this,_1));

        QString fullpath(getFullPath());
        if(!fullpath.isEmpty()) {
            for(App::Document *doc : App::GetApplication().getDocuments()) {
                if(getFullPath(doc->FileName.getValue()) == fullpath)
                    pcDoc = doc;
            }
        }
    }

    ~DocInfo() {
        connFinishRestoreDocument.disconnect();
        connDeleteDocument.disconnect();
        connSaveDocument.disconnect();
    }

    void remove(ViewProviderLink *l) {
        auto me = shared_from_this();
        auto it = links.find(l);
        assert(it!=links.end());
        links.erase(it);
        if(links.empty()) {
            auto it = DocInfoMap.find(filePath);
            assert(it!=DocInfoMap.end());
            DocInfoMap.erase(it);
        }
    }

    void slotFinishRestoreDocument(const App::Document &doc) {
        if(pcDoc) return;
        QString fullpath(getFullPath());
        if(!fullpath.isEmpty() && getFullPath(doc.FileName.getValue())==fullpath){
            Base::Console().Log("attached document %s\n",doc.FileName.getValue());
            pcDoc = const_cast<App::Document*>(&doc);
            for(ViewProviderLink *link:links) 
                link->findLink(true);
        }
    }

    void slotSaveDocument(const App::Document &doc) {
        if(!pcDoc) {
            slotFinishRestoreDocument(doc);
            return;
        }
        if(&doc!=pcDoc) return;
        QString fullpath(getFullPath());
        if(getFullPath(doc.FileName.getValue())!=fullpath) {
            pcDoc = 0;
            auto me = shared_from_this();
            for(auto link : links) 
                link->unlink();
        }
    }

    void slotDeleteDocument(const App::Document &doc) {
        if(pcDoc!=&doc) return;
        pcDoc = 0;
        auto me = shared_from_this();
        for(auto link:links) 
            link->unlink();
    }
};

static std::map<ViewProvider*,ViewProviderLink::LinkInfoPtr> LinkInfoMap;

class ViewProviderLink::LinkInfo :
    public std::enable_shared_from_this<ViewProviderLink::LinkInfo>
{
public:

    boost::signals::connection connChangedObject;
    boost::signals::connection connDeletedObject;

    ViewProviderDocumentObject *vpd;
    std::set<ViewProviderLink*> links;
    std::string name;

    // for group type view providers
    SoGroup *childGroup;
    std::map<SoNode *, App::DocumentObject *> nodeMap;
    std::map<std::string, App::DocumentObject *> nameMap;

    static LinkInfoPtr get(ViewProviderDocumentObject *vp, ViewProviderLink *l) {
        LinkInfoPtr &info = LinkInfoMap[vp];
        if(!info) info = std::make_shared<LinkInfo>(vp);
        info->links.insert(l);
        return info;
    }

    LinkInfo(ViewProviderDocumentObject *vp)
        :vpd(vp),childGroup(0) 
    {
        connChangedObject = vpd->getDocument()->signalChangedObject.connect(
            boost::bind(&LinkInfo::slotChangedObject, this, _1));
        connDeletedObject = vpd->getDocument()->signalDeletedObject.connect(
            boost::bind(&LinkInfo::slotDeletedObject, this, _1));
        update();
    }

    ~LinkInfo() {
        if(childGroup) childGroup->unref();
        connChangedObject.disconnect();
        connDeletedObject.disconnect();
    }

    void remove(ViewProviderLink *l) {
        auto me = shared_from_this();
        auto it = links.find(l);
        assert(it!=links.end());
        links.erase(it);
        if(links.empty()) {
            auto it = LinkInfoMap.find(vpd);
            assert(it!=LinkInfoMap.end());
            LinkInfoMap.erase(it);
        }
    }

    void slotChangedObject(const ViewProviderDocumentObject &v) {
        if(&v != vpd) return;
        update();
        for(ViewProviderLink *link : links)
            link->updateVisual(vpd);
    }

    void slotDeletedObject(const ViewProviderDocumentObject &v) {
        if(&v != vpd) return;
        auto me = shared_from_this();
        for(ViewProviderLink *link : links)
            link->unlink();
        if(childGroup) {
            childGroup->unref();
            childGroup = 0;
        }
    }

    void update() {
        if(!vpd->getChildRoot()) return;

        if(!childGroup) {
            childGroup = new SoGroup;
            childGroup->ref();
        }
        const auto &children = vpd->claimChildren3D();
        if(children.size() == nodeMap.size()) return;

        nameMap.clear();
        childGroup->removeAllChildren();

        //Replace child view provider root node to hijack selection
        for(auto child : children) {
            if(!child->getNameInDocument()) continue;
            ViewProvider *cvp = vpd->getDocument()->getViewProvider(child);
            if(!cvp) continue;
            SoSeparator *myChildRoot = new SoSeparator;
            SoSeparator* childRoot =  cvp->getRoot();
            for(int i=0,count=childRoot->getNumChildren();i<count;++i) 
                myChildRoot->addChild(childRoot->getChild(i));
            childGroup->addChild(myChildRoot);
            nodeMap[myChildRoot] = child;
            nameMap[child->getNameInDocument()] = child;
        }
    }
};

PROPERTY_SOURCE(Gui::ViewProviderLink, Gui::ViewProviderDocumentObject)

ViewProviderLink::ViewProviderLink():pcLinked(0)
{
    ADD_PROPERTY(ScaleFactor,(Base::Vector3d(1.0,1.0,1.0)));
    ADD_PROPERTY(Synchronized,((long)0));

    DisplayMode.setStatus(App::Property::Status::Hidden, true);

    pcRoot->removeAllChildren();
    pcRoot->unref();
    pcRoot = new SoFCSelectionRoot();
    pcRoot->ref();
}

ViewProviderLink::~ViewProviderLink()
{
    unlink(true);
}

void ViewProviderLink::attach(App::DocumentObject *pcObj)
{
    inherited::attach(pcObj);
}

bool ViewProviderLink::useNewSelectionModel(void) const {
    return pcLinked && (linkInfo->childGroup||pcLinked->useNewSelectionModel());
}

std::string ViewProviderLink::getElementPicked(const SoPickedPoint *pp) const {
    std::string ret;
    const char *name;
    App::DocumentObject *pcLinkedObj;
    if(!pp || !pcLinked || 
       !(pcLinkedObj=pcLinked->getObject()) || 
       !(name=pcLinkedObj->getNameInDocument()))
        return ret;

    std::stringstream str;
    if(pcLinked->getDocument()!=getDocument())
        str << '*' << pcLinked->getDocument()->getDocument()->getName() << '.';
    str << name <<'.';
    
    if(linkInfo->childGroup) {
        SoPath *path = pp->getPath();
        int index = path->findNode(linkInfo->childGroup);
        if(index<=0) return ret;
        auto it = linkInfo->nodeMap.find(path->getNode(index+1));
        if(it==linkInfo->nodeMap.end() || !it->second->getNameInDocument()) 
            return ret;
        ViewProvider *vp = pcLinked->getDocument()->getViewProvider(it->second);
        if(!vp) return ret;
        str<<it->second->getNameInDocument()<<'.'<<vp->getElementPicked(pp);
    }else
        str<<pcLinked->getElementPicked(pp);
    ret = str.str();
    return ret;
}

SoDetail* ViewProviderLink::getDetail(const char* subelement) const {
    const char *name;
    App::DocumentObject *pcLinkedObj;
    if(subelement && pcLinked && 
       (pcLinkedObj=pcLinked->getObject()) && 
       (name=pcLinkedObj->getNameInDocument())) 
    {
        std::stringstream str;
        if(subelement[0] == '*')
            str << pcLinked->getDocument()->getDocument()->getName() << '.';
        str << pcLinkedObj->getNameInDocument() << '.';
        const std::string &n = str.str();
        if(boost::algorithm::starts_with(subelement,n)) {
            subelement += n.length();
            if(!linkInfo)
                return pcLinked->getDetail(subelement);
            int i;
            for(i=0;subelement[i]&&subelement[i]!='.';++i);
            if(subelement[i]=='.') {
                auto it = linkInfo->nameMap.find(std::string(subelement,i));
                if(it!=linkInfo->nameMap.end()){
                    ViewProvider *vp = pcLinked->getDocument()->getViewProvider(it->second);
                    if(vp) return vp->getDetail(subelement+i+1);
                }
            }
        }
    }
    return inherited::getDetail(subelement);
}

void ViewProviderLink::unlink(bool unlinkDoc) {
    if(pcLinked) {
        pcRoot->removeAllChildren();
        pcModeSwitch->removeAllChildren();
        pcLinked = 0;
        if(linkInfo) {
            linkInfo->remove(this);
            linkInfo.reset();
        }
    }
    if(unlinkDoc && docInfo) {
        docInfo->remove(this);
        docInfo.reset();
    }
}

bool ViewProviderLink::onDelete(const std::vector<std::string> &svec) {
    unlink(true);
    return inherited::onDelete(svec);
}

void ViewProviderLink::onChanged(const App::Property* prop) {
    if(prop == &ScaleFactor) {
        const Base::Vector3d &v = ScaleFactor.getValue();
        pcTransform->scaleFactor.setValue(v.x,v.y,v.z);
    }else if(prop == &Synchronized) {
        SoFCSelectionRoot *pcSelRoot = static_cast<SoFCSelectionRoot*>(pcRoot);
        pcSelRoot->selectionSync = Synchronized.getValue();
        if(Synchronized.getValue())
            pcSelRoot->resetContext();
    }else
        inherited::onChanged(prop);
}

void ViewProviderLink::updateVisual(ViewProviderDocumentObject *vpLinked) {
    assert(vpLinked);
    if(pcLinked!=vpLinked) {
        unlink();
        pcLinked = vpLinked;
        linkInfo = LinkInfo::get(pcLinked,this);
    }
    pcRoot->removeAllChildren();
    pcModeSwitch->removeAllChildren();
    App::DocumentObject *pcLinkedObj = pcLinked->getObject();
    SoSeparator *root = pcLinked->getRoot();
    if(!root || !pcLinkedObj->getNameInDocument())
        return;
    char name[1024];
    size_t offset;
    if(getDocument()==pcLinked->getDocument())
        offset = snprintf(name,sizeof(name),"%s",pcLinkedObj->getNameInDocument());
    else
        offset = snprintf(name,sizeof(name),"*%s.%s", pcLinkedObj->getDocument()->getName(),
                            pcLinkedObj->getNameInDocument());
    if(offset>=sizeof(name)){
        Base::Console().Warning("ViewProviderLink: name overflow %s.%s\n",
                pcLinkedObj->getDocument()->getName(),pcLinkedObj->getNameInDocument());
    }else{
        char *subname = name+offset;
        for(int i=0,count=root->getNumChildren();i<count;++i) {
            SoNode *node = root->getChild(i);
            if(node == pcLinked->pcTransform) {
                pcRoot->addChild(pcTransform);
                continue;
            }
            if(node == pcLinked->pcModeSwitch) {
                pcRoot->addChild(pcModeSwitch);
                continue;
            }
            pcRoot->addChild(node);
        }
        SoNode *childGroup = linkInfo->childGroup;
        for(int i=0,count=pcLinked->pcModeSwitch->getNumChildren();i<count;++i){
            SoNode *node = pcLinked->pcModeSwitch->getChild(i);
            if(node->getTypeId() == SoFCSelection::getClassTypeId()) {
                SoFCSelection *sel = static_cast<SoFCSelection*>(node);
                SoFCSelection *mysel = new SoFCSelection;
                mysel->objectName = pcObject->getNameInDocument();
                mysel->documentName = pcObject->getDocument()->getName();
                const char *sub = sel->subElementName.getValue().getString();
                if(sub)
                    snprintf(subname,sizeof(name)-offset,".%s",sub);
                else
                    *subname = 0;
                mysel->subElementName = subname;
                mysel->highlightMode = sel->highlightMode;
                mysel->colorHighlight = sel->colorHighlight;
                mysel->selectionMode = sel->selectionMode;
                mysel->colorSelection = sel->colorSelection;
                for(int j=0,c=sel->getNumChildren();j<c;++j)
                    mysel->addChild(sel->getChild(j));
                node = mysel;
            }else if(childGroup && node==pcLinked->getChildRoot())
                node = childGroup;
            pcModeSwitch->addChild(node);
        }
        _iActualMode = pcLinked->_iActualMode;
        if(Visibility.getValue()) 
            setModeSwitch();
    }
}

void ViewProviderLink::findLink(bool touch) {
    if(!docInfo) return;
    if(linkedObjName.empty() || !docInfo->pcDoc) return;
    App::DocumentObject *pcLinkedObj = docInfo->pcDoc->getObject(linkedObjName.c_str());
    if(!pcLinkedObj) return;
    Document *pDoc = Application::Instance->getDocument(pcLinkedObj->getDocument());
    ViewProvider *vp;
    if(pDoc && (vp=pDoc->getViewProvider(pcLinkedObj)) &&
        vp->isDerivedFrom(ViewProviderDocumentObject::getClassTypeId())) {
        updateVisual(static_cast<ViewProviderDocumentObject*>(vp));
        if(touch) pcObject->touch();
    }
}

void ViewProviderLink::updateData(const App::Property *prop) {

    inherited::updateData(prop);

    if(prop->isDerivedFrom(App::PropertyLink::getClassTypeId())){
        if(strcmp(prop->getName(), "Source") == 0)
        {
            App::DocumentObject *pcLinkedObj = static_cast<const App::PropertyLink*>(prop)->getValue();
            if(pcLinkedObj && pcLinkedObj->getNameInDocument()) {
                Document *pDoc = Application::Instance->getDocument(pcLinkedObj->getDocument());
                ViewProvider *vp;
                if(pDoc && (vp=pDoc->getViewProvider(pcLinkedObj)) &&
                vp->isDerivedFrom(ViewProviderDocumentObject::getClassTypeId())) {
                    updateVisual(static_cast<ViewProviderDocumentObject*>(vp));
                    return;
                }
            }
            unlink();
        }
    } else if(prop->isDerivedFrom(App::PropertyString::getClassTypeId())) {
        const std::string &value = static_cast<const App::PropertyString*>(prop)->getStrValue();
        if(strcmp(prop->getName(), "SourceFile") == 0) {
            if(!docInfo || docInfo->filePath!=value) {
                if(docInfo) {
                    docInfo->remove(this);
                    docInfo.reset();
                }
                unlink();
            }
            if(value.length()) {
                docInfo = DocInfo::get(value,this);
                findLink();
            }
        }else if(strcmp(prop->getName(),"ObjectName")==0) {
            if(linkedObjName == value) return;
            unlink();
            linkedObjName = value;
            findLink();
        }
    }else if (prop->isDerivedFrom(App::PropertyPlacement::getClassTypeId()) &&
             strcmp(prop->getName(), "Placement") == 0) 
    {
        const auto &p = static_cast<const App::PropertyPlacement*>(prop)->getValue();
        ViewProviderGeometryObject::updateTransform(p, pcTransform);
    }
}

std::vector<App::DocumentObject*> ViewProviderLink::claimChildren(void) const
{
    std::vector<App::DocumentObject*> ret;
    if(pcLinked && pcLinked->getObject()) ret.push_back(pcLinked->getObject());
    return ret;
}



// Python object -----------------------------------------------------------------------

namespace Gui {
/// @cond DOXERR
PROPERTY_SOURCE_TEMPLATE(Gui::ViewProviderLinkPython, Gui::ViewProviderLink)
/// @endcond

// explicit template instantiation
template class GuiExport ViewProviderPythonFeatureT<Gui::ViewProviderLink>;
}

