/***************************************************************************
 *   Copyright (c) 2005 Jürgen Riegel <juergen.riegel@web.de>              *
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
# include <qstatusbar.h>
# include <qstring.h>
# include <Inventor/details/SoFaceDetail.h>
# include <Inventor/details/SoLineDetail.h>
#endif

#include <Inventor/elements/SoOverrideElement.h>
#include <Inventor/elements/SoLazyElement.h>
#include <Inventor/elements/SoGLLazyElement.h>
#include <Inventor/elements/SoCacheElement.h>
#include <Inventor/elements/SoWindowElement.h>

#include <Inventor/SoFullPath.h>
#include <Inventor/actions/SoGLRenderAction.h>
#include <Inventor/actions/SoHandleEventAction.h>
#include <Inventor/actions/SoAudioRenderAction.h>
#include <Inventor/events/SoKeyboardEvent.h>
#include <Inventor/elements/SoComplexityElement.h>
#include <Inventor/elements/SoComplexityTypeElement.h>
#include <Inventor/elements/SoCoordinateElement.h>
#include <Inventor/elements/SoElements.h>
#include <Inventor/elements/SoFontNameElement.h>
#include <Inventor/elements/SoFontSizeElement.h>
#include <Inventor/elements/SoLineWidthElement.h>
#include <Inventor/elements/SoMaterialBindingElement.h>
#include <Inventor/elements/SoModelMatrixElement.h>
#include <Inventor/elements/SoShapeStyleElement.h>
#include <Inventor/elements/SoProfileCoordinateElement.h>
#include <Inventor/elements/SoProfileElement.h>
#include <Inventor/elements/SoSwitchElement.h>
#include <Inventor/elements/SoUnitsElement.h>
#include <Inventor/elements/SoViewingMatrixElement.h>
#include <Inventor/elements/SoViewportRegionElement.h>
#include <Inventor/elements/SoGLCacheContextElement.h>
#include <Inventor/annex/FXViz/elements/SoShadowStyleElement.h>
#include <Inventor/events/SoMouseButtonEvent.h>
#include <Inventor/misc/SoState.h>
#include <Inventor/misc/SoChildList.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoMaterialBinding.h>
#include <Inventor/nodes/SoNormalBinding.h>
#include <Inventor/events/SoLocation2Event.h>
#include <Inventor/SoPickedPoint.h>
#include <Inventor/threads/SbStorage.h>

#ifdef FC_OS_MACOSX
# include <OpenGL/gl.h>
#else
# ifdef FC_OS_WIN32
#  include <windows.h>
# endif
# include <GL/gl.h>
#endif

#include <Inventor/SbDPLine.h>

#include <QApplication>
#include <QtOpenGL.h>

#include <Base/Console.h>
#include <Base/Tools.h>
#include <App/Application.h>
#include <App/Document.h>
#include <Gui/Document.h>
#include <App/DocumentObject.h>
#include <App/DocumentObserver.h>
#include <App/GeoFeature.h>
#include <App/ComplexGeoData.h>

#include "Inventor/SoFCRenderCacheManager.h"
#include "SoFCUnifiedSelection.h"
#include "Application.h"
#include "MainWindow.h"
#include "Selection.h"
#include "ViewProvider.h"
#include "SoFCInteractiveElement.h"
#include "SoFCSelectionAction.h"
#include "ViewProviderDocumentObject.h"
#include "ViewProviderGeometryObject.h"
#include "ViewParams.h"
#include "DockWindowManager.h"

FC_LOG_LEVEL_INIT("SoFCUnifiedSelection",false,true)

using namespace Gui;

// *************************************************************************

struct PickedInfo {
    std::unique_ptr<SoPickedPoint> ppCopy;
    const SoPickedPoint *pp;
    ViewProviderDocumentObject *vpd;
    // Subname path including the directly picked geometry elements (Vertex, Edge, Face)
    std::string subname;
    // Higher level geometry elements, like Wire, Solid
    std::vector<std::string> elements;

    PickedInfo()
        :pp(0),vpd(0)
    {}

    PickedInfo(PickedInfo &&other)
        :ppCopy(std::move(other.ppCopy))
        ,pp(other.pp)
        ,vpd(other.vpd)
        ,subname(std::move(other.subname))
        ,elements(std::move(other.elements))
    {}

    PickedInfo &operator=(PickedInfo &&other) {
        ppCopy = std::move(other.ppCopy);
        pp = other.pp;
        vpd = other.vpd;
        subname = std::move(other.subname);
        elements = std::move(other.elements);
        return *this;
    }

    void copy() {
        ppCopy.reset(pp->copy());
        pp = ppCopy.get();
    }

};

// *************************************************************************

class SoFCUnifiedSelection::Private {
public:
    Private(SoFCUnifiedSelection * master)
        :master(master)
        ,pcDocument(nullptr)
        ,pcViewer(nullptr)
        ,currentHighlight(static_cast<SoFullPath*>(new SoPath(20)))
        ,detailPath(static_cast<SoFullPath*>(new SoPath(20)))
    {
        setPreSelection = false;
        selectAll = false;
        preSelection = -1;

        preselTime = SbTime::getTimeOfDay();
        preselTimer.setData(this);
        preselTimer.setFunction([](void *data, SoSensor*){
            reinterpret_cast<Private*>(data)->onPreselectTimer();
        });
    }

    ~Private() {
    }

    bool useRenderer() const {
        return ViewParams::getRenderCache() == 3;
    }

    bool render(SoGLRenderAction * action) {
        if (useRenderer()) {
            manager.render(action);
            return true;
        }
        return false;
    }

    void touch() {
        if (this->pcViewer && this->pcViewer->getRootPath()) {
            SoNode * head = this->pcViewer->getRootPath()->getHead();
            if (head) {
                head->touch();
                return;
            }
        }
        master->touch();
    }

    bool doAction(SoAction *);
    void handleEvent(SoHandleEventAction * action);
    void applyOverrideMode(SoState * state) const;

    uint32_t getSelectionColor() const {
        float t = 0.f;
        if (ViewParams::getShowSelectionOnTop())
            t = ViewParams::getTransparencyOnTop();
        return master->colorSelection.getValue().getPackedValue(t);
    }

    void clearHighlight() {
        if (!currentHighlight->getLength())
            return;
        if (useRenderer()) {
            manager.clearHighlight();
            touch();
        } else {
            hlaction.setHighlighted(false);
            hlaction.apply(currentHighlight);
            master->touch();
        }
        currentHighlight->truncate(0);
    }

    bool setHighlight(const PickedInfo &);

    bool setHighlight(SoFullPath *path,
                      const SoDetail *det, 
                      ViewProviderDocumentObject *vpd,
                      const char *element,
                      float x, float y, float z,
                      bool ontop = false);

    bool setSelection(const std::vector<PickedInfo> &,
                      bool ctrlDown,
                      bool shiftDown,
                      bool altDown);

    std::vector<PickedInfo> getPickedList(const SbVec2s &pos,
                                          const SbViewportRegion &vp,
                                          bool singlePick) const;

    std::vector<PickedInfo> getPickedList(SoHandleEventAction* action,
                                          bool singlePick) const;

    static void postProcessPickedList(std::vector<PickedInfo> &,
                                      bool singlePick);

    typedef std::set<std::pair<ViewProvider*, std::string> > Filter;
    void getPickedInfo(std::vector<PickedInfo> &,
                       const SoPickedPointList &,
                       bool singlePick,
                       bool copy,
                       Filter &filter) const;

    void getPickedInfoOnTop(std::vector<PickedInfo> &,
                            bool singlePick,
                            Filter &filter) const;

    void onPreselectTimer();

    SoFCUnifiedSelection * master;
    SoFCRenderCacheManager manager;

    SoHighlightElementAction hlaction;
    SoSelectionElementAction slaction;
    mutable SoFCRayPickAction rayPickAction;

    Gui::Document        *pcDocument;
    View3DInventorViewer *pcViewer;

    CoinPtr<SoFullPath> currentHighlight;
    CoinPtr<SoFullPath> detailPath;

    SbBool setPreSelection;

    bool selectAll;

    // -1 = not handled, 0 = not selected, 1 = selected
    int32_t preSelection;
    SoColorPacker colorpacker;

    SbTime preselTime;
    SoTimerSensor preselTimer;
    SbVec2s preselPos;
    SbViewportRegion preselViewport;

    mutable int pickBackFace = 0;
};

SO_NODE_SOURCE(SoFCUnifiedSelection)

/*!
  Constructor.
*/
SoFCUnifiedSelection::SoFCUnifiedSelection()
    : pimpl (nullptr)
{
    SO_NODE_CONSTRUCTOR(SoFCUnifiedSelection);

    SO_NODE_ADD_FIELD(colorHighlight, (SbColor(1.0f, 0.6f, 0.0f)));
    SO_NODE_ADD_FIELD(colorSelection, (SbColor(0.1f, 0.8f, 0.1f)));
    SO_NODE_ADD_FIELD(highlightMode,  (AUTO));
    SO_NODE_ADD_FIELD(selectionMode,  (ON));
    SO_NODE_ADD_FIELD(selectionRole,  (TRUE));
    SO_NODE_ADD_FIELD(useNewSelection, (TRUE));
    SO_NODE_ADD_FIELD(overrideMode, (""));

    SO_NODE_DEFINE_ENUM_VALUE(HighlightModes, AUTO);
    SO_NODE_DEFINE_ENUM_VALUE(HighlightModes, ON);
    SO_NODE_DEFINE_ENUM_VALUE(HighlightModes, OFF);
    SO_NODE_SET_SF_ENUM_TYPE (highlightMode, HighlightModes);

    this->renderCaching = ViewParams::getRenderCache() == 3 ?
        SoSeparator::OFF : SoSeparator::ON;
    this->boundingBoxCaching = ViewParams::getRenderCache() == 3 ?
        SoSeparator::OFF : SoSeparator::ON;

    this->useNewSelection = ViewParams::instance()->getUseNewSelection();

    pimpl = new Private(this);
}

/*!
  Destructor.
*/
SoFCUnifiedSelection::~SoFCUnifiedSelection()
{
    delete pimpl;
}

// doc from parent
void
SoFCUnifiedSelection::initClass(void)
{
    SO_NODE_INIT_CLASS(SoFCUnifiedSelection,SoSeparator,"Separator");
}

void SoFCUnifiedSelection::finish()
{
    atexit_cleanup();
}

void SoFCUnifiedSelection::getBoundingBox(SoGetBoundingBoxAction * action)
{
    if (pimpl->useRenderer() && pimpl->manager.getSceneNodeId() == getNodeId()) {
        bool usecache = true;
        switch (action->getCurPathCode()) {
        case SoAction::IN_PATH:
            usecache = false;
            break;
        case SoAction::OFF_PATH:
            return; // no need to do any more work
        case SoAction::BELOW_PATH:
        case SoAction::NO_PATH:
            if (action->isInCameraSpace() || action->isResetPath())
                usecache = false;
            break;
        default:
            return;
        }
        if (usecache) {
            SbBox3f bbox;
            pimpl->manager.getBoundingBox(bbox);
            if (!bbox.isEmpty())
                action->extendBy(bbox);
            return;
        }
    }
    inherited::getBoundingBox(action);
}

void SoFCUnifiedSelection::setSelectAll(bool enable)
{
    pimpl->selectAll = enable;
}

void SoFCUnifiedSelection::setDocument(Document * doc)
{
    pimpl->pcDocument = doc;
}

void SoFCUnifiedSelection::setViewer(View3DInventorViewer * viewer)
{
    pimpl->pcViewer = viewer;
}

bool SoFCUnifiedSelection::hasHighlight() {
    return pimpl->currentHighlight->getLength();
}

void SoFCUnifiedSelection::applySettings()
{
    this->highlightMode = ViewParams::instance()->getEnablePreselection()?ON:OFF;
    this->selectionMode = ViewParams::instance()->getEnableSelection()?ON:OFF;

    float trans;
    SbColor color;
    color.setPackedValue(ViewParams::instance()->getHighlightColor(),trans);
    this->colorHighlight = color;

    color.setPackedValue(ViewParams::instance()->getSelectionColor(),trans);
    this->colorSelection = color;
}

const char* SoFCUnifiedSelection::getFileFormatName(void) const
{
    return "Separator";
}

void SoFCUnifiedSelection::write(SoWriteAction * action)
{
    SoOutput * out = action->getOutput();
    if (out->getStage() == SoOutput::WRITE) {
        // Do not write out the fields of this class
        if (this->writeHeader(out, true, false)) return;
        SoGroup::doAction((SoAction *)action);
        this->writeFooter(out);
    }
    else {
        inherited::write(action);
    }
}

int SoFCUnifiedSelection::getPriority(const SoPickedPoint* p)
{
    const SoDetail* detail = p->getDetail();
    if (!detail)                                           return 0;
    if (detail->isOfType(SoFaceDetail::getClassTypeId()))  return 1;
    if (detail->isOfType(SoLineDetail::getClassTypeId()))  return 2;
    if (detail->isOfType(SoPointDetail::getClassTypeId())) return 3;
    return 0;
}

