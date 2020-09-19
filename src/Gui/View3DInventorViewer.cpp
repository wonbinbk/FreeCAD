/***************************************************************************
 *   Copyright (c) 2004 Jürgen Riegel <juergen.riegel@web.de>              *
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
# include <float.h>
# ifdef FC_OS_WIN32
#  include <windows.h>
# endif
# ifdef FC_OS_MACOSX
# include <OpenGL/gl.h>
# else
# include <GL/gl.h>
# endif
# include <Inventor/SbBox.h>
# include <Inventor/SoEventManager.h>
# include <Inventor/actions/SoGetBoundingBoxAction.h>
# include <Inventor/actions/SoGetMatrixAction.h>
# include <Inventor/actions/SoHandleEventAction.h>
# include <Inventor/actions/SoToVRML2Action.h>
# include <Inventor/actions/SoWriteAction.h>
# include <Inventor/elements/SoViewportRegionElement.h>
# include <Inventor/manips/SoClipPlaneManip.h>
# include <Inventor/nodes/SoBaseColor.h>
# include <Inventor/nodes/SoCallback.h>
# include <Inventor/nodes/SoCoordinate3.h>
# include <Inventor/nodes/SoCube.h>
# include <Inventor/nodes/SoDirectionalLight.h>
# include <Inventor/nodes/SoEventCallback.h>
# include <Inventor/nodes/SoFaceSet.h>
# include <Inventor/nodes/SoImage.h>
# include <Inventor/nodes/SoIndexedFaceSet.h>
# include <Inventor/nodes/SoLightModel.h>
# include <Inventor/nodes/SoLocateHighlight.h>
# include <Inventor/nodes/SoMaterial.h>
# include <Inventor/nodes/SoMaterialBinding.h>
# include <Inventor/nodes/SoOrthographicCamera.h>
# include <Inventor/nodes/SoPerspectiveCamera.h>
# include <Inventor/nodes/SoRotationXYZ.h>
# include <Inventor/nodes/SoSeparator.h>
# include <Inventor/nodes/SoShapeHints.h>
# include <Inventor/nodes/SoSwitch.h>
# include <Inventor/nodes/SoTransform.h>
# include <Inventor/nodes/SoTranslation.h>
# include <Inventor/nodes/SoSelection.h>
# include <Inventor/nodes/SoText2.h>
# include <Inventor/actions/SoBoxHighlightRenderAction.h>
# include <Inventor/events/SoEvent.h>
# include <Inventor/events/SoKeyboardEvent.h>
# include <Inventor/events/SoLocation2Event.h>
# include <Inventor/events/SoMotion3Event.h>
# include <Inventor/events/SoMouseButtonEvent.h>
# include <Inventor/actions/SoRayPickAction.h>
# include <Inventor/projectors/SbSphereSheetProjector.h>
# include <Inventor/SoOffscreenRenderer.h>
# include <Inventor/SoPickedPoint.h>
# include <Inventor/VRMLnodes/SoVRMLGroup.h>
# include <Inventor/nodes/SoPickStyle.h>
# include <Inventor/nodes/SoTransparencyType.h>
# include <Inventor/nodes/SoTexture2.h>
# include <QEventLoop>
# include <QKeyEvent>
# include <QWheelEvent>
# include <QMessageBox>
# include <QTimer>
# include <QStatusBar>
# include <QBitmap>
# include <QMimeData>
# include <QApplication>
#endif

#include <Inventor/sensors/SoTimerSensor.h>
#include <Inventor/SoEventManager.h>
#include <Inventor/annex/FXViz/nodes/SoShadowGroup.h>
#include <Inventor/annex/FXViz/nodes/SoShadowStyle.h>
#include <Inventor/annex/FXViz/nodes/SoShadowDirectionalLight.h>
#include <Inventor/annex/FXViz/nodes/SoShadowSpotLight.h>
#include <Inventor/nodes/SoBumpMap.h>
#include <Inventor/nodes/SoTextureUnit.h>
#include <Inventor/nodes/SoTextureCoordinate2.h>
#include <Inventor/manips/SoDirectionalLightManip.h>
#include <Inventor/manips/SoSpotLightManip.h>
#include <Inventor/draggers/SoDirectionalLightDragger.h>

#if !defined(FC_OS_MACOSX)
# include <GL/gl.h>
# include <GL/glu.h>
# include <GL/glext.h>
#endif

#include <QVariantAnimation>

#include <boost/algorithm/string/predicate.hpp>
#include <sstream>
#include <Base/Console.h>
#include <Base/Stream.h>
#include <Base/FileInfo.h>
#include <Base/Sequencer.h>
#include <Base/Tools.h>
#include <Base/UnitsApi.h>
#include <App/GeoFeatureGroupExtension.h>
#include <App/PropertyUnits.h>
#include <App/PropertyFile.h>
#include <App/ComplexGeoDataPy.h>

#include "View3DInventorViewer.h"
#include "ViewProviderDocumentObject.h"
#include "ViewParams.h"
#include "SoFCBackgroundGradient.h"
#include "SoFCColorBar.h"
#include "SoFCColorLegend.h"
#include "SoFCColorGradient.h"
#include "SoFCOffscreenRenderer.h"
#include "SoFCSelection.h"
#include "SoFCUnifiedSelection.h"
#include "SoFCInteractiveElement.h"
#include "SoFCBoundingBox.h"
#include "SoAxisCrossKit.h"
#include "SoFCDirectionalLight.h"
#include "SoFCSpotLight.h"
#include "View3DInventorRiftViewer.h"
#include "Utilities.h"

#include "Selection.h"
#include "SoFCSelectionAction.h"
#include "SoFCVectorizeU3DAction.h"
#include "SoFCVectorizeSVGAction.h"
#include "SoFCDB.h"
#include "Application.h"
#include "MainWindow.h"
#include "NavigationStyle.h"
#include "ViewProvider.h"
#include "SpaceballEvent.h"
#include "GLPainter.h"
#include <Quarter/eventhandlers/EventFilter.h>
#include <Quarter/devices/InputDevice.h>
#include "View3DViewerPy.h"
#include <Gui/NaviCube.h>

#include <Inventor/draggers/SoCenterballDragger.h>
#include <Inventor/annex/Profiler/SoProfiler.h>
#include <Inventor/annex/HardCopy/SoVectorizePSAction.h>
#include <Inventor/elements/SoOverrideElement.h>
#include <Inventor/elements/SoLightModelElement.h>
#include <QGesture>

#include "SoTouchEvents.h"
#include "WinNativeGestureRecognizers.h"
#include "Document.h"
#include "ViewParams.h"

#include "ViewProviderLink.h"

FC_LOG_LEVEL_INIT("3DViewer",true,true)

//#define FC_LOGGING_CB

using namespace Gui;

#ifndef M_PI
#define M_PI       3.14159265358979323846
#endif

/*** zoom-style cursor ******/

#define ZOOM_WIDTH 16
#define ZOOM_HEIGHT 16
#define ZOOM_BYTES ((ZOOM_WIDTH + 7) / 8) * ZOOM_HEIGHT
#define ZOOM_HOT_X 5
#define ZOOM_HOT_Y 7

static unsigned char zoom_bitmap[ZOOM_BYTES] =
{
  0x00, 0x0f, 0x80, 0x1c, 0x40, 0x38, 0x20, 0x70,
  0x90, 0xe4, 0xc0, 0xcc, 0xf0, 0xfc, 0x00, 0x0c,
  0x00, 0x0c, 0xf0, 0xfc, 0xc0, 0xcc, 0x90, 0xe4,
  0x20, 0x70, 0x40, 0x38, 0x80, 0x1c, 0x00, 0x0f
};

static unsigned char zoom_mask_bitmap[ZOOM_BYTES] =
{
 0x00,0x0f,0x80,0x1f,0xc0,0x3f,0xe0,0x7f,0xf0,0xff,0xf0,0xff,0xf0,0xff,0x00,
 0x0f,0x00,0x0f,0xf0,0xff,0xf0,0xff,0xf0,0xff,0xe0,0x7f,0xc0,0x3f,0x80,0x1f,
 0x00,0x0f
};

/*** pan-style cursor *******/

#define PAN_WIDTH 16
#define PAN_HEIGHT 16
#define PAN_BYTES ((PAN_WIDTH + 7) / 8) * PAN_HEIGHT
#define PAN_HOT_X 7
#define PAN_HOT_Y 7

static unsigned char pan_bitmap[PAN_BYTES] =
{
  0xc0, 0x03, 0x60, 0x02, 0x20, 0x04, 0x10, 0x08,
  0x68, 0x16, 0x54, 0x2a, 0x73, 0xce, 0x01, 0x80,
  0x01, 0x80, 0x73, 0xce, 0x54, 0x2a, 0x68, 0x16,
  0x10, 0x08, 0x20, 0x04, 0x40, 0x02, 0xc0, 0x03
};

static unsigned char pan_mask_bitmap[PAN_BYTES] =
{
 0xc0,0x03,0xe0,0x03,0xe0,0x07,0xf0,0x0f,0xe8,0x17,0xdc,0x3b,0xff,0xff,0xff,
 0xff,0xff,0xff,0xff,0xff,0xdc,0x3b,0xe8,0x17,0xf0,0x0f,0xe0,0x07,0xc0,0x03,
 0xc0,0x03
};

/*** rotate-style cursor ****/

#define ROTATE_WIDTH 16
#define ROTATE_HEIGHT 16
#define ROTATE_BYTES ((ROTATE_WIDTH + 7) / 8) * ROTATE_HEIGHT
#define ROTATE_HOT_X 6
#define ROTATE_HOT_Y 8

static unsigned char rotate_bitmap[ROTATE_BYTES] = {
  0xf0, 0xef, 0x18, 0xb8, 0x0c, 0x90, 0xe4, 0x83,
  0x34, 0x86, 0x1c, 0x83, 0x00, 0x81, 0x00, 0xff,
  0xff, 0x00, 0x81, 0x00, 0xc1, 0x38, 0x61, 0x2c,
  0xc1, 0x27, 0x09, 0x30, 0x1d, 0x18, 0xf7, 0x0f
};

static unsigned char rotate_mask_bitmap[ROTATE_BYTES] = {
 0xf0,0xef,0xf8,0xff,0xfc,0xff,0xfc,0xff,0x3c,0xfe,0x1c,0xff,0x00,0xff,0x00,
 0xff,0xff,0x00,0xff,0x00,0xff,0x38,0x7f,0x3c,0xff,0x3f,0xff,0x3f,0xff,0x1f,
 0xf7,0x0f
};


/*!
As ProgressBar has no chance to control the incoming Qt events of Quarter so we need to stop
the event handling to prevent the scenegraph from being selected or deselected
while the progress bar is running.
*/
class Gui::ViewerEventFilter : public QObject
{
public:
    ViewerEventFilter() {}
    ~ViewerEventFilter() {}

    bool eventFilter(QObject* obj, QEvent* event) {

#ifdef GESTURE_MESS
        if (obj->isWidgetType()) {
            View3DInventorViewer* v = dynamic_cast<View3DInventorViewer*>(obj);
            if(v) {
                /* Internally, Qt seems to set up the gestures upon showing the
                 * widget (but after this event is processed), thus invalidating
                 * our settings. This piece takes care to retune gestures on the
                 * next event after the show event.
                 */
                if(v->winGestureTuneState == View3DInventorViewer::ewgtsNeedTuning) {
                    try{
                        WinNativeGestureRecognizerPinch::TuneWindowsGestures(v);
                        v->winGestureTuneState = View3DInventorViewer::ewgtsTuned;
                    } catch (Base::Exception &e) {
                        Base::Console().Warning("Failed to TuneWindowsGestures. Error: %s\n",e.what());
                        v->winGestureTuneState = View3DInventorViewer::ewgtsDisabled;
                    } catch (...) {
                        Base::Console().Warning("Failed to TuneWindowsGestures. Unknown error.\n");
                        v->winGestureTuneState = View3DInventorViewer::ewgtsDisabled;
                    }
                }
                if (event->type() == QEvent::Show && v->winGestureTuneState == View3DInventorViewer::ewgtsTuned)
                    v->winGestureTuneState = View3DInventorViewer::ewgtsNeedTuning;

            }
        }
#endif

        // Bug #0000607: Some mice also support horizontal scrolling which however might
        // lead to some unwanted zooming when pressing the MMB for panning.
        // Thus, we filter out horizontal scrolling.
        if (event->type() == QEvent::Wheel) {
            QWheelEvent* we = static_cast<QWheelEvent*>(event);
            if (we->orientation() == Qt::Horizontal)
                return true;
        }
        else if (event->type() == QEvent::KeyPress) {
            QKeyEvent* ke = static_cast<QKeyEvent*>(event);
            if (ke->matches(QKeySequence::SelectAll)) {
                static_cast<View3DInventorViewer*>(obj)->selectAll();
                return true;
            }
        }
        if (Base::Sequencer().isRunning() && Base::Sequencer().isBlocking())
            return false;

        if (event->type() == Spaceball::ButtonEvent::ButtonEventType) {
            Spaceball::ButtonEvent* buttonEvent = static_cast<Spaceball::ButtonEvent*>(event);
            if (!buttonEvent) {
                Base::Console().Log("invalid spaceball button event\n");
                return true;
            }
        }
        else if (event->type() == Spaceball::MotionEvent::MotionEventType) {
            Spaceball::MotionEvent* motionEvent = static_cast<Spaceball::MotionEvent*>(event);
            if (!motionEvent) {
                Base::Console().Log("invalid spaceball motion event\n");
                return true;
            }
        }

        return false;
    }
};

class SpaceNavigatorDevice : public Quarter::InputDevice {
public:
    SpaceNavigatorDevice(void) {}
    virtual ~SpaceNavigatorDevice() {}
    virtual const SoEvent* translateEvent(QEvent* event) {

        if (event->type() == Spaceball::MotionEvent::MotionEventType) {
            Spaceball::MotionEvent* motionEvent = static_cast<Spaceball::MotionEvent*>(event);
            if (!motionEvent) {
                Base::Console().Log("invalid spaceball motion event\n");
                return NULL;
            }

            motionEvent->setHandled(true);

            float xTrans, yTrans, zTrans;
            xTrans = static_cast<float>(motionEvent->translationX());
            yTrans = static_cast<float>(motionEvent->translationY());
            zTrans = static_cast<float>(motionEvent->translationZ());
            SbVec3f translationVector(xTrans, yTrans, zTrans);

            static float rotationConstant(.0001f);
            SbRotation xRot, yRot, zRot;
            xRot.setValue(SbVec3f(1.0, 0.0, 0.0), static_cast<float>(motionEvent->rotationX()) * rotationConstant);
            yRot.setValue(SbVec3f(0.0, 1.0, 0.0), static_cast<float>(motionEvent->rotationY()) * rotationConstant);
            zRot.setValue(SbVec3f(0.0, 0.0, 1.0), static_cast<float>(motionEvent->rotationZ()) * rotationConstant);

            SoMotion3Event* motion3Event = new SoMotion3Event;
            motion3Event->setTranslation(translationVector);
            motion3Event->setRotation(xRot * yRot * zRot);

            return motion3Event;
        }

        return NULL;
    };
};

template<class PropT, class ValueT, class CallbackT>
ValueT _shadowParam(App::Document *doc, const char *_name, const char *_docu, const ValueT &def, CallbackT cb) {
    if(!doc)
        return def;
    char name[64];
    snprintf(name,sizeof(name)-1,"Shadow_%s",_name);
    auto prop = doc->getPropertyByName(name);
    if(prop && !prop->isDerivedFrom(PropT::getClassTypeId()))
        return def;
    if(!prop) {
        prop = doc->addDynamicProperty(PropT::getClassTypeId().getName(), name, "Shadow", _docu);
        static_cast<PropT*>(prop)->setValue(def);
    }
    cb(*static_cast<PropT*>(prop));
    return static_cast<PropT*>(prop)->getValue();
}

template<class PropT, class ValueT>
ValueT _shadowParam(App::Document *doc, const char *_name, const char *_docu, const ValueT &def) {
    auto cb = [](PropT &){};
    return _shadowParam<PropT, ValueT>(doc, _name, _docu, def, cb);
}

template<class PropT, class ValueT>
void _shadowSetParam(App::Document *doc, const char *_name, const ValueT &def) {
    _shadowParam<PropT, ValueT>(doc, _name, nullptr, def,
        [&def](PropT &prop) {
            Base::ObjectStatusLocker<App::Property::Status,App::Property> guard(App::Property::User3, &prop);
            prop.setValue(def);
        });
}

struct View3DInventorViewer::ShadowInfo
{
    View3DInventorViewer              *owner;

    CoinPtr<SoShadowGroup>            pcShadowGroup;
    CoinPtr<SoFCDirectionalLight>     pcShadowDirectionalLight;
    CoinPtr<SoFCSpotLight>            pcShadowSpotLight;
    CoinPtr<SoGroup>                  pcShadowGroundGroup;
    CoinPtr<SoSwitch>                 pcShadowGroundSwitch;
    CoinPtr<SoCoordinate3>            pcShadowGroundCoords;
    CoinPtr<SoFaceSet>                pcShadowGround;
    CoinPtr<SoShadowStyle>            pcShadowGroundStyle;
    CoinPtr<SoMaterial>               pcShadowMaterial;
    CoinPtr<SoTexture2>               pcShadowGroundTexture;
    CoinPtr<SoTextureCoordinate2>     pcShadowGroundTextureCoords;
    CoinPtr<SoBumpMap>                pcShadowGroundBumpMap;
    CoinPtr<SoLightModel>             pcShadowGroundLightModel;
    CoinPtr<SoShapeHints>             pcShadowGroundShapeHints;
    CoinPtr<SoPickStyle>              pcShadowPickStyle;
    uint32_t                          shadowNodeId = 0;
    uint32_t                          cameraNodeId = 0;
    bool                              shadowExtraRedraw = false;
    bool                              animating = false;

    QTimer                            timer;


    ShadowInfo(View3DInventorViewer *owner)
        :owner(owner)
    {}

    void activate();
    void deactivate();
    void updateShadowGround(const SbBox3f &box);
    void redraw();
    void onRender();
    void toggleDragger(int toggle);

    void getBoundingBox(SbBox3f &box) {
        SoNode *node = nullptr;
        if (pcShadowSpotLight && pcShadowSpotLight->showDragger.getValue())
            node = pcShadowSpotLight;
        else if (pcShadowDirectionalLight && pcShadowDirectionalLight->showDragger.getValue())
            node = pcShadowDirectionalLight;
        if (node) {
            SoGetBoundingBoxAction action(owner->getSoRenderManager()->getViewportRegion());
            action.apply(node);
            box.extendBy(action.getBoundingBox());
        }
    }
};

/** \defgroup View3D 3D Viewer
 *  \ingroup GUI
 *
 * The 3D Viewer is one of the major components in a CAD/CAE systems.
 * Therefore an overview and some remarks to the FreeCAD 3D viewing system.
 *
 * \section overview Overview
 * \todo Overview and complements for the 3D Viewer
 */


// *************************************************************************

View3DInventorViewer::View3DInventorViewer(QWidget* parent, const QtGLWidget* sharewidget)
    : Quarter::SoQTQuarterAdaptor(parent, sharewidget), SelectionObserver(false,0),
      editViewProvider(0), navigation(0),
      renderType(Native), framebuffer(0), axisCross(0), axisGroup(0), editing(false), redirected(false),
      allowredir(false), overrideMode("As Is"), _viewerPy(0)
{
    init();
}

View3DInventorViewer::View3DInventorViewer(const QtGLFormat& format, QWidget* parent, const QtGLWidget* sharewidget)
    : Quarter::SoQTQuarterAdaptor(format, parent, sharewidget), SelectionObserver(false,0),
      editViewProvider(0), navigation(0),
      renderType(Native), framebuffer(0), axisCross(0), axisGroup(0), editing(false), redirected(false),
      allowredir(false), overrideMode("As Is"), _viewerPy(0)
{
    init();
}

