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

#ifndef GUI_SOFCUNIFIEDSELECTION_H
#define GUI_SOFCUNIFIEDSELECTION_H

#include <Inventor/lists/SoNodeList.h>
#include <Inventor/nodes/SoSubNode.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/fields/SoSFBool.h>
#include <Inventor/fields/SoSFColor.h>
#include <Inventor/fields/SoSFEnum.h>
#include <Inventor/fields/SoSFName.h>
#include <Inventor/fields/SoMFName.h>
#include <Inventor/fields/SoSFString.h>
#include <Inventor/nodes/SoLightModel.h>
#include <Inventor/SbTime.h>
#include <Inventor/sensors/SoTimerSensor.h>
#include <Inventor/SbViewportRegion.h>
#include <Inventor/elements/SoReplacedElement.h>
#include "InventorBase.h"
#include "View3DInventorViewer.h"
#include "SoFCSelectionContext.h"
#include "Inventor/SoFCDetail.h"
#include "Inventor/SoFCSwitch.h"
#include "Inventor/SoFCDisplayModeElement.h"
#include <array>
#include <list>
#include <unordered_set>
#include <unordered_map>

class SoFullPath;
class SoPickedPoint;
class SoDetail;
class SoPickedPointList;
class SoRayPickAction;
class SbViewportRegion;
class SbVec2s;
class SbBox3f;
class SbMatrix;

namespace Gui {

class Document;
class ViewProviderDocumentObject;
class SoFCRayPickAction;

/**  Unified Selection node
 *  This is the new selection node for the 3D Viewer which will 
 *  gradually remove all the low level selection nodes in the view
 *  provider. The handling of the highlighting and the selection will
 *  be unified here. 
 *  \author Jürgen Riegel
 */
class GuiExport SoFCUnifiedSelection : public SoSeparator {
    typedef SoSeparator inherited;

    SO_NODE_HEADER(Gui::SoFCUnifiedSelection);

public:
    static void initClass(void);
    static void finish(void);
    SoFCUnifiedSelection(void);
    void applySettings();

    enum HighlightModes {
        AUTO, ON, OFF
    };

    const char* getFileFormatName(void) const;
    void write(SoWriteAction * action);

    SoSFColor colorHighlight;
    SoSFColor colorSelection;
    SoSFEnum highlightMode;
    SoSFEnum selectionMode;
    SoSFBool selectionRole;
    SoSFBool useNewSelection;

    SoSFName overrideMode;

    static SbName DisplayModeTessellation;
    static SbName DisplayModeShaded;
    static SbName DisplayModeHiddenLine;
    static SbName DisplayModeFlatLines;
    static SbName DisplayModeAsIs;
    static SbName DisplayModeNoShading;

    virtual void doAction(SoAction *action);
    //virtual void GLRender(SoGLRenderAction * action);

    virtual void handleEvent(SoHandleEventAction * action);
    virtual void GLRenderBelowPath(SoGLRenderAction * action);
    virtual void GLRenderInPath(SoGLRenderAction * action);
    //static  void turnOffCurrentHighlight(SoGLRenderAction * action);

    virtual void callback(SoCallbackAction *action);

    virtual void getBoundingBox(SoGetBoundingBoxAction * action);

    bool hasHighlight();

    static int getPriority(const SoPickedPoint* p);

    static bool getShowSelectionBoundingBox();

    void setDocument(Document *);
    void setViewer(View3DInventorViewer *);
    void setSelectAll(bool enable);

    SoPickedPoint* getPickedPoint(SoHandleEventAction*) const;

    std::vector<App::SubObjectT> getPickedSelections(const SbVec2s &pos,
                                                     const SbViewportRegion &viewport,
                                                     bool singlePick) const;
protected:
    virtual ~SoFCUnifiedSelection();
    //virtual void redrawHighlighted(SoAction * act, SbBool flag);
    //virtual SbBool readInstance(SoInput *  in, unsigned short  flags); 

    class Private;
    friend class Private;
    Private * pimpl;
};


/** Helper class for change and restore OpenGL depth func
 *
 * Although Coin3D has SoDepthBuffer and SoDepthBufferElement for this purpose,
 * we cannot rely on it, because Coin3D implementation does not account for
 * user code direct change of OpenGL state. And there are user code change
 * glDepthFunc directly.
 */
struct GuiExport FCDepthFunc {
    /** Constructor
     * @param f: the depth function to change to
     */
    FCDepthFunc(int32_t f);