void
SoFCUnifiedSelection::Private::getPickedInfo(std::vector<PickedInfo> &ret,
                                             const SoPickedPointList &points,
                                             bool singlePick,
                                             bool copy,
                                             Filter &filter) const
{
    ViewProvider *last_vp = 0;
    for(int i=0,count=points.getLength();i<count;++i) {
        PickedInfo info;
        info.pp = points[i];
        info.vpd = 0;
        ViewProvider *vp = 0;
        SoFullPath *path = static_cast<SoFullPath *>(info.pp->getPath());
        if (this->pcDocument && path) {
            vp = this->pcDocument->getViewProviderByPathFromHead(path);
            if(singlePick && last_vp && last_vp!=vp)
                return;
        }
        if(!vp || !vp->isDerivedFrom(ViewProviderDocumentObject::getClassTypeId())) {
            if(!singlePick) continue;
            if(ret.empty()) {
                if(copy) info.copy();
                ret.push_back(std::move(info));
            }
            break;
        }
        info.vpd = static_cast<ViewProviderDocumentObject*>(vp);
        if(!(master->useNewSelection.getValue()
                || info.vpd->useNewSelectionModel())
            || !info.vpd->isSelectable())
        {
            if(!singlePick) continue;
            if(ret.empty()) {
                info.vpd = 0;
                if(copy) info.copy();
                ret.push_back(std::move(info));
            }
            break;
        }

        if(!info.vpd->getElementPicked(info.pp,info.subname))
            continue;

        if(singlePick) 
            last_vp = vp;
        else {
            if(!filter.emplace(info.vpd,info.subname).second)
                continue;
            
            App::GeoFeature *geo = nullptr;
            std::pair<std::string, std::string> elementName;
            App::GeoFeature::resolveElement(info.vpd->getObject(),
                    info.subname.c_str(), elementName, false,
                    App::GeoFeature::Normal, nullptr, nullptr, &geo);
            if(geo && geo->getPropertyOfGeometry()) {
                auto data = geo->getPropertyOfGeometry()->getComplexData();
                if(data)
                    info.elements = data->getHigherElements(elementName.second.c_str());
            }
        }
        
        if(copy) info.copy();
        ret.push_back(std::move(info));
    }
}

void
SoFCUnifiedSelection::Private::getPickedInfoOnTop(std::vector<PickedInfo> &ret,
                                                  bool singlePick,
                                                  Filter &filter) const
{
    if(SoFCUnifiedSelection::getShowSelectionBoundingBox())
        return;

    static FC_COIN_THREAD_LOCAL SoTempPath tmpPath(20);
    tmpPath.ref();
    tmpPath.truncate(0);
    if (useRenderer()) {
        const auto & paths = this->manager.getSelectionPaths();
        tmpPath.append(pcViewer->getRootPath());
        int pathLength = tmpPath.getLength();
        SoState * state = this->rayPickAction.getState();
        SoFCSwitch::setOverrideSwitch(state, true);
        for (auto it=paths.lower_bound(1); it!=paths.end(); ++it) {
            tmpPath.append(it->second);
            SoFCSwitch::pushSwitchPath(&tmpPath);
            this->rayPickAction.apply(&tmpPath);
            SoFCSwitch::popSwitchPath();
            tmpPath.truncate(pathLength);
        }
        SoFCSwitch::setOverrideSwitch(state, false);
    }
    else {
        const SoPath *path = pcViewer->getGroupOnTopPath();
        int pathLength = path->getLength();
        if(pathLength && path->getNodeFromTail(0)->isOfType(SoGroup::getClassTypeId())) {
            SoGroup *group = static_cast<SoGroup*>(path->getNodeFromTail(0));
            tmpPath.append(path);
            for(int i=0,count=group->getNumChildren();i<count;++i) {
                auto child = group->getChild(i);
                if(!child->isOfType(SoFCPathAnnotation::getClassTypeId()))
                    continue;
                static_cast<SoFCPathAnnotation*>(child)->doPick(&tmpPath, &this->rayPickAction);
            }
        }
    }
    getPickedInfo(ret,this->rayPickAction.getPrioPickedPointList(),singlePick,true,filter);
    tmpPath.truncate(0);
    tmpPath.unrefNoDelete();
}

std::vector<PickedInfo> 
SoFCUnifiedSelection::Private::getPickedList(SoHandleEventAction* action,
                                             bool singlePick) const
{
    return getPickedList(action->getEvent()->getPosition(),
                         action->getViewportRegion(),
                         singlePick);
}

std::vector<PickedInfo> 
SoFCUnifiedSelection::Private::getPickedList(const SbVec2s &pos,
                                             const SbViewportRegion &viewport,
                                             bool singlePick) const
{
    std::vector<PickedInfo> ret;
    Filter filter;

    FC_TIME_INIT(t);

    float radius = ViewParams::instance()->getPickRadius();
    this->rayPickAction.setRadius(radius);
    this->rayPickAction.setViewportRegion(viewport);
    this->rayPickAction.setPoint(pos);
    this->rayPickAction.setPickAll(!singlePick || !ViewParams::instance()->getUseNewRayPick());
    this->rayPickAction.setPickBackFace(singlePick ? pickBackFace : 0); 

    SoPickStyleElement::set(this->rayPickAction.getState(),
            (!this->rayPickAction.pickBackFace() && singlePick) ?
                SoPickStyleElement::SHAPE_FRONTFACES : SoPickStyleElement::SHAPE);
    SoOverrideElement::setPickStyleOverride(this->rayPickAction.getState(),0,true);

    SoFCDisplayModeElement::set(this->rayPickAction.getState(),0,SbName::empty(),false);

    this->rayPickAction.cleanup();
    getPickedInfoOnTop(ret, singlePick, filter);

    if(ret.empty() || !singlePick) {
        applyOverrideMode(this->rayPickAction.getState());
        SoOverrideElement::setPickStyleOverride(this->rayPickAction.getState(),0,false);
        this->rayPickAction.apply(pcViewer->getSoRenderManager()->getSceneGraph());

        getPickedInfo(ret,this->rayPickAction.getPrioPickedPointList(),singlePick,false,filter);
    }

    if(singlePick && pickBackFace) {
        if(pickBackFace > 1 && this->rayPickAction.getBackFaceCount() < pickBackFace)
            pickBackFace = std::max(1, this->rayPickAction.getBackFaceCount());
        else if (pickBackFace < 0 && this->rayPickAction.getBackFaceCount() < -pickBackFace)
            pickBackFace = std::min(-1, -this->rayPickAction.getBackFaceCount());
    }

    FC_TIME_TRACE(t,"pick radius " << radius << ", count " << ret.size() << ',');

    // postProcessPickedList() is used to resolve overlapping primitives
    // (point, line and face). We will pick point over line over face if
    // the picked points overalps.
    //
    // The disadvantage of doing post process here is that we must obtain all
    // picked points of all objects. When the user zooms the camera far out,
    // the pick radius (default to 5 pixel) may cause large amount (100k+ for
    // bigger assembly) of primitves (mostly points and lines) in the picked
    // point list. The fact that each SoPickedPoint contains a full path of the
    // node hierarchy may cause system slow down, with most of the time
    // spending on cleaning up the path.
    //
    // This problem can be solved by replacing SoRayPickAction with a derived
    // SoFCRayPickAction. After picking each shape node, it will consider the
    // primitive priority before choosing which picked point to retain (see
    // SoFCRayPickAction::afterPick()). With this action, all picked points are
    // processed on the fly, and only the primitive with the closest picked
    // point and the highest priority will be returned.
    //
    if(singlePick && this->rayPickAction.isPickAll())
        postProcessPickedList(ret, singlePick);
    return ret;
}

void
SoFCUnifiedSelection::Private::postProcessPickedList(std::vector<PickedInfo> &ret,
                                                     bool singlePick)
{
    if(ret.size()<=1)
        return;

    // To identify the picking of lines in a concave area we have to 
    // get all intersection points. If we have two or more intersection
    // points where the first is of a face and the second of a line with
    // almost similar coordinates we use the second point, instead.

    int picked_prio = getPriority(ret[0].pp);
    auto last_vpd = ret[0].vpd;
    const SbVec3f& picked_pt = ret.front().pp->getPoint();
    auto itPicked = ret.begin();
    for(auto it=ret.begin()+1;it!=ret.end();++it) {
        auto &info = *it;
        if(last_vpd != info.vpd) 
            break;

        int cur_prio = getPriority(info.pp);
        const SbVec3f& cur_pt = info.pp->getPoint();

        if ((cur_prio > picked_prio) && picked_pt.equals(cur_pt, 0.01f)) {
            itPicked = it;
            picked_prio = cur_prio;
        }
    }

    if(singlePick) {
        std::vector<PickedInfo> sret(1);
        sret[0] = std::move(*itPicked);
        ret = std::move(sret);
        return;
    }
    if(itPicked != ret.begin()) {
        PickedInfo tmp(std::move(*itPicked));
        *itPicked = std::move(*ret.begin());
        *ret.begin() = std::move(tmp);
    }
}

std::vector<App::SubObjectT>
SoFCUnifiedSelection::getPickedSelections(const SbVec2s &pos,
                                          const SbViewportRegion &viewport,
                                          bool singlePick) const
{
    std::vector<App::SubObjectT> sels;
    auto infos = pimpl->getPickedList(pos,viewport,singlePick);
    sels.reserve(infos.size());
    std::set<std::pair<ViewProvider*, std::string> > objSet;
    std::string prefix;
    std::string subname;
    for(auto &info : infos) {
        if(!info.vpd)
            continue;
        if(!objSet.insert(std::make_pair(info.vpd,info.subname)).second)
            continue;
        sels.emplace_back(info.vpd->getObject(),info.subname.c_str());
        if(info.elements.empty())
            continue;
        const char *element = Data::ComplexGeoData::findElementName(info.subname.c_str());

        prefix.assign(info.subname.c_str(), element);
        for(const auto &element : info.elements) {
            subname = prefix;
            subname += element;
            if(objSet.insert(std::make_pair(info.vpd,subname)).second)
                sels.emplace_back(info.vpd->getObject(), subname.c_str());
        }
    }
    return sels;
}

SoPickedPoint *
SoFCUnifiedSelection::getPickedPoint(SoHandleEventAction *action) const {
    auto res = pimpl->getPickedList(action,true);
    if(res.size() && res[0].pp)
        return res[0].pp->copy();
    return 0;
}

void SoFCUnifiedSelection::doAction(SoAction *action)
{
    SoState * state = action->getState();
    state->push();
    pimpl->applyOverrideMode(action->getState());
    if (pimpl->doAction(action))
        inherited::doAction(action);
    state->pop();
}

void SoFCUnifiedSelection::callback(SoCallbackAction *action)
{
    SoState * state = action->getState();
    state->push();
    pimpl->applyOverrideMode(state);
    inherited::callback(action);
    state->pop();
}

SbName SoFCUnifiedSelection::DisplayModeTessellation("Tessellation");
SbName SoFCUnifiedSelection::DisplayModeShaded("Shaded");
SbName SoFCUnifiedSelection::DisplayModeHiddenLine("Hidden Line");
SbName SoFCUnifiedSelection::DisplayModeFlatLines("Flat Lines");
SbName SoFCUnifiedSelection::DisplayModeAsIs("As Is");
SbName SoFCUnifiedSelection::DisplayModeNoShading("No Shading");

void SoFCUnifiedSelection::Private::applyOverrideMode(SoState * state) const
{
    bool shading = true;
    if (state->isElementEnabled(SoFCDisplayModeElement::getClassStackIndex())) {
        SbName mode = master->overrideMode.getValue();
        bool hiddenline = false;
        if (mode == DisplayModeTessellation) {
            shading = false;
            mode = DisplayModeShaded;
            SoOverrideElement::setDrawStyleOverride(state, master, TRUE);
            SoDrawStyleElement::set(state, SoDrawStyleElement::LINES);
        }
        else if (mode == DisplayModeHiddenLine) {
            hiddenline = true;
            mode = DisplayModeFlatLines;
            shading = ViewParams::getHiddenLineShaded();
        }
        else if (mode == DisplayModeNoShading) {
            shading = false;
            mode = DisplayModeFlatLines;
        }
        else if (mode == DisplayModeAsIs)
            mode = SbName::empty();

        SoFCDisplayModeElement::set(state, master, mode, hiddenline);
    }

    if (!shading && state->isElementEnabled(SoLightModelElement::getClassStackIndex())) {
        SoOverrideElement::setLightModelOverride(state, master, TRUE);
        SoLightModelElement::set(state, SoLightModelElement::BASE_COLOR);
    }
}