void View3DInventorViewer::init()
{
    shadowInfo.reset(new ShadowInfo(this));
    shadowInfo->timer.setSingleShot(true);
    connect(&shadowInfo->timer,SIGNAL(timeout()),this,SLOT(redrawShadow()));

    static bool _cacheModeInited;
    if(!_cacheModeInited) {
        _cacheModeInited = true;
        pcViewProviderRoot = nullptr;
        selectionRoot = nullptr;
        setRenderCache(-1);
    }

    shading = true;
    fpsEnabled = false;
    vboEnabled = false;

    attachSelection();

    // Coin should not clear the pixel-buffer, so the background image
    // is not removed.
    this->setClearWindow(false);

    // setting up the defaults for the spin rotation
    initialize();

    SoOrthographicCamera* cam = new SoOrthographicCamera;
    cam->position = SbVec3f(0, 0, 1);
    cam->height = 1;
    cam->nearDistance = 0.5;
    cam->farDistance = 1.5;

    // setup light sources
    SoDirectionalLight* hl = this->getHeadlight();
    backlight = new SoDirectionalLight();
    backlight->ref();
    backlight->setName("backlight");
    backlight->direction.setValue(-hl->direction.getValue());
    backlight->on.setValue(false); // by default off

    // Set up background scenegraph with image in it.
    backgroundroot = new SoSeparator;
    backgroundroot->ref();
    this->backgroundroot->addChild(cam);

    // Background stuff
    pcBackGround = new SoFCBackgroundGradient;
    pcBackGround->ref();
    pcBackGroundSwitch = new SoSwitch;
    pcBackGroundSwitch->ref();
    pcBackGroundSwitch->addChild(pcBackGround);
    backgroundroot->addChild(pcBackGroundSwitch);

    // Set up foreground, overlaid scenegraph.
    this->foregroundroot = new SoSeparator;
    this->foregroundroot->ref();

    SoLightModel* lm = new SoLightModel;
    lm->model = SoLightModel::BASE_COLOR;

    SoBaseColor* bc = new SoBaseColor;
    bc->rgb = SbColor(1, 1, 0);

    cam = new SoOrthographicCamera;
    cam->position = SbVec3f(0, 0, 5);
    cam->height = 10;
    cam->nearDistance = 0;
    cam->farDistance = 10;

    // dragger
    //SoSeparator * dragSep = new SoSeparator();
    //SoScale *scale = new SoScale();
    //scale->scaleFactor = SbVec3f  (0.2,0.2,0.2);
    //dragSep->addChild(scale);
    //SoCenterballDragger *dragger = new SoCenterballDragger();
    //dragger->center = SbVec3f  (0.8,0.8,0);
    ////dragger->rotation = SbRotation(rrot[0],rrot[1],rrot[2],rrot[3]);
    //dragSep->addChild(dragger);

    this->foregroundroot->addChild(cam);
    this->foregroundroot->addChild(lm);
    this->foregroundroot->addChild(bc);
    //this->foregroundroot->addChild(dragSep);

#if 0
    // NOTE: For every mouse click event the SoSelection searches for the picked
    // point which causes a certain slow-down because for all objects the primitives
    // must be created. Using an SoSeparator avoids this drawback.
    SoSelection* selectionRoot = new SoSelection();
    selectionRoot->addSelectionCallback(View3DInventorViewer::selectCB, this);
    selectionRoot->addDeselectionCallback(View3DInventorViewer::deselectCB, this);
    selectionRoot->setPickFilterCallback(View3DInventorViewer::pickFilterCB, this);
#else
    // NOTE: For every mouse click event the SoFCUnifiedSelection searches for the picked
    // point which causes a certain slow-down because for all objects the primitives
    // must be created. Using an SoSeparator avoids this drawback.
    selectionRoot = new Gui::SoFCUnifiedSelection();
    selectionRoot->applySettings();
    selectionRoot->setViewer(this);
#endif
    // set the ViewProvider root node
    pcViewProviderRoot = selectionRoot;

    // increase refcount before passing it to setScenegraph(), to avoid
    // premature destruction
    pcViewProviderRoot->ref();
    // is not really working with Coin3D.
    //redrawOverlayOnSelectionChange(pcSelection);
    setSceneGraph(pcViewProviderRoot);
    // Event callback node
    pEventCallback = new SoEventCallback();
    pEventCallback->setUserData(this);
    pEventCallback->ref();
    pcViewProviderRoot->addChild(pEventCallback);
    pEventCallback->addEventCallback(SoEvent::getClassTypeId(), handleEventCB, this);

    dimensionRoot = new SoSwitch(SO_SWITCH_NONE);
    pcViewProviderRoot->addChild(dimensionRoot);
    dimensionRoot->addChild(new SoSwitch()); //first one will be for the 3d dimensions.
    dimensionRoot->addChild(new SoSwitch()); //second one for the delta dimensions.

    // This is a callback node that logs all action that traverse the Inventor tree.
#if defined (FC_DEBUG) && defined(FC_LOGGING_CB)
    SoCallback* cb = new SoCallback;
    cb->setCallback(interactionLoggerCB, this);
    pcViewProviderRoot->addChild(cb);
#endif

    selAction = new SoSelectionElementAction;
    preselAction = new SoHighlightElementAction;

    selectionAction = new SoFCSelectionAction;
    highlightAction = new SoFCHighlightAction;

    auto pcGroupOnTop = new SoSeparator;
    pcGroupOnTop->renderCaching = SoSeparator::OFF;
    pcGroupOnTop->boundingBoxCaching = SoSeparator::OFF;
    pcGroupOnTop->setName("GroupOnTop");

    auto pickStyle = new SoPickStyle;
    pickStyle->style = SoPickStyle::UNPICKABLE;
    pickStyle->setOverride(true);
    pcGroupOnTop->addChild(pickStyle);

    coin_setenv("COIN_SEPARATE_DIFFUSE_TRANSPARENCY_OVERRIDE", "1", TRUE);
    pcGroupOnTopMaterial = new SoMaterial;
    pcGroupOnTopMaterial->ref();
    pcGroupOnTopMaterial->transparency = ViewParams::getTransparencyOnTop();
    pcGroupOnTopMaterial->diffuseColor.setIgnored(true);
    pcGroupOnTopMaterial->setOverride(true);
    pcGroupOnTop->addChild(pcGroupOnTopMaterial);

    pcGroupOnTopSel = new SoFCSelectionRoot;
    pcGroupOnTopSel->selectionStyle = SoFCSelectionRoot::PassThrough;
    pcGroupOnTopSel->setName("GroupOnTopSel");
    pcGroupOnTopSel->ref();

    pcGroupOnTopPreSel = new SoFCSelectionRoot;
    pcGroupOnTopPreSel->selectionStyle = SoFCSelectionRoot::PassThrough;
    pcGroupOnTopPreSel->setName("GroupOnTopPreSel");
    pcGroupOnTopPreSel->ref();

    pcGroupOnTopSwitch = new SoFCSwitch;
    pcGroupOnTopSwitch->ref();
    pcGroupOnTopSwitch->overrideSwitch = SoFCSwitch::OverrideDefault;
    pcGroupOnTopSwitch->whichChild = SO_SWITCH_ALL;
    pcGroupOnTopSwitch->defaultChild = SO_SWITCH_ALL;
    pcGroupOnTop->addChild(pcGroupOnTopSwitch);

    pcGroupOnTopSwitch->addChild(pcGroupOnTopSel);
    pcGroupOnTopSwitch->addChild(pcGroupOnTopPreSel);

    SoSearchAction sa;
    sa.setNode(pcViewProviderRoot);
    sa.apply(getSoRenderManager()->getSceneGraph());

    auto path = sa.getPath();
    assert(path && path->getLength()>1);
    pcRootPath = path->copy();
    pcRootPath->ref();

    auto sceneNode = path->getNodeFromTail(1);
    assert(sceneNode->isOfType(SoGroup::getClassTypeId()));
    int rootIndex = path->getIndexFromTail(0);
    static_cast<SoGroup*>(sceneNode)->insertChild(pcGroupOnTop,rootIndex);

    pCurrentHighlightPath = nullptr;

    pcGroupOnTopPath = path->copy();
    pcGroupOnTopPath->ref();
    pcGroupOnTopPath->truncate(path->getLength()-1);
    pcGroupOnTopPath->append(pcGroupOnTop);
    pcGroupOnTopPath->append(pcGroupOnTopSwitch);
    pcGroupOnTopPath->append(pcGroupOnTopSel);

    pcClipPlane = 0;

    pcEditingRoot = new SoSeparator;
    pcEditingRoot->ref();
    pcEditingRoot->setName("EditingRoot");
    pcEditingTransform = new SoTransform;
    pcEditingTransform->ref();
    pcEditingTransform->setName("EditingTransform");
    restoreEditingRoot = false;
    pcEditingRoot->addChild(pcEditingTransform);
    // pcViewProviderRoot->addChild(pcEditingRoot);
    static_cast<SoGroup*>(sceneNode)->insertChild(pcEditingRoot,rootIndex+1);

    pcRootMaterial = new SoMaterial;
    pcRootMaterial->ref();
    pcRootMaterial->diffuseColor.setIgnored(true);
    pcViewProviderRoot->addChild(pcRootMaterial);

    // Set our own render action which show a bounding box if
    // the SoFCSelection::BOX style is set
    //
    // Important note:
    // When creating a new GL render action we have to copy over the cache context id
    // because otherwise we may get strange rendering behaviour. For more details see
    // http://forum.freecadweb.org/viewtopic.php?f=10&t=7486&start=120#p74398 and for
    // the fix and some details what happens behind the scene have a look at this
    // http://forum.freecadweb.org/viewtopic.php?f=10&t=7486&p=74777#p74736
    uint32_t id = this->getSoRenderManager()->getGLRenderAction()->getCacheContext();
    this->getSoRenderManager()->setGLRenderAction(new SoBoxSelectionRenderAction);
    this->getSoRenderManager()->getGLRenderAction()->setCacheContext(id);

    // set the transparency and antialiasing settings
//  getGLRenderAction()->setTransparencyType(SoGLRenderAction::SORTED_OBJECT_BLEND);
    getSoRenderManager()->getGLRenderAction()->setTransparencyType(SoGLRenderAction::SORTED_OBJECT_SORTED_TRIANGLE_BLEND);
//  getGLRenderAction()->setSmoothing(true);

    // Settings
    setSeekTime(0.4f);

    if (isSeekValuePercentage() == false)
        setSeekValueAsPercentage(true);

    setSeekDistance(100);
    setViewing(false);

    setBackgroundColor(QColor(25, 25, 25));
    setGradientBackground(true);

    // set some callback functions for user interaction
    addStartCallback(interactionStartCB);
    addFinishCallback(interactionFinishCB);

    //filter a few qt events
    viewerEventFilter = new ViewerEventFilter;
    installEventFilter(viewerEventFilter);
    getEventFilter()->registerInputDevice(new SpaceNavigatorDevice);
    getEventFilter()->registerInputDevice(new GesturesDevice(this));

    this->winGestureTuneState = View3DInventorViewer::ewgtsDisabled;
    try{
        this->grabGesture(Qt::PanGesture);
        this->grabGesture(Qt::PinchGesture);
    #ifdef GESTURE_MESS
        {
            static WinNativeGestureRecognizerPinch* recognizer;//static to avoid creating more than one recognizer, thus causing memory leak and gradual slowdown
            if(recognizer == 0){
                recognizer = new WinNativeGestureRecognizerPinch;
                recognizer->registerRecognizer(recognizer); //From now on, Qt owns the pointer.
            }
        }
        this->winGestureTuneState = View3DInventorViewer::ewgtsNeedTuning;
    #endif
    } catch (Base::Exception &e) {
        Base::Console().Warning("Failed to set up gestures. Error: %s\n", e.what());
    } catch (...) {
        Base::Console().Warning("Failed to set up gestures. Unknown error.\n");
    }

    //create the cursors
    QBitmap cursor = QBitmap::fromData(QSize(ROTATE_WIDTH, ROTATE_HEIGHT), rotate_bitmap);
    QBitmap mask = QBitmap::fromData(QSize(ROTATE_WIDTH, ROTATE_HEIGHT), rotate_mask_bitmap);
    spinCursor = QCursor(cursor, mask, ROTATE_HOT_X, ROTATE_HOT_Y);

    cursor = QBitmap::fromData(QSize(ZOOM_WIDTH, ZOOM_HEIGHT), zoom_bitmap);
    mask = QBitmap::fromData(QSize(ZOOM_WIDTH, ZOOM_HEIGHT), zoom_mask_bitmap);
    zoomCursor = QCursor(cursor, mask, ZOOM_HOT_X, ZOOM_HOT_Y);

    cursor = QBitmap::fromData(QSize(PAN_WIDTH, PAN_HEIGHT), pan_bitmap);
    mask = QBitmap::fromData(QSize(PAN_WIDTH, PAN_HEIGHT), pan_mask_bitmap);
    panCursor = QCursor(cursor, mask, PAN_HOT_X, PAN_HOT_Y);
    naviCube = new NaviCube(this);
    naviCubeEnabled = true;
}

View3DInventorViewer::~View3DInventorViewer()
{
    // to prevent following OpenGL error message: "Texture is not valid in the current context. Texture has not been destroyed"
    aboutToDestroyGLContext();

    // It can happen that a document has several MDI views and when the about to be
    // closed 3D view is in edit mode the corresponding view provider must be restored
    // because otherwise it might be left in a broken state
    // See https://forum.freecadweb.org/viewtopic.php?f=3&t=39720
    if (restoreEditingRoot) {
        resetEditingRoot(false);
    }

    // cleanup
    this->backgroundroot->unref();
    this->backgroundroot = 0;
    this->foregroundroot->unref();
    this->foregroundroot = 0;
    this->pcBackGround->unref();
    this->pcBackGround = 0;
    this->pcBackGroundSwitch->unref();
    this->pcBackGroundSwitch = 0;

    setSceneGraph(0);
    this->pEventCallback->unref();
    this->pEventCallback = 0;
    // Note: It can happen that there is still someone who references
    // the root node but isn't destroyed when closing this viewer so
    // that it prevents all children from being deleted. To reduce this
    // likelihood we explicitly remove all child nodes now.
    coinRemoveAllChildren(this->pcViewProviderRoot);
    this->pcViewProviderRoot->unref();
    this->pcViewProviderRoot = 0;
    this->backlight->unref();
    this->backlight = 0;

    this->pcRootMaterial->unref();
    this->pcRootMaterial = 0;

    if(pCurrentHighlightPath)
        pCurrentHighlightPath->unref();

    this->pcRootPath->unref();
    this->pcRootPath = 0;

    this->pcGroupOnTopPath->unref();
    this->pcGroupOnTopSwitch->unref();
    this->pcGroupOnTopPreSel->unref();
    this->pcGroupOnTopSel->unref();
    this->pcGroupOnTopMaterial->unref();

    delete selAction;
    selAction = 0;
    delete preselAction;
    preselAction = 0;

    delete selectionAction;
    selectionAction = 0;
    delete highlightAction;
    highlightAction = 0;

    this->pcEditingRoot->unref();
    this->pcEditingTransform->unref();

    if(this->pcClipPlane)
        this->pcClipPlane->unref();

    delete this->navigation;

    // Note: When closing the application the main window doesn't exist any more.
    if (getMainWindow())
        getMainWindow()->setPaneText(2, QLatin1String(""));

    detachSelection();

    removeEventFilter(viewerEventFilter);
    delete viewerEventFilter;

    if (_viewerPy) {
        static_cast<View3DInventorViewerPy*>(_viewerPy)->_viewer = 0;
        Py_DECREF(_viewerPy);
    }

    // In the init() function we have overridden the default SoGLRenderAction with our
    // own instance of SoBoxSelectionRenderAction and SoRenderManager destroyed the default.
    // But it does this only once so that now we have to explicitly destroy our instance in
    // order to free the memory.
    SoGLRenderAction* glAction = this->getSoRenderManager()->getGLRenderAction();
    this->getSoRenderManager()->setGLRenderAction(nullptr);
    delete glAction;
}

void View3DInventorViewer::aboutToDestroyGLContext()
{
    if (naviCube) {
        QtGLWidget* gl = qobject_cast<QtGLWidget*>(this->viewport());
        if (gl)
            gl->makeCurrent();
        delete naviCube;
        naviCube = 0;
        naviCubeEnabled = false;
    }
}

void View3DInventorViewer::setDocument(Gui::Document* pcDocument)
{
    // write the document the viewer belongs to the selection node
    guiDocument = pcDocument;
    selectionRoot->setDocument(pcDocument);

    if(pcDocument) {
        const auto &sels = Selection().getSelection(pcDocument->getDocument()->getName(),0);
        for(auto &sel : sels) {
            SelectionChanges Chng(SelectionChanges::ShowSelection,
                    sel.DocName,sel.FeatName,sel.SubName);
            onSelectionChanged(Chng);
        }

        pcDocument->getDocument()->signalChanged.connect(boost::bind(
                    &View3DInventorViewer::slotChangeDocument, this, _1, _2));
    }
}

void View3DInventorViewer::slotChangeDocument(const App::Document &, const App::Property &prop)
{
    if(!prop.getName() || prop.testStatus(App::Property::User3))
        return;

    if(!_applyingOverride
            && boost::starts_with(prop.getName(),"Shadow")
            && overrideMode == "Shadow")
    {
        Base::StateLocker guard(_applyingOverride);
        applyOverrideMode();
    }
}

Document* View3DInventorViewer::getDocument() {
    return guiDocument;
}


void View3DInventorViewer::initialize()
{
    navigation = new CADNavigationStyle();
    navigation->setViewer(this);

    this->axiscrossEnabled = true;
    this->axiscrossSize = 10;
}

View3DInventorViewer::OnTopInfo::OnTopInfo()
    :node(0),alt(false)
{
}

View3DInventorViewer::OnTopInfo::OnTopInfo(OnTopInfo &&other)
    :node(other.node),elements(std::move(other.elements))
{
    other.node = 0;
}

View3DInventorViewer::OnTopInfo::~OnTopInfo() {
    if(node)
        node->unref();
    clearElements();
}

void View3DInventorViewer::OnTopInfo::clearElements() {
    for(auto &v : elements)
        delete v.second;
    elements.clear();
}

void View3DInventorViewer::clearGroupOnTop(bool alt) {
    if(objectsOnTopSel.empty() && objectsOnTopPreSel.empty())
        return;

    SoTempPath tmpPath(10);
    tmpPath.ref();

    if(!alt) {
        if(objectsOnTopPreSel.size()) {
            tmpPath.append(pcGroupOnTopSwitch);
            tmpPath.append(pcGroupOnTopPreSel);
            for(auto &v : objectsOnTopPreSel) {
                auto &info = v.second;
                tmpPath.truncate(2);
                tmpPath.append(info.node);
                tmpPath.append(info.node->getPath());
                selAction->setSecondary(true);
                selAction->setType(SoSelectionElementAction::None);
                selAction->apply(&tmpPath);
            }
            objectsOnTopPreSel.clear();
        }
        pcGroupOnTopPreSel->resetContext();
        coinRemoveAllChildren(pcGroupOnTopPreSel);
    }

    if(objectsOnTopSel.size()) {
        tmpPath.truncate(0);
        tmpPath.append(pcGroupOnTopSwitch);
        tmpPath.append(pcGroupOnTopSel);
        if(alt) {
            for(auto it=objectsOnTopSel.begin();it!=objectsOnTopSel.end();) {
                auto &info = it->second;
                if(!info.alt || info.elements.size()) {
                    info.alt = false;
                    ++it;
                    continue;
                }
                int idx = pcGroupOnTopSel->findChild(info.node);
                if(idx >= 0) {
                    tmpPath.truncate(2);
                    tmpPath.append(info.node);
                    tmpPath.append(info.node->getPath());
                    selAction->setSecondary(false);
                    selAction->setType(SoSelectionElementAction::None);
                    selAction->apply(&tmpPath);
                    pcGroupOnTopSel->removeChild(idx);
                }
                it = objectsOnTopSel.erase(it);
            }
            tmpPath.unrefNoDelete();
            return;
        }

        pcGroupOnTopSel->resetContext();
        coinRemoveAllChildren(pcGroupOnTopSel);

        selAction->setColor(SbColor(0,0,0));
        selAction->setSecondary(false);
        selAction->setType(SoSelectionElementAction::Append);
        for(auto it=objectsOnTopSel.begin();it!=objectsOnTopSel.end();) {
            auto &info = it->second;
            if(!info.alt)
                it = objectsOnTopSel.erase(it);
            else {
                ++it;
                pcGroupOnTopSel->addChild(info.node);
                info.clearElements();
                tmpPath.truncate(2);
                tmpPath.append(info.node);
                tmpPath.append(info.node->getPath());
                selAction->apply(&tmpPath);
            }
        }
    }

    tmpPath.unrefNoDelete();
}

bool View3DInventorViewer::isInGroupOnTop(const char *objname, const char *subname) const {
    if(!objname)
        return false;
    std::string key(objname);
    key += '.';
    auto element = Data::ComplexGeoData::findElementName(subname);
    if(subname)
        key.insert(key.end(),subname,element);
    return isInGroupOnTop(key);
}

bool View3DInventorViewer::isInGroupOnTop(const std::string &key) const {
    auto it = objectsOnTopSel.find(key);
    return it!=objectsOnTopSel.end() && it->second.alt;
}

void View3DInventorViewer::checkGroupOnTop(const SelectionChanges &Reason, bool alt) {
    if (ViewParams::getRenderCache() == 3) {
        clearGroupOnTop();
        return;
    }

    bool preselect = false;

    switch(Reason.Type) {
    case SelectionChanges::SetSelection:
        clearGroupOnTop();
        if(!guiDocument)
            return;
        else {
            auto sels = Gui::Selection().getSelection(guiDocument->getDocument()->getName(),0);
            if(ViewParams::instance()->getMaxOnTopSelections() < (int)sels.size()) {
                // setSelection() is normally used for selectAll(). Let's not blow up
                // the whole scene with all those invisible objects
                selectionRoot->setSelectAll(true);
                return;
            }
            for(auto &sel : sels ) {
                checkGroupOnTop(SelectionChanges(SelectionChanges::AddSelection,
                            sel.DocName,sel.FeatName,sel.SubName));
            }
        }
        return;
    case SelectionChanges::ClrSelection:
        clearGroupOnTop(alt);
        if(!alt)
            selectionRoot->setSelectAll(false);
        return;
    case SelectionChanges::SetPreselect:
    case SelectionChanges::RmvPreselect:
        preselect = true;
        break;
    default:
        break;
    }

    if(!getDocument() || !Reason.pDocName || !Reason.pDocName[0] || !Reason.pObjectName)
        return;
    auto obj = getDocument()->getDocument()->getObject(Reason.pObjectName);
    if(!obj || !obj->getNameInDocument())
        return;
    std::string key(obj->getNameInDocument());
    key += '.';
    auto subname = Reason.pSubName;
    auto element = Data::ComplexGeoData::findElementName(subname);
    if(subname)
        key.insert(key.end(),subname,element);

    switch(Reason.Type) {
    case SelectionChanges::SetPreselect:
        if(Reason.SubType!=2) {
            // 2 means it is triggered from tree view. If not from tree view
            // and not belong to on top object, do not handle it.
            if(!objectsOnTopSel.count(key))
                return;
        }
        break;
    case SelectionChanges::HideSelection:
    case SelectionChanges::RmvPreselect:
    case SelectionChanges::RmvSelection: {

        auto &objs = preselect?objectsOnTopPreSel:objectsOnTopSel;
        auto pcGroup = preselect?pcGroupOnTopPreSel:pcGroupOnTopSel;

        if(preselect && pCurrentHighlightPath) {
            preselAction->setHighlighted(false);
            preselAction->apply(pCurrentHighlightPath);
            pCurrentHighlightPath->unref();
            pCurrentHighlightPath = nullptr;
        }

        auto it = objs.find(key.c_str());
        if(it == objs.end())
            return;
        auto &info = it->second;

        if(Reason.Type == SelectionChanges::HideSelection) {
            if(!info.alt) {
                pcGroup->removeChild(info.node);
                objs.erase(it);
            }
            return;
        }

        SoDetail *det = 0;
        auto eit = info.elements.end();

        if(Reason.Type==SelectionChanges::RmvSelection && alt) {
            // When alt is true, remove this object from the on top group
            // regardless of whether it is previsouly selected with alt or not.
            info.alt = false;
            info.clearElements();
        } else {
            eit = info.elements.find(element);
            if(eit == info.elements.end())
                return;
            else
                det = eit->second;
        }

        if(preselect) {
            auto it2 = objectsOnTopSel.find(key);
            if(it2!=objectsOnTopSel.end() 
                    && it2->second.elements.empty()
                    && !it2->second.alt)
            {
                pcGroupOnTopSel->removeChild(it2->second.node);
                objectsOnTopSel.erase(it2);
            }
        }

        int index = pcGroup->findChild(info.node);
        if(index < 0) {
            objs.erase(it);
            return;
        }

        auto path = info.node->getPath();
        SoTempPath tmpPath(3 + (path ? path->getLength() : 0));
        tmpPath.ref();
        tmpPath.append(pcGroupOnTopSwitch);
        tmpPath.append(pcGroup);
        tmpPath.append(info.node);
        if(path)
            tmpPath.append(path);

        if(pcGroup == pcGroupOnTopSel) {
            selAction->setElement(det);
            selAction->setSecondary(false);
            selAction->setType(det?SoSelectionElementAction::Remove:SoSelectionElementAction::None);
            selAction->apply(&tmpPath);
            selAction->setElement(0);

            if(info.alt && eit!=info.elements.end() && info.elements.size()==1) {
                selAction->setColor(SbColor(0,0,0));
                selAction->setType(SoSelectionElementAction::All);
                selAction->apply(&tmpPath);
            }
        } else {
            selAction->setElement(det);
            selAction->setSecondary(true);
            selAction->setType(det?SoSelectionElementAction::Remove:SoSelectionElementAction::None);
            selAction->apply(&tmpPath);
            selAction->setElement(0);
        }

        tmpPath.unrefNoDelete();

        if(eit != info.elements.end()) {
            delete eit->second;
            info.elements.erase(eit);
        }
        if(info.elements.empty() && !info.alt) {
            pcGroup->removeChild(index);
            objs.erase(it);

            if(alt)
                Gui::Selection().rmvPreselect();
        }
        return;
    }
    default:
        break;
    }

    if(!preselect && element && element[0]
            && ViewParams::instance()->getShowSelectionBoundingBox())
        return;

    auto &objs = preselect?objectsOnTopPreSel:objectsOnTopSel;
    auto pcGroup = preselect?pcGroupOnTopPreSel:pcGroupOnTopSel;

    auto vp = Base::freecad_dynamic_cast<ViewProviderDocumentObject>(
            Application::Instance->getViewProvider(obj));
    if(!vp)
        return;
    auto svp = vp;
    if(subname && *subname) {
        auto sobj = obj->getSubObject(subname);
        if(!sobj || !sobj->getNameInDocument())
            return;
        if(sobj!=obj) {
            svp = Base::freecad_dynamic_cast<ViewProviderDocumentObject>(
                    Application::Instance->getViewProvider(sobj));
            if(!svp)
                return;
        }
    }
    int onTop;
    // onTop==2 means on top only if whole object is selected,
    // onTop==3 means on top only if some sub-element is selected
    // onTop==1 means either
    if(Gui::Selection().needPickedList()
            || (alt && Reason.Type == SelectionChanges::AddSelection)
            || ViewParams::instance()->getShowSelectionOnTop()
            || isInGroupOnTop(key))
        onTop = 1;
    else if(vp->OnTopWhenSelected.getValue())
        onTop = vp->OnTopWhenSelected.getValue();
    else
        onTop = svp->OnTopWhenSelected.getValue();

    if(preselect) {
        preselAction->setHighlighted(true);
        preselAction->setColor(selectionRoot->colorHighlight.getValue());
        preselAction->apply(pcGroupOnTopPreSel);
        if(!onTop)
            onTop = 2;
    }else if (Reason.Type == SelectionChanges::AddSelection) {
        if(!onTop)
            return;
        selAction->setColor(selectionRoot->colorSelection.getValue());
    }
    if(onTop==2 || onTop==3) {
        if(subname && *subname) {
            size_t len = strlen(subname);
            if(subname[len-1]=='.') {
                // ending with '.' means whole object selection
                if(onTop == 3)
                    return;
            }else if(onTop==2)
                return;
        }else if(onTop==3)
            return;
    }

    SoTempPath path(10);
    path.ref();

    SoDetail *det = 0;
    if(vp->getDetailPath(subname, &path,true,det) && path.getLength()) {
        auto &info = objs[key];
        if(!info.node) {
            info.node = new SoFCPathAnnotation(vp,subname,this);
            info.node->ref();
            info.node->setPath(&path);
            pcGroup->addChild(info.node);
        }
        if(alt && Reason.Type == SelectionChanges::AddSelection) {
            if(!info.alt) {
                info.alt = true;
                if(info.elements.empty()) {
                    SoTempPath tmpPath(path.getLength()+3);
                    tmpPath.ref();
                    tmpPath.append(pcGroupOnTopSwitch);
                    tmpPath.append(pcGroup);
                    tmpPath.append(info.node);
                    tmpPath.append(&path);
                    selAction->setColor(SbColor(0,0,0));
                    selAction->setSecondary(false);
                    selAction->setType(SoSelectionElementAction::All);
                    selAction->apply(&tmpPath);
                    tmpPath.unrefNoDelete();
                }
            }
        } else if(info.elements.emplace(element,det).second) {
            SoTempPath tmpPath(path.getLength()+3);
            tmpPath.ref();
            tmpPath.append(pcGroupOnTopSwitch);
            tmpPath.append(pcGroup);
            tmpPath.append(info.node);
            tmpPath.append(&path);
            if(pcGroup == pcGroupOnTopSel) {
                selAction->setSecondary(false);
                if(info.elements.size()==1 && info.alt) {
                    selAction->setType(SoSelectionElementAction::None);
                    selAction->apply(&tmpPath);
                }
                info.node->setDetail(!!det);
                selAction->setElement(det);
                selAction->setType(det?SoSelectionElementAction::Append:SoSelectionElementAction::All);
                selAction->apply(&tmpPath);
                selAction->setElement(0);
            } else if (det) {
                // We are preselecting some element. In this case, we do not
                // use PreSelGroup for highlighting, but instead rely on
                // OnTopGroup. Becasue we want SoBrepFaceSet to pick the proper
                // highlight color, in case it conflicts with the selection or
                // the object's original color.

                info.node->setPath(0);

                auto &selInfo = objectsOnTopSel[key];
                if(!selInfo.node) {
                    selInfo.node = new SoFCPathAnnotation(vp,subname,this);
                    selInfo.node->ref();
                    selInfo.node->setPath(&path);
                    selInfo.node->setDetail(true);
                    pcGroupOnTopSel->addChild(selInfo.node);
                }

                if(pCurrentHighlightPath) {
                    preselAction->setHighlighted(false);
                    preselAction->apply(pCurrentHighlightPath);
                    pCurrentHighlightPath->unref();
                    pCurrentHighlightPath = nullptr;
                }

                SoTempPath tmpPath2(path.getLength()+3);
                tmpPath2.ref();
                tmpPath2.append(pcGroupOnTopSwitch);
                tmpPath2.append(pcGroupOnTopSel);
                tmpPath2.append(selInfo.node);
                tmpPath2.append(&path);
                preselAction->setHighlighted(true);
                preselAction->setElement(det);
                preselAction->apply(&tmpPath2);
                preselAction->setElement(0);

                pCurrentHighlightPath = tmpPath2.copy();
                pCurrentHighlightPath->ref();
                tmpPath2.unrefNoDelete();
            } else {
                // NOTE: assuming preselect is only applicable to one single
                // object(or sub-element) at a time. If in the future we shall
                // support multiple preselect element, include the element in
                // the objectsOnTopPreSel key.
                info.node->setDetail(false);
                selAction->setSecondary(true);
                selAction->setElement(0);
                selAction->setType(SoSelectionElementAction::None);
                selAction->apply(&tmpPath);
            }
            det = 0;
            tmpPath.unrefNoDelete();
        }
    }

    delete det;
    path.unrefNoDelete();
}