    /** Constructure that does nothing
     *
     * This allows you to delay depth function setting by calling set()
     */
    FCDepthFunc();

    /** Destructor
     * Restore the depth function if changed
     */
    ~FCDepthFunc();

    /** Change depth function
     * @param f: the depth function to change to
     */
    void set(int32_t f);

    /// restore depth function
    void restore();

    /// Stores the depth function before changing
    int32_t func;

    /// Indicate whether the depth function is changed and will be restored
    bool changed;

    /// Whether to restore depth test
    bool dtest;
};

/// For rendering a given path on top
class GuiExport SoFCPathAnnotation : public SoSeparator {
    typedef SoSeparator inherited;

    SO_NODE_HEADER(Gui::SoFCPathAnnotation);
public:
    static void initClass(void);
    static void finish(void);
    SoFCPathAnnotation(ViewProvider *vp=0, const char *subname=0, View3DInventorViewer *viewer=0);

    void setPath(SoPath *);
    SoPath *getPath() {return path;}
    void setDetail(bool det);
    bool hasDetail() {return det;}

    virtual void GLRenderBelowPath(SoGLRenderAction * action);
    virtual void GLRender(SoGLRenderAction * action);
    virtual void GLRenderInPath(SoGLRenderAction * action);

    virtual void getBoundingBox(SoGetBoundingBoxAction * action);
    void doPick(SoPath *path, SoRayPickAction *action);

    virtual void doAction(SoAction *action);

protected:
    virtual ~SoFCPathAnnotation();

protected:
    ViewProvider *viewProvider;
    std::string subname;
    View3DInventorViewer *viewer;
    SoPath *path;
    SoNodeList tmpPath;
    bool det;
};

/// Separator node that tracks render caching setting
class GuiExport SoFCSeparator : public SoSeparator {
    typedef SoSeparator inherited;

    SO_NODE_HEADER(Gui::SoFCSeparator);

public:
    static void initClass(void);
    static void finish(void);
    SoFCSeparator(bool trackCacheMode=true);

    virtual void GLRenderBelowPath(SoGLRenderAction * action);

    static void setCacheMode(CacheEnabled mode) {
        CacheMode = mode;
    }
    static CacheEnabled getCacheMode() {
        return CacheMode;
    }

private:
    bool trackCacheMode;
    static CacheEnabled CacheMode;
};

class GuiExport SoFCSelectionRoot : public SoFCSeparator {
    typedef SoFCSeparator inherited;

    SO_NODE_HEADER(Gui::SoFCSelectionRoot);
  
public:
    static void initClass(void);
    static void finish(void);
    SoFCSelectionRoot(bool trackCacheMode=false, ViewProvider *vp=0);

    ViewProvider *getViewProvider() const {return viewProvider;}
    void setViewProvider(ViewProvider *vp);

    virtual void GLRenderBelowPath(SoGLRenderAction * action);
    virtual void GLRenderInPath(SoGLRenderAction * action);

    virtual void doAction(SoAction *action);
    virtual void pick(SoPickAction * action);
    virtual void rayPick(SoRayPickAction * action);
    virtual void handleEvent(SoHandleEventAction * action);
    virtual void search(SoSearchAction * action);
    virtual void getPrimitiveCount(SoGetPrimitiveCountAction * action);
    virtual void getBoundingBox(SoGetBoundingBoxAction * action);
    virtual void getMatrix(SoGetMatrixAction * action);
    virtual void callback(SoCallbackAction *action);

    static bool handleSelectionAction(SoAction *action, SoNode *node,
                                      SoFCDetail::Type detailType,
                                      SoFCSelectionContextExPtr selContext,
                                      SoFCSelectionCounter &counter);

    template<class T>
    static std::shared_ptr<T> getRenderContext(SoNode *node, std::shared_ptr<T> def = std::shared_ptr<T>()) {
        return std::dynamic_pointer_cast<T>(getNodeContext(SelStack,node,def));
    }