bool SoFCUnifiedSelection::Private::doAction(SoAction * action)
{
    if (action->getTypeId() == SoFCEnableHighlightAction::getClassTypeId()) {
        SoFCEnableHighlightAction *preaction = (SoFCEnableHighlightAction*)action;
        if (preaction->highlight) {
            master->highlightMode = SoFCUnifiedSelection::AUTO;
        }
        else {
            master->highlightMode = SoFCUnifiedSelection::OFF;
        }
    }

    if (action->getTypeId() == SoFCEnableSelectionAction::getClassTypeId()) {
        SoFCEnableSelectionAction *selaction = (SoFCEnableSelectionAction*)action;
        if (selaction->selection) {
            master->selectionMode = SoFCUnifiedSelection::ON;
        }
        else {
            master->selectionMode = SoFCUnifiedSelection::OFF;
        }
    }

    if (action->getTypeId() == SoFCSelectionColorAction::getClassTypeId()) {
        SoFCSelectionColorAction *colaction = (SoFCSelectionColorAction*)action;
        master->colorSelection = colaction->selectionColor;
    }

    if (action->getTypeId() == SoFCHighlightColorAction::getClassTypeId()) {
        SoFCHighlightColorAction *colaction = (SoFCHighlightColorAction*)action;
        master->colorHighlight = colaction->highlightColor;
    }

    if (action->getTypeId() == SoFCHighlightAction::getClassTypeId()) {
        SoFCHighlightAction *hilaction = static_cast<SoFCHighlightAction*>(action);
        // Do not clear currently highlighted object when setting new pre-selection
        if (!setPreSelection && hilaction->SelChange->Type == SelectionChanges::RmvPreselect) {
            clearHighlight();
        } else if (master->highlightMode.getValue() != OFF 
                    && !setPreSelection
                    && hilaction->SelChange->Type == SelectionChanges::SetPreselect)
        {
            clearHighlight();
            App::Document* doc = App::GetApplication().getDocument(hilaction->SelChange->pDocName);
            App::DocumentObject* obj = doc->getObject(hilaction->SelChange->pObjectName);
            ViewProvider*vp = Application::Instance->getViewProvider(obj);
            if (vp && vp->isDerivedFrom(ViewProviderDocumentObject::getClassTypeId()) &&
                (master->useNewSelection.getValue()||vp->useNewSelectionModel()) && vp->isSelectable()) 
            {
                detailPath->truncate(0);
                SoDetail *det = 0;
                if(vp->getDetailPath(hilaction->SelChange->pSubName,detailPath,true,det)) {
                    setHighlight(detailPath,det,static_cast<ViewProviderDocumentObject*>(vp),
                                 hilaction->SelChange->pSubName, 
                                 hilaction->SelChange->x,
                                 hilaction->SelChange->y,
                                 hilaction->SelChange->z,
                                 hilaction->SelChange->SubType == 2);
                }
                delete det;
            }
        }
        if(master->useNewSelection.getValue())
            return false;
    }

    if (action->getTypeId() == SoFCSelectionAction::getClassTypeId()) {
        SoFCSelectionAction *selaction = static_cast<SoFCSelectionAction*>(action);
        if(master->selectionMode.getValue() == ON 
            && (selaction->SelChange->Type == SelectionChanges::AddSelection 
                || selaction->SelChange->Type == SelectionChanges::RmvSelection))
        {
            // selection changes inside the 3d view are handled in handleEvent()
            App::Document* doc = App::GetApplication().getDocument(selaction->SelChange->pDocName);
            App::DocumentObject* obj = doc->getObject(selaction->SelChange->pObjectName);
            ViewProvider*vp = Application::Instance->getViewProvider(obj);
            if (vp && (master->useNewSelection.getValue()||vp->useNewSelectionModel()) && vp->isSelectable()) {
                SoDetail *detail = nullptr;
                detailPath->truncate(0);
                if (useRenderer()) {
                    if (selaction->SelChange->Type != SelectionChanges::AddSelection) {
                        manager.removeSelection(selaction->SelChange->Object.getSubNameNoElement(true),
                                                selaction->SelChange->Object.getOldElementName());
                        touch();
                        return false;
                    }
                }
                if(vp->getDetailPath(selaction->SelChange->pSubName,detailPath,true,detail)) {
                    SoSelectionElementAction::Type type = SoSelectionElementAction::None;
                    if (selaction->SelChange->Type == SelectionChanges::AddSelection) {
                        if (detail)
                            type = SoSelectionElementAction::Append;
                        else
                            type = SoSelectionElementAction::All;
                    }
                    else {
                        if (detail)
                            type = SoSelectionElementAction::Remove;
                        else
                            type = SoSelectionElementAction::None;
                    }

                    if (useRenderer()) {
                        if(detailPath->getLength())
                            manager.addSelection(selaction->SelChange->Object.getSubNameNoElement(true),
                                                 selaction->SelChange->Object.getOldElementName(),
                                                 detailPath,
                                                 detail,
                                                 getSelectionColor(),
                                                 ViewParams::getShowSelectionOnTop()); 
                        else
                            manager.addSelection(selaction->SelChange->Object.getSubNameNoElement(true),
                                                 selaction->SelChange->Object.getOldElementName(),
                                                 vp->getRoot(),
                                                 getSelectionColor(),
                                                 ViewParams::getShowSelectionOnTop()); 
                        touch();
                    }
                    else {
                        slaction.setType(type);
                        slaction.setColor(master->colorSelection.getValue());
                        slaction.setElement(detail);
                        if(detailPath->getLength())
                            slaction.apply(detailPath);
                        else
                            slaction.apply(vp->getRoot());
                        slaction.setElement(nullptr);
                    }
                }
                detailPath->truncate(0);
                delete detail;
            }
        }else if (selaction->SelChange->Type == SelectionChanges::ClrSelection) {
            if (useRenderer()) {
                manager.clearSelection();
                touch();
            }
            else {
                slaction.setType(SoSelectionElementAction::None);
                for(int i=0;i<master->getNumChildren();++i)
                    slaction.apply(master->getChild(i));
            }
        }else if(master->selectionMode.getValue() == ON 
                    && selaction->SelChange->Type == SelectionChanges::SetSelection) {
            std::vector<ViewProvider*> vps;
            if (this->pcDocument)
                vps = this->pcDocument->getViewProvidersOfType(ViewProviderDocumentObject::getClassTypeId());
            for (std::vector<ViewProvider*>::iterator it = vps.begin(); it != vps.end(); ++it) {
                ViewProviderDocumentObject* vpd = static_cast<ViewProviderDocumentObject*>(*it);
                if (master->useNewSelection.getValue() || vpd->useNewSelectionModel()) {
                    SoSelectionElementAction::Type type;
                    if(Selection().isSelected(vpd->getObject()) && vpd->isSelectable())
                        type = SoSelectionElementAction::All;
                    else
                        type = SoSelectionElementAction::None;

                    if (useRenderer()) {
                        if (type == SoSelectionElementAction::All) {
                            manager.addSelection(selaction->SelChange->Object.getSubNameNoElement(true), 
                                                 selaction->SelChange->Object.getOldElementName(),
                                                 vpd->getRoot(),
                                                 getSelectionColor(),
                                                 ViewParams::getShowSelectionOnTop());
                            touch();
                        }
                    }
                    else {
                        slaction.setType(type);
                        slaction.setColor(master->colorSelection.getValue());
                        slaction.apply(vpd->getRoot());
                    }
                }
            }
        }
        if(master->useNewSelection.getValue())
            return false;
    }
    return true;
}

void SoFCUnifiedSelection::Private::onPreselectTimer() {
    if(preselTimer.isScheduled())
        preselTimer.unschedule();

    auto infos = getPickedList(preselPos, preselViewport, true);
    if(infos.size())
        setHighlight(infos[0]);
    else {
        // Do not remove preslection in case of dock overlay mouse pass through
        if (!DockWindowManager::instance()->isUnderOverlay())
            setHighlight(PickedInfo());
    }

    preselTime = SbTime::getTimeOfDay();
}

bool SoFCUnifiedSelection::Private::setHighlight(const PickedInfo &info) {
    if(!info.pp) {
        return setHighlight(0,0,0,0,0.0,0.0,0.0);
    }
    const auto &pt = info.pp->getPoint();
    const SoDetail *det = info.pp->getDetail();
    if(det && !Data::ComplexGeoData::hasElementName(info.subname.c_str()))
        det = nullptr;
    return setHighlight(static_cast<SoFullPath*>(info.pp->getPath()), 
            det, info.vpd, info.subname.c_str(), pt[0],pt[1],pt[2]);
}

bool
SoFCUnifiedSelection::Private::setHighlight(SoFullPath *path,
                                            const SoDetail *det, 
                                            ViewProviderDocumentObject *vpd,
                                            const char *subname,
                                            float x, float y, float z,
                                            bool ontop) 
{
    Base::FlagToggler<SbBool> flag(setPreSelection);

    bool highlighted = false;
    if(path && path->getLength() && 
       vpd && vpd->getObject() && vpd->getObject()->getNameInDocument()) 
    {
        const char *docname = vpd->getObject()->getDocument()->getName();
        const char *objname = vpd->getObject()->getNameInDocument();

        this->preSelection = 1;
        int ret = Gui::Selection().setPreselect(docname,objname,subname,x,y,z,0,true);
        if(ret<0 && this->currentHighlight->getLength())
            return true;

        if(ret) {
            clearHighlight();
            currentHighlight->append(path);
            highlighted = true;
        }
    }

    if(!highlighted) {
        this->preSelection = 0;
        clearHighlight();
    }
    else {
        if (useRenderer()) {
            float t = 0.f;
            ontop = ontop || ViewParams::getPreSelectionOnTop();
            if (ontop)
                t = ViewParams::getTransparencyOnTop();
            App::SubObjectT obj(vpd->getObject(), subname);
            if (manager.isOnTop(obj.getSubNameNoElement(true))) {
                ontop = true;
                // we'll square the alpha make it more visible
                float a = 1.f - ViewParams::getTransparencyOnTop();
                t = 1.f - a*a;
            }
            manager.setHighlight(currentHighlight, det,
                    master->colorHighlight.getValue().getPackedValue(t), ontop);
            touch();
        }
        else {
            hlaction.setHighlighted(true);
            hlaction.setColor(master->colorHighlight.getValue());
            hlaction.setElement(det);
            hlaction.apply(currentHighlight);
            master->touch();
        }
    }
    return highlighted;
}