/// @cond DOXERR
void View3DInventorViewer::onSelectionChanged(const SelectionChanges &_Reason)
{
    if(!getDocument())
        return;

    SelectionChanges Reason(_Reason);

    if(Reason.pDocName && *Reason.pDocName && 
       strcmp(getDocument()->getDocument()->getName(),Reason.pDocName)!=0)
        return;

    switch(Reason.Type) {
    case SelectionChanges::ShowSelection:
        Reason.Type = SelectionChanges::AddSelection;
        // fall through
    case SelectionChanges::HideSelection:
    case SelectionChanges::SetPreselect:
    case SelectionChanges::RmvPreselect:
    case SelectionChanges::SetSelection:
    case SelectionChanges::AddSelection:     
    case SelectionChanges::RmvSelection:
    case SelectionChanges::ClrSelection:
        checkGroupOnTop(Reason);
        break;
    default:
        return;
    }

    if(Reason.Type == SelectionChanges::HideSelection)
        Reason.Type = SelectionChanges::RmvSelection;

    switch(Reason.Type) {
    case SelectionChanges::RmvPreselect:
    case SelectionChanges::SetPreselect:
        if (highlightAction->SelChange)
            FC_WARN("Recursive highlight notification");
        else {
            highlightAction->SelChange = &Reason;
            highlightAction->apply(pcViewProviderRoot);
            highlightAction->SelChange = nullptr;
        }
        break;
    default:
        if (selectionAction->SelChange)
            FC_WARN("Recursive selection notification");
        else {
            selectionAction->SelChange = &Reason;
            selectionAction->apply(pcViewProviderRoot);
            selectionAction->SelChange = nullptr;
        }
    }
}
/// @endcond

SbBool View3DInventorViewer::searchNode(SoNode* node) const
{
    if (node == pcEditingRoot)
        return true;
    SoSearchAction searchAction;
    searchAction.setNode(node);
    searchAction.setInterest(SoSearchAction::FIRST);
    if (pcEditingRoot->getNumChildren()) {
        searchAction.apply(pcEditingRoot);
        if (searchAction.getPath())
            return true;
    }
    searchAction.apply(this->getSceneGraph());
    SoPath* selectionPath = searchAction.getPath();
    return selectionPath ? true : false;
}

SbBool View3DInventorViewer::hasViewProvider(ViewProvider* pcProvider) const
{
    return _ViewProviderSet.find(pcProvider) != _ViewProviderSet.end();
}

SbBool View3DInventorViewer::containsViewProvider(const ViewProvider* vp) const
{
    return hasViewProvider(const_cast<ViewProvider*>(vp));
    // SoSearchAction sa;
    // sa.setNode(vp->getRoot());
    // sa.setSearchingAll(true);
    // sa.apply(getSoRenderManager()->getSceneGraph());
    // return sa.getPath() != nullptr;
}

/// adds an ViewProvider to the view, e.g. from a feature
void View3DInventorViewer::addViewProvider(ViewProvider* pcProvider)
{
    if(!_ViewProviderSet.insert(pcProvider).second)
        return;

    SoSeparator* root = pcProvider->getRoot();

    if (root) {
        if(!guiDocument->isClaimed3D(pcProvider) && pcProvider->canAddToSceneGraph())
            pcViewProviderRoot->addChild(root);
    }

    SoSeparator* fore = pcProvider->getFrontRoot();
    if (fore)
        foregroundroot->addChild(fore);

    SoSeparator* back = pcProvider->getBackRoot();
    if (back)
        backgroundroot->addChild(back);
}

void View3DInventorViewer::removeViewProvider(ViewProvider* pcProvider)
{
    if (this->editViewProvider == pcProvider)
        resetEditingViewProvider();

    auto it = _ViewProviderSet.find(pcProvider);
    if(it == _ViewProviderSet.end())
        return;
    _ViewProviderSet.erase(it);

    SoSeparator* root = pcProvider->getRoot();

    if (root) {
        int index = pcViewProviderRoot->findChild(root);
        if(index>=0)
            pcViewProviderRoot->removeChild(index);
    }

    SoSeparator* fore = pcProvider->getFrontRoot();
    if (fore)
        foregroundroot->removeChild(fore);

    SoSeparator* back = pcProvider->getBackRoot();
    if (back)
        backgroundroot->removeChild(back);
}

void View3DInventorViewer::toggleViewProvider(ViewProvider *vp) {
    if(!_ViewProviderSet.count(vp))
        return;
    SoSeparator* root = vp->getRoot();
    if(!root || !guiDocument)
        return;
    int index = pcViewProviderRoot->findChild(root);
    if(index>=0) {
        if(guiDocument->isClaimed3D(vp) || !vp->canAddToSceneGraph())
            pcViewProviderRoot->removeChild(index);
    } else if(!guiDocument->isClaimed3D(vp) && vp->canAddToSceneGraph())
        pcViewProviderRoot->addChild(root);
}

void View3DInventorViewer::setEditingTransform(const Base::Matrix4D &mat) {
    if(pcEditingTransform) {
        double dMtrx[16];
        mat.getGLMatrix(dMtrx);
        pcEditingTransform->setMatrix(SbMatrix(
                    dMtrx[0], dMtrx[1], dMtrx[2],  dMtrx[3],
                    dMtrx[4], dMtrx[5], dMtrx[6],  dMtrx[7],
                    dMtrx[8], dMtrx[9], dMtrx[10], dMtrx[11],
                    dMtrx[12],dMtrx[13],dMtrx[14], dMtrx[15]));
    }
}

void View3DInventorViewer::setupEditingRoot(SoNode *node, const Base::Matrix4D *mat) {
    if(!editViewProvider) 
        return;
    resetEditingRoot(false);

    if(mat)
        setEditingTransform(*mat);
    else
        setEditingTransform(getDocument()->getEditingTransform());
    if(node) {
        restoreEditingRoot = false;
        pcEditingRoot->addChild(node);
        return;
    }

    if(ViewParams::getEditingAutoTransparent()) {
        pcRootMaterial->setOverride(true);
        pcRootMaterial->transparency = ViewParams::getEditingTransparency();
    }

    restoreEditingRoot = true;
    auto root = editViewProvider->getRoot();
    for(int i=0,count=root->getNumChildren();i<count;++i) {
        SoNode *node = root->getChild(i);
        if(node != editViewProvider->getTransformNode())
            pcEditingRoot->addChild(node);
    }
    coinRemoveAllChildren(root);
    ViewProviderLink::updateLinks(editViewProvider);
}

void View3DInventorViewer::resetEditingRoot(bool updateLinks) {
    pcRootMaterial->setOverride(false);
    pcRootMaterial->transparency = 0.0;

    if(!editViewProvider || pcEditingRoot->getNumChildren()<=1)
        return;
    if(!restoreEditingRoot) {
        pcEditingRoot->getChildren()->truncate(1);
        return;
    }
    restoreEditingRoot = false;
    auto root = editViewProvider->getRoot();
    if(root->getNumChildren()) 
        FC_ERR("WARNING!!! Editing view provider root node is tampered");
    root->addChild(editViewProvider->getTransformNode());
    for(int i=1,count=pcEditingRoot->getNumChildren();i<count;++i)
        root->addChild(pcEditingRoot->getChild(i));
    pcEditingRoot->getChildren()->truncate(1);
    if(updateLinks)
        ViewProviderLink::updateLinks(editViewProvider);
}

SoPickedPoint* View3DInventorViewer::getPointOnRay(const SbVec2s& pos, ViewProvider* vp) const
{
    SoPath *path;
    if(vp == editViewProvider && pcEditingRoot->getNumChildren()>1) {
        path = new SoPath(1);
        path->ref();
        path->append(pcEditingRoot);
    }else{
        //first get the path to this node and calculate the current transformation
        SoSearchAction sa;
        sa.setNode(vp->getRoot());
        sa.setSearchingAll(true);
        sa.apply(getSoRenderManager()->getSceneGraph());
        path = sa.getPath();
        if (!path)
            return nullptr;
        path->ref();
    }
    SoGetMatrixAction gm(getSoRenderManager()->getViewportRegion());
    gm.apply(path);

    SoTransform* trans = new SoTransform;
    trans->setMatrix(gm.getMatrix());
    trans->ref();
    
    // build a temporary scenegraph only keeping this viewproviders nodes and the accumulated 
    // transformation
    SoSeparator* root = new SoSeparator;
    root->ref();
    root->addChild(getSoRenderManager()->getCamera());
    root->addChild(trans);
    root->addChild(path->getTail());

    //get the picked point
    SoRayPickAction rp(getSoRenderManager()->getViewportRegion());
    rp.setPoint(pos);
    rp.setRadius(getPickRadius());
    rp.apply(root);
    root->unref();
    trans->unref();
    path->unref();

    SoPickedPoint* pick = rp.getPickedPoint();
    return (pick ? new SoPickedPoint(*pick) : 0);
}

SoPickedPoint* View3DInventorViewer::getPointOnRay(const SbVec3f& pos,const SbVec3f& dir, ViewProvider* vp) const
{
    // Note: There seems to be a  bug with setRay() which causes SoRayPickAction
    // to fail to get intersections between the ray and a line
    
    SoPath *path;
    if(vp == editViewProvider && pcEditingRoot->getNumChildren()>1) {
        path = new SoPath(1);
        path->ref();
        path->append(pcEditingRoot);
    }else{
        //first get the path to this node and calculate the current setTransformation
        SoSearchAction sa;
        sa.setNode(vp->getRoot());
        sa.setSearchingAll(true);
        sa.apply(getSoRenderManager()->getSceneGraph());
        path = sa.getPath();
        if (!path)
            return nullptr;
        path->ref();
    }
    SoGetMatrixAction gm(getSoRenderManager()->getViewportRegion());
    gm.apply(path);
    
    // build a temporary scenegraph only keeping this viewproviders nodes and the accumulated 
    // transformation
    SoTransform* trans = new SoTransform;
    trans->ref();
    trans->setMatrix(gm.getMatrix());
    
    SoSeparator* root = new SoSeparator;
    root->ref();
    root->addChild(getSoRenderManager()->getCamera());
    root->addChild(trans);
    root->addChild(path->getTail());
    
    //get the picked point
    SoRayPickAction rp(getSoRenderManager()->getViewportRegion());
    rp.setRay(pos,dir);
    rp.setRadius(getPickRadius());
    rp.apply(root);
    root->unref();
    trans->unref();
    path->unref();

    // returns a copy of the point
    SoPickedPoint* pick = rp.getPickedPoint();
    //return (pick ? pick->copy() : 0); // needs the same instance of CRT under MS Windows
    return (pick ? new SoPickedPoint(*pick) : 0);
}

void View3DInventorViewer::setEditingViewProvider(Gui::ViewProvider* p, int ModNum)
{
    this->editViewProvider = p;
    this->editViewProvider->setEditViewer(this, ModNum);
    addEventCallback(SoEvent::getClassTypeId(), Gui::ViewProvider::eventCallback,this->editViewProvider);
}

/// reset from edit mode
void View3DInventorViewer::resetEditingViewProvider()
{
    if (this->editViewProvider) {

        // In case the event action still has grabbed a node when leaving edit mode
        // force to release it now
        SoEventManager* mgr = getSoEventManager();
        SoHandleEventAction* heaction = mgr->getHandleEventAction();
        if (heaction && heaction->getGrabber())
            heaction->releaseGrabber();

        resetEditingRoot();

        this->editViewProvider->unsetEditViewer(this);
        removeEventCallback(SoEvent::getClassTypeId(), Gui::ViewProvider::eventCallback,this->editViewProvider);
        this->editViewProvider = 0;
    }
}

/// reset from edit mode
SbBool View3DInventorViewer::isEditingViewProvider() const
{
    return this->editViewProvider ? true : false;
}

/// display override mode
void View3DInventorViewer::setOverrideMode(const std::string& mode)
{
    if (mode == overrideMode)
        return;

    shadowInfo->deactivate();
    overrideMode = mode;
    applyOverrideMode();
}

void View3DInventorViewer::applyOverrideMode()
{
    this->overrideBGColor = 0;
    auto views = getDocument()->getViewProvidersOfType(Gui::ViewProvider::getClassTypeId());

    const char * mode = this->overrideMode.c_str();
    if (SoFCUnifiedSelection::DisplayModeNoShading == mode) {
        this->shading = false;
        this->selectionRoot->overrideMode = SoFCUnifiedSelection::DisplayModeNoShading;
        this->getSoRenderManager()->setRenderMode(SoRenderManager::AS_IS);
    }
    else if (SoFCUnifiedSelection::DisplayModeTessellation == mode) {
        this->shading = true;
        this->selectionRoot->overrideMode = SoFCUnifiedSelection::DisplayModeTessellation;
        this->getSoRenderManager()->setRenderMode(SoRenderManager::HIDDEN_LINE);
    }
    else if (SoFCUnifiedSelection::DisplayModeHiddenLine == mode) {
        if(ViewParams::getHiddenLineOverrideBackground())
            this->overrideBGColor = ViewParams::getHiddenLineBackground();
        this->shading = ViewParams::getHiddenLineShaded();
        this->selectionRoot->overrideMode = SoFCUnifiedSelection::DisplayModeHiddenLine;
        this->getSoRenderManager()->setRenderMode(SoRenderManager::AS_IS);
    }
    else if (overrideMode == "Shadow") {
        shadowInfo->activate();
    }
    else {
        this->shading = true;
        this->selectionRoot->overrideMode = overrideMode.c_str();
        this->getSoRenderManager()->setRenderMode(SoRenderManager::AS_IS);
    }
}

void View3DInventorViewer::ShadowInfo::deactivate()
{
    if(pcShadowGroup) {
        auto superScene = static_cast<SoGroup*>(owner->getSoRenderManager()->getSceneGraph());
        int index = superScene->findChild(pcShadowGroup);
        if(index >= 0)
            superScene->replaceChild(index, owner->pcViewProviderRoot);
        pcShadowGroup.reset();
        pcShadowGroundSwitch->whichChild = -1;
    }
}