    /** Returns selection context for rendering. 
     *
     * @param node: the querying node
     * @param def: default context if none is found
     * @param ctx2: secondary context output
     *
     * @return Returned the primary context for selection, and the context is
     * always stored in the first encountered SoFCSelectionRoot in the path. It
     * is keyed using the entire sequence of SoFCSelectionRoot along the path
     * to \c node, replacing the first SoFCSelectionRoot with the given node. 
     *
     * @return Secondary context returned in \c ctx2 is for customized
     * highlighting, and is not affected by mouse event. The highlight is
     * applied manually using SoSelectionElementAction. It is stored in the
     * last encountered SoFCSelectionRoot, and is keyed using the querying 
     * \c node and (if there are more than one SoFCSelectionRoot along the
     * path) the first SoFCSelectionRoot. The reason is so that any link to a
     * node (new links means additional SoFCSelectionRoot added in front) with
     * customized subelement highlight will also show the highlight. Secondary
     * context can be chained, which why the secondary context type must provide
     * an function called merge() for getRenderContext() to merge the context.
     * See SoFCSelectionContext::merge() for an implementation of merging multiple
     * context.
     *
     * @note For simplicity reason, currently secondary context is only freed
     * when the storage SoFCSSelectionRoot node is freed.
     */
    template<class T>
    static std::shared_ptr<T> getRenderContext(SoNode *node, std::shared_ptr<T> def, std::shared_ptr<T> &ctx2) 
    {
        ctx2 = std::dynamic_pointer_cast<T>(getNodeContext2(SelStack,node,T::merge));
        return std::dynamic_pointer_cast<T>(getNodeContext(SelStack,node,def));
    }

    /** Get the selection context for an action. 
     *
     * @param action: the action. SoSelectionElementAction has any option to
     * query for secondary context. \sa getRenderContext for detail about
     * secondary context
     * @param node: the querying node
     * @param def: default context if none is found, only used if querying
     * non-secondary context
     * @param create: create a new context if none is found
     *
     * @return If no SoFCSelectionRoot is found in the current path of action,
     * \c def is returned. Otherwise a selection context returned. A new one
     * will be created if none is found.
     */
    template<class T>
    static std::shared_ptr<T> getActionContext(
            SoAction *action, SoNode *node, std::shared_ptr<T> def=std::shared_ptr<T>(), bool create=true) 
    {
        auto res = findActionContext(action,node,create,false);
        if(!res.second) {
            if(res.first)
                return std::shared_ptr<T>();
            // default context is only applicable for non-secondary context query
            return def;
        }
        // make a new context if there is none
        auto &ctx = *res.second;
        if(ctx) {
            auto ret = std::dynamic_pointer_cast<T>(ctx);
            if(!ret)
                ctx.reset();
        }
        if(!ctx && create) 
            ctx = std::make_shared<T>();
        return std::static_pointer_cast<T>(ctx);
    }

    static bool removeActionContext(SoAction *action, SoNode *node) {
        return findActionContext(action,node,false,true).second!=0;
    }

    template<class T>
    static std::shared_ptr<T> getSecondaryActionContext(SoAction *action, SoNode *node) {
        auto it = ActionStacks.find(action);
        if(it == ActionStacks.end())
            return std::shared_ptr<T>();
        return std::dynamic_pointer_cast<T>(getNodeContext2(it->second,node,T::merge));
    }

    static void checkSelection(bool &sel, SbColor &selColor, bool &hl, SbColor &hlColor);

    static void moveActionStack(SoAction *from, SoAction *to, bool erase);

    static SoFCSelectionRoot *getCurrentRoot(bool front=false, SoFCSelectionRoot *def=0);

    static SoFCSelectionRoot *getCurrentActionRoot(
            SoAction *action, bool front=false, SoFCSelectionRoot *def=0);

    int getRenderPathCode() const;

    void resetContext();

    static bool checkColorOverride(SoState *state);

    bool hasColorOverride() const {
        return overrideColor;
    }

    void setColorOverride(App::Color c) {
        overrideColor = true;
        colorOverride = SbColor(c.r,c.g,c.b);
        transOverride = c.a;
    }

    void removeColorOverride() {
        overrideColor = false;
    }

    enum SelectStyles {
        Full, Box, PassThrough, Unpickable
    };
    SoSFEnum selectionStyle;

    static bool renderBBox(SoGLRenderAction *action, SoNode *node,
            const SbColor &color, const SbMatrix *mat=0, bool force=false);

    static bool renderBBox(SoGLRenderAction *action, SoNode *node,
        const SbBox3f &bbox, SbColor color, const SbMatrix *mat=0);

    static void setupSelectionLineRendering(SoState *state, SoNode *node, const uint32_t *color);

protected:
    virtual ~SoFCSelectionRoot();

    void renderPrivate(SoGLRenderAction *, bool inPath);
    bool _renderPrivate(SoGLRenderAction *, bool inPath, bool &pushed);

