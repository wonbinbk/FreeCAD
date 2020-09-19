/***************************************************************************
 *   Copyright (c) 2011 Juergen Riegel <FreeCAD@juergen-riegel.net>        *
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
# include <QMessageBox>
# include <QAction>
# include <QApplication>
# include <QMenu>
# include <Inventor/nodes/SoSwitch.h>
# include <Inventor/details/SoFaceDetail.h>
#endif

#include <Gui/Command.h>
#include <Gui/MDIView.h>
#include <Gui/Control.h>
#include <Gui/Application.h>
#include <Gui/Document.h>
#include <Gui/BitmapFactory.h>
#include <Gui/SoFCUnifiedSelection.h>
#include <Gui/CommandT.h>
#include <Gui/Tree.h>
#include <Base/Exception.h>
#include <Mod/Part/Gui/SoBrepFaceSet.h>
#include <Mod/Part/Gui/SoBrepEdgeSet.h>
#include <Mod/Part/Gui/SoBrepPointSet.h>
#include <Mod/PartDesign/App/Body.h>
#include <Mod/PartDesign/App/Feature.h>
#include <Mod/Sketcher/App/SketchObject.h>

#include "Utils.h"
#include "TaskFeatureParameters.h"

#include "ViewProvider.h"
#include "ViewProviderPy.h"

using namespace PartDesignGui;

PROPERTY_SOURCE_WITH_EXTENSIONS(PartDesignGui::ViewProvider, PartGui::ViewProviderPart)

ViewProvider::ViewProvider()
:oldWb(""), oldTip(NULL), isSetTipIcon(false)
{
    PartGui::ViewProviderAttachExtension::initExtension(this);
}

ViewProvider::~ViewProvider()
{
}

bool ViewProvider::doubleClicked(void)
{
#if 0
    // TODO May be move to setEdit()? (2015-07-26, Fat-Zer)
	if (body != NULL) {
        // Drop into insert mode so that the user doesn't see all the geometry that comes later in the tree
        // Also, this way the user won't be tempted to use future geometry as external references for the sketch
		oldTip = body->Tip.getValue();
        if (oldTip != this->pcObject)
            Gui::Command::doCommand(Gui::Command::Gui,"FreeCADGui.runCommand('PartDesign_MoveTip')");
        else
            oldTip = NULL;
    } else {
        oldTip = NULL;
    }
#endif

    try {
	    PartDesign::Body* body = PartDesign::Body::findBodyOf(getObject());
        std::string Msg("Edit ");
        Msg += this->pcObject->Label.getValue();
        Gui::Command::openCommand(Msg.c_str());
        PartDesignGui::setEdit(pcObject,body);
    }
    catch (const Base::Exception&) {
        Gui::Command::abortCommand();
    }
    return true;
}

void ViewProvider::setupContextMenu(QMenu* menu, QObject* receiver, const char* member)
{
    auto feat = Base::freecad_dynamic_cast<PartDesign::Feature>(getObject());
    if(feat) {
        QAction* act = menu->addAction(QObject::tr(feat->Suppress.getValue()?"Unsuppress":"Suppress"),
                receiver, member);
        act->setData(QVariant((int)ViewProvider::UserEditMode+1));
    }
    QAction* act = menu->addAction(QObject::tr("Set colors..."), receiver, member);
    act->setData(QVariant((int)ViewProvider::Color));
}

bool ViewProvider::setEdit(int ModNum)
{
    if (ModNum == ViewProvider::Default ) {
        // When double-clicking on the item for this feature the
        // object unsets and sets its edit mode without closing
        // the task panel
        Gui::TaskView::TaskDialog *dlg = Gui::Control().activeDialog();
        TaskDlgFeatureParameters *featureDlg = qobject_cast<TaskDlgFeatureParameters *>(dlg);
        // NOTE: if the dialog is not partDesigan dialog the featureDlg will be NULL
        if (featureDlg && featureDlg->viewProvider() != this) {
            featureDlg = 0; // another feature left open its task panel
        }
        if (dlg && !featureDlg) {
            QMessageBox msgBox;
            msgBox.setText(QObject::tr("A dialog is already open in the task panel"));
            msgBox.setInformativeText(QObject::tr("Do you want to close this dialog?"));
            msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
            msgBox.setDefaultButton(QMessageBox::Yes);
            int ret = msgBox.exec();
            if (ret == QMessageBox::Yes) {
                Gui::Control().reject();
            } else {
                return false;
            }
        }

        // clear the selection (convenience)
        Gui::Selection().clearSelection();

        // always change to PartDesign WB, remember where we come from
        oldWb = Gui::Command::assureWorkbench("PartDesignWorkbench");

        // start the edit dialog if
        if (!featureDlg) {
            featureDlg = this->getEditDialog();
            if (!featureDlg) { // Shouldn't generally happen
                throw Base::RuntimeError ("Failed to create new edit dialog.");
            }
        }

        Gui::Control().showDialog(featureDlg);
        return true;
    } else if (ModNum == ViewProvider::UserEditMode+1 ) {
        auto feat = Base::freecad_dynamic_cast<PartDesign::Feature>(getObject());
        if(feat) {
            std::ostringstream ss;
            ss << (feat->Suppress.getValue()?"Unsuppress":"Suppress") << " " << feat->getNameInDocument();
            App::GetApplication().setActiveTransaction(ss.str().c_str());
            try {
                if(feat->Suppress.getValue())
                    Gui::cmdAppObject(feat, "Suppress = False"); 
                else
                    Gui::cmdAppObject(feat, "Suppress = True"); 
                Gui::cmdAppDocument(App::GetApplication().getActiveDocument(), "recompute()");
            } catch (Base::Exception &e) {
                e.ReportException();
            }
            App::GetApplication().closeActiveTransaction();
        }
        return false;
    } else {
        return PartGui::ViewProviderPart::setEdit(ModNum);
    }
}


TaskDlgFeatureParameters *ViewProvider::getEditDialog() {
    throw Base::NotImplementedError("getEditDialog() not implemented");
}


void ViewProvider::unsetEdit(int ModNum)
{
    // return to the WB we were in before editing the PartDesign feature
    if (!oldWb.empty())
        Gui::Command::assureWorkbench(oldWb.c_str());

    if (ModNum == ViewProvider::Default) {
        // when pressing ESC make sure to close the dialog
#if 0
        PartDesign::Body* activeBody = Gui::Application::Instance->activeView()->getActiveObject<PartDesign::Body*>(PDBODYKEY);
#endif
        Gui::Control().closeDialog();
#if 0
        if ((activeBody != NULL) && (oldTip != NULL)) {
            Gui::Selection().clearSelection();
            Gui::Selection().addSelection(oldTip->getDocument()->getName(), oldTip->getNameInDocument());
            Gui::Command::doCommand(Gui::Command::Gui,"FreeCADGui.runCommand('PartDesign_MoveTip')");
        }
#endif
        oldTip = NULL;
    }
    else {
        PartGui::ViewProviderPart::unsetEdit(ModNum);
        oldTip = NULL;
    }
}

void ViewProvider::updateData(const App::Property* prop)
{
    auto feature = Base::freecad_dynamic_cast<PartDesign::Feature>(getObject());
    if(!feature) {
        inherited::updateData(prop);
        return;
    }

    if(prop == &feature->SuppressedShape) {
        if (feature->SuppressedShape.getShape().isNull()) {
            enableFullSelectionHighlight();
        } else {
            auto node = getDisplayMaskMode("Flat Lines");
            if (!pSuppressedView && node && node->isOfType(SoGroup::getClassTypeId())) {
                pSuppressedView.reset(new PartGui::ViewProviderPart);
                pSuppressedView->setShapePropertyName("SuppressedShape");
                pSuppressedView->forceUpdate();
                pSuppressedView->MapFaceColor.setValue(false);    
                pSuppressedView->MapLineColor.setValue(false);    
                pSuppressedView->MapPointColor.setValue(false);    
                pSuppressedView->MapTransparency.setValue(false);    
                pSuppressedView->ForceMapColors.setValue(false);
                pSuppressedView->ShapeColor.setValue(App::Color(1.0f));
                pSuppressedView->LineColor.setValue(App::Color(1.0f));
                pSuppressedView->PointColor.setValue(App::Color(1.0f));
                pSuppressedView->Selectable.setValue(false);
                pSuppressedView->enableFullSelectionHighlight(false, false, false);
                pSuppressedView->setStatus(Gui::SecondaryView,true);

                auto switchNode = getModeSwitch();
                if(switchNode->isOfType(SoFCSwitch::getClassTypeId()))
                    static_cast<SoFCSwitch*>(switchNode)->overrideSwitch = SoFCSwitch::OverrideVisible;

                pSuppressedView->attach(feature);

                static_cast<SoGroup*>(node)->addChild(pSuppressedView->getRoot());
            }

            if(pSuppressedView)
                enableFullSelectionHighlight(false, false, false);
        }

        if(pSuppressedView)
            pSuppressedView->updateData(prop);

    } else if (prop == &feature->Suppress) {
        signalChangeIcon();
    }

    inherited::updateData(prop);
}

void ViewProvider::updateVisual()
{
    inherited::updateVisual();
    auto feature = Base::freecad_dynamic_cast<PartDesign::Feature>(getObject());
    if (feature && feature->SuppressedShape.getShape().isNull()) {
        std::vector<int> faces;
        std::vector<int> edges;
        std::vector<int> vertices;
        feature->getGeneratedIndices(faces,edges,vertices);

        lineset->highlightIndices.setNum(edges.size());
        lineset->highlightIndices.setValues(0,edges.size(),&edges[0]);
        nodeset->highlightIndices.setNum(vertices.size());
        nodeset->highlightIndices.setValues(0,vertices.size(),&vertices[0]);
        faceset->highlightIndices.setNum(faces.size());
        faceset->highlightIndices.setValues(0,faces.size(),&faces[0]);
    }
}

void ViewProvider::onChanged(const App::Property* prop) {

    //if the object is inside of a body we make sure it is the only visible one on activation
    if(prop == &Visibility && Visibility.getValue()) {

        Part::BodyBase* body = Part::BodyBase::findBodyOf(getObject());
        if(body) {

            //hide all features in the body other than this object
            for(App::DocumentObject* obj : body->Group.getValues()) {
                if(obj == getObject())
                    continue;
                if(body->BaseFeature.getValue()==obj
                        || obj->isDerivedFrom(PartDesign::Feature::getClassTypeId()))
                {
                   auto vpd = Base::freecad_dynamic_cast<Gui::ViewProviderDocumentObject>(
                           Gui::Application::Instance->getViewProvider(obj));
                   if(vpd && vpd->Visibility.getValue())
                       vpd->Visibility.setValue(false);
                }
            }
        }
    }

    PartGui::ViewProviderPartExt::onChanged(prop);
}

void ViewProvider::setTipIcon(bool onoff) {
    isSetTipIcon = onoff;

    signalChangeIcon();
}

QIcon ViewProvider::mergeOverlayIcons (const QIcon & orig) const
{
    QIcon mergedicon = orig;

    auto feat = Base::freecad_dynamic_cast<PartDesign::Feature>(getObject());
    if(feat && feat->Suppress.getValue()) {
        int w = Gui::treeViewIconSize();
        QIcon overlay(Gui::BitmapFactory().pixmap("disabled").scaledToWidth(w));
        QPixmap pixmap = mergedicon.pixmap(w,w,QIcon::Disabled);
        QPainter painter(&pixmap);
        overlay.paint(&painter,0,0,w,w,Qt::AlignCenter);
        mergedicon = QIcon(pixmap);
    }

    if(isSetTipIcon) {
        QPixmap px;

        static const char * const feature_tip_xpm[]={
            "8 6 3 1",
            ". c None",
            "# c #00cc00",
            "a c #ffffff",
            "..####..",
            ".##aa##.",
            "##aaaa##",
            "##aaaa##",
            ".##aa##.",
            "..####.."};
        px = QPixmap(feature_tip_xpm);

        mergedicon = Gui::BitmapFactoryInst::mergePixmap(mergedicon, px, Gui::BitmapFactoryInst::BottomRight);

    }

    return Gui::ViewProvider::mergeOverlayIcons(mergedicon);
}

bool ViewProvider::onDelete(const std::vector<std::string> &)
{
    PartDesign::Feature* feature = static_cast<PartDesign::Feature*>(getObject());

    App::DocumentObject* previousfeat = feature->BaseFeature.getValue();

    // Visibility - we want:
    // 1. If the visible object is not the one being deleted, we leave that one visible.
    // 2. If the visible object is the one being deleted, we make the previous object visible.
    if (isShow() && previousfeat && Gui::Application::Instance->getViewProvider(previousfeat)) {
        Gui::Application::Instance->getViewProvider(previousfeat)->show();
    }

    // find surrounding features in the tree
    Part::BodyBase* body = PartDesign::Body::findBodyOf(getObject());

    if (body != NULL) {
        // Deletion from the tree of a feature is handled by Document.removeObject, which has no clue
        // about what a body is. Therefore, Bodies, although an "activable" container, know nothing
        // about what happens at Document level with the features they contain.
        //
        // The Deletion command StdCmdDelete::activated, however does notify the viewprovider corresponding
        // to the feature (not body) of the imminent deletion (before actually doing it).
        //
        // Consequently, the only way of notifying a body of the imminent deletion of one of its features
        // so as to do the clean up required (moving basefeature references, tip management) is from the
        // viewprovider, so we call it here.
        //
        // fixes (#3084)

        FCMD_OBJ_CMD(body,"removeObject(" << Gui::Command::getObjectCmd(feature) << ')');
    }

    return true;
}

void ViewProvider::setBodyMode(bool bodymode) {

    std::vector<App::Property*> props;
    getPropertyList(props);

    auto vp = getBodyViewProvider();
    if(!vp)
        return;
    
#if 1
    // Realthunder: I want to test element color mapping in PartDesign.
    // If it works well, then there is no reason to hide all the properties.
    (void)bodymode;
#else
    for(App::Property* prop : props) {

        //we keep visibility and selectibility per object
        if(prop == &Visibility ||
           prop == &Selectable)
            continue;

        //we hide only properties which are available in the body, not special ones
        if(!vp->getPropertyByName(prop->getName()))
            continue;

        prop->setStatus(App::Property::Hidden, bodymode);
    }
#endif
}

void ViewProvider::makeTemporaryVisible(bool onoff)
{
    //make sure to not use the overridden versions, as they change properties
    if (onoff) {
        if (VisualTouched) {
            updateVisual();
        }
        Gui::ViewProvider::show();
    }
    else
        Gui::ViewProvider::hide();
}

PyObject* ViewProvider::getPyObject()
{
    if (!pyViewObject)
        pyViewObject = new ViewProviderPy(this);
    pyViewObject->IncRef();
    return pyViewObject;
}

ViewProviderBody* ViewProvider::getBodyViewProvider() {

    auto body = PartDesign::Body::findBodyOf(getObject());
    auto doc = getDocument();
    if(body && doc) {
        auto vp = doc->getViewProvider(body);
        if(vp && vp->isDerivedFrom(ViewProviderBody::getClassTypeId()))
           return static_cast<ViewProviderBody*>(vp);
    }

    return nullptr;
}

bool ViewProvider::hasBaseFeature() const{
    auto feature = dynamic_cast<PartDesign::Feature*>(getObject());
    if(feature && feature->getBaseObject(true))
        return true;
    return PartGui::ViewProviderPart::hasBaseFeature();
}

namespace Gui {
/// @cond DOXERR
PROPERTY_SOURCE_TEMPLATE(PartDesignGui::ViewProviderPython, PartDesignGui::ViewProvider)
/// @endcond

// explicit template instantiation
template class PartDesignGuiExport ViewProviderPythonFeatureT<PartDesignGui::ViewProvider>;
}