void View3DInventorViewer::ShadowInfo::activate()
{
    owner->shading = true;

    App::Document *doc = owner->guiDocument?owner->guiDocument->getDocument():nullptr;

    static const char *_ShadowDisplayMode[] = {"Flat Lines", "Shaded", "As Is", nullptr};
    int displayMode = _shadowParam<App::PropertyEnumeration>(doc, "DisplayMode",
            ViewParams::docShadowDisplayMode(), ViewParams::getShadowDisplayMode(),
            [](App::PropertyEnumeration &prop) {
                if (!prop.getEnum().isValid())
                    prop.setEnums(_ShadowDisplayMode);
            });

    App::PropertyBool *flatlines = Base::freecad_dynamic_cast<App::PropertyBool>(
            doc->getPropertyByName("FlatLines"));
    if (flatlines) {
        owner->selectionRoot->overrideMode = flatlines->getValue()?"Shaded":"Flat Lines";
        _shadowSetParam<App::PropertyEnumeration>(doc, "DisplayMode", flatlines->getValue()?0:1);
        doc->removeDynamicProperty("Shadow_FlatLines");
    } else {
        SbName mode;
        switch (displayMode) {
        case 0:
            mode = SoFCUnifiedSelection::DisplayModeFlatLines;
            break;
        case 1:
            mode = SoFCUnifiedSelection::DisplayModeShaded;
            break;
        default:
            mode = SoFCUnifiedSelection::DisplayModeAsIs;
            break;
        }
        if (owner->selectionRoot->overrideMode.getValue() != mode)
            owner->selectionRoot->overrideMode = mode;
    }
    owner->getSoRenderManager()->setRenderMode(SoRenderManager::AS_IS);

    bool spotlight = _shadowParam<App::PropertyBool>(doc, "SpotLight", 
            ViewParams::docShadowSpotLight(), ViewParams::getShadowSpotLight());

    if(pcShadowGroup) {
        if((spotlight && pcShadowGroup->findChild(pcShadowSpotLight)<0)
            || (!spotlight && pcShadowGroup->findChild(pcShadowDirectionalLight)<0))
        {
            coinRemoveAllChildren(pcShadowGroup);
            auto superScene = static_cast<SoGroup*>(owner->getSoRenderManager()->getSceneGraph());
            int index = superScene->findChild(pcShadowGroup);
            if(index >= 0)
                superScene->replaceChild(index, owner->pcViewProviderRoot);
            pcShadowGroup.reset();
        }
    }
    if(!pcShadowGroup) {
        pcShadowGroup = new SoShadowGroup;
        // pcShadowGroup->renderCaching = SoSeparator::OFF;
        // pcShadowGroup->boundingBoxCaching = SoSeparator::OFF;

        if(!pcShadowDirectionalLight)
            pcShadowDirectionalLight = new SoFCDirectionalLight;

        if(!pcShadowSpotLight)
            pcShadowSpotLight = new SoFCSpotLight;

        auto shadowStyle = new SoShadowStyle;
        shadowStyle->style = SoShadowStyle::NO_SHADOWING;
        pcShadowGroup->addChild(shadowStyle);

        if(spotlight)
            pcShadowGroup->addChild(pcShadowSpotLight);
        else
            pcShadowGroup->addChild(pcShadowDirectionalLight);

        shadowStyle = new SoShadowStyle;
        shadowStyle->style = SoShadowStyle::CASTS_SHADOW_AND_SHADOWED;
        pcShadowGroup->addChild(shadowStyle);

        pcShadowPickStyle = new SoPickStyle;
        pcShadowGroup->addChild(pcShadowPickStyle);

        pcShadowGroup->addChild(owner->pcViewProviderRoot);

        if(!pcShadowGroundSwitch) {
            pcShadowGroundSwitch = new SoSwitch;

            pcShadowGroundStyle = new SoShadowStyle;
            pcShadowGroundStyle->style = SoShadowStyle::SHADOWED;

            pcShadowMaterial = new SoMaterial;

            pcShadowGroundTextureCoords = new SoTextureCoordinate2;

            pcShadowGroundTexture = new SoTexture2;
            // pcShadowGroundTexture->model = SoMultiTextureImageElement::BLEND;

            pcShadowGroundCoords = new SoCoordinate3;

            pcShadowGround = new SoFaceSet;

            pcShadowGroundShapeHints = new SoShapeHints;
            pcShadowGroundShapeHints->vertexOrdering = SoShapeHints::COUNTERCLOCKWISE;

            auto pickStyle = new SoPickStyle;
            pickStyle->style = SoPickStyle::UNPICKABLE;

            auto tu = new SoTextureUnit;
            tu->unit = 1;

            pcShadowGroundLightModel = new SoLightModel;

            pcShadowGroundGroup = new SoSeparator;
            pcShadowGroundGroup->addChild(pcShadowGroundLightModel);
            pcShadowGroundGroup->addChild(pickStyle);
            pcShadowGroundGroup->addChild(pcShadowGroundShapeHints);
            pcShadowGroundGroup->addChild(pcShadowGroundTextureCoords);
            pcShadowGroundGroup->addChild(tu);

            // We deliberately insert the same SoTextureCoordinate2 twice.
            // The first one with default texture unit 0, and the second
            // one with unit 1. The reason for unit 1 is because unit 0
            // texture does not work with bump map (Coin3D bug?). The
            // reason for unit 0 texture coordinate is because Coin3D
            // crashes if there is at least one texture coordinate node,
            // but no unit 0 texture coordinate, with the following call
            // stack.
            //
            // SoMultiTextureCoordinateelement::get4()
            // SoMultiTextureCoordinateelement::get4()
            // SoFaceSet::generatePrimitives()
            // SoShape::validatePVCache()
            // SoShape::shouldGLRender()
            // ...
            //
            pcShadowGroundGroup->addChild(pcShadowGroundTextureCoords);

            pcShadowGroundGroup->addChild(pcShadowGroundTexture);
            pcShadowGroundGroup->addChild(pcShadowMaterial);
            pcShadowGroundGroup->addChild(pcShadowGroundCoords);
            pcShadowGroundGroup->addChild(pcShadowGroundStyle);
            pcShadowGroundGroup->addChild(pcShadowGround);

            pcShadowGroundSwitch->addChild(pcShadowGroundGroup);
        }

        pcShadowGroup->addChild(pcShadowGroundSwitch);
    }

    static const App::PropertyFloatConstraint::Constraints _precision_cstr(0.0,1.0,0.1);
    // pcShadowGroup->quality = _shadowParam<App::PropertyFloatConstraint>(
    //         doc, "Quality", 1.0f,
    //         [](App::PropertyFloatConstraint &prop) {
    //             if(!prop.getConstraints())
    //                 prop.setConstraints(&_precision_cstr);
    //         });

    pcShadowGroup->precision = _shadowParam<App::PropertyFloatConstraint>(doc, "Precision",
            ViewParams::docShadowPrecision(), ViewParams::getShadowPrecision(),
            [](App::PropertyFloatConstraint &prop) {
                if(!prop.getConstraints())
                    prop.setConstraints(&_precision_cstr);
            });

    SoLight *light;
    auto _dir = _shadowParam<App::PropertyVector>(
            doc, "LightDirection", nullptr,
            Base::Vector3d(ViewParams::getShadowLightDirectionX(),
                            ViewParams::getShadowLightDirectionY(),
                            ViewParams::getShadowLightDirectionZ()));
    _dir.Normalize();
    SbVec3f dir(_dir.x,_dir.y,_dir.z);

    SbBox3f bbox;
    owner->getSceneBoundBox(bbox);

    static const App::PropertyPrecision::Constraints _epsilon_cstr(0.0,1000.0,1e-5);
    pcShadowGroup->epsilon = _shadowParam<App::PropertyPrecision>(doc, "Epsilon",
            ViewParams::docShadowEpsilon(), ViewParams::getShadowEpsilon(),
            [](App::PropertyFloatConstraint &prop) {
                if(prop.getConstraints() != &_epsilon_cstr)
                    prop.setConstraints(&_epsilon_cstr);
            });

    static const App::PropertyFloatConstraint::Constraints _threshold_cstr(0.0,1.0,0.1);
    pcShadowGroup->threshold = _shadowParam<App::PropertyFloatConstraint>(doc, "Threshold",
            ViewParams::docShadowThreshold(), ViewParams::getShadowThreshold(),
            [](App::PropertyFloatConstraint &prop) {
                if(prop.getConstraints() != &_threshold_cstr)
                    prop.setConstraints(&_threshold_cstr);
            });

    if(spotlight) {
        light = pcShadowSpotLight;
        pcShadowSpotLight->direction = dir;
        Base::Vector3d initPos;
        if(!bbox.isEmpty()) {
            SbVec3f center = bbox.getCenter();
            initPos.x = center[0];
            initPos.y = center[1];
            initPos.z = center[2] + (_dir.z < 0 ? 1.0f : -1.0f) * (bbox.getMax()[2] - bbox.getMin()[2]);
        }
        auto pos = _shadowParam<App::PropertyVector>(doc, "SpotLightPosition", nullptr, initPos);
        pcShadowSpotLight->location = SbVec3f(pos.x,pos.y,pos.z);
        static const App::PropertyFloatConstraint::Constraints _drop_cstr(-0.01,1.0,0.01);
        pcShadowSpotLight->dropOffRate =
            _shadowParam<App::PropertyFloatConstraint>(doc, "SpotLightDropOffRate", nullptr, 0.0,
                [](App::PropertyFloatConstraint &prop) {
                    if(!prop.getConstraints())
                        prop.setConstraints(&_drop_cstr);
                });
        pcShadowSpotLight->cutOffAngle =
            M_PI * _shadowParam<App::PropertyAngle>(doc, "SpotLightCutOffAngle", nullptr, 45.0) / 180.0;

        // pcShadowGroup->visibilityFlag = SoShadowGroup::ABSOLUTE_RADIUS;
        // pcShadowGroup->visibilityNearRadius = _shadowParam<App::PropertyFloat>(doc, "SpotLightRadiusNear", -1.0);
        // pcShadowGroup->visibilityRadius = _shadowParam<App::PropertyFloat>(doc, "SpotLightRadius", -1.0);
    } else {
        pcShadowDirectionalLight->direction = dir;

        light = pcShadowDirectionalLight;
        if(light->isOfType(SoShadowDirectionalLight::getClassTypeId())) {
            static const App::PropertyFloatConstraint::Constraints _dist_cstr(-1.0,DBL_MAX,10.0);
            static_cast<SoShadowDirectionalLight*>(light)->maxShadowDistance = 
                _shadowParam<App::PropertyFloatConstraint>(doc, "MaxDistance",
                    ViewParams::docShadowMaxDistance(), ViewParams::getShadowMaxDistance(),
                    [](App::PropertyFloatConstraint &prop) {
                        if(!prop.getConstraints())
                            prop.setConstraints(&_dist_cstr);
                    });
        }
    }

    static const App::PropertyFloatConstraint::Constraints _cstr(0.0,1000.0,0.1);
    light->intensity = _shadowParam<App::PropertyFloatConstraint>(doc, "LightIntensity",
            ViewParams::docShadowLightIntensity(), ViewParams::getShadowLightIntensity(),
            [](App::PropertyFloatConstraint &prop) {
                if(!prop.getConstraints())
                    prop.setConstraints(&_cstr);
            });

    App::Color color = _shadowParam<App::PropertyColor>(doc, "LightColor",
            ViewParams::docShadowLightColor(), App::Color((uint32_t)ViewParams::getShadowLightColor()));
    SbColor sbColor;
    float f;
    sbColor.setPackedValue(color.getPackedValue(),f);
    light->color = sbColor;

    color = _shadowParam<App::PropertyColor>(doc, "GroundColor",
            ViewParams::docShadowGroundColor(), App::Color((uint32_t)ViewParams::getShadowGroundColor()));
    sbColor.setPackedValue(color.getPackedValue(),f);
    pcShadowMaterial->diffuseColor = sbColor;
    pcShadowMaterial->specularColor = SbColor(0,0,0);

    static const App::PropertyFloatConstraint::Constraints _transp_cstr(0.0,1.0,0.1);
    double transp = _shadowParam<App::PropertyFloatConstraint>(doc, "GroundTransparency",
            ViewParams::docShadowGroundTransparency(), ViewParams::getShadowGroundTransparency(),
            [](App::PropertyFloatConstraint &prop) {
                if(!prop.getConstraints())
                    prop.setConstraints(&_transp_cstr);
            });

    if(_shadowParam<App::PropertyBool>(doc, "GroundBackFaceCull",
            ViewParams::docShadowGroundBackFaceCull(), ViewParams::getShadowGroundBackFaceCull()))
        pcShadowGroundShapeHints->shapeType = SoShapeHints::SOLID;
    else
        pcShadowGroundShapeHints->shapeType = SoShapeHints::UNKNOWN_SHAPE_TYPE;

    pcShadowMaterial->transparency = transp;
    pcShadowGroundStyle->style = (transp == 1.0 ? 0x4 : 0) | SoShadowStyle::SHADOWED;

    if(_shadowParam<App::PropertyBool>(doc, "ShowGround",
            ViewParams::docShadowShowGround(), ViewParams::getShadowShowGround()))
        pcShadowGroundSwitch->whichChild = 0;
    else
        pcShadowGroundSwitch->whichChild = -1;

    if(!bbox.isEmpty())
        updateShadowGround(bbox);

    pcShadowGroundTexture->filename = _shadowParam<App::PropertyFileIncluded>(doc, "GroundTexture",
            ViewParams::docShadowGroundTexture(), ViewParams::getShadowGroundTexture().c_str());
    
    const char *bumpmap = _shadowParam<App::PropertyFileIncluded>(doc, "GroundBumpMap",
            ViewParams::docShadowGroundBumpMap(), ViewParams::getShadowGroundBumpMap().c_str());
    if(bumpmap && bumpmap[0]) {
        if(!pcShadowGroundBumpMap) {
            pcShadowGroundBumpMap = new SoBumpMap;
        }
        pcShadowGroundBumpMap->filename = bumpmap;
        if (pcShadowGroundGroup->findChild(pcShadowGroundBumpMap) < 0) {
            int idx = pcShadowGroundGroup->findChild(pcShadowMaterial);
            if (idx >= 0)
                pcShadowGroundGroup->insertChild(pcShadowGroundBumpMap,idx);
        }
    } else if (pcShadowGroundBumpMap) {
        int idx = pcShadowGroundGroup->findChild(pcShadowGroundBumpMap);
        if (idx >= 0)
            pcShadowGroundGroup->removeChild(idx);
    }

    if(_shadowParam<App::PropertyBool>(doc, "GroundShading",
            ViewParams::docShadowGroundShading(), ViewParams::getShadowGroundShading()))
        pcShadowGroundLightModel->model = SoLightModel::PHONG;
    else
        pcShadowGroundLightModel->model = SoLightModel::BASE_COLOR;

    SbBool isActive = TRUE;
    if (_shadowParam<App::PropertyBool>(doc, "TransparentShadow",
            ViewParams::docShadowTransparentShadow(), ViewParams::getShadowTransparentShadow()))
        isActive |= 2;
    if (pcShadowGroup->isActive.getValue() != isActive)
        pcShadowGroup->isActive = isActive;

    auto superScene = static_cast<SoGroup*>(owner->getSoRenderManager()->getSceneGraph());
    int index = superScene->findChild(owner->pcViewProviderRoot);
    if(index >= 0)
        superScene->replaceChild(index, pcShadowGroup);
}

void View3DInventorViewer::setViewportCB(void*, SoAction* action)
{
    // Make sure to override the value set inside SoOffscreenRenderer::render()
    if (action->isOfType(SoGLRenderAction::getClassTypeId())) {
        SoFCOffscreenRenderer& renderer = SoFCOffscreenRenderer::instance();
        const SbViewportRegion& vp = renderer.getViewportRegion();
        SoViewportRegionElement::set(action->getState(), vp);
        static_cast<SoGLRenderAction*>(action)->setViewportRegion(vp);
    }
}

void View3DInventorViewer::clearBufferCB(void*, SoAction* action)
{
    if (action->isOfType(SoGLRenderAction::getClassTypeId())) {
        // do stuff specific for GL rendering here.
        glClear(GL_DEPTH_BUFFER_BIT);
    }
}

void View3DInventorViewer::setGLWidgetCB(void* userdata, SoAction* action)
{
    //FIXME: This causes the Coin error message:
    // Coin error in SoNode::GLRenderS(): GL error: 'GL_STACK_UNDERFLOW', nodetype:
    // Separator (set envvar COIN_GLERROR_DEBUGGING=1 and re-run to get more information)
    if (action->isOfType(SoGLRenderAction::getClassTypeId())) {
        QWidget* gl = reinterpret_cast<QWidget*>(userdata);
        SoGLWidgetElement::set(action->getState(), qobject_cast<QtGLWidget*>(gl));
    }
}

void View3DInventorViewer::handleEventCB(void* ud, SoEventCallback* n)
{
    View3DInventorViewer* that = reinterpret_cast<View3DInventorViewer*>(ud);
    SoGLRenderAction* glra = that->getSoRenderManager()->getGLRenderAction();
    SoAction* action = n->getAction();
    SoGLRenderActionElement::set(action->getState(), glra);
    SoGLWidgetElement::set(action->getState(), qobject_cast<QtGLWidget*>(that->getGLWidget()));
}

void View3DInventorViewer::setGradientBackground(bool on)
{
    int whichChild = on?0:-1;
    if(pcBackGroundSwitch->whichChild.getValue() != whichChild)
        pcBackGroundSwitch->whichChild.setValue(whichChild);
}

bool View3DInventorViewer::hasGradientBackground() const
{
    return pcBackGroundSwitch->whichChild.getValue() == 0;
}

void View3DInventorViewer::setGradientBackgroundColor(const SbColor& fromColor,
                                                      const SbColor& toColor)
{
    pcBackGround->setColorGradient(fromColor, toColor);
}

void View3DInventorViewer::setGradientBackgroundColor(const SbColor& fromColor,
                                                      const SbColor& toColor,
                                                      const SbColor& midColor)
{
    pcBackGround->setColorGradient(fromColor, toColor, midColor);
}

void View3DInventorViewer::setEnabledFPSCounter(bool on)
{
    fpsEnabled = on;
}

void View3DInventorViewer::setEnabledVBO(bool on)
{
    vboEnabled = on;
}

bool View3DInventorViewer::isEnabledVBO() const
{
    return vboEnabled;
}

void View3DInventorViewer::setRenderCache(int mode)
{
    static int canAutoCache = -1;

    if (mode<0) {
        // Work around coin bug of unmatched call of
        // SoGLLazyElement::begin/endCaching() when on top rendering
        // transparent object with SORTED_OBJECT_SORTED_TRIANGLE_BLEND
        // transparency type.
        //
        // For more details see:
        // https://forum.freecadweb.org/viewtopic.php?f=18&t=43305&start=10#p412537
        coin_setenv("COIN_AUTO_CACHING", "0", TRUE);

        int setting = ViewParams::instance()->getRenderCache();
        if (mode == -2) {
            if (pcViewProviderRoot && setting != 1)
                pcViewProviderRoot->renderCaching = SoSeparator::ON;
            mode = 2;
        }
        else {
            if (pcViewProviderRoot)
                pcViewProviderRoot->renderCaching = SoSeparator::AUTO;
            mode = setting;
        }
    }

    if (canAutoCache < 0) {
        const char *env = coin_getenv("COIN_AUTO_CACHING");
        canAutoCache = env ? atoi(env) : 1;
    }

    // If coin auto cache is disabled, do not use 'Auto' render cache mode, but
    // fallback to 'Distributed' mode.
    if (!canAutoCache && mode != 2 && mode != 3)
        mode = 1;

    auto caching = mode == 0 ? SoSeparator::AUTO :
                  (mode == 1 ? SoSeparator::ON :
                               SoSeparator::OFF);

    if (this->selectionRoot)
        this->selectionRoot->renderCaching = mode == 3 ?
            SoSeparator::OFF : SoSeparator::ON;
    SoFCSeparator::setCacheMode(caching);
}

void View3DInventorViewer::setEnabledNaviCube(bool on)
{
    naviCubeEnabled = on;
}

bool View3DInventorViewer::isEnabledNaviCube(void) const
{
    return naviCubeEnabled;
}

void View3DInventorViewer::setNaviCubeCorner(int c)
{
    if (naviCube)
        naviCube->setCorner(static_cast<NaviCube::Corner>(c));
}

NaviCube* View3DInventorViewer::getNavigationCube() const
{
    return naviCube;
}

void View3DInventorViewer::setAxisCross(bool on)
{
    SoNode* scene = getSoRenderManager()->getSceneGraph();
    SoSeparator* sep = static_cast<SoSeparator*>(scene);

    if (on) {
        if (!axisGroup) {
            axisCross = new Gui::SoShapeScale;
            Gui::SoAxisCrossKit* axisKit = new Gui::SoAxisCrossKit();
            axisKit->set("xAxis.appearance.drawStyle", "lineWidth 2");
            axisKit->set("yAxis.appearance.drawStyle", "lineWidth 2");
            axisKit->set("zAxis.appearance.drawStyle", "lineWidth 2");
            axisCross->setPart("shape", axisKit);
            axisCross->scaleFactor = 1.0f;
            axisGroup = new SoSkipBoundingGroup;
            axisGroup->addChild(axisCross);

            sep->addChild(axisGroup);
        }
    }
    else {
        if (axisGroup) {
            sep->removeChild(axisGroup);
            axisGroup = 0;
        }
    }
}

bool View3DInventorViewer::hasAxisCross(void)
{
    return axisGroup;
}

void View3DInventorViewer::setNavigationType(Base::Type t)
{
    if (t.isBad())
        return;

    this->winGestureTuneState = View3DInventorViewer::ewgtsNeedTuning; //triggers enable/disable rotation gesture when preferences change

    if (this->navigation && this->navigation->getTypeId() == t)
        return; // nothing to do

    Base::BaseClass* base = static_cast<Base::BaseClass*>(t.createInstance());
    if (!base)
        return;

    if (!base->getTypeId().isDerivedFrom(NavigationStyle::getClassTypeId())) {
        delete base;
#if FC_DEBUG
        SoDebugError::postWarning("View3DInventorViewer::setNavigationType",
                                  "Navigation object must be of type NavigationStyle.");
#endif // FC_DEBUG
        return;
    }

    NavigationStyle* ns = static_cast<NavigationStyle*>(base);
    if (this->navigation) {
        ns->operator = (*this->navigation);
        delete this->navigation;
    }
    this->navigation = ns;
    this->navigation->setViewer(this);
}

NavigationStyle* View3DInventorViewer::navigationStyle() const
{
    return this->navigation;
}

SoDirectionalLight* View3DInventorViewer::getBacklight(void) const
{
    return this->backlight;
}

void View3DInventorViewer::setBacklight(SbBool on)
{
    this->backlight->on = on;
}

SbBool View3DInventorViewer::isBacklight(void) const
{
    return this->backlight->on.getValue();
}

void View3DInventorViewer::setSceneGraph(SoNode* root)
{
    inherited::setSceneGraph(root);
    if (!root) {
        _ViewProviderSet.clear();
        editViewProvider = 0;
    }

    SoSearchAction sa;
    sa.setNode(this->backlight);
    //we want the rendered scene with all lights and cameras, viewer->getSceneGraph would return
    //the geometry scene only
    SoNode* scene = this->getSoRenderManager()->getSceneGraph();
    if (scene && scene->getTypeId().isDerivedFrom(SoSeparator::getClassTypeId())) {
        sa.apply(scene);
        if (!sa.getPath())
            static_cast<SoSeparator*>(scene)->insertChild(this->backlight, 0);
    }
}

void View3DInventorViewer::savePicture(int w, int h, int s, const QColor& bg, QImage& img) const
{
    // Save picture methods:
    // FramebufferObject -- viewer renders into FBO (no offscreen)
    // CoinOffscreenRenderer -- Coin's offscreen rendering method
    // PixelBuffer -- Qt's pixel buffer used for offscreen rendering (only Qt4)
    // Otherwise (Default) -- Qt's FBO used for offscreen rendering
    std::string saveMethod = App::GetApplication().GetParameterGroupByPath
        ("User parameter:BaseApp/Preferences/View")->GetASCII("SavePicture");

    bool useFramebufferObject = false;
    bool useGrabFramebuffer = false;
    bool usePixelBuffer = false;
    bool useCoinOffscreenRenderer = false;
    if (saveMethod == "FramebufferObject") {
        useFramebufferObject = true;
    }
    else if (saveMethod == "GrabFramebuffer") {
        useGrabFramebuffer = true;
    }
    else if (saveMethod == "PixelBuffer") {
        usePixelBuffer = true;
    }
    else if (saveMethod == "CoinOffscreenRenderer") {
        useCoinOffscreenRenderer = true;
    }

    if (useFramebufferObject) {
        View3DInventorViewer* self = const_cast<View3DInventorViewer*>(this);
        self->imageFromFramebuffer(w, h, s, bg, img);
        return;
    }
    else if (useGrabFramebuffer) {
        View3DInventorViewer* self = const_cast<View3DInventorViewer*>(this);
        img = self->grabFramebuffer();
        img = img.mirrored();
        img = img.scaledToWidth(w);
        return;
    }

    // if no valid color use the current background
    bool useBackground = false;
    SbViewportRegion vp(getSoRenderManager()->getViewportRegion());

    if (w>0 && h>0)
        vp.setWindowSize((short)w, (short)h);

    //NOTE: To support pixels per inch we must use SbViewportRegion::setPixelsPerInch( ppi );
    //The default value is 72.0.
    //If we need to support grayscale images with must either use SoOffscreenRenderer::LUMINANCE or
    //SoOffscreenRenderer::LUMINANCE_TRANSPARENCY.

    SoCallback* cb = 0;

    // for an invalid color use the viewer's current background color
    QColor bgColor;
    if (!bg.isValid()) {
        if (!hasGradientBackground()) {
            bgColor = this->backgroundColor();
        }
        else {
            useBackground = true;
            cb = new SoCallback;
            cb->setCallback(clearBufferCB);
        }
    }
    else {
        bgColor = bg;
    }

    SoSeparator* root = new SoSeparator;
    root->ref();

#if (COIN_MAJOR_VERSION >= 4)
    // The behaviour in Coin4 has changed so that when using the same instance of 'SoFCOffscreenRenderer'
    // multiple times internally the biggest viewport size is stored and set to the SoGLRenderAction.
    // The trick is to add a callback node and override the viewport size with what we want.
    if (useCoinOffscreenRenderer) {
        SoCallback* cbvp = new SoCallback;
        cbvp->setCallback(setViewportCB);
        root->addChild(cbvp);
    }
#endif

    SoCamera* camera = getSoRenderManager()->getCamera();

    if (useBackground) {
        root->addChild(backgroundroot);
        root->addChild(cb);
    }

    if (!this->shading) {
        SoLightModel* lm = new SoLightModel;
        lm->model = SoLightModel::BASE_COLOR;
        root->addChild(lm);
    }

    root->addChild(getHeadlight());
    root->addChild(camera);
    SoCallback* gl = new SoCallback;
    gl->setCallback(setGLWidgetCB, this->getGLWidget());
    root->addChild(gl);
    root->addChild(pcGroupOnTopSwitch);
    root->addChild(pcViewProviderRoot);

#if !defined(HAVE_QT5_OPENGL)
    if (useBackground)
        root->addChild(cb);
#endif

    root->addChild(foregroundroot);

    try {
        // render the scene
        if (!useCoinOffscreenRenderer) {
            SoQtOffscreenRenderer renderer(vp);
            renderer.setNumPasses(s);
            renderer.setInternalTextureFormat(getInternalTextureFormat());
            renderer.setPbufferEnable(usePixelBuffer);
            if (bgColor.isValid())
                renderer.setBackgroundColor(SbColor4f(bgColor.redF(), bgColor.greenF(), bgColor.blueF(), bgColor.alphaF()));
            if (!renderer.render(root))
                throw Base::RuntimeError("Offscreen rendering failed");

            renderer.writeToImage(img);
            root->unref();
        }
        else {
            SoFCOffscreenRenderer& renderer = SoFCOffscreenRenderer::instance();
            renderer.setViewportRegion(vp);
            renderer.getGLRenderAction()->setSmoothing(true);
            renderer.getGLRenderAction()->setNumPasses(s);
            renderer.getGLRenderAction()->setTransparencyType(SoGLRenderAction::SORTED_OBJECT_SORTED_TRIANGLE_BLEND);
            if (bgColor.isValid())
                renderer.setBackgroundColor(SbColor(bgColor.redF(), bgColor.greenF(), bgColor.blueF()));
            if (!renderer.render(root))
                throw Base::RuntimeError("Offscreen rendering failed");

            renderer.writeToImage(img);
            root->unref();
        }

        if (!bgColor.isValid() || bgColor.alphaF() == 1.0) {
            QImage image(img.width(), img.height(), QImage::Format_RGB32);
            QPainter painter(&image);
            painter.fillRect(image.rect(), Qt::black);
            painter.drawImage(0, 0, img);
            painter.end();
            img = image;
        }
    }
    catch (...) {
        root->unref();
        throw; // re-throw exception
    }
}

