/***************************************************************************
 *   Copyright (c) 2011 Werner Mayer <wmayer[at]users.sourceforge.net>     *
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
# ifdef FC_OS_WIN32
# include <windows.h>
# endif
# ifdef FC_OS_MACOSX
# include <OpenGL/gl.h>
# else
# include <GL/gl.h>
# endif
# include <float.h>
# include <algorithm>
# include <Inventor/SoPickedPoint.h>
# include <Inventor/SoPrimitiveVertex.h>
# include <Inventor/actions/SoCallbackAction.h>
# include <Inventor/actions/SoGetBoundingBoxAction.h>
# include <Inventor/actions/SoGetPrimitiveCountAction.h>
# include <Inventor/actions/SoGLRenderAction.h>
# include <Inventor/actions/SoPickAction.h>
# include <Inventor/actions/SoWriteAction.h>
# include <Inventor/bundles/SoMaterialBundle.h>
# include <Inventor/bundles/SoTextureCoordinateBundle.h>
# include <Inventor/elements/SoOverrideElement.h>
# include <Inventor/elements/SoCoordinateElement.h>
# include <Inventor/elements/SoGLCoordinateElement.h>
# include <Inventor/elements/SoGLCacheContextElement.h>
# include <Inventor/elements/SoLineWidthElement.h>
# include <Inventor/elements/SoPointSizeElement.h>
# include <Inventor/errors/SoDebugError.h>
# include <Inventor/errors/SoReadError.h>
# include <Inventor/details/SoFaceDetail.h>
# include <Inventor/details/SoLineDetail.h>
# include <Inventor/misc/SoState.h>
#endif

#include "SoBrepPointSet.h"
#include <Gui/SoFCUnifiedSelection.h>
#include <Gui/SoFCSelectionAction.h>

using namespace PartGui;

namespace PartGui {
// global helper for setting renderCaching in parent SoSeparator
void checkRenderCaching(SoAction *action, bool enable, 
        int &canSetRenderCaching, bool &renderCaching) 
{
    if(canSetRenderCaching==0) return;

    const SoPath *path = action->getCurPath();
    if(!path || !path->getLength()) return;

    if(enable && 
       (Gui::Selection().hasSelection() ||
        Gui::SoFCUnifiedSelection::hasHighlight()))
    {
        // only turn caching back on when there is no selection nor preselection
        enable = false;
    }

    if(canSetRenderCaching==1 && enable==renderCaching)
        return;

    for(int i=0,c=path->getLength();i<c;++i) {
        SoNode *node = path->getNodeFromTail(i);
        if(!node->getTypeId().isDerivedFrom(SoSeparator::getClassTypeId()))
            continue;
        SoSeparator *sep = static_cast<SoSeparator*>(node);
        if(canSetRenderCaching<0) {
            if(sep->renderCaching.getValue()!=SoSeparator::AUTO) {
                canSetRenderCaching = 0;
                return;
            }
            canSetRenderCaching = 1;
        }
        sep->renderCaching = enable?SoSeparator::AUTO:SoSeparator::OFF;
        renderCaching = enable;
        return;
    }
}
}


SO_NODE_SOURCE(SoBrepPointSet);

class SoBrepPointSet::SelContext {
public:
    SoSFInt32 highlightIndex;
    SoMFInt32 selectionIndex;
    SbColor selectionColor;
    SbColor highlightColor;
    SoColorPacker colorpacker;

    SelContext() {
        highlightIndex = -1;
        selectionIndex = -1;
        selectionIndex.setNum(0);
    }
};

void SoBrepPointSet::initClass()
{
    SO_NODE_INIT_CLASS(SoBrepPointSet, SoPointSet, "PointSet");
}

SoBrepPointSet::SoBrepPointSet()
    :selContext(std::make_shared<SelContext>())
    ,canSetRenderCaching(-1)
    ,renderCaching(true)
{
    SO_NODE_CONSTRUCTOR(SoBrepPointSet);
}

void SoBrepPointSet::GLRender(SoGLRenderAction *action)
{
    const SoCoordinateElement* coords = SoCoordinateElement::getInstance(action->getState());
    int num = coords->getNum() - this->startIndex.getValue();
    if (num < 0) {
        // Fixes: #0000545: Undo revolve causes crash 'illegal storage'
        return;
    }
    SelContextPtr ctx = Gui::SoFCSelectionRoot::getRenderContext<SelContext>(this,selContext);

    bool checkCaching = !renderCaching;
    if (ctx && ctx->highlightIndex.getValue() >= 0){
        checkCaching = false;
        renderHighlight(action,ctx);
    }
    if (ctx && ctx->selectionIndex.getNum() > 0) {
        checkCaching = false;
        renderSelection(action,ctx);
        if(num == ctx->selectionIndex.getNum()) //full selection
            return;
    }

    if(checkCaching)
        PartGui::checkRenderCaching(action,true,canSetRenderCaching,renderCaching);

    inherited::GLRender(action);

    // Workaround for #0000433
//#if !defined(FC_OS_WIN32)
    if (ctx && ctx->highlightIndex.getValue() >= 0)
        renderHighlight(action,ctx);
    if (ctx && ctx->selectionIndex.getNum() > 0)
        renderSelection(action,ctx);
//#endif
}

void SoBrepPointSet::GLRenderBelowPath(SoGLRenderAction * action)
{
    inherited::GLRenderBelowPath(action);
}

void SoBrepPointSet::renderShape(const SoGLCoordinateElement * const coords, 
                                 const int32_t *cindices,
                                 int numindices)
{
    const SbVec3f * coords3d = coords->getArrayPtr3();
    if(coords3d == nullptr)
        return;

    int previ;
    const int32_t *end = cindices + numindices;
    glBegin(GL_POINTS);
    while (cindices < end) {
        previ = *cindices++;
        glVertex3fv((const GLfloat*) (coords3d + previ));
    }
    glEnd();
}

void SoBrepPointSet::renderHighlight(SoGLRenderAction *action, SelContextPtr ctx)
{
    SoState * state = action->getState();
    state->push();
    float ps = SoPointSizeElement::get(state);
    if (ps < 4.0f) SoPointSizeElement::set(state, this, 4.0f);

    SoLazyElement::setEmissive(state, &ctx->highlightColor);
    SoOverrideElement::setEmissiveColorOverride(state, this, true);
    SoLazyElement::setDiffuse(state, this,1, &ctx->highlightColor,&ctx->colorpacker);
    SoOverrideElement::setDiffuseColorOverride(state, this, true);

    const SoCoordinateElement * coords;
    const SbVec3f * normals;

    this->getVertexData(state, coords, normals, false);

    SoMaterialBundle mb(action);
    mb.sendFirst(); // make sure we have the correct material

    int32_t id = ctx->highlightIndex.getValue();
    if (id < this->startIndex.getValue() || id >= coords->getNum()) {
        SoDebugError::postWarning("SoBrepPointSet::renderHighlight", "highlightIndex out of range");
    }
    else {
        renderShape(static_cast<const SoGLCoordinateElement*>(coords), &id, 1);
    }
    state->pop();
}

void SoBrepPointSet::renderSelection(SoGLRenderAction *action, SelContextPtr ctx)
{
    SoState * state = action->getState();
    state->push();
    float ps = SoPointSizeElement::get(state);
    if (ps < 4.0f) SoPointSizeElement::set(state, this, 4.0f);

    SoLazyElement::setEmissive(state, &ctx->selectionColor);
    SoOverrideElement::setEmissiveColorOverride(state, this, true);
    SoLazyElement::setDiffuse(state, this,1, &ctx->selectionColor,&ctx->colorpacker);
    SoOverrideElement::setDiffuseColorOverride(state, this, true);

    const SoCoordinateElement * coords;
    const SbVec3f * normals;
    const int32_t * cindices;
    int numcindices;

    this->getVertexData(state, coords, normals, false);

    SoMaterialBundle mb(action);
    mb.sendFirst(); // make sure we have the correct material

    cindices = ctx->selectionIndex.getValues(0);
    numcindices = ctx->selectionIndex.getNum();

    if (!validIndexes(coords, this->startIndex.getValue(), cindices, numcindices)) {
        SoDebugError::postWarning("SoBrepPointSet::renderSelection", "selectionIndex out of range");
    }
    else {
        renderShape(static_cast<const SoGLCoordinateElement*>(coords), cindices, numcindices);
    }
    state->pop();
}

bool SoBrepPointSet::validIndexes(const SoCoordinateElement* coords, int32_t startIndex, const int32_t * cindices, int numcindices) const
{
    for (int i=0; i<numcindices; i++) {
        int32_t id = cindices[i];
        if (id < startIndex || id >= coords->getNum()) {
            return false;
        }
    }
    return true;
}

void SoBrepPointSet::doAction(SoAction* action)
{
    if (action->getTypeId() == Gui::SoHighlightElementAction::getClassTypeId()) {
        Gui::SoHighlightElementAction* hlaction = static_cast<Gui::SoHighlightElementAction*>(action);
        SelContextPtr ctx = Gui::SoFCSelectionRoot::getActionContext<SelContext>(action,this,selContext);
        if (!hlaction->isHighlighted()) {
            ctx->highlightIndex = -1;
            checkRenderCaching(action,ctx);
            return;
        }
        const SoDetail* detail = hlaction->getElement();
        if (detail) {
            if (!detail->isOfType(SoPointDetail::getClassTypeId())) {
                ctx->highlightIndex = -1;
                checkRenderCaching(action,ctx);
                return;
            }

            int index = static_cast<const SoPointDetail*>(detail)->getCoordinateIndex();
            if(index!=ctx->highlightIndex.getValue()) {
                ctx->highlightIndex.setValue(index);
                ctx->highlightColor = hlaction->getColor();
                checkRenderCaching(action,ctx);
            }
        }
        return;
    }
    else if (action->getTypeId() == Gui::SoSelectionElementAction::getClassTypeId()) {
        Gui::SoSelectionElementAction* selaction = static_cast<Gui::SoSelectionElementAction*>(action);
        SelContextPtr ctx = Gui::SoFCSelectionRoot::getActionContext<SelContext>(action,this,selContext);
        ctx->selectionColor = selaction->getColor();
        if (selaction->getType() == Gui::SoSelectionElementAction::All) {
            const SoCoordinateElement* coords = SoCoordinateElement::getInstance(action->getState());
            int num = coords->getNum() - this->startIndex.getValue();
            ctx->selectionIndex.setNum(num);
            int32_t* v = ctx->selectionIndex.startEditing();
            int32_t s = this->startIndex.getValue();
            for (int i=0; i<num;i++)
                v[i] = i + s;
            ctx->selectionIndex.finishEditing();
            checkRenderCaching(action,ctx);
            return;
        }
        else if (selaction->getType() == Gui::SoSelectionElementAction::None) {
            ctx->selectionIndex.setNum(0);
            checkRenderCaching(action,ctx);
            return;
        }

        const SoDetail* detail = selaction->getElement();
        if (detail) {
            if (!detail->isOfType(SoPointDetail::getClassTypeId())) {
                return;
            }

            int index = static_cast<const SoPointDetail*>(detail)->getCoordinateIndex();
            switch (selaction->getType()) {
            case Gui::SoSelectionElementAction::Append:
                {
                    if (ctx->selectionIndex.find(index) < 0) {
                        int start = ctx->selectionIndex.getNum();
                        ctx->selectionIndex.set1Value(start, index);
                        checkRenderCaching(action,ctx);
                    }
                }
                break;
            case Gui::SoSelectionElementAction::Remove:
                {
                    int start = ctx->selectionIndex.find(index);
                    if (start >= 0) {
                        ctx->selectionIndex.deleteValues(start,1);
                        checkRenderCaching(action,ctx);
                    }
                }
                break;
            default:
                break;
            }
        }
    }

    inherited::doAction(action);
}

void SoBrepPointSet::checkRenderCaching(SoAction *action, SelContextPtr ctx) {
    touch();
    PartGui::checkRenderCaching(action,
            ctx->highlightIndex.getValue()<0 && !ctx->selectionIndex.getNum(),
            canSetRenderCaching, renderCaching);
}