bool
SoFCUnifiedSelection::Private::setSelection(const std::vector<PickedInfo> &infos, 
                                            bool ctrlDown,
                                            bool shiftDown,
                                            bool altDown) 
{
    if(infos.empty() || !infos[0].vpd) return false;

    std::vector<SelectionSingleton::SelObj> sels;
    std::set<std::pair<ViewProvider*, std::string> > objSet;
    if(infos.size()>1) {
        std::string subname;
        std::string prefix;
        for(auto &info : infos) {
            if(!info.vpd || !objSet.insert(std::make_pair(info.vpd, info.subname)).second)
                continue;
            SelectionSingleton::SelObj sel;
            sel.pObject = info.vpd->getObject();
            sel.pDoc = sel.pObject->getDocument();
            sel.DocName = sel.pDoc->getName();
            sel.FeatName = sel.pObject->getNameInDocument();
            sel.TypeName = sel.pObject->getTypeId().getName();
            sel.SubName = info.subname.c_str();
            const auto &pt = info.pp->getPoint();
            sel.x = pt[0];
            sel.y = pt[1];
            sel.z = pt[2];
            sels.push_back(sel);

            if(info.elements.empty())
                continue;
            const char *element = Data::ComplexGeoData::findElementName(info.subname.c_str());
            prefix.assign(info.subname.c_str(), element);
            for(const auto &element : info.elements) {
                subname = prefix;
                subname += element;
                auto res = objSet.insert(std::make_pair(info.vpd, subname));
                if(res.second) {
                    sel.SubName = res.first->second.c_str();
                    sels.push_back(sel);
                }
            }
        }
    }

    const auto &info = infos[0];
    auto vpd = info.vpd;
    if(!vpd) return false;
    const char *objname = vpd->getObject()->getNameInDocument();
    if(!objname) return false;
    const char *docname = vpd->getObject()->getDocument()->getName();

    bool hasNext = false;
    const SoPickedPoint * pp = info.pp;
    const SoDetail *det = pp->getDetail();
    if(det && !Data::ComplexGeoData::hasElementName(info.subname.c_str()))
        det = 0;
    SoDetail *detNext = 0;
    SoFullPath *pPath = static_cast<SoFullPath*>(pp->getPath());
    const auto &pt = pp->getPoint();
    SoSelectionElementAction::Type type = SoSelectionElementAction::None;
    HighlightModes mymode = (HighlightModes) master->highlightMode.getValue();
    char buf[513];

    if (ctrlDown) {
        if(Gui::Selection().isSelected(docname,objname,info.subname.c_str(),0))
            Gui::Selection().rmvSelection(docname,objname,info.subname.c_str(),&sels);
        else {
            SelectionNoTopParentCheck guard;
            bool ok = Gui::Selection().addSelection(docname,objname,
                    info.subname.c_str(), pt[0] ,pt[1] ,pt[2], &sels);
            if (ok && mymode == OFF) {
                snprintf(buf,512,"Selected: %s.%s.%s (%g, %g, %g)",
                        docname,objname,info.subname.c_str()
                        ,fabs(pt[0])>1e-7?pt[0]:0.0
                        ,fabs(pt[1])>1e-7?pt[1]:0.0
                        ,fabs(pt[2])>1e-7?pt[2]:0.0);

                getMainWindow()->showMessage(QString::fromLatin1(buf));
            }
        }
        return true;
    }

    // Hierarchy ascending
    //
    // If the clicked subelement is already selected, check if there is an
    // upper hierarchy, and select that hierarchy instead. 
    //
    // For example, let's suppose PickedInfo above reports
    // 'link.link2.box.Face1', and below Selection().getSelectedElement returns
    // 'link.link2.box.', meaning that 'box' is the current selected hierarchy,
    // and the user is clicking the box again.  So we shall go up one level,
    // and select 'link.link2.'
    //

    std::string subName = info.subname;
    std::string objectName = objname;

    const char *subSelected = Gui::Selection().getSelectedElement(
                                vpd->getObject(),subName.c_str());

    FC_TRACE("select " << (subSelected?subSelected:"'null'") << ", " << 
            objectName << ", " << subName);
    std::string newElement;
    if(subSelected
        && ((ViewParams::getHierarchyAscend() && !ctrlDown && !shiftDown && !altDown)
             || Data::ComplexGeoData::hasElementName(subSelected)))
    {
        newElement = Data::ComplexGeoData::newElementName(subSelected);
        subSelected = newElement.c_str();
        std::string nextsub;
        const char *next = strrchr(subSelected,'.');
        if(next && next!=subSelected) {
            if(next[1]==0) {
                // The convention of dot separated SubName demands a mandatory
                // ending dot for every object name reference inside SubName.
                // The non-object sub-element, however, must not end with a dot.
                // So, next[1]==0 here means current selection is a whole object
                // selection (because no sub-element), so we shall search
                // upwards for the second last dot, which is the end of the
                // parent name of the current selected object
                for(--next;next!=subSelected;--next) {
                    if(*next == '.') break;
                }
            }
            if(*next == '.')
                nextsub = std::string(subSelected,next-subSelected+1);
        }
        if(nextsub.length() || *subSelected!=0) {
            hasNext = true;
            subName = nextsub;
            detailPath->truncate(0);
            if(vpd->getDetailPath(subName.c_str(),detailPath,true,detNext) && 
               detailPath->getLength()) 
            {
                pPath = detailPath;
                det = detNext;
                FC_TRACE("select next " << objectName << ", " << subName);
            }
        }
    }

#if 0 // ViewProviderDocumentObject now has default implementation of getElementPicked

    // If no next hierarchy is found, do another try on view provider hierarchies, 
    // which is used by geo feature group.
    if(!hasNext) {
        bool found = false;
        auto vps = this->pcDocument->getViewProvidersByPath(pPath);
        for(auto it=vps.begin();it!=vps.end();++it) {
            auto vpdNext = it->first;
            if(Gui::Selection().isSelected(vpdNext->getObject(),"")) {
                found = true;
                continue;
            }
            if(!found || !vpdNext->useNewSelectionModel() || !vpdNext->isSelectable())
                continue;
            hasNext = true;
            vpd = vpdNext;
            det = 0;
            pPath->truncate(it->second+1);
            objectName = vpd->getObject()->getNameInDocument();
            subName = "";
            break;
        }
    }
#endif

    FC_TRACE("clearing selection");
    Gui::Selection().clearSelection();
    FC_TRACE("add selection");
    SelectionNoTopParentCheck guard;
    bool ok = Gui::Selection().addSelection(docname, objectName.c_str() ,subName.c_str(), 
            pt[0] ,pt[1] ,pt[2], &sels);
    if (ok)
        type = hasNext?SoSelectionElementAction::All:SoSelectionElementAction::Append;

#if 1
    (void)pPath;
    (void)type;
#else
    if (mymode == OFF) {
        snprintf(buf,512,"Selected: %s.%s.%s (%g, %g, %g)",
                docname, objectName.c_str() ,subName.c_str()
                ,fabs(pt[0])>1e-7?pt[0]:0.0
                ,fabs(pt[1])>1e-7?pt[1]:0.0
                ,fabs(pt[2])>1e-7?pt[2]:0.0);

        getMainWindow()->showMessage(QString::fromLatin1(buf));
    }

    // Apply action only if ShowSelectionTop is off. If it is on,
    // View3DInventorView::onSelectionChange() will apply the action to its on
    // top group. If ShowSelectionTop is on and we still apply the action here,
    // there will be wrong selection highlight in the following case,
    //
    // * Make sure ShowSelectionOnTop is active,
    // * Create an App::Part, add two children, say a Box and a Cylinder.
    // * Select the whole Cylinder in tree view.
    // * Select Cylinder.Edge3 in 3D view, this will clear the whole Cylinder
    //   selection
    // * CTRL select any face of the Box, this will bring the box on top, and
    //   reveal that its Edge3 is also highlighted.
    //
    // TODO: find out why!
    //
    if (pPath) {
        if (!ViewParams::getShowSelectionOnTop()) {
            FC_TRACE("applying action");
            this->slaction.setType(type);
            this->slaction.setColor(master->colorSelection.getValue());
            this->slaction.setElement(det);
            this->slaction.apply(pPath);
            this->slaction.setElement(nullptr);
            FC_TRACE("applied action");
            master->touch();
        }
    }
#endif

    if(detNext) delete detNext;
    return true;
}

void
SoFCUnifiedSelection::handleEvent(SoHandleEventAction * action)
{
    if (selectionRole.getValue())
        pimpl->handleEvent(action);

    inherited::handleEvent(action);
}

void
SoFCUnifiedSelection::Private::handleEvent(SoHandleEventAction * action)
{
    HighlightModes mymode = (HighlightModes) master->highlightMode.getValue();
    const SoEvent * event = action->getEvent();

    bool doPick = false;

    bool shiftDown = event->wasShiftDown();
    if(event->isOfType(SoKeyboardEvent::getClassTypeId())
            && static_cast<const SoKeyboardEvent*>(event)->getKey() == SoKeyboardEvent::LEFT_SHIFT)
    {
        shiftDown = (static_cast<const SoKeyboardEvent*>(event)->getState() == SoKeyboardEvent::DOWN);
    }
    if (shiftDown) {
        if(!pickBackFace) {
            pickBackFace = 1;
            doPick = true;
        }
    } else if (pickBackFace) {
        pickBackFace = 0;
        doPick = true;
    }

    // mouse press events for (de)selection
    if (event->isOfType(SoMouseButtonEvent::getClassTypeId()) && 
             master->selectionMode.getValue() == SoFCUnifiedSelection::ON) {
        const SoMouseButtonEvent* e = static_cast<const SoMouseButtonEvent *>(event);
        if (SoMouseButtonEvent::isButtonReleaseEvent(e,SoMouseButtonEvent::BUTTON1)) {
            // check to see if the mouse is over a geometry...
            auto infos = this->getPickedList(action,!Selection().needPickedList());
            if(setSelection(infos,event->wasCtrlDown(),event->wasShiftDown(),event->wasAltDown()))
                action->setHandled();
        } // mouse release
        else if (shiftDown && event->wasCtrlDown()) {
            if (SoMouseButtonEvent::isButtonPressEvent(e, SoMouseButtonEvent::BUTTON4)) {
                if(pickBackFace == 1)
                    pickBackFace = -1;
                else
                    --pickBackFace;
                doPick = true;
                FC_LOG("back face " << pickBackFace);
            } else if (SoMouseButtonEvent::isButtonPressEvent(e, SoMouseButtonEvent::BUTTON5)) {
                if(pickBackFace == -1)
                    pickBackFace = 1;
                else
                    ++pickBackFace;
                doPick = true;
                FC_LOG("back face " << pickBackFace);
            }
        }
    }

    // If we don't need to pick for locate highlighting,
    // then just behave as separator and return.
    // NOTE: we still have to pick for ON even though we don't have
    // to re-render, because the app needs to be notified as the mouse
    // goes over locate highlight nodes.
    //if (highlightMode.getValue() == OFF) {
    //    inherited::handleEvent( action );
    //    return;
    //}

    //
    // If this is a mouseMotion event, then check for locate highlighting
    //
    if (doPick || event->isOfType(SoLocation2Event::getClassTypeId())) {
        // NOTE: If preselection is off then we do not check for a picked point because otherwise this search may slow
        // down extremely the system on really big data sets. In this case we just check for a picked point if the data
        // set has been selected.
        if (mymode == AUTO || mymode == ON) {
            double delay = ViewParams::instance()->getPreSelectionDelay();

            preselPos = action->getEvent()->getPosition();
            preselViewport = action->getViewportRegion();

            // Rate limit picking action
            if(delay>0.0 && (SbTime::getTimeOfDay()-preselTime).getValue()<delay) {
                if(!preselTimer.isScheduled()) {
                    preselTimer.setInterval(delay);
                    preselTimer.schedule();
                }
            } else {
                onPreselectTimer();
            }
        }
    }
}

static FC_COIN_THREAD_LOCAL bool _ShowBoundBox;

bool SoFCUnifiedSelection::getShowSelectionBoundingBox() {
    return ViewParams::instance()->getShowSelectionBoundingBox()
        || _ShowBoundBox;
}

void SoFCUnifiedSelection::GLRenderInPath(SoGLRenderAction * action)
{
    SoState * state = action->getState();
    state->push();
    pimpl->applyOverrideMode(action->getState());
    inherited::GLRenderInPath(action);
    state->pop();
}

void SoFCUnifiedSelection::GLRenderBelowPath(SoGLRenderAction * action)
{
    SoState *state = action->getState();
    state->push();
    SoGLLazyElement::getInstance(state)->reset(state,
                                               SoLazyElement::BLENDING_MASK);

    bool bbox = _ShowBoundBox;
    if(pimpl->selectAll)
        _ShowBoundBox = true;

    pimpl->applyOverrideMode(action->getState());

    if (!pimpl->render(action))
        inherited::GLRenderBelowPath(action);

    _ShowBoundBox = bbox;

    // nothing picked, so restore the arrow cursor if needed
    if (pimpl->preSelection == 0) {
        // this is called when a selection gate forbade to select an object
        // and the user moved the mouse to an empty area
        pimpl->preSelection = -1;
        QtGLWidget* window;
        SoGLWidgetElement::get(state, window);
        QWidget* parent = window ? window->parentWidget() : 0;
        if (parent) {
            QCursor c = parent->cursor();
            if (c.shape() == Qt::ForbiddenCursor) {
                c.setShape(Qt::ArrowCursor);
                parent->setCursor(c);
            }
        }
    }
    state->pop();
}

// ---------------------------------------------------------------

SO_ACTION_SOURCE(SoHighlightElementAction)

void SoHighlightElementAction::initClass()
{
    SO_ACTION_INIT_CLASS(SoHighlightElementAction,SoAction);

    SO_ENABLE(SoHighlightElementAction, SoSwitchElement);

    SO_ACTION_ADD_METHOD(SoNode,nullAction);

    SO_ENABLE(SoHighlightElementAction, SoCoordinateElement);

    SO_ACTION_ADD_METHOD(SoGroup,callDoAction);
    SO_ACTION_ADD_METHOD(SoIndexedLineSet,callDoAction);
    SO_ACTION_ADD_METHOD(SoIndexedFaceSet,callDoAction);
    SO_ACTION_ADD_METHOD(SoPointSet,callDoAction);
}

SoHighlightElementAction::SoHighlightElementAction () : _highlight(false), _det(0)
{
    SO_ACTION_CONSTRUCTOR(SoHighlightElementAction);
}

SoHighlightElementAction::~SoHighlightElementAction()
{
}

void SoHighlightElementAction::beginTraversal(SoNode *node)
{
    traverse(node);
}

void SoHighlightElementAction::callDoAction(SoAction *action,SoNode *node)
{
    node->doAction(action);
}

void SoHighlightElementAction::setHighlighted(SbBool ok)
{
    this->_highlight = ok;
}

SbBool SoHighlightElementAction::isHighlighted() const
{
    return this->_highlight;
}

void SoHighlightElementAction::setColor(const SbColor& c)
{
    this->_color = c;
}

const SbColor& SoHighlightElementAction::getColor() const
{
    return this->_color;
}

void SoHighlightElementAction::setElement(const SoDetail* det)
{
    this->_det = det;
}

const SoDetail* SoHighlightElementAction::getElement() const
{
    return this->_det;
}

// ---------------------------------------------------------------

SO_ACTION_SOURCE(SoSelectionElementAction)