void View3DInventorViewer::saveGraphic(int pagesize, const QColor& bgcolor, SoVectorizeAction* va) const
{
    if (bgcolor.isValid())
        va->setBackgroundColor(true, SbColor(bgcolor.redF(), bgcolor.greenF(), bgcolor.blueF()));

    float border = 10.0f;
    SbVec2s vpsize = this->getSoRenderManager()->getViewportRegion().getViewportSizePixels();
    float vpratio = ((float)vpsize[0]) / ((float)vpsize[1]);

    if (vpratio > 1.0f) {
        va->setOrientation(SoVectorizeAction::LANDSCAPE);
        vpratio = 1.0f / vpratio;
    }
    else {
        va->setOrientation(SoVectorizeAction::PORTRAIT);
    }

    va->beginStandardPage(SoVectorizeAction::PageSize(pagesize), border);

    // try to fill as much "paper" as possible
    SbVec2f size = va->getPageSize();

    float pageratio = size[0] / size[1];
    float xsize, ysize;

    if (pageratio < vpratio) {
        xsize = size[0];
        ysize = xsize / vpratio;
    }
    else {
        ysize = size[1];
        xsize = ysize * vpratio;
    }

    float offx = border + (size[0]-xsize) * 0.5f;
    float offy = border + (size[1]-ysize) * 0.5f;

    va->beginViewport(SbVec2f(offx, offy), SbVec2f(xsize, ysize));
    va->calibrate(this->getSoRenderManager()->getViewportRegion());

    va->apply(this->getSoRenderManager()->getSceneGraph());

    va->endViewport();
    va->endPage();
}

AbstractMouseSelection *
View3DInventorViewer::startSelection(View3DInventorViewer::SelectionMode mode)
{
    navigation->startSelection(NavigationStyle::SelectionMode(mode));
    return navigation->currentSelection();
}

void View3DInventorViewer::stopSelection()
{
    navigation->stopSelection();
}

bool View3DInventorViewer::isSelecting() const
{
    return navigation->isSelecting();
}

const std::vector<SbVec2s>& View3DInventorViewer::getPolygon(SelectionRole* role) const
{
    return navigation->getPolygon(role);
}

void View3DInventorViewer::setSelectionEnabled(const SbBool enable)
{
    selectionRoot->selectionRole.setValue(enable);
}

SbBool View3DInventorViewer::isSelectionEnabled(void) const
{
    return selectionRoot->selectionRole.getValue();
}

SbVec2f View3DInventorViewer::screenCoordsOfPath(SoPath* path) const
{
    // Generate a matrix (well, a SoGetMatrixAction) that
    // moves us to the picked object's coordinate space.
    SoGetMatrixAction gma(getSoRenderManager()->getViewportRegion());
    gma.apply(path);

    // Use that matrix to translate the origin in the picked
    // object's coordinate space into object space
    SbVec3f imageCoords(0, 0, 0);
    SbMatrix m = gma.getMatrix().transpose();
    m.multMatrixVec(imageCoords, imageCoords);

    // Now, project the object space coordinates of the object
    // into "normalized" screen coordinates.
    SbViewVolume  vol = getSoRenderManager()->getCamera()->getViewVolume();
    vol.projectToScreen(imageCoords, imageCoords);

    // Translate "normalized" screen coordinates to pixel coords.
    //
    // Note: for some reason, projectToScreen() doesn't seem to
    // handle non-square viewports properly.  The X and Y are
    // scaled such that [0,1] fits within the smaller of the window
    // width or height.  For instance, in a window that's 400px
    // tall and 800px wide, the Y will be within [0,1], but X can
    // vary within [-0.5,1.5]...
    int width = getGLWidget()->width(),
        height = getGLWidget()->height();

    if (width >= height) {
        // "Landscape" orientation, to square
        imageCoords[0] *= height;
        imageCoords[0] += (width-height) / 2.0;
        imageCoords[1] *= height;

    }
    else {
        // "Portrait" orientation
        imageCoords[0] *= width;
        imageCoords[1] *= width;
        imageCoords[1] += (height-width) / 2.0;
    }

    return SbVec2f(imageCoords[0], imageCoords[1]);
}

std::vector<SbVec2f> View3DInventorViewer::getGLPolygon(const std::vector<SbVec2s>& pnts) const
{
    const SbViewportRegion& vp = this->getSoRenderManager()->getViewportRegion();
    const SbVec2s& sz = vp.getWindowSize();
    short w,h;
    sz.getValue(w,h);
    const SbVec2s& sp = vp.getViewportSizePixels();
    const SbVec2s& op = vp.getViewportOriginPixels();
    const SbVec2f& siz = vp.getViewportSize();
    float dX, dY;
    siz.getValue(dX, dY);
    float fRatio = vp.getViewportAspectRatio();

    std::vector<SbVec2f> poly;
    for (std::vector<SbVec2s>::const_iterator it = pnts.begin(); it != pnts.end(); ++it) {
        SbVec2s loc = *it - op;
        SbVec2f pos((float)loc[0]/(float)sp[0], (float)loc[1]/(float)sp[1]);
        float pX,pY;
        pos.getValue(pX,pY);

        // now calculate the real points respecting aspect ratio information
        //
        if (fRatio > 1.0f) {
            pX = (pX - 0.5f*dX) * fRatio + 0.5f*dX;
            pos.setValue(pX,pY);
        }
        else if (fRatio < 1.0f) {
            pY = (pY - 0.5f*dY) / fRatio + 0.5f*dY;
            pos.setValue(pX,pY);
        }

        poly.push_back(pos);
    }

    return poly;
}

std::vector<SbVec2f> View3DInventorViewer::getGLPolygon(SelectionRole* role) const
{
    const std::vector<SbVec2s>& pnts = navigation->getPolygon(role);
    return getGLPolygon(pnts);
}

// defined in SoFCDB.cpp
extern SoNode* replaceSwitchesInSceneGraph(SoNode*);

void View3DInventorViewer::dump(const char *filename, bool onlyVisible) const
{
    SoGetPrimitiveCountAction action;
    action.setCanApproximate(true);

    SoNode *node;
    if(overrideMode == "Shadow" && shadowInfo->pcShadowGroup)
        node = shadowInfo->pcShadowGroup;
    else
        node = pcViewProviderRoot;

    action.apply(node);
    if (onlyVisible) {
        node = replaceSwitchesInSceneGraph(node);
        node->ref();
    }

    if ( action.getTriangleCount() > 100000 || action.getPointCount() > 30000 || action.getLineCount() > 10000 )
        dumpToFile(node, filename, true);
    else
        dumpToFile(node, filename, false);

    if (onlyVisible) {
        node->unref();
    }
}

bool View3DInventorViewer::dumpToFile(SoNode* node, const char* filename, bool binary) const
{
    bool ret = false;
    Base::FileInfo fi(filename);

    if (fi.hasExtension("idtf") || fi.hasExtension("svg")) {
        int ps=4;
        QColor c = Qt::white;
        std::unique_ptr<SoVectorizeAction> vo;

        if (fi.hasExtension("svg")) {
            vo = std::unique_ptr<SoVectorizeAction>(new SoFCVectorizeSVGAction());
        }
        else if (fi.hasExtension("idtf")) {
            vo = std::unique_ptr<SoVectorizeAction>(new SoFCVectorizeU3DAction());
        }
        else if (fi.hasExtension("ps") || fi.hasExtension("eps")) {
            vo = std::unique_ptr<SoVectorizeAction>(new SoVectorizePSAction());
        }
        else {
            throw Base::ValueError("Not supported vector graphic");
        }

        SoVectorOutput* out = vo->getOutput();
        if (!out || !out->openFile(filename)) {
            std::ostringstream a_out;
            a_out << "Cannot open file '" << filename << "'";
            throw Base::FileSystemError(a_out.str());
        }

        saveGraphic(ps,c,vo.get());
        out->closeFile();
    }
    else {
        // Try VRML and Inventor format
        ret = SoFCDB::writeToFile(node, filename, binary);
    }

    return ret;
}

/**
 * Sets the SoFCInteractiveElement to \a true.
 */
void View3DInventorViewer::interactionStartCB(void*, SoQTQuarterAdaptor* viewer)
{
    SoGLRenderAction* glra = viewer->getSoRenderManager()->getGLRenderAction();
    SoFCInteractiveElement::set(glra->getState(), viewer->getSceneGraph(), true);
}

/**
 * Sets the SoFCInteractiveElement to \a false and forces a redraw.
 */
void View3DInventorViewer::interactionFinishCB(void*, SoQTQuarterAdaptor* viewer)
{
    SoGLRenderAction* glra = viewer->getSoRenderManager()->getGLRenderAction();
    SoFCInteractiveElement::set(glra->getState(), viewer->getSceneGraph(), false);
    viewer->redraw();
}

/**
 * Logs the type of the action that traverses the Inventor tree.
 */
void View3DInventorViewer::interactionLoggerCB(void*, SoAction* action)
{
    Base::Console().Log("%s\n", action->getTypeId().getName().getString());
}

void View3DInventorViewer::addGraphicsItem(GLGraphicsItem* item)
{
    this->graphicsItems.push_back(item);
}

void View3DInventorViewer::removeGraphicsItem(GLGraphicsItem* item)
{
    this->graphicsItems.remove(item);
}

std::list<GLGraphicsItem*> View3DInventorViewer::getGraphicsItems() const
{
    return graphicsItems;
}

std::list<GLGraphicsItem*> View3DInventorViewer::getGraphicsItemsOfType(const Base::Type& type) const
{
    std::list<GLGraphicsItem*> items;
    for (std::list<GLGraphicsItem*>::const_iterator it = this->graphicsItems.begin(); it != this->graphicsItems.end(); ++it) {
        if ((*it)->isDerivedFrom(type))
            items.push_back(*it);
    }

    return items;
}

void View3DInventorViewer::clearGraphicsItems()
{
    this->graphicsItems.clear();
}

int View3DInventorViewer::getNumSamples()
{
    int samples = App::GetApplication().GetParameterGroupByPath
        ("User parameter:BaseApp/Preferences/View")->GetInt("AntiAliasing", 0);

    switch (samples) {
    case View3DInventorViewer::MSAA2x:
        return 2;
    case View3DInventorViewer::MSAA4x:
        return 4;
    case View3DInventorViewer::MSAA8x:
        return 8;
    case View3DInventorViewer::Smoothing:
        return 1;
    default:
        return 0;
    }
}

GLenum View3DInventorViewer::getInternalTextureFormat() const
{
#if defined(HAVE_QT5_OPENGL)
    ParameterGrp::handle hGrp = App::GetApplication().GetParameterGroupByPath
        ("User parameter:BaseApp/Preferences/View");
    std::string format = hGrp->GetASCII("InternalTextureFormat", "Default");

    if (format == "GL_RGB") {
        return GL_RGB;
    }
    else if (format == "GL_RGBA") {
        return GL_RGBA;
    }
    else if (format == "GL_RGB8") {
        return GL_RGB8;
    }
    else if (format == "GL_RGBA8") {
        return GL_RGBA8;
    }
    else if (format == "GL_RGB10") {
        return GL_RGB10;
    }
    else if (format == "GL_RGB10_A2") {
        return GL_RGB10_A2;
    }
    else if (format == "GL_RGB16") {
        return GL_RGB16;
    }
    else if (format == "GL_RGBA16") {
        return GL_RGBA16;
    }
    else if (format == "GL_RGB32F") {
        return GL_RGB32F_ARB;
    }
    else if (format == "GL_RGBA32F") {
        return GL_RGBA32F_ARB;
    }
    else {
        QOpenGLFramebufferObjectFormat fboFormat;
        return fboFormat.internalTextureFormat();
    }
#else
    //return GL_RGBA;
    return GL_RGB;
#endif
}

void View3DInventorViewer::setRenderType(const RenderType type)
{
    renderType = type;

    glImage = QImage();
    if (type != Framebuffer) {
        delete framebuffer;
        framebuffer = 0;
    }

    switch (type) {
    case Native:
        break;
    case Framebuffer:
        if (!framebuffer) {
            const SbViewportRegion vp = this->getSoRenderManager()->getViewportRegion();
            SbVec2s size = vp.getViewportSizePixels();
            int width = size[0];
            int height = size[1];

            QtGLWidget* gl = static_cast<QtGLWidget*>(this->viewport());
            gl->makeCurrent();
#if !defined(HAVE_QT5_OPENGL)
            framebuffer = new QtGLFramebufferObject(width, height, QtGLFramebufferObject::Depth);
            renderToFramebuffer(framebuffer);
#else
            QOpenGLFramebufferObjectFormat fboFormat;
            fboFormat.setSamples(getNumSamples());
            fboFormat.setAttachment(QtGLFramebufferObject::Depth);
            QtGLFramebufferObject* fbo = new QtGLFramebufferObject(width, height, fboFormat);
            if (fbo->format().samples() > 0) {
                renderToFramebuffer(fbo);
                framebuffer = new QtGLFramebufferObject(fbo->size());
                // this is needed to be able to render the texture later
                QOpenGLFramebufferObject::blitFramebuffer(framebuffer, fbo);
                delete fbo;
            }
            else {
                renderToFramebuffer(fbo);
                framebuffer = fbo;
            }
#endif
        }
        break;
    case Image:
        {
            glImage = grabFramebuffer();
        }
        break;
    }
}

View3DInventorViewer::RenderType View3DInventorViewer::getRenderType() const
{
    return this->renderType;
}

QImage View3DInventorViewer::grabFramebuffer()
{
    QtGLWidget* gl = static_cast<QtGLWidget*>(this->viewport());
    gl->makeCurrent();

    QImage res;
#if !defined(HAVE_QT5_OPENGL)
    int w = gl->width();
    int h = gl->height();
    QImage img(QSize(w,h), QImage::Format_RGB32);
    glReadPixels(0, 0, w, h, GL_BGRA, GL_UNSIGNED_BYTE, img.bits());
    res = img;
#else
    const SbViewportRegion vp = this->getSoRenderManager()->getViewportRegion();
    SbVec2s size = vp.getViewportSizePixels();
    int width = size[0];
    int height = size[1];

    int samples = getNumSamples();
    if (samples == 0) {
        // if anti-aliasing is off we can directly use glReadPixels
        QImage img(QSize(width, height), QImage::Format_RGB32);
        glReadPixels(0, 0, width, height, GL_BGRA, GL_UNSIGNED_BYTE, img.bits());
        res = img;
    }
    else {
        QOpenGLFramebufferObjectFormat fboFormat;
        fboFormat.setSamples(getNumSamples());
        fboFormat.setAttachment(QOpenGLFramebufferObject::Depth);
        fboFormat.setTextureTarget(GL_TEXTURE_2D);
        fboFormat.setInternalTextureFormat(getInternalTextureFormat());

        QOpenGLFramebufferObject fbo(width, height, fboFormat);
        renderToFramebuffer(&fbo);

        res = fbo.toImage(false);

        QImage image(res.width(), res.height(), QImage::Format_RGB32);
        QPainter painter(&image);
        painter.fillRect(image.rect(),Qt::black);
        painter.drawImage(0, 0, res);
        painter.end();
        res = image;
    }
#endif

    return res;
}

void View3DInventorViewer::imageFromFramebuffer(int width, int height, int samples,
                                                const QColor& bgcolor, QImage& img)
{
    QtGLWidget* gl = static_cast<QtGLWidget*>(this->viewport());
    gl->makeCurrent();

    const QtGLContext* context = QtGLContext::currentContext();
    if (!context) {
        Base::Console().Warning("imageFromFramebuffer failed because no context is active\n");
        return;
    }

    QtGLFramebufferObjectFormat fboFormat;
    fboFormat.setSamples(samples);
    fboFormat.setAttachment(QtGLFramebufferObject::Depth);
    // With enabled alpha a transparent background is supported but
    // at the same time breaks semi-transparent models. A workaround
    // is to use a certain background color using GL_RGB as texture
    // format and in the output image search for the above color and
    // replaces it with the color requested by the user.
    fboFormat.setInternalTextureFormat(getInternalTextureFormat());

    QtGLFramebufferObject fbo(width, height, fboFormat);

    const QColor col = backgroundColor();
    bool on = hasGradientBackground();

    int alpha = 255;
    QColor bgopaque = bgcolor;
    if (bgopaque.isValid()) {
        // force an opaque background color
        alpha = bgopaque.alpha();
        if (alpha < 255)
            bgopaque.setRgb(255,255,255);
        setBackgroundColor(bgopaque);
        setGradientBackground(false);
    }

    renderToFramebuffer(&fbo);
    setBackgroundColor(col);
    setGradientBackground(on);
    img = fbo.toImage();

    // if background color isn't opaque manipulate the image
    if (alpha < 255) {
        QImage image(img.constBits(), img.width(), img.height(), QImage::Format_ARGB32);
        img = image.copy();
        QRgb rgba = bgcolor.rgba();
        QRgb rgb = bgopaque.rgb();
        QRgb * bits = (QRgb*) img.bits();
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                if (*bits == rgb)
                    *bits = rgba;
                bits++;
            }
        }
    } else if (alpha == 255) {
        QImage image(img.width(), img.height(), QImage::Format_RGB32);
        QPainter painter(&image);
        painter.fillRect(image.rect(),Qt::black);
        painter.drawImage(0, 0, img);
        painter.end();
        img = image;
    }
}

void View3DInventorViewer::renderToFramebuffer(QtGLFramebufferObject* fbo)
{
    static_cast<QtGLWidget*>(this->viewport())->makeCurrent();
    fbo->bind();
    int width = fbo->size().width();
    int height = fbo->size().height();

    glDisable(GL_TEXTURE_2D);
    glEnable(GL_LIGHTING);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LINE_SMOOTH);

    const QColor col = this->backgroundColor();
    glViewport(0, 0, width, height);
    glClearColor(col.redF(), col.greenF(), col.blueF(), col.alphaF());
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // If on then transparent areas may shine through opaque areas
    //glDepthRange(0.1,1.0);

    SoBoxSelectionRenderAction gl(SbViewportRegion(width, height));
    // When creating a new GL render action we have to copy over the cache context id
    // For further details see init().
    uint32_t id = this->getSoRenderManager()->getGLRenderAction()->getCacheContext();
    gl.setCacheContext(id);
    gl.setTransparencyType(SoGLRenderAction::SORTED_OBJECT_SORTED_TRIANGLE_BLEND);

    gl.apply(this->backgroundroot);
    // The render action of the render manager has set the depth function to GL_LESS
    // while creating a new render action has it set to GL_LEQUAL. So, in order to get
    // the exact same result set it explicitly to GL_LESS.
    glDepthFunc(GL_LESS);
    gl.apply(this->getSoRenderManager()->getSceneGraph());
    gl.apply(this->foregroundroot);

    if (this->axiscrossEnabled) {
        this->drawAxisCross();
    }

    fbo->release();
}

void View3DInventorViewer::actualRedraw()
{
    switch (renderType) {
    case Native:
        renderScene();
        break;
    case Framebuffer:
        renderFramebuffer();
        break;
    case Image:
        renderGLImage();
        break;
    }
}

void View3DInventorViewer::renderFramebuffer()
{
    const SbViewportRegion vp = this->getSoRenderManager()->getViewportRegion();
    SbVec2s size = vp.getViewportSizePixels();

    glDisable(GL_LIGHTING);
    glViewport(0, 0, size[0], size[1]);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glDisable(GL_DEPTH_TEST);

    glClear(GL_COLOR_BUFFER_BIT);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, this->framebuffer->texture());
    glColor3f(1.0, 1.0, 1.0);

    glBegin(GL_QUADS);
        glTexCoord2f(0.0f, 0.0f);
        glVertex2f(-1.0, -1.0f);
        glTexCoord2f(1.0f, 0.0f);
        glVertex2f(1.0f, -1.0f);
        glTexCoord2f(1.0f, 1.0f);
        glVertex2f(1.0f, 1.0f);
        glTexCoord2f(0.0f, 1.0f);
        glVertex2f(-1.0f, 1.0f);
    glEnd();

    printDimension();
    navigation->redraw();

    for (std::list<GLGraphicsItem*>::iterator it = this->graphicsItems.begin(); it != this->graphicsItems.end(); ++it)
        (*it)->paintGL();

    if (naviCubeEnabled)
        naviCube->drawNaviCube();

    glEnable(GL_LIGHTING);
    glEnable(GL_DEPTH_TEST);
}

void View3DInventorViewer::renderGLImage()
{
    const SbViewportRegion vp = this->getSoRenderManager()->getViewportRegion();
    SbVec2s size = vp.getViewportSizePixels();

    glDisable(GL_LIGHTING);
    glViewport(0, 0, size[0], size[1]);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, size[0], 0, size[1], 0, 100);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glDisable(GL_DEPTH_TEST);
    glClear(GL_COLOR_BUFFER_BIT);

    glRasterPos2f(0,0);
    glDrawPixels(glImage.width(),glImage.height(),GL_BGRA,GL_UNSIGNED_BYTE,glImage.bits());

    printDimension();
    navigation->redraw();

    for (std::list<GLGraphicsItem*>::iterator it = this->graphicsItems.begin(); it != this->graphicsItems.end(); ++it)
        (*it)->paintGL();

    if (naviCubeEnabled)
        naviCube->drawNaviCube();

    glEnable(GL_LIGHTING);
    glEnable(GL_DEPTH_TEST);
}

// #define ENABLE_GL_DEPTH_RANGE
// The calls of glDepthRange inside renderScene() causes problems with transparent objects
// so that's why it is disabled now: http://forum.freecadweb.org/viewtopic.php?f=3&t=6037&hilit=transparency

// Documented in superclass. Overrides this method to be able to draw
// the axis cross, if selected, and to keep a continuous animation
// upon spin.
void View3DInventorViewer::renderScene(void)
{
    // Must set up the OpenGL viewport manually, as upon resize
    // operations, Coin won't set it up until the SoGLRenderAction is
    // applied again. And since we need to do glClear() before applying
    // the action..
    const SbViewportRegion vp = this->getSoRenderManager()->getViewportRegion();
    SbVec2s origin = vp.getViewportOriginPixels();
    SbVec2s size = vp.getViewportSizePixels();
    glViewport(origin[0], origin[1], size[0], size[1]);

    bool restoreGradient = false;

    QColor col;
    if(overrideBGColor) {
        col = App::Color(overrideBGColor).asValue<QColor>();
        if(hasGradientBackground()) {
            setGradientBackground(false);
            restoreGradient = true;
        }
    } else
        col = this->backgroundColor();
    glClearColor(col.redF(), col.greenF(), col.blueF(), 0.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);

#if defined(ENABLE_GL_DEPTH_RANGE)
    // using 90% of the z-buffer for the background and the main node
    glDepthRange(0.1,1.0);
#endif

    // Render our scenegraph with the image.
    SoGLRenderAction* glra = this->getSoRenderManager()->getGLRenderAction();
    SoState* state = glra->getState();
    SoGLWidgetElement::set(state, qobject_cast<QtGLWidget*>(this->getGLWidget()));
    SoGLRenderActionElement::set(state, glra);
    SoGLVBOActivatedElement::set(state, this->vboEnabled);
    glra->apply(this->backgroundroot);

    navigation->updateAnimation();

    SoBoxSelectionRenderAction *glbra = nullptr;
    if(glra->isOfType(SoBoxSelectionRenderAction::getClassTypeId())) {
        glbra = static_cast<SoBoxSelectionRenderAction*>(glra);
        glbra->checkRootNode(this->getSoRenderManager()->getSceneGraph());
    }
    try {
        // Render normal scenegraph.
        inherited::actualRedraw();
    }
    catch (const Base::MemoryException&) {
        // FIXME: If this exception appears then the background and camera position get broken somehow. (Werner 2006-02-01)
        for (std::set<ViewProvider*>::iterator it = _ViewProviderSet.begin(); it != _ViewProviderSet.end(); ++it)
            (*it)->hide();

        inherited::actualRedraw();
        QMessageBox::warning(parentWidget(), QObject::tr("Out of memory"),
                             QObject::tr("Not enough memory available to display the data."));
    }
    glbra->checkRootNode(nullptr);

#if defined (ENABLE_GL_DEPTH_RANGE)
    // using 10% of the z-buffer for the foreground node
    glDepthRange(0.0,0.1);
#endif

    // Render overlay front scenegraph.
    glra->apply(this->foregroundroot);

    if (this->axiscrossEnabled) {
        this->drawAxisCross();
    }

#if defined (ENABLE_GL_DEPTH_RANGE)
    // using the main portion of z-buffer again (for frontbuffer highlighting)
    glDepthRange(0.1,1.0);
#endif

    // Immediately reschedule to get continuous spin animation.
    if (this->isAnimating()) {
        this->getSoRenderManager()->scheduleRedraw();
    } else 
        shadowInfo->onRender();

#if 0 // this breaks highlighting of edges
    glDisable(GL_LIGHTING);
    glDisable(GL_DEPTH_TEST);
#endif

    printDimension();
    navigation->redraw();

    for (std::list<GLGraphicsItem*>::iterator it = this->graphicsItems.begin(); it != this->graphicsItems.end(); ++it)
        (*it)->paintGL();

    //fps rendering
    if (fpsEnabled) {
        std::stringstream stream;
        stream.precision(1);
        stream.setf(std::ios::fixed | std::ios::showpoint);
        stream << framesPerSecond[0] << " ms / " << framesPerSecond[1] << " fps";
        draw2DString(stream.str().c_str(), SbVec2s(10,10), SbVec2f(0.1f,0.1f));
    }

    if (naviCubeEnabled)
        naviCube->drawNaviCube();

#if 0 // this breaks highlighting of edges
    glEnable(GL_LIGHTING);
    glEnable(GL_DEPTH_TEST);
#endif

    if(restoreGradient)
        setGradientBackground(true);
}