    class Stack : public std::vector<SoFCSelectionRoot*> {
    public:
        std::unordered_set<SoFCSelectionRoot*> nodeSet;
        size_t offset = 0;
    };

    bool doActionPrivate(Stack &stack, SoAction *);

    static SoFCSelectionContextBasePtr getNodeContext(
            Stack &stack, SoNode *node, SoFCSelectionContextBasePtr def);
    static SoFCSelectionContextBasePtr getNodeContext2(
            Stack &stack, SoNode *node, SoFCSelectionContextBase::MergeFunc *merge);
    static std::pair<bool,SoFCSelectionContextBasePtr*> findActionContext(
            SoAction *action, SoNode *node, bool create, bool erase);

    static Stack SelStack;
    static std::unordered_map<SoAction*,Stack> ActionStacks;
    struct StackComp {
        bool operator()(const Stack &a, const Stack &b) const;
    };

    typedef std::map<Stack,SoFCSelectionContextBasePtr,StackComp> ContextMap;
    ContextMap contextMap;
    ContextMap contextMap2;//holding secondary context

    struct SelContext: SoFCSelectionContextBase {
    public:
        SbColor selColor;
        SbColor hlColor;
        bool selAll = false;
        bool hlAll = false;
        bool hideAll = false;
        static MergeFunc merge;

        virtual bool isCounted() const {return selAll || hideAll;}
    };
    typedef std::shared_ptr<SelContext> SelContextPtr;
    typedef std::vector<SbColor> ColorStack;
    static ColorStack SelColorStack;
    static ColorStack HlColorStack;
    static SoFCSelectionRoot *ShapeColorNode;
    bool overrideColor = false;
    SbColor colorOverride;
    float transOverride;
    SoColorPacker shapeColorPacker;

    SoFCSelectionCounter selCounter;

    ViewProvider *viewProvider;

    int renderPathCode=0;
};

/**
 * @author Werner Mayer
 */
class GuiExport SoHighlightElementAction : public SoAction
{
    SO_ACTION_HEADER(SoHighlightElementAction);

public:
    SoHighlightElementAction ();
    ~SoHighlightElementAction();

    void setHighlighted(SbBool);
    SbBool isHighlighted() const;
    void setColor(const SbColor&);
    const SbColor& getColor() const;
    void setElement(const SoDetail*);
    const SoDetail* getElement() const;

    static void initClass();

protected:
    virtual void beginTraversal(SoNode *node);

private:
    static void callDoAction(SoAction *action,SoNode *node);

private:
    SbBool _highlight;
    SbColor _color;
    const SoDetail* _det;
};

/**
 * @author Werner Mayer
 */
class GuiExport SoSelectionElementAction : public SoAction
{
    SO_ACTION_HEADER(SoSelectionElementAction);

public:
    enum Type {None, Append, Remove, All, Color, Hide, Show};

    SoSelectionElementAction (Type=None, bool secondary = false);
    ~SoSelectionElementAction();

    Type getType() const;
    void setType(Type type) {
        _type = type;
    }

    void setColor(const SbColor&);
    const SbColor& getColor() const;
    void setElement(const SoDetail*);
    const SoDetail* getElement() const;

    bool isSecondary() const {return _secondary;}
    void setSecondary(bool enable) {
        _secondary = enable;
    }

    const std::map<std::string,App::Color> &getColors() const {
        return _colors;
    }
    void setColors(const std::map<std::string,App::Color> &colors) {
        _colors = colors;
    }
    void swapColors(std::map<std::string,App::Color> &colors) {
        _colors.swap(colors);
    }

    static void initClass();

protected:
    virtual void beginTraversal(SoNode *node);

private:
    static void callDoAction(SoAction *action,SoNode *node);

private:
    Type _type;
    SbColor _color;
    const SoDetail* _det;
    std::map<std::string,App::Color> _colors;
    bool _secondary;
};

/**
 * @author Werner Mayer
 */
class GuiExport SoVRMLAction : public SoAction
{
    SO_ACTION_HEADER(SoVRMLAction);

public:
    SoVRMLAction();
    ~SoVRMLAction();
    void setOverrideMode(SbBool);
    SbBool isOverrideMode() const;

    static void initClass();

private:
    SbBool overrideMode;
    std::list<int> bindList;
    static void callDoAction(SoAction *action,SoNode *node);

};


} // namespace Gui

#endif // !GUI_SOFCUNIFIEDSELECTION_H