void SoSelectionElementAction::initClass()
{
    SO_ACTION_INIT_CLASS(SoSelectionElementAction,SoAction);

    SO_ENABLE(SoSelectionElementAction, SoSwitchElement);

    SO_ACTION_ADD_METHOD(SoNode,nullAction);

    SO_ENABLE(SoSelectionElementAction, SoCoordinateElement);

    SO_ACTION_ADD_METHOD(SoCoordinate3,callDoAction);
    SO_ACTION_ADD_METHOD(SoGroup,callDoAction);
    SO_ACTION_ADD_METHOD(SoIndexedLineSet,callDoAction);
    SO_ACTION_ADD_METHOD(SoIndexedFaceSet,callDoAction);
    SO_ACTION_ADD_METHOD(SoPointSet,callDoAction);
}

SoSelectionElementAction::SoSelectionElementAction (Type t, bool secondary)
    : _type(t), _det(0), _secondary(secondary)
{
    SO_ACTION_CONSTRUCTOR(SoSelectionElementAction);
}

SoSelectionElementAction::~SoSelectionElementAction()
{
}

void SoSelectionElementAction::beginTraversal(SoNode *node)
{
    traverse(node);
}

void SoSelectionElementAction::callDoAction(SoAction *action,SoNode *node)
{
    node->doAction(action);
}

SoSelectionElementAction::Type
SoSelectionElementAction::getType() const
{
    return this->_type;
}

void SoSelectionElementAction::setColor(const SbColor& c)
{
    this->_color = c;
}

const SbColor& SoSelectionElementAction::getColor() const
{
    return this->_color;
}

void SoSelectionElementAction::setElement(const SoDetail* det)
{
    this->_det = det;
}

const SoDetail* SoSelectionElementAction::getElement() const
{
    return this->_det;
}

// ---------------------------------------------------------------

SO_ACTION_SOURCE(SoVRMLAction)

void SoVRMLAction::initClass()
{
    SO_ACTION_INIT_CLASS(SoVRMLAction,SoAction);

    SO_ENABLE(SoVRMLAction, SoSwitchElement);

    SO_ACTION_ADD_METHOD(SoNode,nullAction);

    SO_ENABLE(SoVRMLAction, SoCoordinateElement);
    SO_ENABLE(SoVRMLAction, SoMaterialBindingElement);
    SO_ENABLE(SoVRMLAction, SoLazyElement);
    SO_ENABLE(SoVRMLAction, SoShapeStyleElement);

    SO_ACTION_ADD_METHOD(SoCoordinate3,callDoAction);
    SO_ACTION_ADD_METHOD(SoMaterialBinding,callDoAction);
    SO_ACTION_ADD_METHOD(SoMaterial,callDoAction);
    SO_ACTION_ADD_METHOD(SoNormalBinding,callDoAction);
    SO_ACTION_ADD_METHOD(SoGroup,callDoAction);
    SO_ACTION_ADD_METHOD(SoIndexedLineSet,callDoAction);
    SO_ACTION_ADD_METHOD(SoIndexedFaceSet,callDoAction);
    SO_ACTION_ADD_METHOD(SoPointSet,callDoAction);
}

SoVRMLAction::SoVRMLAction() : overrideMode(true)
{
    SO_ACTION_CONSTRUCTOR(SoVRMLAction);
}

SoVRMLAction::~SoVRMLAction()
{
}

void SoVRMLAction::setOverrideMode(SbBool on)
{
    overrideMode = on;
}

SbBool SoVRMLAction::isOverrideMode() const
{
    return overrideMode;
}

void SoVRMLAction::callDoAction(SoAction *action, SoNode *node)
{
    if (node->getTypeId().isDerivedFrom(SoNormalBinding::getClassTypeId()) && action->isOfType(SoVRMLAction::getClassTypeId())) {
        SoVRMLAction* vrmlAction = static_cast<SoVRMLAction*>(action);
        if (vrmlAction->overrideMode) {
            SoNormalBinding* bind = static_cast<SoNormalBinding*>(node);
            vrmlAction->bindList.push_back(bind->value.getValue());
            // this normal binding causes some problems for the part view provider
            // See also #0002222: Number of normals in exported VRML is wrong
            if (bind->value.getValue() == static_cast<int>(SoNormalBinding::PER_VERTEX_INDEXED))
                bind->value = SoNormalBinding::OVERALL;
        }
        else if (!vrmlAction->bindList.empty()) {
            static_cast<SoNormalBinding*>(node)->value = static_cast<SoNormalBinding::Binding>(vrmlAction->bindList.front());
            vrmlAction->bindList.pop_front();
        }
    }

    node->doAction(action);
}

// ---------------------------------------------------------------------------------

bool SoFCSelectionRoot::StackComp::operator()(const Stack &a, const Stack &b) const {
    if(a.size()-a.offset < b.size()-b.offset) 
        return true;
    if(a.size()-a.offset > b.size()-b.offset) 
        return false;
    auto it1=a.rbegin(), end1=a.rend()-a.offset; 
    auto it2=b.rbegin();
    for(;it1!=end1;++it1,++it2) {
        if(*it1 < *it2) 
            return true;
        if(*it1 > *it2) 
            return false;
    }
    return false;
}

// ---------------------------------------------------------------------------------
SoSeparator::CacheEnabled SoFCSeparator::CacheMode = SoSeparator::AUTO;
SO_NODE_SOURCE(SoFCSeparator)

SoFCSeparator::SoFCSeparator(bool trackCacheMode)
    :trackCacheMode(trackCacheMode)
{
    SO_NODE_CONSTRUCTOR(SoFCSeparator);
    if(!trackCacheMode) {
        renderCaching = SoSeparator::OFF;
        boundingBoxCaching = SoSeparator::OFF;
    } else
        boundingBoxCaching = SoSeparator::ON;
}

void SoFCSeparator::GLRenderBelowPath(SoGLRenderAction * action) {
    if(trackCacheMode && renderCaching.getValue()!=CacheMode)
        renderCaching = CacheMode;
    inherited::GLRenderBelowPath(action);
}

void SoFCSeparator::initClass(void)
{
    SO_NODE_INIT_CLASS(SoFCSeparator,SoSeparator,"FCSeparator");
}

void SoFCSeparator::finish()
{
    atexit_cleanup();
}

// ---------------------------------------------------------------------------------
// Thread local data for bounding box rendering
//
// The code is inspred by Coin SoLevelOfDetails.cpp.
typedef struct {
    SoGetBoundingBoxAction * bboxaction;
    SoCube *cube;
} SoFCBBoxRenderInfo;

static void so_bbox_construct_data(void * closure)
{
    SoFCBBoxRenderInfo * data = (SoFCBBoxRenderInfo*) closure;
    data->bboxaction = NULL;
    data->cube = NULL;
}

static void so_bbox_destruct_data(void * closure)
{
    SoFCBBoxRenderInfo * data = (SoFCBBoxRenderInfo*) closure;
    delete data->bboxaction;
    if(data->cube)
        data->cube->unref();
}

static SbStorage * so_bbox_storage = NULL;

// called from atexit
static void so_bbox_cleanup(void)
{
    delete so_bbox_storage;
}

// ---------------------------------------------------------------------------------

SoFCSelectionRoot::Stack SoFCSelectionRoot::SelStack;
std::unordered_map<SoAction*,SoFCSelectionRoot::Stack> SoFCSelectionRoot::ActionStacks;
SoFCSelectionRoot::ColorStack SoFCSelectionRoot::SelColorStack;
SoFCSelectionRoot::ColorStack SoFCSelectionRoot::HlColorStack;
SoFCSelectionRoot* SoFCSelectionRoot::ShapeColorNode;

SO_NODE_SOURCE(SoFCSelectionRoot)

SoFCSelectionRoot::SoFCSelectionRoot(bool trackCacheMode, ViewProvider *vp)
    :SoFCSeparator(trackCacheMode), viewProvider(vp)
{
    SO_NODE_CONSTRUCTOR(SoFCSelectionRoot);
    SO_NODE_ADD_FIELD(selectionStyle,(Full));
    SO_NODE_DEFINE_ENUM_VALUE(SelectStyles, Full);
    SO_NODE_DEFINE_ENUM_VALUE(SelectStyles, Box);
    SO_NODE_DEFINE_ENUM_VALUE(SelectStyles, PassThrough);
    SO_NODE_DEFINE_ENUM_VALUE(SelectStyles, Unpickable);
    SO_NODE_SET_SF_ENUM_TYPE(selectionStyle, SelectStyles);
}

SoFCSelectionRoot::~SoFCSelectionRoot()
{
}

void SoFCSelectionRoot::initClass(void)
{
    SO_NODE_INIT_CLASS(SoFCSelectionRoot,SoFCSeparator,"FCSelectionRoot");

    so_bbox_storage = new SbStorage(sizeof(SoFCBBoxRenderInfo),
            so_bbox_construct_data, so_bbox_destruct_data);

    // cc_coin_atexit((coin_atexit_f*) so_bbox_cleanup);
}

void SoFCSelectionRoot::finish()
{
    so_bbox_cleanup();
    atexit_cleanup();
}

void SoFCSelectionRoot::setViewProvider(ViewProvider *vp) {
    viewProvider = vp;
}

SoFCSelectionRoot *SoFCSelectionRoot::getCurrentRoot(bool front, SoFCSelectionRoot *def) {
    if(SelStack.size()) 
        return static_cast<SoFCSelectionRoot*>(front?SelStack.front():SelStack.back());
    return def;
}

SoFCSelectionRoot *SoFCSelectionRoot::getCurrentActionRoot(
        SoAction *action, bool front, SoFCSelectionRoot *def) 
{
    auto it = ActionStacks.find(action);
    if(it == ActionStacks.end() || it->second.empty())
        return def;
    return static_cast<SoFCSelectionRoot*>(front?it->second.front():it->second.back());
}

int SoFCSelectionRoot::getRenderPathCode() const {
    return renderPathCode - 1;
}

SoFCSelectionContextBasePtr SoFCSelectionRoot::getNodeContext(
        Stack &stack, SoNode *node, SoFCSelectionContextBasePtr def)
{
    if(stack.empty())
        return def;

    SoFCSelectionRoot *front = stack.front();

    // NOTE: _node is not necessary of type SoFCSelectionRoot, but it is safe
    // here since we only use it as searching key, although it is probably not
    // a best practice.
    stack.front() = static_cast<SoFCSelectionRoot*>(node);

    auto it = front->contextMap.find(stack);
    stack.front() = front;
    if(it!=front->contextMap.end()) 
        return it->second;
    return SoFCSelectionContextBasePtr();
}

SoFCSelectionContextBasePtr 
SoFCSelectionRoot::getNodeContext2(Stack &stack, SoNode *node, SoFCSelectionContextBase::MergeFunc *merge) 
{
    SoFCSelectionContextBasePtr ret;
    if(stack.empty() || stack.back()->contextMap2.empty())
        return ret;

    int status = 0;
    auto *back = stack.back();
    auto &map = back->contextMap2;
    stack.back() = static_cast<SoFCSelectionRoot*>(node);
    for(stack.offset=0;stack.offset<stack.size();++stack.offset) {
        auto it = map.find(stack);
        SoFCSelectionContextBasePtr ctx;
        if(it!=map.end())
            ctx = it->second;
        status = merge(status,ret,ctx,stack.offset==stack.size()-1?0:stack[stack.offset]);
        if(status<0)
            break;
    }
    stack.offset = 0;
    stack.back() = back;
    return ret;
}

std::pair<bool,SoFCSelectionContextBasePtr*> SoFCSelectionRoot::findActionContext(
        SoAction *action, SoNode *_node, bool create, bool erase)
{
    std::pair<bool,SoFCSelectionContextBasePtr*> res(false,0);
    if(action->isOfType(SoSelectionElementAction::getClassTypeId()))
        res.first = static_cast<SoSelectionElementAction*>(action)->isSecondary();

    auto it = ActionStacks.find(action);
    if(it==ActionStacks.end() || it->second.empty())
        return res;

    auto &stack = it->second;
    
    auto node = static_cast<SoFCSelectionRoot*>(_node);

    if(res.first) {
        auto back = stack.back();
        stack.back() = node;
        if(create)
            res.second = &back->contextMap2[stack];
        else {
            auto it = back->contextMap2.find(stack);
            if(it!=back->contextMap2.end()) {
                res.second = &it->second;
                if(erase)
                    back->contextMap2.erase(it);
            }
        }
        stack.back() = back;
    }else{
        auto front = stack.front();
        stack.front() = node;
        if(create) 
            res.second = &front->contextMap[stack];
        else {
            auto it = front->contextMap.find(stack);
            if(it!=front->contextMap.end()) {
                res.second = &it->second;
                if(erase)
                    front->contextMap.erase(it);
            }
        }
        stack.front() = front;
    }
    return res;
}

void SoFCSelectionRoot::setupSelectionLineRendering(
        SoState *state, SoNode *node, const uint32_t *color)
{
    float width = SoLineWidthElement::get(state);
    if(width < 1.0)
        width = 1.0;

    if(Gui::ViewParams::instance()->getSelectionLineThicken()>1.0)
        width *= Gui::ViewParams::instance()->getSelectionLineThicken();

    SoShadowStyleElement::set(state, SoShadowStyleElement::NO_SHADOWING);

    SoLineWidthElement::set(state,width);

    SoLightModelElement::set(state,SoLightModelElement::BASE_COLOR);
    SoOverrideElement::setLightModelOverride(state, node, TRUE);
    SoMaterialBindingElement::set(state,SoMaterialBindingElement::OVERALL);
    SoOverrideElement::setMaterialBindingOverride(state, node, TRUE);

    SoLazyElement::setPacked(state, node, 1, color, false);
}