void View3DInventorViewer::setSeekMode(SbBool on)
{
    // Overrides this method to make sure any animations are stopped
    // before we go into seek mode.

    // Note: this method is almost identical to the setSeekMode() in the
    // SoQtFlyViewer and SoQtPlaneViewer, so migrate any changes.

    if (this->isAnimating()) {
        this->stopAnimating();
    }

    inherited::setSeekMode(on);
    navigation->setViewingMode(on ? NavigationStyle::SEEK_WAIT_MODE :
                               (this->isViewing() ?
                                NavigationStyle::IDLE : NavigationStyle::INTERACT));
}

void View3DInventorViewer::printDimension()
{
    SoCamera* cam = getSoRenderManager()->getCamera();
    if (!cam) return; // no camera there

    SoType t = getSoRenderManager()->getCamera()->getTypeId();
    if (t.isDerivedFrom(SoOrthographicCamera::getClassTypeId())) {
        const SbViewportRegion& vp = getSoRenderManager()->getViewportRegion();
        const SbVec2s& size = vp.getWindowSize();
        short dimX, dimY;
        size.getValue(dimX, dimY);

        float fHeight = static_cast<SoOrthographicCamera*>(getSoRenderManager()->getCamera())->height.getValue();
        float fWidth = fHeight;

        if (dimX > dimY)
            fWidth *= ((float)dimX)/((float)dimY);
        else if (dimX < dimY)
            fHeight *= ((float)dimY)/((float)dimX);

        // Translate screen units into user's unit schema
        Base::Quantity qWidth(Base::Quantity::MilliMetre);
        Base::Quantity qHeight(Base::Quantity::MilliMetre);
        qWidth.setValue(fWidth);
        qHeight.setValue(fHeight);
        QString wStr = Base::UnitsApi::schemaTranslate(qWidth);
        QString hStr = Base::UnitsApi::schemaTranslate(qHeight);

        // Create final string and update window
        QString dim = QString::fromLatin1("%1 x %2")
                      .arg(wStr, hStr);
        getMainWindow()->setPaneText(2, dim);
    }
    else
        getMainWindow()->setPaneText(2, QLatin1String(""));
}

void View3DInventorViewer::selectAll()
{
    std::vector<App::DocumentObject*> objs;

    for (std::set<ViewProvider*>::iterator it = _ViewProviderSet.begin(); it != _ViewProviderSet.end(); ++it) {
        if ((*it)->getTypeId().isDerivedFrom(ViewProviderDocumentObject::getClassTypeId())) {
            ViewProviderDocumentObject* vp = static_cast<ViewProviderDocumentObject*>(*it);
            App::DocumentObject* obj = vp->getObject();

            if (obj) objs.push_back(obj);
        }
    }

    if (!objs.empty())
        Gui::Selection().setSelection(objs.front()->getDocument()->getName(), objs);
}

bool View3DInventorViewer::processSoEvent(const SoEvent* ev)
{
    if (naviCubeEnabled && naviCube->processSoEvent(ev))
        return true;
    if (isRedirectedToSceneGraph()) {
        SbBool processed = inherited::processSoEvent(ev);

        if (!processed)
            processed = navigation->processEvent(ev);

        return processed;
    }

    if (ev->getTypeId().isDerivedFrom(SoKeyboardEvent::getClassTypeId())) {
        // filter out 'Q' and 'ESC' keys
        const SoKeyboardEvent* const ke = static_cast<const SoKeyboardEvent*>(ev);

        switch (ke->getKey()) {
        case SoKeyboardEvent::ESCAPE:
            if (QApplication::queryKeyboardModifiers() == Qt::ShiftModifier)
                Selection().clearSelection();
            else {
                Selection().rmvPreselect();
                toggleShadowLightManip(0);
            }
            //fall through
        case SoKeyboardEvent::Q: // ignore 'Q' keys (to prevent app from being closed)
            return inherited::processSoEvent(ev);
        default:
            break;
        }
    } else if (ev->isOfType(SoMouseButtonEvent::getClassTypeId()) 
                && ev->wasShiftDown()
                && ev->wasCtrlDown())
    {
        if(static_cast<const SoMouseButtonEvent*>(ev)->getButton() == SoMouseButtonEvent::BUTTON4
                || static_cast<const SoMouseButtonEvent*>(ev)->getButton() == SoMouseButtonEvent::BUTTON5)
            return processSoEventBase(ev);
    }

    return navigation->processEvent(ev);
}

SbBool View3DInventorViewer::processSoEventBase(const SoEvent* const ev)
{
    return inherited::processSoEvent(ev);
}

SbVec3f View3DInventorViewer::getViewDirection() const
{
    SoCamera* cam = this->getSoRenderManager()->getCamera();

    if (!cam) return SbVec3f(0,0,-1);  // this is the default

    SbVec3f projDir = cam->getViewVolume().getProjectionDirection();
    return projDir;
}

void View3DInventorViewer::setViewDirection(SbVec3f dir)
{
    SoCamera* cam = this->getSoRenderManager()->getCamera();
    if (cam)
        cam->orientation.setValue(SbRotation(SbVec3f(0, 0, -1), dir));
}

SbVec3f View3DInventorViewer::getUpDirection() const
{
    SoCamera* cam = this->getSoRenderManager()->getCamera();

    if (!cam) return SbVec3f(0,1,0);

    SbRotation camrot = cam->orientation.getValue();
    SbVec3f upvec(0, 1, 0); // init to default up vector
    camrot.multVec(upvec, upvec);
    return upvec;
}

SbRotation View3DInventorViewer::getCameraOrientation() const
{
    SoCamera* cam = this->getSoRenderManager()->getCamera();

    if (!cam)
        return SbRotation(0,0,0,1); // this is the default

    return cam->orientation.getValue();
}

SbVec3f View3DInventorViewer::getPointOnScreen(const SbVec2s& pnt) const
{
    const SbViewportRegion& vp = this->getSoRenderManager()->getViewportRegion();

    short x,y;
    pnt.getValue(x,y);
    SbVec2f siz = vp.getViewportSize();
    float dX, dY;
    siz.getValue(dX, dY);

    float fRatio = vp.getViewportAspectRatio();
    float pX = (float)x / float(vp.getViewportSizePixels()[0]);
    float pY = (float)y / float(vp.getViewportSizePixels()[1]);

    // now calculate the real points respecting aspect ratio information
    //
    if (fRatio > 1.0f) {
        pX = (pX - 0.5f*dX) * fRatio + 0.5f*dX;
    }
    else if (fRatio < 1.0f) {
        pY = (pY - 0.5f*dY) / fRatio + 0.5f*dY;
    }

    SoCamera* pCam = this->getSoRenderManager()->getCamera();

    if (!pCam) return SbVec3f();  // return invalid point

    SbViewVolume  vol = pCam->getViewVolume();

    float nearDist = pCam->nearDistance.getValue();
    float farDist = pCam->farDistance.getValue();
    float focalDist = pCam->focalDistance.getValue();

    if (focalDist < nearDist || focalDist > farDist)
        focalDist = 0.5f*(nearDist + farDist);

    SbLine line;
    SbVec3f pt;
    SbPlane focalPlane = vol.getPlane(focalDist);
    vol.projectPointToLine(SbVec2f(pX,pY), line);
    focalPlane.intersect(line, pt);

    return pt;
}

void View3DInventorViewer::getNearPlane(SbVec3f& rcPt, SbVec3f& rcNormal) const
{
    SoCamera* pCam = getSoRenderManager()->getCamera();

    if (!pCam) return;  // just do nothing

    SbViewVolume vol = pCam->getViewVolume();

    // get the normal of the front clipping plane
    SbPlane nearPlane = vol.getPlane(vol.nearDist);
    float d = nearPlane.getDistanceFromOrigin();
    rcNormal = nearPlane.getNormal();
    rcNormal.normalize();
    float nx, ny, nz;
    rcNormal.getValue(nx, ny, nz);
    rcPt.setValue(d*rcNormal[0], d*rcNormal[1], d*rcNormal[2]);
}

void View3DInventorViewer::getFarPlane(SbVec3f& rcPt, SbVec3f& rcNormal) const
{
    SoCamera* pCam = getSoRenderManager()->getCamera();

    if (!pCam) return;  // just do nothing

    SbViewVolume vol = pCam->getViewVolume();

    // get the normal of the back clipping plane
    SbPlane farPlane = vol.getPlane(vol.nearDist+vol.nearToFar);
    float d = farPlane.getDistanceFromOrigin();
    rcNormal = farPlane.getNormal();
    rcNormal.normalize();
    float nx, ny, nz;
    rcNormal.getValue(nx, ny, nz);
    rcPt.setValue(d*rcNormal[0], d*rcNormal[1], d*rcNormal[2]);
}

SbVec3f View3DInventorViewer::projectOnNearPlane(const SbVec2f& pt) const
{
    SbVec3f pt1, pt2;
    SoCamera* cam = this->getSoRenderManager()->getCamera();

    if (!cam) return SbVec3f();  // return invalid point

    SbViewVolume vol = cam->getViewVolume();
    vol.projectPointToLine(pt, pt1, pt2);
    return pt1;
}

SbVec3f View3DInventorViewer::projectOnFarPlane(const SbVec2f& pt) const
{
    SbVec3f pt1, pt2;
    SoCamera* cam = this->getSoRenderManager()->getCamera();

    if (!cam) return SbVec3f();  // return invalid point

    SbViewVolume vol = cam->getViewVolume();
    vol.projectPointToLine(pt, pt1, pt2);
    return pt2;
}

void View3DInventorViewer::toggleClippingPlane(int toggle, bool beforeEditing,
        bool noManip, const Base::Placement &pla)
{
    if(pcClipPlane) {
        if(toggle<=0) {
            pcViewProviderRoot->removeChild(pcClipPlane);
            pcClipPlane->unref();
            pcClipPlane = 0;
        }
        return;
    }else if(toggle==0)
        return;

    Base::Vector3d dir;
    pla.getRotation().multVec(Base::Vector3d(0,0,-1),dir);
    Base::Vector3d base = pla.getPosition();

    if(!noManip) {
        SoClipPlaneManip* clip = new SoClipPlaneManip;
        pcClipPlane = clip;
        SoGetBoundingBoxAction action(this->getSoRenderManager()->getViewportRegion());
        action.apply(this->getSoRenderManager()->getSceneGraph());
        SbBox3f box = action.getBoundingBox();

        if (!box.isEmpty()) {
            // adjust to overall bounding box of the scene
            clip->setValue(box, SbVec3f(dir.x,dir.y,dir.z), 1.0f);
        }
    }else
        pcClipPlane = new SoClipPlane;
    pcClipPlane->plane.setValue(
            SbPlane(SbVec3f(dir.x,dir.y,dir.z),SbVec3f(base.x,base.y,base.z)));
    pcClipPlane->ref();
    if(beforeEditing)
        pcViewProviderRoot->insertChild(pcClipPlane,0);
    else 
        pcViewProviderRoot->insertChild(pcClipPlane,pcViewProviderRoot->findChild(pcEditingRoot)+1);
}

bool View3DInventorViewer::hasClippingPlane() const
{
    return !!pcClipPlane;
}

/**
 * This method picks the closest point to the camera in the underlying scenegraph
 * and returns its location and normal.
 * If no point was picked false is returned.
 */
bool View3DInventorViewer::pickPoint(const SbVec2s& pos,SbVec3f& point,SbVec3f& norm) const
{
    // attempting raypick in the event_cb() callback method
    SoRayPickAction rp(getSoRenderManager()->getViewportRegion());
    rp.setPoint(pos);
    rp.apply(getSoRenderManager()->getSceneGraph());
    SoPickedPoint* Point = rp.getPickedPoint();

    if (Point) {
        point = Point->getObjectPoint();
        norm  = Point->getObjectNormal();
        return true;
    }

    return false;
}

/**
 * This method is provided for convenience and does basically the same as method
 * above unless that it returns an SoPickedPoint object with additional information.
 * \note It is in the response of the client programmer to delete the returned
 * SoPickedPoint object.
 */
SoPickedPoint* View3DInventorViewer::pickPoint(const SbVec2s& pos) const
{
    SoRayPickAction rp(getSoRenderManager()->getViewportRegion());
    rp.setPoint(pos);
    rp.apply(getSoRenderManager()->getSceneGraph());

    // returns a copy of the point
    SoPickedPoint* pick = rp.getPickedPoint();
    //return (pick ? pick->copy() : 0); // needs the same instance of CRT under MS Windows
    return (pick ? new SoPickedPoint(*pick) : 0);
}

SoPickedPoint* View3DInventorViewer::getPickedPoint(SoEventCallback* n) const
{
    if (selectionRoot) 
        return selectionRoot->getPickedPoint(n->getAction());
    auto pp = n->getPickedPoint();
    return pp?pp->copy():0;
}

std::vector<App::SubObjectT>
View3DInventorViewer::getPickedList(const SbVec2s &_pos, bool singlePick, bool mapCoords) const {
    SbVec2s pos;
    if (!mapCoords)
        pos = _pos;
    else {
        QPoint p = this->mapFromGlobal(QPoint(_pos[0],_pos[1]));
        pos[0] = p.x();
        pos[1] = this->height() - p.y() - 1;
#if QT_VERSION >= 0x050000
        pos *= this->devicePixelRatio();
#endif
    }
    return selectionRoot->getPickedSelections(pos,
            getSoRenderManager()->getViewportRegion(), singlePick);
}

std::vector<App::SubObjectT>
View3DInventorViewer::getPickedList(bool singlePick) const {
    auto pos = QCursor::pos();
    return this->getPickedList(SbVec2s(pos.x(), pos.y()), singlePick, true);
}

SbBool View3DInventorViewer::pubSeekToPoint(const SbVec2s& pos)
{
    return this->seekToPoint(pos);
}

void View3DInventorViewer::pubSeekToPoint(const SbVec3f& pos)
{
    this->seekToPoint(pos);
}

void View3DInventorViewer::setCameraOrientation(const SbRotation& rot, SbBool moveTocenter)
{
    navigation->setCameraOrientation(rot, moveTocenter);
}

void View3DInventorViewer::setCameraType(SoType t)
{
    inherited::setCameraType(t);

    if (t.isDerivedFrom(SoPerspectiveCamera::getClassTypeId())) {
        // When doing a viewAll() for an orthographic camera and switching
        // to perspective the scene looks completely strange because of the
        // heightAngle. Setting it to 45 deg also causes an issue with a too
        // close camera but we don't have this other ugly effect.
        SoCamera* cam = this->getSoRenderManager()->getCamera();

        if(cam == 0) return;

        static_cast<SoPerspectiveCamera*>(cam)->heightAngle = (float)(M_PI / 4.0);
    }
}

namespace Gui {
    class CameraAnimation : public QVariantAnimation
    {
        SoCamera* camera;
        SbRotation startRot, endRot;
        SbVec3f startPos, endPos;

    public:
        CameraAnimation(SoCamera* camera, const SbRotation& rot, const SbVec3f& pos)
            : camera(camera), endRot(rot), endPos(pos)
        {
            startPos = camera->position.getValue();
            startRot = camera->orientation.getValue();
        }
        virtual ~CameraAnimation()
        {
        }
    protected:
        void updateCurrentValue(const QVariant & value)
        {
            int steps = endValue().toInt();
            int curr = value.toInt();

            float s = static_cast<float>(curr)/static_cast<float>(steps);
            SbVec3f curpos = startPos * (1.0f-s) + endPos * s;
            SbRotation currot = SbRotation::slerp(startRot, endRot, s);
            camera->orientation.setValue(currot);
            camera->position.setValue(curpos);
        }
    };
}

void View3DInventorViewer::moveCameraTo(const SbRotation& rot, const SbVec3f& pos, int steps, int ms)
{
    SoCamera* cam = this->getSoRenderManager()->getCamera();
    if (cam == 0) return;

    CameraAnimation anim(cam, rot, pos);
    anim.setDuration(Base::clamp<int>(ms,0,5000));
    anim.setStartValue(static_cast<int>(0));
    anim.setEndValue(steps);

    QEventLoop loop;
    QObject::connect(&anim, SIGNAL(finished()), &loop, SLOT(quit()));
    anim.start();
    loop.exec(QEventLoop::ExcludeUserInputEvents);

    cam->orientation.setValue(rot);
    cam->position.setValue(pos);
}

bool View3DInventorViewer::getSceneBoundBox(Base::BoundBox3d &box) const {
    // in the scene graph we may have objects which we want to exclude
    // when doing a fit all. Such objects must be part of the group
    // SoSkipBoundingGroup.
    SoSearchAction sa;
    sa.setType(SoSkipBoundingGroup::getClassTypeId());
    sa.setInterest(SoSearchAction::ALL);
    sa.apply(pcViewProviderRoot);
    const SoPathList& pathlist = sa.getPaths();

    for (int i = 0; i < pathlist.getLength(); i++) {
        SoPath* path = pathlist[i];
        SoSkipBoundingGroup* group = static_cast<SoSkipBoundingGroup*>(path->getTail());
        group->mode = SoSkipBoundingGroup::EXCLUDE_BBOX;
    }

    if(guiDocument && ViewParams::instance()->getUseTightBoundingBox()) {
        SoGetBoundingBoxAction action(this->getSoRenderManager()->getViewportRegion());
        for(int i=0;i<pcViewProviderRoot->getNumChildren();++i) {
            auto node = pcViewProviderRoot->getChild(i);
            auto vp = guiDocument->getViewProvider(node);
            if(!vp) {
                action.apply(node);
                auto bbox = action.getBoundingBox();
                if(!bbox.isEmpty()) {
                    float minx,miny,minz,maxx,maxy,maxz;
                    bbox.getBounds(minx,miny,minz,maxx,maxy,maxz);
                    box.Add(Base::BoundBox3d(minx,miny,minz,maxx,maxy,maxz));
                }
                continue;
            }
            if(!vp->isVisible())
                continue;
            auto sbox = vp->getBoundingBox(0,0,true,this);
            if(sbox.IsValid())
                box.Add(sbox);
        }
    } else {
        SoGetBoundingBoxAction action(this->getSoRenderManager()->getViewportRegion());
        action.apply(pcViewProviderRoot);
        auto bbox = action.getBoundingBox();
        if(!bbox.isEmpty()) {
            float minx,miny,minz,maxx,maxy,maxz;
            bbox.getBounds(minx,miny,minz,maxx,maxy,maxz);
            box.MinX = minx;
            box.MinY = miny;
            box.MinZ = minz;
            box.MaxX = maxx;
            box.MaxY = maxy;
            box.MaxZ = maxz;
        }
    }

    for (int i = 0; i < pathlist.getLength(); i++) {
        SoPath* path = pathlist[i];
        SoSkipBoundingGroup* group = static_cast<SoSkipBoundingGroup*>(path->getTail());
        group->mode = SoSkipBoundingGroup::INCLUDE_BBOX;
    }
    return box.IsValid();
}

bool View3DInventorViewer::getSceneBoundBox(SbBox3f &box) const {
    Base::BoundBox3d fcbox;
    getSceneBoundBox(fcbox);
    if(!fcbox.IsValid())
        return false;
    box.setBounds(fcbox.MinX,fcbox.MinY,fcbox.MinZ,
                  fcbox.MaxX,fcbox.MaxY,fcbox.MaxZ);
    SbSphere sphere;
    sphere.circumscribe(box); // why do we need this?
    if (sphere.getRadius() == 0)
        return false;
    return true;
}

void View3DInventorViewer::animatedViewAll(const SbBox3f &box, int steps, int ms)
{
    SoCamera* cam = this->getSoRenderManager()->getCamera();
    if (!cam)
        return;

    SbVec3f campos = cam->position.getValue();
    SbRotation camrot = cam->orientation.getValue();
    SbViewportRegion vp = this->getSoRenderManager()->getViewportRegion();

#if (COIN_MAJOR_VERSION >= 3)
    float aspectRatio = vp.getViewportAspectRatio();
#endif

    SbSphere sphere;
    sphere.circumscribe(box);
    if (sphere.getRadius() == 0)
        return;

    SbVec3f direction, pos;
    camrot.multVec(SbVec3f(0, 0, -1), direction);

    bool isOrthographic = false;
    float height = 0;
    float diff = 0;

    if (cam->isOfType(SoOrthographicCamera::getClassTypeId())) {
        isOrthographic = true;
        height = static_cast<SoOrthographicCamera*>(cam)->height.getValue();
#if (COIN_MAJOR_VERSION >= 3)
        if (aspectRatio < 1.0f)
            diff = sphere.getRadius() * 2 - height * aspectRatio;
        else
#endif
        diff = sphere.getRadius() * 2 - height;
        pos = (box.getCenter() - direction * sphere.getRadius());
    }
    else if (cam->isOfType(SoPerspectiveCamera::getClassTypeId())) {
        float movelength = sphere.getRadius()/float(tan(static_cast<SoPerspectiveCamera*>
            (cam)->heightAngle.getValue() / 2.0));
        pos = box.getCenter() - direction * movelength;
    }

    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(&timer, SIGNAL(timeout()), &loop, SLOT(quit()));

    Base::StateLocker guard(shadowInfo->animating);
    for (int i=0; i<steps; i++) {
        float s = float(i)/float(steps);

        if (isOrthographic) {
            float camHeight = height + diff * s;
            static_cast<SoOrthographicCamera*>(cam)->height.setValue(camHeight);
        }

        SbVec3f curpos = campos * (1.0f-s) + pos * s;
        cam->position.setValue(curpos);
        timer.start(Base::clamp<int>(ms,0,5000));
        loop.exec(QEventLoop::ExcludeUserInputEvents);
    }
    shadowInfo->onRender();
}

#if BUILD_VR
extern View3DInventorRiftViewer* oculusStart(void);
extern bool oculusUp   (void);
extern void oculusStop (void);
void oculusSetTestScene(View3DInventorRiftViewer *window);
#endif

void View3DInventorViewer::viewVR(void)
{
#if BUILD_VR
    if (oculusUp()) {
        oculusStop();
    }
    else {
        View3DInventorRiftViewer* riftWin = oculusStart();
        riftWin->setSceneGraph(pcViewProviderRoot);
    }
#endif
}

void View3DInventorViewer::boxZoom(const SbBox2s& box)
{
    navigation->boxZoom(box);
}