bool SoFCSelectionRoot::renderBBox(SoGLRenderAction *action, SoNode *node,
        const SbColor &color, const SbMatrix *mat, bool force)
{
    auto data = (SoFCBBoxRenderInfo*) so_bbox_storage->get();
    if (data->bboxaction == NULL) {
        // The viewport region will be replaced every time the action is
        // used, so we can just feed it a dummy here.
        data->bboxaction = new SoGetBoundingBoxAction(SbViewportRegion());
    }

    auto state = action->getState();

    if(!force && !action->isRenderingDelayedPaths()
            && ViewParams::instance()->getShowSelectionOnTop())
        return false;

    data->bboxaction->setViewportRegion(action->getViewportRegion());
    SoSwitchElement::set(data->bboxaction->getState(), SoSwitchElement::get(action->getState()));

    bool project = !mat && ViewParams::instance()->getRenderProjectedBBox();
    if(project || !node->isOfType(SoGroup::getClassTypeId()))
        data->bboxaction->apply(node);
    else {
        SoTempPath resetPath(2);
        resetPath.ref();
        auto group = static_cast<SoGroup*>(node);
        for(int i=0,count=group->getNumChildren();i<count;++i) {
            auto child = group->getChild(i);
            if(child->isOfType(SoTransform::getClassTypeId())) {
                resetPath.append(group);
                resetPath.append(child);
                data->bboxaction->setResetPath(&resetPath,false);
                break;
            }
        }
        data->bboxaction->apply(node);
        data->bboxaction->setResetPath(0);
        resetPath.unrefNoDelete();
    }
    
    SbXfBox3f xbbox = data->bboxaction->getXfBoundingBox();
    if(xbbox.isEmpty())
        return false;

    if(project)
        xbbox.transform(SoModelMatrixElement::get(state));
    renderBBox(action,node,xbbox.project(),color,mat);
    return true;
}

bool SoFCSelectionRoot::renderBBox(SoGLRenderAction *action, SoNode *node, 
        const SbBox3f &bbox, SbColor color, const SbMatrix *mat) 
{
    auto data = (SoFCBBoxRenderInfo*) so_bbox_storage->get();
    if (data->cube == NULL) {
        data->cube = new SoCube;
        data->cube->ref();
    }

    SoState *state = action->getState();
    state->push();

    if(mat) {
        SoModelMatrixElement::mult(state, node, *mat);
    } else if(ViewParams::instance()->getRenderProjectedBBox()) {
        // reset model matrix, since we will transform and project the bounding box
        // by ourself, so that it is always rendered to be aligned with the global
        // axes regardless of the current model matrix.
        SoModelMatrixElement::makeIdentity(state,node);

    } else if(node->isOfType(SoGroup::getClassTypeId())) {
        // if not, then search for the transform node and setup the model matrix
        auto group = static_cast<SoGroup*>(node);
        for(int i=0,count=group->getNumChildren();i<count;++i) {
            auto child = group->getChild(i);
            if(child->isOfType(SoTransform::getClassTypeId())) {
                SbMatrix matrix;
                auto transform = static_cast<SoTransform*>(child);
                matrix.setTransform(transform->translation.getValue(),
                                    transform->rotation.getValue(),
                                    transform->scaleFactor.getValue(),
                                    transform->scaleOrientation.getValue(),
                                    transform->center.getValue());
                SoModelMatrixElement::mult(state, node, matrix);
                break;
            }
        }
    }

    uint32_t packed = color.getPackedValue(0.0);
    setupSelectionLineRendering(state,node,&packed);

    SoDrawStyleElement::set(state,SoDrawStyleElement::LINES);
    SoLineWidthElement::set(state,ViewParams::instance()->getSelectionBBoxLineWidth());

    float x, y, z;
    bbox.getSize(x, y, z);
    data->cube->width  = x;
    data->cube->height  = y;
    data->cube->depth = z;

    SoModelMatrixElement::translateBy(state,node,bbox.getCenter());

    SoMaterialBundle mb(action);
    mb.sendFirst();

    FCDepthFunc guard;
    GLboolean clamped = true;

    clamped = glIsEnabled(GL_DEPTH_CLAMP);
    if(!clamped)
        glEnable(GL_DEPTH_CLAMP);

    if(!action->isRenderingDelayedPaths())
        guard.set(GL_LEQUAL);

    data->cube->GLRender(action);

    if(!clamped)
        glDisable(GL_DEPTH_CLAMP);

    state->pop();
    return true;
}

static std::time_t _CyclicLastReported;

struct SelectionRootPathCode {
    SelectionRootPathCode(SoAction *action, int &code)
        :code(code)
    {
        code = action->getCurPathCode() + 1;
    }

    ~SelectionRootPathCode() {
        code = 0;
    }

    int &code;
};

void SoFCSelectionRoot::renderPrivate(SoGLRenderAction * action, bool inPath) {
    if(renderPathCode) {
        std::time_t t = std::time(0);
        if(_CyclicLastReported < t) {
            _CyclicLastReported = t+5;
            FC_ERR("Cyclic scene graph: " << getName());
        }
        return;
    }
    SelectionRootPathCode guard(action,renderPathCode);

    auto state = action->getState();
    bool pushed = false;
    SelStack.push_back(this);
    if(_renderPrivate(action,inPath,pushed)) {
        if(inPath)
            inherited::GLRenderInPath(action);
        else
            inherited::GLRenderBelowPath(action);
    }
    if(pushed)
        state->pop();
    SelStack.pop_back();
}

bool SoFCSelectionRoot::_renderPrivate(SoGLRenderAction * action, bool inPath, bool &pushed) {

    auto state = action->getState();
    selCounter.checkCache(state,true);

    if(!SoFCSwitch::testTraverseState(SoFCSwitch::TraverseOverride)
            || action->getCurPathCode()!=SoAction::IN_PATH)
    {
        auto ctx2 = std::static_pointer_cast<SelContext>(getNodeContext2(SelStack,this,SelContext::merge));
        if(ctx2 && ctx2->hideAll)
            return false;
    }

    SelContextPtr ctx = getRenderContext<SelContext>(this);

    int style = selectionStyle.getValue();
    if((style==SoFCSelectionRoot::Box || SoFCUnifiedSelection::getShowSelectionBoundingBox())
       && ctx && !ctx->hideAll && (ctx->selAll || ctx->hlAll)) 
    {
        if (style==SoFCSelectionRoot::PassThrough) {
            style = SoFCSelectionRoot::Box;
        } else {
            if(!SoFCSwitch::testTraverseState(SoFCSwitch::TraverseInvisible)) {
                if(inPath)
                    inherited::GLRenderInPath(action);
                else
                    inherited::GLRenderBelowPath(action);
            }

            if(_ShowBoundBox || !ViewParams::instance()->getShowSelectionOnTop()) {
                SoCacheElement::invalidate(state);
                if(ViewParams::instance()->getUseTightBoundingBox() && viewProvider) {
                    Base::Matrix4D mat;
                    bool project = ViewParams::instance()->getRenderProjectedBBox();
                    if(project)
                        mat = ViewProvider::convert(SoModelMatrixElement::get(state));
                    auto fcbox = viewProvider->getBoundingBox(0,&mat,project);
                    SbBox3f bbox(fcbox.MinX,fcbox.MinY,fcbox.MinZ,
                                fcbox.MaxX,fcbox.MaxY,fcbox.MaxZ);
                    renderBBox(action,this,bbox,ctx->hlAll?ctx->hlColor:ctx->selColor);
                } else
                    renderBBox(action,this,ctx->hlAll?ctx->hlColor:ctx->selColor);
            }
            return false;
        }
    }

    // Here, we are not setting (pre)selection color override here.
    // Instead, we are checking and setting up for any secondary context
    // color override.
    //
    // When the current selection style is full highlight, we should ignore any
    // secondary override. If the style is bounding box, however, we should
    // honour the secondary color override.

    bool colorPushed = false;
    if(!ShapeColorNode && overrideColor && 
        !SoOverrideElement::getDiffuseColorOverride(state) &&
        (style==SoFCSelectionRoot::Box || !ctx || (!ctx->selAll && !ctx->hideAll)))
    {
        ShapeColorNode = this;
        colorPushed = true;
        if(!pushed) {
            pushed = true;
            state->push();
        }
        auto &packer = ShapeColorNode->shapeColorPacker;
        auto &trans = ShapeColorNode->transOverride;
        auto &color = ShapeColorNode->colorOverride;
        if(!SoOverrideElement::getTransparencyOverride(state) && trans) {
            SoLazyElement::setTransparency(state, ShapeColorNode, 1, &trans, &packer);
            SoOverrideElement::setTransparencyOverride(state,ShapeColorNode,true);
        }
        SoLazyElement::setDiffuse(state, ShapeColorNode, 1, &color, &packer);
        SoOverrideElement::setDiffuseColorOverride(state,ShapeColorNode,true);
        SoMaterialBindingElement::set(state, ShapeColorNode, SoMaterialBindingElement::OVERALL);
        SoOverrideElement::setMaterialBindingOverride(state,ShapeColorNode,true);

        SoTextureEnabledElement::set(state,ShapeColorNode,false);
    }

    if(!ctx) {
        if(inPath)
            inherited::GLRenderInPath(action);
        else
            inherited::GLRenderBelowPath(action);
    } else {
        bool selPushed;
        bool hlPushed;
        if((hlPushed = ctx->hlAll)) 
            HlColorStack.push_back(ctx->hlColor);
        if((selPushed = ctx->selAll))
            SelColorStack.push_back(ctx->selColor);

        if(!ViewParams::instance()->getShowSelectionOnTop() 
                && selPushed
                && style != SoFCSelectionRoot::Box)
        {
            if(!pushed) {
                pushed = true;
                state->push();
            }
            // Setting color override here is essential for proper caching
            auto &color = SelColorStack.back();
            SoLazyElement::setEmissive(state, &color);
            SoOverrideElement::setEmissiveColorOverride(state,this,true);
            if (SoLazyElement::getLightModel(state) == SoLazyElement::BASE_COLOR) {
                auto &packer = shapeColorPacker;
                SoLazyElement::setDiffuse(state, this, 1, &color, &packer);
                SoOverrideElement::setDiffuseColorOverride(state,this,true);
                SoMaterialBindingElement::set(state, this, SoMaterialBindingElement::OVERALL);
                SoOverrideElement::setMaterialBindingOverride(state,this,true);
            }
        }

        if(inPath)
            inherited::GLRenderInPath(action);
        else
            inherited::GLRenderBelowPath(action);

        if(selPushed) 
            SelColorStack.pop_back();
        if(hlPushed)
            HlColorStack.pop_back();
    }

    if(colorPushed) 
        ShapeColorNode = 0;

    return false;
}

void SoFCSelectionRoot::GLRenderBelowPath(SoGLRenderAction * action) {
    renderPrivate(action,false);
}

void SoFCSelectionRoot::GLRenderInPath(SoGLRenderAction * action) {
    if(action->getCurPathCode() == SoAction::BELOW_PATH)
        return GLRenderBelowPath(action);
    renderPrivate(action,true);
}

bool SoFCSelectionRoot::checkColorOverride(SoState *state) {
    if(ShapeColorNode) {
        if(!SoOverrideElement::getDiffuseColorOverride(state)) {
            state->push();
            auto &packer = ShapeColorNode->shapeColorPacker;
            auto &trans = ShapeColorNode->transOverride;
            auto &color = ShapeColorNode->colorOverride;
            if(!SoOverrideElement::getTransparencyOverride(state) && trans) {
                SoLazyElement::setTransparency(state, ShapeColorNode, 1, &trans, &packer);
                SoOverrideElement::setTransparencyOverride(state,ShapeColorNode,true);
            }
            SoLazyElement::setDiffuse(state, ShapeColorNode, 1, &color, &packer);
            SoOverrideElement::setDiffuseColorOverride(state,ShapeColorNode,true);
            SoMaterialBindingElement::set(state, ShapeColorNode, SoMaterialBindingElement::OVERALL);
            SoOverrideElement::setMaterialBindingOverride(state,ShapeColorNode,true);

            SoTextureEnabledElement::set(state,ShapeColorNode,false);
            return true;
        }
    }
    return false;
}

void SoFCSelectionRoot::checkSelection(bool &sel, SbColor &selColor, bool &hl, SbColor &hlColor) {
    sel = false;
    hl = false;
    if((sel = !SelColorStack.empty()))
        selColor = SelColorStack.back();
    if((hl = !HlColorStack.empty()))
        hlColor = HlColorStack.back();
}

void SoFCSelectionRoot::resetContext() {
    contextMap.clear();
}