void View3DInventorViewer::viewAll()
{
    SbBox3f box;
    if(!getSceneBoundBox(box))
        return;

    shadowInfo->updateShadowGround(box);

    // Set the height angle to 45 deg
    SoCamera* cam = this->getSoRenderManager()->getCamera();

    if (cam && cam->getTypeId().isDerivedFrom(SoPerspectiveCamera::getClassTypeId()))
        static_cast<SoPerspectiveCamera*>(cam)->heightAngle = (float)(M_PI / 4.0);

    if (isAnimationEnabled())
        animatedViewAll(box, 10, 20);

    viewBoundBox(box);
}

void View3DInventorViewer::ShadowInfo::updateShadowGround(const SbBox3f &box)
{
    App::Document *doc = owner->guiDocument?owner->guiDocument->getDocument():nullptr;

    if (!pcShadowGroup || !doc)
        return;

    SbVec3f size = box.getSize();
    SbVec3f center = box.getCenter();

    if(pcShadowDirectionalLight) {
        static const App::PropertyFloatConstraint::Constraints _cstr(1.0,1000.0,0.1);
        double scale = _shadowParam<App::PropertyFloatConstraint>(doc, "BoundBoxScale",
                ViewParams::docShadowBoundBoxScale(), ViewParams::getShadowBoundBoxScale(),
            [](App::PropertyFloatConstraint &prop) {
                if(!prop.getConstraints())
                    prop.setConstraints(&_cstr);
            });
        pcShadowDirectionalLight->bboxSize = size * float(scale);
        pcShadowDirectionalLight->bboxCenter = center;
    }

    if(pcShadowGroundSwitch && pcShadowGroundSwitch->whichChild.getValue()>=0) {
        float z = size[2];
        float width, length;
        if(_shadowParam<App::PropertyBool>(doc, "GroundSizeAuto",
                    "Auto adjust ground size based on the scene bounding box", true))
        {
            double scale = _shadowParam<App::PropertyFloat>(doc, "GroundSizeScale",
                    ViewParams::docShadowGroundScale(), ViewParams::getShadowGroundScale());
            if(scale <= 0.0)
                scale = 1.0;
            width = length = scale * std::max(std::max(size[0],size[1]),size[2]);
        } else {
            width = _shadowParam<App::PropertyLength>(doc, "GroundSizeX", "", 100.0);
            length = _shadowParam<App::PropertyLength>(doc, "GroundSizeY", "", 100.0);
        }

        Base::Placement pla = _shadowParam<App::PropertyPlacement>(
                doc, "GroundPlacement",
                "Ground placement. If 'GroundAutoPosition' is on, this specifies an additional offset of the ground",
                Base::Placement());

        if(!_shadowParam<App::PropertyBool>(doc, "GroundAutoPosition",
                    "Auto place the ground face at the Z bottom of the scene", true))
        {
            center[0] = pla.getPosition().x;
            center[1] = pla.getPosition().y;
            z = pla.getPosition().z;
            pla = Base::Placement();
        } else {
            z = center[2]-z/2-1;
        }
        SbVec3f coords[4] = {
            {center[0]-width, center[1]-length, z},
            {center[0]+width, center[1]-length, z},
            {center[0]+width, center[1]+length, z},
            {center[0]-width, center[1]+length, z},
        };
        if(!pla.isIdentity()) {
            SbMatrix mat = ViewProvider::convert(pla.toMatrix());
            for(auto &coord : coords)
                mat.multVecMatrix(coord,coord);
        }
        pcShadowGroundCoords->point.setValues(0, 4, coords);

        static const App::PropertyQuantityConstraint::Constraints _texture_cstr = {0,DBL_MAX,10.0};
        float textureSize = _shadowParam<App::PropertyLength>(doc, "GroundTextureSize",
            ViewParams::docShadowGroundTextureSize(), ViewParams::getShadowGroundTextureSize(),
            [](App::PropertyLength &prop) {
                if(prop.getConstraints() != &_texture_cstr)
                    prop.setConstraints(&_texture_cstr);
            });
        if(textureSize < 1e-5)
            pcShadowGroundTextureCoords->point.setNum(0);
        else {
            float w = width*2.0/textureSize;
            float l = length*2.0/textureSize;
            SbVec2f points[4] = {{0,l}, {w,l}, {w,0}, {0,0}};
            pcShadowGroundTextureCoords->point.setValues(0,4,points);
        }

        SbBox3f gbox = box;
        for(int i=0; i<4; ++i)
            gbox.extendBy(coords[i]);
        size = gbox.getSize();
    }

    static const App::PropertyIntegerConstraint::Constraints _smooth_cstr(0,100,1);
    double smoothBorder = _shadowParam<App::PropertyIntegerConstraint>(doc, "SmoothBorder",
            ViewParams::docShadowSmoothBorder(), ViewParams::getShadowSmoothBorder(),
            [](App::PropertyIntegerConstraint &prop) {
                if(prop.getConstraints() != &_smooth_cstr)
                    prop.setConstraints(&_smooth_cstr);
            });

    static const App::PropertyIntegerConstraint::Constraints _spread_cstr(0,1000000,500);
    double spread = _shadowParam<App::PropertyIntegerConstraint>(doc, "SpreadSize",
            ViewParams::docShadowSpreadSize(), ViewParams::getShadowSpreadSize(),
            [](App::PropertyIntegerConstraint &prop) {
                if(prop.getConstraints() != &_spread_cstr)
                    prop.setConstraints(&_spread_cstr);
            });

    static const App::PropertyIntegerConstraint::Constraints _sample_cstr(0,7,1);
    double sample = _shadowParam<App::PropertyIntegerConstraint>(doc, "SpreadSampleSize",
            ViewParams::docShadowSpreadSampleSize(), ViewParams::getShadowSpreadSampleSize(),
            [](App::PropertyIntegerConstraint &prop) {
                if(prop.getConstraints() != &_sample_cstr)
                    prop.setConstraints(&_sample_cstr);
            });

    float maxSize = std::max(size[0],std::max(size[1],size[2]));
    if (maxSize > 256.0 && pcShadowGroup->findChild(pcShadowSpotLight)>=0)
        spread *= 256.0/maxSize;
    pcShadowGroup->smoothBorder = smoothBorder/10.0f + sample/100.0f + spread/1000000.0f;
}

void View3DInventorViewer::viewAll(float factor)
{
    SoCamera* cam = this->getSoRenderManager()->getCamera();

    if (!cam) return;

    if (factor <= 0.0f) return;

    if (factor != 1.0f) {
        SbBox3f box;
        if(!getSceneBoundBox(box))
            return;

        float dx,dy,dz;
        box.getSize(dx,dy,dz);

        float x,y,z;
        box.getCenter().getValue(x,y,z);

        box.setBounds(x-dx*factor,y-dy*factor,z-dz*factor,
                      x+dx*factor,y+dy*factor,z+dz*factor);

        viewBoundBox(box);
    }
    else {
        viewAll();
    }
}

// Recursively check if any sub-element intersects with a given projected 2D polygon
static int
checkElementIntersection(ViewProviderDocumentObject *vp, const char *subname,
                         const Base::ViewProjMethod &proj, const Base::Polygon2d &polygon,
                         Base::Matrix4D mat, bool transform=true, int depth=0)
{
    auto obj = vp->getObject();
    if(!obj || !obj->getNameInDocument())
        return 0;

    if (subname && subname[0]) {
        App::DocumentObject *parent = 0;
        std::string childName;
        auto sobj = obj->resolve(subname,&parent,&childName,0,0,&mat,transform,depth+1);
        if(!sobj) 
            return 0;
        if(!ViewParams::getShowSelectionOnTop()) {
            int vis;
            if(!parent || (vis=parent->isElementVisibleEx(childName.c_str(),App::DocumentObject::GS_SELECT))<0)
                vis = sobj->Visibility.getValue()?1:0;
            if(!vis)
                return 0;
        }
        auto svp = Base::freecad_dynamic_cast<ViewProviderDocumentObject>(
                Application::Instance->getViewProvider(sobj));
        if(!svp)
            return 0;
        vp = svp;
        obj = sobj;
        transform = false;
    }

    auto bbox3 = vp->getBoundingBox(0,&mat,transform);
    if(!bbox3.IsValid())
        return 0;

    auto bbox = bbox3.ProjectBox(&proj);
    if(!bbox.Intersect(polygon)) 
        return 0;

    const auto &subs = obj->getSubObjects(App::DocumentObject::GS_SELECT);
    if(subs.size()) {
        int res = 0;
        for(auto &sub : subs) {
            int r = checkElementIntersection(vp, sub.c_str(), proj, polygon, mat, false, depth+1);
            if (r > 0)
                return 1;
            // Return < 0 means either the object does not have shape, or the shape
            // type does not implement sub-element intersection check.
            if (r < 0)
                res = -1;
        }
        return res;
    }

    Base::PyGILStateLocker lock;
    PyObject *pyobj = 0;
    obj->getSubObject(0,&pyobj,&mat,transform,depth);
    if(!pyobj)
        return -1;
    Py::Object pyobject(pyobj,true);
    if(!PyObject_TypeCheck(pyobj,&Data::ComplexGeoDataPy::Type))
        return -1;
    auto data = static_cast<Data::ComplexGeoDataPy*>(pyobj)->getComplexGeoDataPtr();
    int res = -1;
    for(auto type : data->getElementTypes()) {
        size_t count = data->countSubElements(type);
        if(!count)
            continue;
        for(size_t i=1;i<=count;++i) {
            std::string element(type);
            element += std::to_string(i);
            std::unique_ptr<Data::Segment> segment(data->getSubElementByName(element.c_str()));
            if(!segment)
                continue;
            std::vector<Base::Vector3d> points;
            std::vector<Base::Vector3d> pointNormals; // not used
            std::vector<Data::ComplexGeoData::Facet> faces;

            // Call getFacesFromSubelement to obtain the triangulation of
            // the segment.
            data->getFacesFromSubelement(segment.get(),points,pointNormals,faces);
            if(faces.empty())
                continue;

            res = 0;

            Base::Polygon2d loop;
            for(auto &facet : faces) {
                auto v = proj(points[facet.I1]);
                loop.Add(Base::Vector2d(v.x, v.y));
                v = proj(points[facet.I2]);
                loop.Add(Base::Vector2d(v.x, v.y));
                v = proj(points[facet.I3]);
                loop.Add(Base::Vector2d(v.x, v.y));
                if(polygon.Intersect(loop))
                    return 1;
            }
        }
    }
    return res;
}

void View3DInventorViewer::viewSelection(bool extend)
{
    if(!guiDocument)
        return;

    SoCamera* cam = this->getSoRenderManager()->getCamera();
    if(!cam)
        return;

    // When calling viewSelection(extend = true), we are supposed to make sure
    // the current view volume includes at least include some geometry
    // sub-element of all current selection. The volume does not have to include
    // the entire selection. The implementation below uses the screen dimension
    // as a rectangle selection and recursively test intersection. The algorithm
    // used is similar to Command Std_BoxElementSelection.
    SbViewVolume vv = cam->getViewVolume();
    ViewVolumeProjection proj(vv);
    Base::Polygon2d polygon;
    SbViewportRegion viewport = getSoRenderManager()->getViewportRegion();
    const SbVec2s& sp = viewport.getViewportSizePixels();
    auto pos = getGLPolygon({{0,0}, sp});
    polygon.Add(Base::Vector2d(pos[0][0], pos[1][1]));
    polygon.Add(Base::Vector2d(pos[0][0], pos[0][1]));
    polygon.Add(Base::Vector2d(pos[1][0], pos[0][1]));
    polygon.Add(Base::Vector2d(pos[1][0], pos[1][1]));

    Base::BoundBox3d bbox;
    for(auto &sel : Selection().getSelection(guiDocument->getDocument()->getName(),0)) {
        auto vp = Base::freecad_dynamic_cast<ViewProviderDocumentObject>(
                guiDocument->getViewProvider(sel.pObject));
        if(!vp)
            continue;

        if(!extend || !checkElementIntersection(vp, sel.SubName, proj, polygon, Base::Matrix4D()))
            bbox.Add(vp->getBoundingBox(sel.SubName));
    }

    if (bbox.IsValid()) {
        SbBox3f box(bbox.MinX,bbox.MinY,bbox.MinZ,bbox.MaxX,bbox.MaxY,bbox.MaxZ);
        if(extend) { // whether to extend the current view volume to include the selection

            // Replace the following bounding box intersection test with finer
            // sub-element intersection test.
#if 0
            SbVec3f center = box.getCenter();
            SbVec3f size = box.getSize() 
                * 0.5f * ViewParams::instance()->getViewSelectionExtendFactor();

            // scale the box by the configured factor, so that we don't have to
            // change the camera if the selection is partially in view.
            SbBox3f sbox(center-size, center+size);

            int cullbits = 7;
            // test if the scaled box is completely outside of view
            if(!sbox.outside(vv.getMatrix(),cullbits)) {
                return;
            }
#endif

            float vx,vy,vz;
            SbVec3f vcenter = vv.getProjectionPoint()+vv.getProjectionDirection()*(vv.getDepth()*0.5+vv.getNearDist());
            vcenter.getValue(vx,vy,vz);

            float radius = std::max(vv.getWidth(),vv.getHeight())*0.5f;

            // A rough estimation of the view bounding box. Note that
            // SoCamera::viewBoundingBox() is not accurate as well. It uses a
            // sphere to surround the bounding box for easy calculation.
            SbBox3f vbox(vx-radius,vy-radius,vz-radius,vx+radius,vy+radius,vz+radius);

            // extend the view box to include the selection
            vbox.extendBy(box);

            // obtain the entire scene bounding box
            SbBox3f scenebox;
            getSceneBoundBox(scenebox);

            // extend to include the selection, just to be sure
            scenebox.extendBy(box);

            float minx, miny, minz, maxx, maxy, maxz;
            vbox.getBounds(minx, miny, minz, maxx, maxy, maxz);

            // clip the extended current view box to the scene box
            float minx2, miny2, minz2, maxx2, maxy2, maxz2;
            scenebox.getBounds(minx2, miny2, minz2, maxx2, maxy2, maxz2);
            if(minx < minx2) minx = minx2;
            if(miny < miny2) miny = miny2;
            if(minz < minz2) minz = minz2;
            if(maxx > maxx2) maxx = maxx2;
            if(maxy > maxy2) maxy = maxy2;
            if(maxz > maxz2) maxz = maxz2;
            box.setBounds(minx, miny, minz, maxx, maxy, maxz);
        }
        viewBoundBox(box);
    }
}

void View3DInventorViewer::viewBoundBox(const SbBox3f &box) {
    SoCamera* cam = getSoRenderManager()->getCamera();
    if(!cam)
        return;

#if (COIN_MAJOR_VERSION >= 4)
    float aspectratio = getSoRenderManager()->getViewportRegion().getViewportAspectRatio();
    switch (cam->viewportMapping.getValue()) {
        case SoCamera::CROP_VIEWPORT_FILL_FRAME:
        case SoCamera::CROP_VIEWPORT_LINE_FRAME:
        case SoCamera::CROP_VIEWPORT_NO_FRAME:
            aspectratio = 1.0f;
            break;
        default:
            break;
    }
    cam->viewBoundingBox(box,aspectratio,1.0);
#else
    SoTempPath path(2);
    path.ref();
    auto pcGroup = new SoGroup;
    pcGroup->ref();
    auto pcTransform = new SoTransform;
    pcGroup->addChild(pcTransform);
    pcTransform->translation = box.getCenter();
    auto *pcCube = new SoCube;
    pcGroup->addChild(pcCube);
    float sizeX,sizeY,sizeZ;
    box.getSize(sizeX,sizeY,sizeZ);
    pcCube->width = sizeX;
    pcCube->height = sizeY;
    pcCube->depth = sizeZ;
    path.append(pcGroup);
    path.append(pcCube);
    cam->viewAll(&path,getSoRenderManager()->getViewportRegion());
    path.unrefNoDelete();
    pcGroup->unref();
#endif
}

/*!
  Decide if it should be possible to start a spin animation of the
  model in the viewer by releasing the mouse button while dragging.

  If the \a enable flag is \c false and we're currently animating, the
  spin will be stopped.
*/
void
View3DInventorViewer::setAnimationEnabled(const SbBool enable)
{
    navigation->setAnimationEnabled(enable);
}

/*!
  Query whether or not it is possible to start a spinning animation by
  releasing the left mouse button while dragging the mouse.
*/

SbBool
View3DInventorViewer::isAnimationEnabled(void) const
{
    return navigation->isAnimationEnabled();
}

/*!
  Query if the model in the viewer is currently in spinning mode after
  a user drag.
*/
SbBool View3DInventorViewer::isAnimating(void) const
{
    return navigation->isAnimating();
}

/*!
 * Starts programmatically the viewer in animation mode. The given axis direction
 * is always in screen coordinates, not in world coordinates.
 */
void View3DInventorViewer::startAnimating(const SbVec3f& axis, float velocity)
{
    navigation->startAnimating(axis, velocity);
}

void View3DInventorViewer::stopAnimating(void)
{
    navigation->stopAnimating();
}

void View3DInventorViewer::setPopupMenuEnabled(const SbBool on)
{
    navigation->setPopupMenuEnabled(on);
}

SbBool View3DInventorViewer::isPopupMenuEnabled(void) const
{
    return navigation->isPopupMenuEnabled();
}

/*!
  Set the flag deciding whether or not to show the axis cross.
*/

void
View3DInventorViewer::setFeedbackVisibility(const SbBool enable)
{
    if (enable == this->axiscrossEnabled) {
        return;
    }

    this->axiscrossEnabled = enable;

    if (this->isViewing()) {
        this->getSoRenderManager()->scheduleRedraw();
    }
}

/*!
  Check if the feedback axis cross is visible.
*/

SbBool
View3DInventorViewer::isFeedbackVisible(void) const
{
    return this->axiscrossEnabled;
}

/*!
  Set the size of the feedback axiscross.  The value is interpreted as
  an approximate percentage chunk of the dimensions of the total
  canvas.
*/
void
View3DInventorViewer::setFeedbackSize(const int size)
{
    if (size < 1) {
        return;
    }

    this->axiscrossSize = size;

    if (this->isFeedbackVisible() && this->isViewing()) {
        this->getSoRenderManager()->scheduleRedraw();
    }
}

/*!
  Return the size of the feedback axis cross. Default is 10.
*/

int
View3DInventorViewer::getFeedbackSize(void) const
{
    return this->axiscrossSize;
}

/*!
  Decide whether or not the mouse pointer cursor should be visible in
  the rendering canvas.
*/
void View3DInventorViewer::setCursorEnabled(SbBool /*enable*/)
{
    this->setCursorRepresentation(navigation->getViewingMode());
}

void View3DInventorViewer::afterRealizeHook(void)
{
    inherited::afterRealizeHook();
    this->setCursorRepresentation(navigation->getViewingMode());
}

// Documented in superclass. This method overridden from parent class
// to make sure the mouse pointer cursor is updated.
void View3DInventorViewer::setViewing(SbBool enable)
{
    if (this->isViewing() == enable) {
        return;
    }

    navigation->setViewingMode(enable ?
        NavigationStyle::IDLE : NavigationStyle::INTERACT);
    inherited::setViewing(enable);
}

//****************************************************************************

// Bitmap representations of an "X", a "Y" and a "Z" for the axis cross.
static GLubyte xbmp[] = { 0x11,0x11,0x0a,0x04,0x0a,0x11,0x11 };
static GLubyte ybmp[] = { 0x04,0x04,0x04,0x04,0x0a,0x11,0x11 };
static GLubyte zbmp[] = { 0x1f,0x10,0x08,0x04,0x02,0x01,0x1f };

void View3DInventorViewer::drawAxisCross(void)
{
    // FIXME: convert this to a superimposition scenegraph instead of
    // OpenGL calls. 20020603 mortene.

    // Store GL state.
    glPushAttrib(GL_ALL_ATTRIB_BITS);
    GLfloat depthrange[2];
    glGetFloatv(GL_DEPTH_RANGE, depthrange);
    GLdouble projectionmatrix[16];
    glGetDoublev(GL_PROJECTION_MATRIX, projectionmatrix);

    glDepthFunc(GL_ALWAYS);
    glDepthMask(GL_TRUE);
    glDepthRange(0, 0);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);
    glEnable(GL_COLOR_MATERIAL);
    glDisable(GL_BLEND); // Kills transparency.

    // Set the viewport in the OpenGL canvas. Dimensions are calculated
    // as a percentage of the total canvas size.
    SbVec2s view = this->getSoRenderManager()->getSize();
    const int pixelarea =
        int(float(this->axiscrossSize)/100.0f * std::min(view[0], view[1]));
#if 0 // middle of canvas
    SbVec2s origin(view[0]/2 - pixelarea/2, view[1]/2 - pixelarea/2);
#endif // middle of canvas
#if 1 // lower right of canvas
    SbVec2s origin(view[0] - pixelarea, 0);
#endif // lower right of canvas
    glViewport(origin[0], origin[1], pixelarea, pixelarea);

    // Set up the projection matrix.
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    const float NEARVAL = 0.1f;
    const float FARVAL = 10.0f;
    const float dim = NEARVAL * float(tan(M_PI / 8.0)); // FOV is 45 deg (45/360 = 1/8)
    glFrustum(-dim, dim, -dim, dim, NEARVAL, FARVAL);


    // Set up the model matrix.
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    SbMatrix mx;
    SoCamera* cam = this->getSoRenderManager()->getCamera();

    // If there is no camera (like for an empty scene, for instance),
    // just use an identity rotation.
    if (cam) {
        mx = cam->orientation.getValue();
    }
    else {
        mx = SbMatrix::identity();
    }

    mx = mx.inverse();
    mx[3][2] = -3.5; // Translate away from the projection point (along z axis).
    glLoadMatrixf((float*)mx);


    // Find unit vector end points.
    SbMatrix px;
    glGetFloatv(GL_PROJECTION_MATRIX, (float*)px);
    SbMatrix comb = mx.multRight(px);

    SbVec3f xpos;
    comb.multVecMatrix(SbVec3f(1,0,0), xpos);
    xpos[0] = (1 + xpos[0]) * view[0]/2;
    xpos[1] = (1 + xpos[1]) * view[1]/2;
    SbVec3f ypos;
    comb.multVecMatrix(SbVec3f(0,1,0), ypos);
    ypos[0] = (1 + ypos[0]) * view[0]/2;
    ypos[1] = (1 + ypos[1]) * view[1]/2;
    SbVec3f zpos;
    comb.multVecMatrix(SbVec3f(0,0,1), zpos);
    zpos[0] = (1 + zpos[0]) * view[0]/2;
    zpos[1] = (1 + zpos[1]) * view[1]/2;


    // Render the cross.
    {
        glLineWidth(2.0);

        enum { XAXIS, YAXIS, ZAXIS };
        int idx[3] = { XAXIS, YAXIS, ZAXIS };
        float val[3] = { xpos[2], ypos[2], zpos[2] };

        // Bubble sort.. :-}
        if (val[0] < val[1]) {
            std::swap(val[0], val[1]);
            std::swap(idx[0], idx[1]);
        }

        if (val[1] < val[2]) {
            std::swap(val[1], val[2]);
            std::swap(idx[1], idx[2]);
        }

        if (val[0] < val[1]) {
            std::swap(val[0], val[1]);
            std::swap(idx[0], idx[1]);
        }

        assert((val[0] >= val[1]) && (val[1] >= val[2])); // Just checking..

        for (int i=0; i < 3; i++) {
            glPushMatrix();

            if (idx[i] == XAXIS) {                        // X axis.
                if (stereoMode() != Quarter::SoQTQuarterAdaptor::MONO)
                    glColor3f(0.500f, 0.5f, 0.5f);
                else
                    glColor3f(0.500f, 0.125f, 0.125f);
            }
            else if (idx[i] == YAXIS) {                   // Y axis.
                glRotatef(90, 0, 0, 1);

                if (stereoMode() != Quarter::SoQTQuarterAdaptor::MONO)
                    glColor3f(0.400f, 0.4f, 0.4f);
                else
                    glColor3f(0.125f, 0.500f, 0.125f);
            }
            else {                                        // Z axis.
                glRotatef(-90, 0, 1, 0);

                if (stereoMode() != Quarter::SoQTQuarterAdaptor::MONO)
                    glColor3f(0.300f, 0.3f, 0.3f);
                else
                    glColor3f(0.125f, 0.125f, 0.500f);
            }

            this->drawArrow();
            glPopMatrix();
        }
    }

    // Render axis notation letters ("X", "Y", "Z").
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, view[0], 0, view[1], -1, 1);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    GLint unpack;
    glGetIntegerv(GL_UNPACK_ALIGNMENT, &unpack);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    if (stereoMode() != Quarter::SoQTQuarterAdaptor::MONO)
        glColor3fv(SbVec3f(1.0f, 1.0f, 1.0f).getValue());
    else
        glColor3fv(SbVec3f(0.0f, 0.0f, 0.0f).getValue());

    glRasterPos2d(xpos[0], xpos[1]);
    glBitmap(8, 7, 0, 0, 0, 0, xbmp);
    glRasterPos2d(ypos[0], ypos[1]);
    glBitmap(8, 7, 0, 0, 0, 0, ybmp);
    glRasterPos2d(zpos[0], zpos[1]);
    glBitmap(8, 7, 0, 0, 0, 0, zbmp);

    glPixelStorei(GL_UNPACK_ALIGNMENT, unpack);
    glPopMatrix();

    // Reset original state.

    // FIXME: are these 3 lines really necessary, as we push
    // GL_ALL_ATTRIB_BITS at the start? 20000604 mortene.
    glDepthRange(depthrange[0], depthrange[1]);
    glMatrixMode(GL_PROJECTION);
    glLoadMatrixd(projectionmatrix);

    glPopAttrib();
}