void SoFCSelectionRoot::moveActionStack(SoAction *from, SoAction *to, bool erase) {
    auto it = ActionStacks.find(from);
    if(it == ActionStacks.end())
        return;
    auto &stack = ActionStacks[to];
    assert(stack.empty());
    stack.swap(it->second);
    if(erase)
        ActionStacks.erase(it);
}

#define BEGIN_ACTION \
    auto &stack = ActionStacks[action];\
    if(ViewParams::instance()->getCoinCycleCheck() \
        && !stack.nodeSet.insert(this).second) \
    {\
        std::time_t t = std::time(0);\
        if(_CyclicLastReported < t) {\
            _CyclicLastReported = t+5;\
            FC_ERR("Cyclic scene graph: " << getName());\
        }\
        return;\
    }\
    stack.push_back(this);\
    auto size = stack.size();

#define END_ACTION \
    if(stack.size()!=size || stack.back()!=this)\
        FC_ERR("action stack fault");\
    else {\
        stack.nodeSet.erase(this);\
        stack.pop_back();\
        if(stack.empty())\
            ActionStacks.erase(action);\
    }

void SoFCSelectionRoot::pick(SoPickAction * action) {
    BEGIN_ACTION;
    if(doActionPrivate(stack,action))
        inherited::pick(action);
    END_ACTION;
}

void SoFCSelectionRoot::rayPick(SoRayPickAction * action) {
    if(selectionStyle.getValue() == Unpickable)
        return;
    BEGIN_ACTION;
    if(doActionPrivate(stack,action)) {
        if(action->getCurPathCode() == SoAction::IN_PATH) {
            // skip cached bounding box cull test when traverse in path
            inherited::doAction(action);
        } else
            inherited::rayPick(action);
    }
    END_ACTION;
}

void SoFCSelectionRoot::handleEvent(SoHandleEventAction * action) {
    BEGIN_ACTION;
    inherited::handleEvent(action);
    END_ACTION;
}

void SoFCSelectionRoot::search(SoSearchAction * action) {
    BEGIN_ACTION;
    inherited::search(action);
    END_ACTION;
}

void SoFCSelectionRoot::getPrimitiveCount(SoGetPrimitiveCountAction * action) {
    BEGIN_ACTION;
    inherited::getPrimitiveCount(action);
    END_ACTION;
}

void SoFCSelectionRoot::getBoundingBox(SoGetBoundingBoxAction * action)
{
    BEGIN_ACTION;
    if(doActionPrivate(stack,action)) {
        selCounter.checkCache(action->getState(),true);
        inherited::getBoundingBox(action);
    }
    END_ACTION;
}

void SoFCSelectionRoot::getMatrix(SoGetMatrixAction * action) {
    BEGIN_ACTION;
    if(doActionPrivate(stack,action))
        inherited::getMatrix(action);
    END_ACTION;
}

void SoFCSelectionRoot::callback(SoCallbackAction *action) {
    BEGIN_ACTION;
    inherited::callback(action);
    END_ACTION;
}

void SoFCSelectionRoot::doAction(SoAction *action) {
    if(selectionStyle.getValue() == Unpickable) {
        if(action->isOfType(SoSelectionElementAction::getClassTypeId())
                && !static_cast<SoSelectionElementAction*>(action)->isSecondary())
            return;
        if(action->isOfType(SoHighlightElementAction::getClassTypeId()))
            return;
    }
    BEGIN_ACTION
    if(doActionPrivate(stack,action))
        inherited::doAction(action);
    END_ACTION
}

bool SoFCSelectionRoot::doActionPrivate(Stack &stack, SoAction *action) {
    // Selection action short-circuit optimization. In case of whole object
    // selection/pre-selection, we shall store a SelContext keyed by ourself.
    // And the action traversal can be short-curcuited once the first targeted
    // SoFCSelectionRoot is found here. New function checkSelection() is exposed
    // to check for whole object selection. This greatly improve performance on
    // large group.

    SelContextPtr ctx2;
    bool ctx2Searched = false;
    bool isTail = false;
    if(action->getCurPathCode()==SoAction::IN_PATH) {
        auto path = action->getPathAppliedTo();
        if(path) {
            isTail = path->getTail()==this || 
                     (path->getLength()>1 
                      && path->getNodeFromTail(1)==this
                      && path->getTail()->isOfType(SoSwitch::getClassTypeId()));
        }

        if(!action->isOfType(SoSelectionElementAction::getClassTypeId())
                && !SoFCSwitch::testTraverseState(SoFCSwitch::TraverseOverride))
        {
            ctx2Searched = true;
            ctx2 = std::static_pointer_cast<SelContext>(getNodeContext2(stack,this,SelContext::merge));
            if(ctx2 && ctx2->hideAll)
                return false;
        }
        if(!isTail)
            return true;
    }else if(action->getWhatAppliedTo()!=SoAction::NODE && action->getCurPathCode()!=SoAction::BELOW_PATH)
        return true;

    if(action->isOfType(SoSelectionElementAction::getClassTypeId())) {
        auto selAction = static_cast<SoSelectionElementAction*>(action);

        if(selAction->isSecondary()) {
            if(selAction->getType() == SoSelectionElementAction::Show ||
               (selAction->getType() == SoSelectionElementAction::Color && 
                selAction->getColors().empty() &&
                action->getWhatAppliedTo()==SoAction::NODE))
            {
                auto ctx = getActionContext(action,this,SelContextPtr(),false);
                selCounter.checkAction(selAction,ctx);
                if(ctx && ctx->hideAll) {
                    ctx->hideAll = false;
                    if(!ctx->hlAll && !ctx->selAll)
                        removeActionContext(action,this);
                    touch();
                }
                // applied to a node means clear all visibility setting, so
                // return true to propgate the action
                return selAction->getType()==SoSelectionElementAction::Color ||
                       action->getWhatAppliedTo()==SoAction::NODE;

            }else if(selAction->getType() == SoSelectionElementAction::Hide) {
                if(action->getCurPathCode()==SoAction::BELOW_PATH || isTail) {
                    auto ctx = getActionContext(action,this,SelContextPtr());
                    selCounter.checkAction(selAction,ctx);
                    if(ctx && !ctx->hideAll) {
                        ctx->hideAll = true;
                        touch();
                    }
                    return false;
                }
            }
            return true;
        } 
        
        if(selAction->getType() == SoSelectionElementAction::None) {
            if(action->getWhatAppliedTo() == SoAction::NODE) {
                // Here the 'select none' action is applied to a node, and we
                // are the first SoFCSelectionRoot encountered (which means all
                // children stores selection context here, both whole object
                // and element selection), then we can simply perform the
                // action by clearing the selection context here, and save the
                // time for traversing a potentially large amount of children
                // nodes.
                resetContext();
                touch();
                return false;
            }

            auto ctx = getActionContext(action,this,SelContextPtr(),false);
            selCounter.checkAction(selAction,ctx);
            if(ctx && ctx->selAll) {
                ctx->selAll = false;
                touch();
                return false;
            }
        } else if(selAction->getType() == SoSelectionElementAction::All) {
            auto ctx = getActionContext(action,this,SelContextPtr());
            assert(ctx);
            selCounter.checkAction(selAction,ctx);
            ctx->selAll = true;
            ctx->selColor = selAction->getColor();
            touch();
            return false;
        }
        return true;
    }

    if(action->isOfType(SoHighlightElementAction::getClassTypeId())) {
        auto hlAction = static_cast<SoHighlightElementAction*>(action);
        if(hlAction->isHighlighted()) {
            selCounter.checkAction(hlAction);
            if(hlAction->getElement()) {
                auto ctx = getActionContext(action,this,SelContextPtr(),false);
                if(ctx && ctx->hlAll) {
                    ctx->hlAll = false;
                    touch();
                }
            } else {
                auto ctx = getActionContext(action,this,SelContextPtr());
                assert(ctx);
                ctx->hlAll = true;
                ctx->hlColor = hlAction->getColor();
                touch();
                return false;
            }
        } else {
            auto ctx = getActionContext(action,this,SelContextPtr(),false);
            if(ctx && ctx->hlAll) {
                ctx->hlAll = false;
                touch();
                return false;
            }
        }
        return true;
    }

    if(!ctx2Searched
            && (action->getCurPathCode() != SoAction::IN_PATH
                || !SoFCSwitch::testTraverseState(SoFCSwitch::TraverseOverride)))
    {
        ctx2 = std::static_pointer_cast<SelContext>(getNodeContext2(stack,this,SelContext::merge));
        if(ctx2 && ctx2->hideAll)
            return false;
    }
    return true;
}

int SoFCSelectionRoot::SelContext::merge(int status, SoFCSelectionContextBasePtr &output, 
        SoFCSelectionContextBasePtr input, SoFCSelectionRoot *)
{
    auto ctx = std::dynamic_pointer_cast<SelContext>(input);
    if(ctx && ctx->hideAll) {
        output = ctx;
        return -1;
    }
    return status;
}

bool SoFCSelectionRoot::handleSelectionAction(SoAction *action,
                                              SoNode *node,
                                              SoFCDetail::Type detailType, 
                                              SoFCSelectionContextExPtr selContext,
                                              SoFCSelectionCounter &selCounter)
{
    if (action->getTypeId() == SoHighlightElementAction::getClassTypeId()) {
        SoHighlightElementAction* hlaction = static_cast<SoHighlightElementAction*>(action);
        selCounter.checkAction(hlaction);
        if (!hlaction->isHighlighted()) {
            auto ctx = getActionContext(action,node,selContext,false);
            if(ctx) {
                ctx->removeHighlight();
                node->touch();
            }
            return true;
        }

        const SoDetail* detail = hlaction->getElement();
        if (!detail) {
            auto ctx = getActionContext(action,node,selContext);
            ctx->highlightAll();
            ctx->highlightColor = hlaction->getColor();
            node->touch();
        } else if (detail->isOfType(SoFCDetail::getClassTypeId())) {
            const auto &indices = static_cast<const SoFCDetail*>(detail)->getIndices(detailType);

            auto ctx = getActionContext(action,node,selContext,!indices.empty());

            if(ctx && ctx->highlightIndex != indices) {
                ctx->highlightColor = hlaction->getColor();
                ctx->highlightIndex = indices;
                node->touch();
            }
        } else {
            int index = -1;
            switch(detailType) {
            case SoFCDetail::Face:
                if (detail->isOfType(SoFaceDetail::getClassTypeId()))
                    index = static_cast<const SoFaceDetail*>(detail)->getPartIndex();
                break;
            case SoFCDetail::Edge:
                if (detail->isOfType(SoLineDetail::getClassTypeId()))
                    index = static_cast<const SoLineDetail*>(detail)->getLineIndex();
                break;
            case SoFCDetail::Vertex:
                if (detail->isOfType(SoPointDetail::getClassTypeId()))
                    index = static_cast<const SoPointDetail*>(detail)->getCoordinateIndex();
                break;
            default:
                break;
            }
            if(index>=0) {
                auto ctx = getActionContext(action,node,selContext);
                ctx->highlightColor = hlaction->getColor();
                ctx->highlightIndex.clear();
                ctx->highlightIndex.insert(index);
                node->touch();
            } else {
                auto ctx = getActionContext(action,node,selContext,false);
                if(ctx && ctx->isHighlighted()) {
                    ctx->removeHighlight();
                    node->touch();
                }
            }
        }
        return true;
    }

    if (action->getTypeId() != SoSelectionElementAction::getClassTypeId())
        return false;

    SoSelectionElementAction* selaction = static_cast<SoSelectionElementAction*>(action);
    switch(selaction->getType()) {
    case SoSelectionElementAction::All: {
        auto ctx = getActionContext(action,node,selContext);
        selCounter.checkAction(selaction,ctx);
        ctx->selectionColor = selaction->getColor();
        ctx->selectionIndex.clear();
        ctx->selectionIndex.emplace(-1,0);
        node->touch();
        break;
    } case SoSelectionElementAction::None:
        if(selaction->isSecondary()) {
            if(removeActionContext(action,node))
                node->touch();
        }else {
            auto ctx = getActionContext(action,node,selContext,false);
            if(ctx) {
                ctx->selectionIndex.clear();
                ctx->colors.clear();
                node->touch();
            }
        }
        break;
    case SoSelectionElementAction::Color:
        if(selaction->isSecondary() && detailType == SoFCDetail::Face) {
            const auto &colors = selaction->getColors();
            auto ctx = getActionContext(action,node,selContext,false);
            if(colors.empty()) {
                if(ctx) {
                    ctx->colors.clear();
                    if(ctx->isSelectAll())
                        removeActionContext(action,node);
                    node->touch();
                }
                return true;
            }
            static std::string element("Face");
            if(colors.begin()->first.empty() || colors.lower_bound(element)!=colors.end()) {
                if(!ctx) {
                    ctx = getActionContext<SoFCSelectionContextEx>(action,node);
                    selCounter.checkAction(selaction,ctx);
                    ctx->selectAll();
                }
                if(ctx->setColors(selaction->getColors(),element))
                    node->touch();
            }
        }
        break;
    case SoSelectionElementAction::Remove:
    case SoSelectionElementAction::Append: {
        const SoDetail* detail = selaction->getElement();
        if (detail && detail->isOfType(SoFCDetail::getClassTypeId())) {
            const auto &indices = static_cast<const SoFCDetail*>(detail)->getIndices(detailType);
            if(indices.size()) {
                bool touched = false;
                if (selaction->getType() == SoSelectionElementAction::Append) {
                    auto ctx = getActionContext(action,node,selContext);
                    selCounter.checkAction(selaction,ctx);
                    ctx->selectionColor = selaction->getColor();
                    if(ctx->isSelectAll())
                        ctx->selectionIndex.clear();
                    for(int index : indices) {
                        if(ctx->addIndex(index))
                            touched = true;
                    }
                }else{
                    auto ctx = getActionContext(action,node,selContext,false);
                    if(ctx) {
                        for(int index : indices) {
                            if(ctx->removeIndex(index))
                                touched = true;
                        }
                    }
                }
                if(touched)
                    node->touch();
                return true;
            }
            // NOTE! if indices is empty, we shall not return here, because
            // we need to check if this is a secondary selection action. See
            // comments below.

        } else if(detail) {
            int index = -1;
            switch(detailType) {
            case SoFCDetail::Face:
                if (detail->isOfType(SoFaceDetail::getClassTypeId()))
                    index = static_cast<const SoFaceDetail*>(detail)->getPartIndex();
                break;
            case SoFCDetail::Edge:
                if (detail->isOfType(SoLineDetail::getClassTypeId()))
                    index = static_cast<const SoLineDetail*>(detail)->getLineIndex();
                break;
            case SoFCDetail::Vertex:
                if (detail->isOfType(SoPointDetail::getClassTypeId()))
                    index = static_cast<const SoPointDetail*>(detail)->getCoordinateIndex();
                break;
            default:
                break;
            }
            if(index >= 0) {
                if (selaction->getType() == SoSelectionElementAction::Append) {
                    auto ctx = getActionContext(action,node,selContext);
                    selCounter.checkAction(selaction,ctx);
                    ctx->selectionColor = selaction->getColor();
                    if(ctx->isSelectAll())
                        ctx->selectionIndex.clear();
                    if(ctx->addIndex(index))
                        node->touch();
                }else{
                    auto ctx = getActionContext(action,node,selContext,false);
                    if(ctx && ctx->removeIndex(index))
                        node->touch();
                }
                return true;
            }
        }

        if(selaction->isSecondary()) {
            // For secondary context, a detail of different type means the user
            // may want to partial render only other type of geometry. So we
            // call below to obtain a action context.  If no secondary context
            // exist, it will create an empty one, and an empty secondary
            // context inhibites drawing here.
            auto ctx = getActionContext<SoFCSelectionContextEx>(action,node);
            selCounter.checkAction(selaction,ctx);
            node->touch();
        }
        break;

    } default:
        break;
    }

    return true;
}

/////////////////////////////////////////////////////////////////////////////

FCDepthFunc::FCDepthFunc(int32_t f)
    :func(0),changed(false),dtest(false)
{
    set(f);
}

FCDepthFunc::FCDepthFunc()
    :func(0),changed(false),dtest(false)
{}

FCDepthFunc::~FCDepthFunc() {
    restore();
}

void FCDepthFunc::restore() {
    if(func && changed) {
        changed = false;
        glDepthFunc(func);
    }
    if(dtest) {
        dtest = false;
        glDisable(GL_DEPTH_TEST);
    }
}
    
void FCDepthFunc::set(int32_t f) {
    int32_t oldFunc;
    glGetIntegerv(GL_DEPTH_FUNC, &oldFunc);
    if(!func)
        func = oldFunc;
    if(oldFunc!=f) {
        changed = true;
        glDepthFunc(f);
    }
    if(!dtest && !glIsEnabled(GL_DEPTH_TEST)) {
        dtest = true;
        glEnable(GL_DEPTH_TEST);
    }
}

/////////////////////////////////////////////////////////////////////////////

SO_NODE_SOURCE(SoFCPathAnnotation)

SoFCPathAnnotation::SoFCPathAnnotation(ViewProvider *vp, const char *sub, View3DInventorViewer *viewer)
    :viewProvider(vp), subname(sub?sub:""), viewer(viewer)
{
    SO_NODE_CONSTRUCTOR(SoFCPathAnnotation);
    path = 0;
    det = false;
    this->renderCaching = SoSeparator::OFF;
    this->boundingBoxCaching = SoSeparator::OFF;
}

SoFCPathAnnotation::~SoFCPathAnnotation()
{
    setPath(0);
}

void SoFCPathAnnotation::finish() 
{
    atexit_cleanup();
}

void SoFCPathAnnotation::initClass(void)
{
    SO_NODE_INIT_CLASS(SoFCPathAnnotation,SoSeparator,"Separator");
}

void SoFCPathAnnotation::GLRender(SoGLRenderAction * action)
{
    switch (action->getCurPathCode()) {
    case SoAction::NO_PATH:
    case SoAction::BELOW_PATH:
        this->GLRenderBelowPath(action);
        break;
    case SoAction::OFF_PATH:
        break;
    case SoAction::IN_PATH:
        this->GLRenderInPath(action);
        break;
    }
}

void SoFCPathAnnotation::GLRenderBelowPath(SoGLRenderAction * action)
{
    if(!path || !path->getLength() || !tmpPath.getLength())
        return;

    if(path->getLength() != tmpPath.getLength()) {
        // The auditing SoPath may be truncated due to harmless things such as
        // fliping a SoSwitch sibling node. So we keep a SoNodeList (tmpPath)
        // around to try to restore the path.
        for(int i=path->getLength()-1;i<tmpPath.getLength()-1;++i) {
            auto node = path->getNode(i);
            if(node->isOfType(SoGroup::getClassTypeId())) {
                int idx = static_cast<SoGroup*>(node)->findChild(tmpPath[i+1]);
                if(idx >= 0) {
                    path->append(idx);
                    continue;
                }
            }
            setPath(0);
            return;
        }
    }

    if (action->isRenderingDelayedPaths()) {
        GLboolean dtest = glIsEnabled(GL_DEPTH_TEST);
        if(dtest)
            glDisable(GL_DEPTH_TEST);

        // SoFCSelectionRoot will trigger switching override for all lower
        // hierarchy SoFCSwitch nodes. This means all lower hierarchy
        // children will made visible. This could cause slow down in
        // rendering. Our goal here is to only override switches within the
        // configured path, and turn off visibility override below the path
        SoFCSwitch::pushSwitchPath(path);

        if(det)
            inherited::GLRenderInPath(action);
        else {
            bool bbox = SoFCUnifiedSelection::getShowSelectionBoundingBox();
            if(!bbox) {
                for(int i=0,count=path->getLength();i<count;++i) {
                    auto node = path->getNodeFromTail(i);
                    if(node->isOfType(SoFCSelectionRoot::getClassTypeId())) {
                        if (static_cast<SoFCSelectionRoot*>(node)->selectionStyle.getValue() == SoFCSelectionRoot::Box)
                            bbox = true;
                        break;
                    }
                }
            }

            if(!bbox)
                inherited::GLRenderInPath(action);
            else {
                bool sel = false;
                bool hl = false;
                float trans;
                SbColor selColor,hlColor;
                SoFCSelectionRoot::checkSelection(sel,selColor,hl,hlColor);
                if(!sel && !hl)
                    selColor.setPackedValue(ViewParams::instance()->getSelectionColor(),trans);

                // push a null entry to skip SoFCSwitch manipulation in
                // case ViewProvider::getBoundingBox() needs to use SoGetBoundingBoxAction()
                SoFCSwitch::pushSwitchPath(nullptr);

                if(!viewProvider || det) {
                    SoFCSelectionRoot::renderBBox(action,this,hl?hlColor:selColor);
                } else {
                    auto state = action->getState();

                    if(ViewParams::instance()->getRenderProjectedBBox()) {
                        if(!ViewParams::instance()->getUseTightBoundingBox()) 
                            SoFCSelectionRoot::renderBBox(action,this,hl?hlColor:selColor);
                        else {
                            Base::Matrix4D mat = ViewProvider::convert(SoModelMatrixElement::get(state));
                            auto fcbox = viewProvider->getBoundingBox(subname.c_str(),&mat,true,viewer);
                            SbBox3f bbox(fcbox.MinX,fcbox.MinY,fcbox.MinZ,
                                        fcbox.MaxX,fcbox.MaxY,fcbox.MaxZ);
                            SoFCSelectionRoot::renderBBox(action,this,bbox,hl?hlColor:selColor);
                        }
                    } else {
                        auto vpd = Base::freecad_dynamic_cast<ViewProviderDocumentObject>(viewProvider);
                        if(vpd && vpd->getObject() && vpd->getObject()->getNameInDocument()) {
                            Base::Matrix4D mat;
                            auto vp = Application::Instance->getViewProvider(
                                    vpd->getObject()->getSubObject(subname.c_str(),0,&mat));
                            if(vp) {
                                SbMatrix matrix = ViewProvider::convert(mat);
                                if(!ViewParams::instance()->getUseTightBoundingBox())
                                    SoFCSelectionRoot::renderBBox(action,vp->getRoot(),hl?hlColor:selColor,&matrix);
                                else {
                                    auto fcbox = vp->getBoundingBox(0,0,false,viewer);
                                    SbBox3f bbox(fcbox.MinX,fcbox.MinY,fcbox.MinZ,
                                            fcbox.MaxX,fcbox.MaxY,fcbox.MaxZ);
                                    SoFCSelectionRoot::renderBBox(action,this,bbox,hl?hlColor:selColor,&matrix);
                                }
                            }
                        }
                    }
                }
                SoFCSwitch::popSwitchPath();
            }
        }

        SoFCSwitch::popSwitchPath();
        if(dtest)
            glEnable(GL_DEPTH_TEST);

    } else {
        auto curPath = action->getCurPath();
        SoPath *newPath = new SoPath(curPath->getLength()+path->getLength());
        newPath->append(curPath);
        newPath->append(path);
        action->addDelayedPath(newPath);
    }
}

void SoFCPathAnnotation::GLRenderInPath(SoGLRenderAction * action)
{
    GLRenderBelowPath(action);
}

void SoFCPathAnnotation::setDetail(bool d) {
    det = d;
}

void SoFCPathAnnotation::setPath(SoPath *newPath) {
    if(path) {
        path->unref();
        path = 0;
        for(int i=0;i<tmpPath.getLength();++i) {
            auto node = tmpPath[i];
            if(node->isOfType(SoFCSwitch::getClassTypeId())) {
                auto &notify = static_cast<SoFCSwitch*>(node)->childNotify;
                if(notify.getValue()>0)
                    notify = notify.getValue()-1;
            }
        }
        tmpPath.truncate(0);
    }
    if(!newPath || !newPath->getLength())
        return;

    for(int i=0;i<newPath->getLength();++i) {
        auto node = newPath->getNode(i);
        if(node->isOfType(SoFCSwitch::getClassTypeId())) {
            auto &notify = static_cast<SoFCSwitch*>(node)->childNotify;
            notify = notify.getValue()+1;
        }
        tmpPath.append(node);
    }
    path = newPath->copy();
    path->ref();
    coinRemoveAllChildren(this);
    addChild(path->getNode(0));
}

void SoFCPathAnnotation::getBoundingBox(SoGetBoundingBoxAction * action)
{
    if(path) {
        SoFCSwitch::pushSwitchPath(path);
        // TODO: it is better to use SbStorage
        static FC_COIN_THREAD_LOCAL SoGetBoundingBoxAction *bboxAction = 0;
        if(!bboxAction)
            bboxAction = new SoGetBoundingBoxAction(SbViewportRegion());
        bboxAction->setViewportRegion(action->getViewportRegion());
        SoFCSelectionRoot::moveActionStack(action,bboxAction,false);
        SoSwitchElement::set(bboxAction->getState(),SoSwitchElement::get(action->getState()));
        bboxAction->apply(path);
        SoFCSelectionRoot::moveActionStack(bboxAction,action,true);
        auto bbox = bboxAction->getBoundingBox();
        if(!bbox.isEmpty())
            action->extendBy(bbox);
        SoFCSwitch::popSwitchPath();
    }
}

void SoFCPathAnnotation::doPick(SoPath *curPath, SoRayPickAction *action) {
    if(path) {
        SoFCSwitch::pushSwitchPath(path);
        int length = curPath->getLength();
        curPath->append(this);
        curPath->append(path);
        action->apply(curPath);
        curPath->truncate(length);
        SoFCSwitch::popSwitchPath();
    }
}
void SoFCPathAnnotation::doAction(SoAction *action) {
    if(path)
        SoFCSwitch::pushSwitchPath(path);
    inherited::doAction(action);
    if(path)
        SoFCSwitch::popSwitchPath();
}