// Draw an arrow for the axis representation directly through OpenGL.
void View3DInventorViewer::drawArrow(void)
{
    glBegin(GL_LINES);
    glVertex3f(0.0f, 0.0f, 0.0f);
    glVertex3f(1.0f, 0.0f, 0.0f);
    glEnd();
    glDisable(GL_CULL_FACE);
    glBegin(GL_TRIANGLES);
    glVertex3f(1.0f, 0.0f, 0.0f);
    glVertex3f(1.0f - 1.0f / 3.0f, +0.5f / 4.0f, 0.0f);
    glVertex3f(1.0f - 1.0f / 3.0f, -0.5f / 4.0f, 0.0f);
    glVertex3f(1.0f, 0.0f, 0.0f);
    glVertex3f(1.0f - 1.0f / 3.0f, 0.0f, +0.5f / 4.0f);
    glVertex3f(1.0f - 1.0f / 3.0f, 0.0f, -0.5f / 4.0f);
    glEnd();
    glBegin(GL_QUADS);
    glVertex3f(1.0f - 1.0f / 3.0f, +0.5f / 4.0f, 0.0f);
    glVertex3f(1.0f - 1.0f / 3.0f, 0.0f, +0.5f / 4.0f);
    glVertex3f(1.0f - 1.0f / 3.0f, -0.5f / 4.0f, 0.0f);
    glVertex3f(1.0f - 1.0f / 3.0f, 0.0f, -0.5f / 4.0f);
    glEnd();
}

// ************************************************************************

// Set cursor graphics according to mode.
void View3DInventorViewer::setCursorRepresentation(int modearg)
{
    // There is a synchronization problem between Qt and SoQt which
    // happens when popping up a context-menu. In this case the
    // Qt::WA_UnderMouse attribute is reset and never set again
    // even if the mouse is still in the canvas. Thus, the cursor
    // won't be changed as long as the user doesn't leave and enter
    // the canvas. To fix this we explicitly set Qt::WA_UnderMouse
    // if the mouse is inside the canvas.
    QWidget* glWindow = this->getGLWidget();

    // When a widget is added to the QGraphicsScene and the user
    // hovered over it the 'WA_SetCursor' attribute is set to the
    // GL widget but never reset and thus would cause that the
    // cursor on this widget won't be set.
    if (glWindow)
        glWindow->setAttribute(Qt::WA_SetCursor, false);

    if (glWindow && glWindow->rect().contains(QCursor::pos()))
        glWindow->setAttribute(Qt::WA_UnderMouse);

    QWidget *widget = QWidget::mouseGrabber();
    if (!widget)
        widget = this->getWidget();

    switch (modearg) {
    case NavigationStyle::IDLE:
    case NavigationStyle::INTERACT:
        if (isEditing())
            widget->setCursor(this->editCursor);
        else
            widget->setCursor(QCursor(Qt::ArrowCursor));
        break;

    case NavigationStyle::DRAGGING:
    case NavigationStyle::SPINNING:
        widget->setCursor(spinCursor);
        break;

    case NavigationStyle::ZOOMING:
        widget->setCursor(zoomCursor);
        break;

    case NavigationStyle::SEEK_MODE:
    case NavigationStyle::SEEK_WAIT_MODE:
    case NavigationStyle::BOXZOOM:
        widget->setCursor(Qt::CrossCursor);
        break;

    case NavigationStyle::PANNING:
        widget->setCursor(panCursor);
        break;

    case NavigationStyle::SELECTION:
        widget->setCursor(Qt::PointingHandCursor);
        break;

    default:
        assert(0);
        break;
    }
}

void View3DInventorViewer::setEditing(SbBool edit)
{
    this->editing = edit;
    this->getWidget()->setCursor(QCursor(Qt::ArrowCursor));
    this->editCursor = QCursor();
}

void View3DInventorViewer::setComponentCursor(const QCursor& cursor)
{
    this->getWidget()->setCursor(cursor);
}

void View3DInventorViewer::setEditingCursor(const QCursor& cursor)
{
    this->getWidget()->setCursor(cursor);
    this->editCursor = this->getWidget()->cursor();
}

void View3DInventorViewer::selectCB(void* viewer, SoPath* path)
{
    ViewProvider* vp = static_cast<View3DInventorViewer*>(viewer)->getViewProviderByPath(path);
    if (vp && vp->useNewSelectionModel()) {
    }
}

void View3DInventorViewer::deselectCB(void* viewer, SoPath* path)
{
    ViewProvider* vp = static_cast<View3DInventorViewer*>(viewer)->getViewProviderByPath(path);
    if (vp && vp->useNewSelectionModel()) {
    }
}

SoPath* View3DInventorViewer::pickFilterCB(void* viewer, const SoPickedPoint* pp)
{
    ViewProvider* vp = static_cast<View3DInventorViewer*>(viewer)->getViewProviderByPath(pp->getPath());
    if (vp && vp->useNewSelectionModel()) {
        std::string e = vp->getElement(pp->getDetail());
        vp->getSelectionShape(e.c_str());
        static char buf[513];
        snprintf(buf,512,"Hovered: %s (%f,%f,%f)"
                 ,e.c_str()
                 ,pp->getPoint()[0]
                 ,pp->getPoint()[1]
                 ,pp->getPoint()[2]);

        getMainWindow()->showMessage(QString::fromLatin1(buf),3000);
    }

    return pp->getPath();
}

void View3DInventorViewer::addEventCallback(SoType eventtype, SoEventCallbackCB* cb, void* userdata)
{
    pEventCallback->addEventCallback(eventtype, cb, userdata);
}

void View3DInventorViewer::removeEventCallback(SoType eventtype, SoEventCallbackCB* cb, void* userdata)
{
    pEventCallback->removeEventCallback(eventtype, cb, userdata);
}

ViewProvider* View3DInventorViewer::getViewProviderByPath(SoPath* path) const
{
    if(guiDocument)
        return guiDocument->getViewProviderByPathFromHead(path);
    return 0;
}

ViewProvider* View3DInventorViewer::getViewProviderByPathFromTail(SoPath* path) const
{
    if(guiDocument)
        return guiDocument->getViewProviderByPathFromTail(path);
    return 0;
}

std::vector<ViewProvider*> View3DInventorViewer::getViewProvidersOfType(const Base::Type& typeId) const
{
    std::vector<ViewProvider*> views;
    for (std::set<ViewProvider*>::const_iterator it = _ViewProviderSet.begin(); it != _ViewProviderSet.end(); ++it) {
        if ((*it)->getTypeId().isDerivedFrom(typeId)) {
            views.push_back(*it);
        }
    }

    return views;
}

void View3DInventorViewer::turnAllDimensionsOn()
{
    dimensionRoot->whichChild = SO_SWITCH_ALL;
}

void View3DInventorViewer::turnAllDimensionsOff()
{
    dimensionRoot->whichChild = SO_SWITCH_NONE;
}

void View3DInventorViewer::eraseAllDimensions()
{
    coinRemoveAllChildren(static_cast<SoSwitch*>(dimensionRoot->getChild(0)));
    coinRemoveAllChildren(static_cast<SoSwitch*>(dimensionRoot->getChild(1)));
}

void View3DInventorViewer::turn3dDimensionsOn()
{
    static_cast<SoSwitch*>(dimensionRoot->getChild(0))->whichChild = SO_SWITCH_ALL;
}

void View3DInventorViewer::turn3dDimensionsOff()
{
    static_cast<SoSwitch*>(dimensionRoot->getChild(0))->whichChild = SO_SWITCH_NONE;
}

void View3DInventorViewer::addDimension3d(SoNode* node)
{
    static_cast<SoSwitch*>(dimensionRoot->getChild(0))->addChild(node);
}

void View3DInventorViewer::addDimensionDelta(SoNode* node)
{
    static_cast<SoSwitch*>(dimensionRoot->getChild(1))->addChild(node);
}

void View3DInventorViewer::turnDeltaDimensionsOn()
{
    static_cast<SoSwitch*>(dimensionRoot->getChild(1))->whichChild = SO_SWITCH_ALL;
}

void View3DInventorViewer::turnDeltaDimensionsOff()
{
    static_cast<SoSwitch*>(dimensionRoot->getChild(1))->whichChild = SO_SWITCH_NONE;
}

PyObject *View3DInventorViewer::getPyObject(void)
{
    if (!_viewerPy)
        _viewerPy = new View3DInventorViewerPy(this);

    Py_INCREF(_viewerPy);
    return _viewerPy;
}

/**
 * Drops the event \a e and loads the files into the given document.
 */
void View3DInventorViewer::dropEvent (QDropEvent * e)
{
    const QMimeData* data = e->mimeData();
    if (data->hasUrls() && guiDocument) {
        getMainWindow()->loadUrls(guiDocument->getDocument(), data->urls());
    }
    else {
        inherited::dropEvent(e);
    }
}

void View3DInventorViewer::dragEnterEvent (QDragEnterEvent * e)
{
    // Here we must allow uri drags and check them in dropEvent
    const QMimeData* data = e->mimeData();
    if (data->hasUrls()) {
        e->accept();
    }
    else {
        inherited::dragEnterEvent(e);
    }
}

void View3DInventorViewer::dragMoveEvent(QDragMoveEvent *e)
{
    const QMimeData* data = e->mimeData();
    if (data->hasUrls() && guiDocument) {
        e->accept();
    }
    else {
        inherited::dragMoveEvent(e);
    }
}

void View3DInventorViewer::dragLeaveEvent(QDragLeaveEvent *e)
{
    inherited::dragLeaveEvent(e);
}

void View3DInventorViewer::callEventFilter(QEvent *e)
{
    getEventFilter()->eventFilter(this, e);
}

void View3DInventorViewer::ShadowInfo::onRender()
{
    if (!pcShadowGroup)
        return;
    SoCamera* cam = owner->getSoRenderManager()->getCamera();
    if(cam) {
        if(animating || shadowNodeId != pcShadowGroup->getNodeId() || cameraNodeId != cam->getNodeId())
            timer.start(100);
        else if (shadowExtraRedraw) {
            shadowExtraRedraw = false;
            owner->getSoRenderManager()->scheduleRedraw();
        }
    }
}

void View3DInventorViewer::redrawShadow()
{
    shadowInfo->redraw();
}

void View3DInventorViewer::ShadowInfo::redraw()
{
    if (animating) {
        timer.start(100);
        return;
    }
    timer.stop();
    SoCamera* cam = owner->getSoRenderManager()->getCamera();
    if(pcShadowGroup && pcShadowGroundSwitch && cam) {
        // Work around coin shadow rendering bug. On Windows, (and occasionally
        // on Linux), when shadow group is touched, it renders nothing when the
        // shadow cache is freshly built. We work around this issue using an
        // extra redraw, and the node renders fine with the already built
        // cache.
        //
        // Amendment: directional shadow light requires update on camera change
        // (not sure why or if it's absolutely needed yet). A patch has been
        // added to Coin3D to perform only quick partial update if there is no
        // scene changes.  We shall schedule an extra redraw to perform a full
        // update by touching the shadow group.
        pcShadowGround->touch();
        shadowNodeId = pcShadowGroup->getNodeId();
        cameraNodeId = cam->getNodeId();
        owner->getSoRenderManager()->scheduleRedraw();
        shadowExtraRedraw = ViewParams::getShadowExtraRedraw();
    }
}

void View3DInventorViewer::toggleShadowLightManip(int toggle)
{
    shadowInfo->toggleDragger(toggle);
}

void View3DInventorViewer::ShadowInfo::toggleDragger(int toggle)
{
    App::Document *doc = owner->guiDocument?owner->guiDocument->getDocument():nullptr;
    if (!pcShadowGroup || !doc)
        return;

    bool dirlight = pcShadowGroup->findChild(pcShadowDirectionalLight) >= 0;
    SoSFBool &showDragger = dirlight?pcShadowDirectionalLight->showDragger:pcShadowSpotLight->showDragger;

    if (showDragger.getValue() && toggle <= 0) {
        showDragger = FALSE;
        pcShadowPickStyle->style = SoPickStyle::SHAPE;

        App::GetApplication().setActiveTransaction("Change shadow light");

        SbVec3f dir;
        if (dirlight)
            dir = pcShadowDirectionalLight->direction.getValue();
        else {
            dir = pcShadowSpotLight->direction.getValue();

            SbVec3f pos = pcShadowSpotLight->location.getValue();
            _shadowSetParam<App::PropertyVector>(doc, "SpotLightPosition",
                    Base::Vector3d(pos[0], pos[1], pos[2]));

            _shadowSetParam<App::PropertyAngle>(doc, "SpotLightCutOffAngle",
                    pcShadowSpotLight->cutOffAngle.getValue() * 180.0 / M_PI);
        }
        _shadowSetParam<App::PropertyVector>(doc, "LightDirection",
                Base::Vector3d(dir[0], dir[1], dir[2]));

        App::GetApplication().closeActiveTransaction();

    } else if (!showDragger.getValue() && toggle != 0) {
        pcShadowPickStyle->style = SoPickStyle::UNPICKABLE;
        SbBox3f bbox;
        showDragger = TRUE;
        owner->getSceneBoundBox(bbox);
        this->getBoundingBox(bbox);
        if (!bbox.isEmpty())
            owner->viewBoundBox(bbox);
    }
}

static std::vector<std::string> getBoxSelection(const Base::Vector3d *dir,
        ViewProviderDocumentObject *vp, bool center, bool pickElement,
        const Base::ViewProjMethod &proj, const Base::Polygon2d &polygon,
        const Base::Matrix4D &mat, bool transform=true, int depth=0)
{
    std::vector<std::string> ret;
    auto obj = vp->getObject();
    if(!obj || !obj->getNameInDocument())
        return ret;

    // DO NOT check this view object Visibility, let the caller do this. Because
    // we may be called by upper object hierarchy that manages our visibility.

    auto bbox3 = vp->getBoundingBox(0,&mat,transform);
    if(!bbox3.IsValid())
        return ret;

    auto bbox = bbox3.ProjectBox(&proj);

    // check if both two boundary points are inside polygon, only
    // valid since we know the given polygon is a box.
    if(!pickElement 
            && polygon.Contains(Base::Vector2d(bbox.MinX,bbox.MinY))
            && polygon.Contains(Base::Vector2d(bbox.MaxX,bbox.MaxY))) 
    {
        ret.emplace_back("");
        return ret;
    }

    if(!bbox.Intersect(polygon)) 
        return ret;

    const auto &subs = obj->getSubObjects(App::DocumentObject::GS_SELECT);
    if(subs.empty()) {
        if(!pickElement) {
            if(!center || polygon.Contains(bbox.GetCenter()))
                ret.emplace_back("");
            return ret;
        }
        Base::PyGILStateLocker lock;
        PyObject *pyobj = 0;
        Base::Matrix4D matCopy(mat);
        obj->getSubObject(0,&pyobj,&matCopy,transform,depth);
        if(!pyobj)
            return ret;
        Py::Object pyobject(pyobj,true);
        if(!PyObject_TypeCheck(pyobj,&Data::ComplexGeoDataPy::Type))
            return ret;
        auto data = static_cast<Data::ComplexGeoDataPy*>(pyobj)->getComplexGeoDataPtr();
        for(auto type : data->getElementTypes()) {
            size_t count = data->countSubElements(type);
            if(!count)
                continue;
            for(size_t i=1;i<=count;++i) {
                std::string element(type);
                element += std::to_string(i);
                std::unique_ptr<Data::Segment> segment(data->getSubElementByName(element.c_str()));
                if(!segment)
                    continue;
                std::vector<Base::Vector3d> points;
                std::vector<Data::ComplexGeoData::Line> lines;

                std::vector<Base::Vector3d> pointNormals; // not used
                std::vector<Data::ComplexGeoData::Facet> faces;

                // Call getFacesFromSubelement to obtain the triangulation of
                // the segment.
                data->getFacesFromSubelement(segment.get(),points,pointNormals,faces);
                if(faces.empty())
                    continue;

                Base::Polygon2d loop;
                bool hit = false;
                for(auto &facet : faces) {
                    // back face cull
                    if (dir) {
                        Base::Vector3d normal = (points[facet.I2] - points[facet.I1])
                            % (points[facet.I3] - points[facet.I1]);
                        normal.Normalize();
                        if (normal.Dot(*dir) < 0.0f)
                            continue;
                    }
                    auto v = proj(points[facet.I1]);
                    loop.Add(Base::Vector2d(v.x, v.y));
                    v = proj(points[facet.I2]);
                    loop.Add(Base::Vector2d(v.x, v.y));
                    v = proj(points[facet.I3]);
                    loop.Add(Base::Vector2d(v.x, v.y));
                    if (!center) {
                        if(polygon.Intersect(loop)) {
                            hit = true;
                            break;
                        }
                        loop.DeleteAll();
                    }
                }
                if (center && loop.GetCtVectors()
                           && polygon.Contains(loop.CalcBoundBox().GetCenter()))
                    hit = true;
                if (hit)
                    ret.push_back(element);
            }
        }
        return ret;
    }

    size_t count = 0;
    for(auto &sub : subs) {
        App::DocumentObject *parent = 0;
        std::string childName;
        Base::Matrix4D smat(mat);
        auto sobj = obj->resolve(sub.c_str(),&parent,&childName,0,0,&smat,transform,depth+1);
        if(!sobj) 
            continue;
        int vis;
        if(!parent || (vis=parent->isElementVisibleEx(childName.c_str(),App::DocumentObject::GS_SELECT))<0)
            vis = sobj->Visibility.getValue()?1:0;

        if(!vis)
            continue;

        auto svp = dynamic_cast<ViewProviderDocumentObject*>(Application::Instance->getViewProvider(sobj));
        if(!svp)
            continue;

        const auto &sels = getBoxSelection(dir,svp,center,pickElement,proj,polygon,smat,false,depth+1);
        if(sels.size()==1 && sels[0] == "")
            ++count;
        for(auto &sel : sels)
            ret.emplace_back(sub+sel);
    }
    if(count==subs.size()) {
        ret.resize(1);
        ret[0].clear();
    }
    return ret;
}


std::vector<App::SubObjectT>
View3DInventorViewer::getPickedList(const std::vector<SbVec2f> &pts,
                                    bool center, 
                                    bool pickElement,
                                    bool backfaceCull,
                                    bool currentSelection,
                                    bool unselect,
                                    bool mapCoords) const
{
    std::vector<App::SubObjectT> res;

    App::Document* doc = App::GetApplication().getActiveDocument();
    if (!doc)
        return res;

    auto getPt = [this,mapCoords](const SbVec2f &p) -> Base::Vector2d {
        Base::Vector2d pt(p[0], p[1]);
        if (mapCoords) {
            pt.y = this->height() - pt.y - 1;
            if (this->width())
                pt.x /= this->width();
            if (this->height())
                pt.y /= this->height();
        }
        return pt;
    };

    Base::Polygon2d polygon;
    if (pts.size() == 2) {
        auto pt1 = getPt(pts[0]);
        auto pt2 = getPt(pts[1]);
        polygon.Add(Base::Vector2d(pt1.x, pt1.y));
        polygon.Add(Base::Vector2d(pt1.x, pt2.y));
        polygon.Add(Base::Vector2d(pt2.x, pt2.y));
        polygon.Add(Base::Vector2d(pt2.x, pt1.y));
    } else {
        for (auto &pt : pts)
            polygon.Add(getPt(pt));
    }

    Base::Vector3d vdir, *pdir = nullptr;
    if (backfaceCull) {
        SbVec3f pnt, dir;
        this->getNearPlane(pnt, dir);
        vdir = Base::Vector3d(dir[0],dir[1],dir[2]);
        pdir = &vdir;
    }

    SoCamera* cam = this->getSoRenderManager()->getCamera();
    SbViewVolume vv = cam->getViewVolume();
    Gui::ViewVolumeProjection proj(vv);

    std::set<App::SubObjectT> sels; 
    std::map<App::SubObjectT, std::vector<const App::SubObjectT*> > selObjs; 
    if(currentSelection || unselect) {
        for (auto &sel : Gui::Selection().getSelectionT(doc->getName(),0)) {
            auto r = sels.insert(sel);
            const App::SubObjectT &objT = *r.first;
            if (currentSelection || (unselect && !pickElement))
                selObjs[App::SubObjectT(sel.getDocumentName().c_str(),
                                        sel.getObjectName().c_str(),
                                        sel.getSubNameNoElement().c_str())].push_back(&objT);
        }
    }

    auto handler = [&](App::SubObjectT &&objT) {
        if (!unselect) {
            res.push_back(std::move(objT));
            return;
        }
        if (pickElement) {
            if (sels.count(objT))
                res.push_back(std::move(objT));
            return;
        }
        auto it = selObjs.find(objT);
        if (it != selObjs.end()) {
            for (auto selT : it->second)
                res.push_back(*selT);
        }
    };

    if(currentSelection && sels.size()) {
        for(auto &v : selObjs) {
            auto &sel = v.first;
            App::DocumentObject *obj = sel.getObject();
            if (!obj)
                continue;
            Base::Matrix4D mat;
            App::DocumentObject *sobj = obj->getSubObject(sel.getSubName().c_str(),0,&mat);
            auto vp = Base::freecad_dynamic_cast<ViewProviderDocumentObject>(
                    Application::Instance->getViewProvider(sobj));
            if(!vp)
                continue;
            for(auto &sub : getBoxSelection(pdir,vp,center,pickElement,proj,polygon,mat,false))
                handler(App::SubObjectT(obj, (sel.getSubName()+sub).c_str()));
        }

    } else {
        for(auto obj : doc->getObjects()) {
            if(App::GeoFeatureGroupExtension::isNonGeoGroup(obj)
                    || App::GeoFeatureGroupExtension::getGroupOfObject(obj))
                continue;

            auto vp = Base::freecad_dynamic_cast<ViewProviderDocumentObject>(
                    Application::Instance->getViewProvider(obj));
            if (!vp || !vp->isVisible() || !vp->isShowable())
                continue;

            Base::Matrix4D mat;
            for(auto &sub : getBoxSelection(pdir,vp,center,pickElement,proj,polygon,mat))
                handler(App::SubObjectT(obj, sub.c_str()));
        }
    }

    return res;
}

#include "moc_View3DInventorViewer.cpp"
