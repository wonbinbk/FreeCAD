/****************************************************************************
 *   Copyright (c) 2020 Zheng, Lei (realthunder) <realthunder.dev@gmail.com>*
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
# include <QPointer>
# include <QPainter>
# include <QDockWidget>
# include <QMdiArea>
# include <QTabBar>
# include <QTreeView>
# include <QHeaderView>
# include <QToolTip>
# include <QAction>
# include <QKeyEvent>
# include <QTextStream>
# include <QComboBox>
# include <QBoxLayout>
# include <QSpacerItem>
# include <QSplitter>
# include <QStackedWidget>
# include <QMenu>
#endif

#if QT_VERSION >= 0x050000
# include <QWindow>
#endif

#include <QPropertyAnimation>

#include <array>
#include <unordered_map>

#include <Base/Tools.h>
#include <Base/Console.h>
#include "BitmapFactory.h"
#include "MainWindow.h"
#include "ViewParams.h"
#include "View3DInventor.h"
#include "View3DInventorViewer.h"
#include "SplitView3DInventor.h"
#include "Application.h"
#include "Control.h"
#include "TaskView/TaskView.h"
#include "ComboView.h"
#include "Tree.h"
#include <App/Application.h>
#include "propertyeditor/PropertyEditor.h"
#include "OverlayWidgets.h"

FC_LOG_LEVEL_INIT("Dock", true, true);

using namespace Gui;

#ifdef FC_HAS_DOCK_OVERLAY

static OverlayTabWidget *_LeftOverlay;
static OverlayTabWidget *_RightOverlay;
static OverlayTabWidget *_TopOverlay;
static OverlayTabWidget *_BottomOverlay;
static std::array<OverlayTabWidget*, 4> _Overlays;

static OverlayDragFrame *_DragFrame;

static const int _TitleButtonSize = 12;
static const int _MinimumOverlaySize = 30;

#define TITLE_BUTTON_COLOR "# c #202020"

static const char *_PixmapOverlay[]={
    "10 10 2 1",
    ". c None",
    TITLE_BUTTON_COLOR,
    "##########",
    "#........#",
    "#........#",
    "##########",
    "#........#",
    "#........#",
    "#........#",
    "#........#",
    "#........#",
    "##########",
};

// -----------------------------------------------------------

OverlayProxyWidget::OverlayProxyWidget(OverlayTabWidget *tabOverlay)
    :QWidget(tabOverlay->parentWidget()), owner(tabOverlay), _hintColor(QColor(50,50,50,150))
{
    dockArea = owner->getDockArea();
    timer.setSingleShot(true);
    connect(&timer, SIGNAL(timeout()), this, SLOT(onTimer()));
}

bool OverlayProxyWidget::isActivated() const
{
    return drawLine && isVisible();
}

bool OverlayProxyWidget::hitTest(QPoint pt, bool delay)
{
    if (!isVisible())
        return false;

    QTabBar *tabbar = owner->tabBar();
    if (tabbar->isVisible() && tabbar->tabAt(tabbar->mapFromGlobal(pt))>=0)
        return true;

    pt = mapFromGlobal(pt);
    int hit = 0;
    QSize s = this->size();
    int hintSize = ViewParams::getDockOverlayHintTriggerSize();
    switch(dockArea) {
    case Qt::LeftDockWidgetArea:
        hit = (pt.y() >= 0 && pt.y() <= s.height() && pt.x() > 0 && pt.x() < hintSize);
        if (hit && pt.x() <= s.width())
            hit = 2;
        break;
    case Qt::RightDockWidgetArea:
        hit = (pt.y() >= 0 && pt.y() <= s.height() && pt.x() < s.width() && pt.x() > -hintSize);
        if (hit && pt.x() >= 0)
            hit = 2;
        break;
    case Qt::TopDockWidgetArea:
        hit = (pt.x() >= 0 && pt.x() <= s.width() && pt.y() > 0 && pt.y() < hintSize);
        if (hit && pt.y() <= s.height())
            hit = 2;
        break;
    case Qt::BottomDockWidgetArea:
        hit = (pt.x() >= 0 && pt.x() <= s.width() && pt.y() < s.height() && pt.y() > -hintSize);
        if (hit && pt.y() >= 0)
            hit = 2;
        break;
    }
    if (hit) {
        if (drawLine)
            timer.stop();
        else if (delay) {
            if (!timer.isActive())
                timer.start(ViewParams::getDockOverlayHintDelay());
        } else {
            timer.stop();
            owner->setState(OverlayTabWidget::State_Hint);
            drawLine = true;
            update();
        }
        if(hit > 1 && ViewParams::getDockOverlayActivateOnHover()) {
            if (owner->isVisible() && owner->tabBar()->isVisible()) {
                QSize size = owner->tabBar()->size();
                QPoint pt = owner->tabBar()->mapToGlobal(
                                QPoint(size.width(), size.height()));
                QPoint pos = QCursor::pos();
                switch(this->dockArea) {
                case Qt::LeftDockWidgetArea:
                case Qt::RightDockWidgetArea:
                    if (pos.y() < pt.y())
                        return false;
                    break;
                case Qt::TopDockWidgetArea:
                case Qt::BottomDockWidgetArea:
                    if (pos.x() < pt.x())
                        return false;
                    break;
                default:
                    break;
                }
            }
            owner->setState(OverlayTabWidget::State_Normal);
            OverlayManager::instance()->refresh();
        }

    } else if (!drawLine)
        timer.stop();
    else if (delay) {
        if (!timer.isActive())
            timer.start(ViewParams::getDockOverlayHintDelay());
    } else {
        timer.stop();
        owner->setState(OverlayTabWidget::State_Normal);
        drawLine = false;
        update();
    }
    return hit;
}

void OverlayProxyWidget::onTimer()
{
    hitTest(QCursor::pos(), false);
}

void OverlayProxyWidget::enterEvent(QEvent *)
{
    if(!owner->count())
        return;

    if (!drawLine) {
        if (!timer.isActive())
            timer.start(ViewParams::getDockOverlayHintDelay());
    }
}

void OverlayProxyWidget::leaveEvent(QEvent *)
{
    // drawLine = false;
    // update();
}

void OverlayProxyWidget::hideEvent(QHideEvent *)
{
    drawLine = false;
}

void OverlayProxyWidget::mousePressEvent(QMouseEvent *ev)
{
    if(!owner->count() || ev->button() != Qt::LeftButton)
        return;

    owner->setState(OverlayTabWidget::State_Normal);
    OverlayManager::instance()->refresh(this);
}

QBrush OverlayProxyWidget::hintColor() const
{
    return _hintColor;
}

void OverlayProxyWidget::setHintColor(const QBrush &brush)
{
    _hintColor = brush;
}

void OverlayProxyWidget::paintEvent(QPaintEvent *)
{
    if(!drawLine)
        return;
    QPainter painter(this);
    painter.setOpacity(_hintColor.color().alphaF());
    painter.setPen(Qt::transparent);
    painter.setBrush(_hintColor);

    QRect rect = this->rect();
    if (owner->isVisible() && owner->tabBar()->isVisible()) {
        QSize size = owner->tabBar()->size();
        QPoint pt = owner->tabBar()->mapToGlobal(
                        QPoint(size.width(), size.height()));
        pt = this->mapFromGlobal(pt);
        switch(this->dockArea) {
            case Qt::LeftDockWidgetArea:
            case Qt::RightDockWidgetArea:
                rect.setTop(pt.y());
                break;
            case Qt::TopDockWidgetArea:
            case Qt::BottomDockWidgetArea:
                rect.setLeft(pt.x());
                break;
            default:
                break;
        }
    }
    painter.drawRect(rect);
}

OverlayToolButton::OverlayToolButton(QWidget *parent)
    :QToolButton(parent)
{
    setCursor(Qt::ArrowCursor);
}

OverlayTabWidget::OverlayTabWidget(QWidget *parent, Qt::DockWidgetArea pos)
    :QTabWidget(parent), dockArea(pos)
{
    // This is necessary to capture any focus lost from switching the tab,
    // otherwise the lost focus will leak to the parent, i.e. MdiArea, which may
    // cause unexpected Mdi sub window switching.
    setFocusPolicy(Qt::StrongFocus);

    splitter = new QSplitter(this);

    _graphicsEffect = new OverlayGraphicsEffect(splitter);
    splitter->setGraphicsEffect(_graphicsEffect);

    _graphicsEffectTab = new OverlayGraphicsEffect(this);
    _graphicsEffectTab->setEnabled(false);
    tabBar()->setGraphicsEffect(_graphicsEffectTab);

    switch(pos) {
    case Qt::LeftDockWidgetArea:
        _LeftOverlay = this;
        setTabPosition(QTabWidget::West);
        splitter->setOrientation(Qt::Vertical);
        break;
    case Qt::RightDockWidgetArea:
        _RightOverlay = this;
        setTabPosition(QTabWidget::East);
        splitter->setOrientation(Qt::Vertical);
        break;
    case Qt::TopDockWidgetArea:
        _TopOverlay = this;
        setTabPosition(QTabWidget::North);
        splitter->setOrientation(Qt::Horizontal);
        break;
    case Qt::BottomDockWidgetArea:
        _BottomOverlay = this;
        setTabPosition(QTabWidget::South);
        splitter->setOrientation(Qt::Horizontal);
        break;
    default:
        break;
    }

    proxyWidget = new OverlayProxyWidget(this);
    proxyWidget->hide();
    _setOverlayMode(proxyWidget,true);

    setOverlayMode(true);
    hide();

    actOverlay.setIcon(QPixmap(_PixmapOverlay));
    actOverlay.setData(QString::fromLatin1("OBTN Overlay"));
    actOverlay.setParent(this);
    addAction(&actOverlay);

    static QIcon pxTransparent;
    if(pxTransparent.isNull()) {
        const char * const bytes[]={
            "10 10 2 1",
            ". c None",
            TITLE_BUTTON_COLOR,
            "..........",
            "...####...",
            ".##....##.",
            "##..##..##",
            "#..####..#",
            "#..####..#",
            "##..##..##",
            ".##....##.",
            "...####...",
            "..........",
        };
        pxTransparent = QIcon(QPixmap(bytes));
    }
    actTransparent.setIcon(pxTransparent);
    actTransparent.setCheckable(true);
    actTransparent.setData(QString::fromLatin1("OBTN Transparent"));
    actTransparent.setParent(this);
    // addAction(&actTransparent);

    QPixmap pxAutoHide;
    if(pxAutoHide.isNull()) {
        const char * const bytes[]={
            "10 10 2 1",
            ". c None",
            TITLE_BUTTON_COLOR,
            "...#######",
            ".........#",
            "..##.....#",
            ".##......#",
            "#######..#",
            "#######..#",
            ".##......#",
            "..##.....#",
            ".........#",
            "...#######",
        };
        pxAutoHide = QPixmap(bytes);
    }
    switch(dockArea) {
    case Qt::LeftDockWidgetArea:
        actAutoHide.setIcon(pxAutoHide);
        break;
    case Qt::RightDockWidgetArea:
        actAutoHide.setIcon(pxAutoHide.transformed(QTransform().scale(-1,1)));
        break;
    case Qt::TopDockWidgetArea:
        actAutoHide.setIcon(pxAutoHide.transformed(QTransform().rotate(90)));
        break;
    case Qt::BottomDockWidgetArea:
        actAutoHide.setIcon(pxAutoHide.transformed(QTransform().rotate(-90)));
        break;
    default:
        break;
    }
    actAutoHide.setCheckable(true);
    actAutoHide.setData(QString::fromLatin1("OBTN AutoHide"));
    actAutoHide.setParent(this);
    addAction(&actAutoHide);

    static QIcon pxEditHide;
    if(pxEditHide.isNull()) {
        const char * const bytes[]={
            "10 10 2 1",
            ". c None",
            TITLE_BUTTON_COLOR,
            "##....##..",
            "###..#.##.",
            ".####..###",
            "..###.#..#",
            "..####..#.",
            ".#..####..",
            "##...###..",
            "##...####.",
            "#####..###",
            "####....##",
        };
        pxEditHide = QIcon(QPixmap(bytes));
    }
    actEditHide.setIcon(pxEditHide);
    actEditHide.setCheckable(true);
    actEditHide.setData(QString::fromLatin1("OBTN EditHide"));
    actEditHide.setParent(this);
    addAction(&actEditHide);

    static QIcon pxEditShow;
    if(pxEditShow.isNull()) {
        const char * const bytes[]={
            "10 10 2 1",
            ". c None",
            TITLE_BUTTON_COLOR,
            "......##..",
            ".....#.##.",
            "....#..###",
            "...#..#..#",
            "..##.#..#.",
            ".#.##..#..",
            "##..###...",
            "##...#....",
            "#####.....",
            "####......",
        };
        pxEditShow = QIcon(QPixmap(bytes));
    }
    actEditShow.setIcon(pxEditShow);
    actEditShow.setCheckable(true);
    actEditShow.setData(QString::fromLatin1("OBTN EditShow"));
    actEditShow.setParent(this);
    addAction(&actEditShow);

    // actAutoMode.setCheckable(true);
    // actAutoMode.setData(QString::fromLatin1("OBTN AutoMode"));
    // actAutoMode.setParent(this);
    // syncAction();
    // addAction (&actAutoMode);

    static QIcon pxIncrease;
    if(pxIncrease.isNull()) {
        const char * const bytes[]={
            "10 10 2 1",
            ". c None",
            TITLE_BUTTON_COLOR,
            "....##....",
            "....##....",
            "....##....",
            "....##....",
            "##########",
            "##########",
            "....##....",
            "....##....",
            "....##....",
            "....##....",
        };
        pxIncrease = QIcon(QPixmap(bytes));
    }
    actIncrease.setIcon(pxIncrease);
    actIncrease.setData(QString::fromLatin1("OBTN Increase"));
    actIncrease.setParent(this);
    // addAction(&actIncrease);

    static QIcon pxDecrease;
    if(pxDecrease.isNull()) {
        const char * const bytes[]={
            "10 10 2 1",
            ". c None",
            TITLE_BUTTON_COLOR,
            "..........",
            "..........",
            "..........",
            "..........",
            "##########",
            "##########",
            "..........",
            "..........",
            "..........",
            "..........",
        };
        pxDecrease = QIcon(QPixmap(bytes));
    }
    actDecrease.setIcon(pxDecrease);
    actDecrease.setData(QString::fromLatin1("OBTN Decrease"));
    actDecrease.setParent(this);
    // addAction(&actDecrease);

    retranslate();

    connect(tabBar(), SIGNAL(tabBarClicked(int)), this, SLOT(onCurrentChanged(int)));
    connect(tabBar(), SIGNAL(tabMoved(int,int)), this, SLOT(onTabMoved(int,int)));
    tabBar()->installEventFilter(this);
    connect(splitter, SIGNAL(splitterMoved(int,int)), this, SLOT(onSplitterMoved()));

    timer.setSingleShot(true);
    connect(&timer, SIGNAL(timeout()), this, SLOT(setupLayout()));

    repaintTimer.setSingleShot(true);
    connect(&repaintTimer, SIGNAL(timeout()), this, SLOT(onRepaint()));

    _animator = new QPropertyAnimation(this, "animation", this);
    _animator->setStartValue(0.0);
    _animator->setEndValue(1.0);
    connect(_animator, SIGNAL(stateChanged(QAbstractAnimation::State, 
                                           QAbstractAnimation::State)),
            this, SLOT(onAnimationStateChanged()));
}

void OverlayTabWidget::onAnimationStateChanged()
{
    if (_animator->state() != QAbstractAnimation::Running) {
        setAnimation(0);
        if (_animator->startValue().toReal() == 0.0) {
            hide();
            OverlayManager::instance()->refresh();
        }
    }
}

void OverlayTabWidget::setAnimation(qreal t)
{
    if (t != _animation) {
        _animation = t;
        setupLayout();
    }
}

void OverlayTabWidget::startShow()
{
    if (isVisible() || _state != State_Normal)
        return;
    int duration = ViewParams::getDockOverlayAnimationDuration();
    if (duration) {
        _animator->setStartValue(1.0);
        _animator->setEndValue(0.0);
        _animator->setDuration(duration);
        _animator->setEasingCurve((QEasingCurve::Type)ViewParams::getDockOverlayAnimationCurve());
        _animator->start();
    }
    proxyWidget->hide();
    show();
}

void OverlayTabWidget::startHide()
{
    if (!isVisible()
            || _state != State_Normal
            || (_animator->state() == QAbstractAnimation::Running
                && _animator->startValue().toReal() == 0.0))
        return;
    int duration = ViewParams::getDockOverlayAnimationDuration();
    if (!duration)
        hide();
    else {
        _animator->setStartValue(0.0);
        _animator->setEndValue(1.0);
        _animator->setDuration(duration);
        _animator->setEasingCurve((QEasingCurve::Type)ViewParams::getDockOverlayAnimationCurve());
        _animator->start();
    }
}

bool OverlayTabWidget::event(QEvent *ev)
{
    switch(ev->type()) {
    case QEvent::MouseButtonRelease:
        if(mouseGrabber() == this) {
            releaseMouse();
            ev->accept();
            return true;
        }
        break;
    case QEvent::MouseMove:
    case QEvent::ContextMenu:
        if(QApplication::mouseButtons() == Qt::NoButton && mouseGrabber() == this) {
            releaseMouse();
            ev->accept();
            return true;
        }
        break;
    case QEvent::MouseButtonPress:
        ev->accept();
        return true;
    default:
        break;
    }
    return QTabWidget::event(ev);
}

int OverlayTabWidget::testAlpha(const QPoint &_pos)
{
    if (!count() || (!isOverlayed() && !isTransparent()) || !isVisible())
        return -1;

    if (tabBar()->isVisible() && tabBar()->tabAt(tabBar()->mapFromGlobal(_pos))>=0)
        return -1;

    if (titleBar->isVisible() && titleBar->rect().contains(titleBar->mapFromGlobal(_pos)))
        return -1;

    if (!splitter->isVisible())
        return 0;

    auto pos = splitter->mapFromGlobal(_pos);
    QSize size = splitter->size();
    if (pos.x() < 0 || pos.y() < 0
            || pos.x() >= size.width()
            || pos.y() >= size.height())
    {
        if (this->rect().contains(this->mapFromGlobal(_pos)))
            return 0;
        return -1;
    }

    if (_image.isNull()) {
        auto pixmap = splitter->grab();
        _imageScale = pixmap.devicePixelRatio();
        _image = pixmap.toImage();
    }

    int res = qAlpha(_image.pixel(pos*_imageScale));
    int radius = ViewParams::getDockOverlayAlphaRadius();
    if (res || radius<=0 )
        return res;

    radius *= _imageScale;
    for (int i=-radius; i<radius; ++i) {
        for (int j=-radius; j<radius; ++j) {
            if (pos.x()+i < 0 || pos.y()+j < 0
                    || pos.x()+i >= size.width()
                    || pos.y()+j >= size.height())
                continue;
            res = qAlpha(_image.pixel(pos*_imageScale + QPoint(i,j)));
            if (res)
                return res;
        }
    }
    return 0;
}

void OverlayTabWidget::paintEvent(QPaintEvent *ev)
{
    Base::StateLocker guard(repainting);
    repaintTimer.stop();
    if (!_image.isNull())
        _image = QImage();
    QTabWidget::paintEvent(ev);
}

void OverlayTabWidget::onRepaint()
{
    Base::StateLocker guard(repainting);
    repaintTimer.stop();
    if (!_image.isNull())
        _image = QImage();
    splitter->repaint();
}

void OverlayTabWidget::scheduleRepaint()
{
    if(!repainting
            && isVisible() 
            && _graphicsEffect
            && _graphicsEffect->enabled())
    {
        repaintTimer.start(100);
    }
}

QColor OverlayTabWidget::effectColor() const
{
    return _graphicsEffect->color();
}

void OverlayTabWidget::setEffectColor(const QColor &color)
{
    _graphicsEffect->setColor(color);
    _graphicsEffectTab->setColor(color);
}

int OverlayTabWidget::effectWidth() const
{
    return _graphicsEffect->size().width();
}

void OverlayTabWidget::setEffectWidth(int s)
{
    auto size = _graphicsEffect->size();
    size.setWidth(s);
    _graphicsEffect->setSize(size);
    _graphicsEffectTab->setSize(size);
}

int OverlayTabWidget::effectHeight() const
{
    return _graphicsEffect->size().height();
}

void OverlayTabWidget::setEffectHeight(int s)
{
    auto size = _graphicsEffect->size();
    size.setHeight(s);
    _graphicsEffect->setSize(size);
    _graphicsEffectTab->setSize(size);
}

qreal OverlayTabWidget::effectOffsetX() const
{
    return _graphicsEffect->offset().x();
}

void OverlayTabWidget::setEffectOffsetX(qreal d)
{
    auto offset = _graphicsEffect->offset();
    offset.setX(d);
    _graphicsEffect->setOffset(offset);
    _graphicsEffectTab->setOffset(offset);
}

qreal OverlayTabWidget::effectOffsetY() const
{
    return _graphicsEffect->offset().y();
}

void OverlayTabWidget::setEffectOffsetY(qreal d)
{
    auto offset = _graphicsEffect->offset();
    offset.setY(d);
    _graphicsEffect->setOffset(offset);
    _graphicsEffectTab->setOffset(offset);
}

qreal OverlayTabWidget::effectBlurRadius() const
{
    return _graphicsEffect->blurRadius();
}

void OverlayTabWidget::setEffectBlurRadius(qreal r)
{
    _graphicsEffect->setBlurRadius(r);
    _graphicsEffectTab->setBlurRadius(r);
}

bool OverlayTabWidget::effectEnabled() const
{
    return _effectEnabled;
}

void OverlayTabWidget::setEffectEnabled(bool enable)
{
    _effectEnabled = enable;
}

bool OverlayTabWidget::eventFilter(QObject *o, QEvent *ev)
{
    if(ev->type() == QEvent::Resize && o == tabBar()) {
        if (_state == State_Normal)
            timer.start(10);
    }
    return QTabWidget::eventFilter(o, ev);
}

void OverlayTabWidget::restore(ParameterGrp::handle handle)
{
    std::string widgets = handle->GetASCII("Widgets","");
    for(auto &name : QString::fromLatin1(widgets.c_str()).split(QLatin1Char(','))) {
        if(name.isEmpty())
            continue;
        auto dock = getMainWindow()->findChild<QDockWidget*>(name);
        if(dock)
            addWidget(dock, dock->windowTitle());
    }
    int width = handle->GetInt("Width", 0);
    int height = handle->GetInt("Height", 0);
    int offset1 = handle->GetInt("Offset1", 0);
    int offset2 = handle->GetInt("Offset3", 0);
    setOffset(QSize(offset1,offset2));
    setSizeDelta(handle->GetInt("Offset2", 0));
    if(width && height) {
        QRect rect = geometry();
        setRect(QRect(rect.left(),rect.top(),width,height));
    }
    setAutoHide(handle->GetBool("AutoHide", false));
    setTransparent(handle->GetBool("Transparent", false));
    setEditHide(handle->GetBool("EditHide", false));
    setEditShow(handle->GetBool("EditShow", false));

    std::string savedSizes = handle->GetASCII("Sizes","");
    QList<int> sizes;
    for(auto &size : QString::fromLatin1(savedSizes.c_str()).split(QLatin1Char(',')))
        sizes.append(size.toInt());

    getSplitter()->setSizes(sizes);
    hGrp = handle;
}

void OverlayTabWidget::saveTabs()
{
    if(!hGrp)
        return;

    std::ostringstream os;
    for(int i=0,c=count(); i<c; ++i) {
        auto dock = dockWidget(i);
        if(dock && dock->objectName().size())
            os << dock->objectName().toLatin1().constData() << ",";
    }
    hGrp->SetASCII("Widgets", os.str().c_str());

    if(splitter->isVisible()) {
        os.str("");
        for(int size : splitter->sizes())
            os << size << ",";
        hGrp->SetASCII("Sizes", os.str().c_str());
    }
}

void OverlayTabWidget::onTabMoved(int from, int to)
{
    QWidget *w = splitter->widget(from);
    splitter->insertWidget(to,w);
    saveTabs();
}

void OverlayTabWidget::setTitleBar(QWidget *w)
{
    titleBar = w;
}

void OverlayTabWidget::changeEvent(QEvent *e)
{
    QTabWidget::changeEvent(e);
    if (e->type() == QEvent::LanguageChange)
        retranslate();
}

void OverlayTabWidget::retranslate()
{
    actTransparent.setToolTip(tr("Toggle transparent mode"));
    actAutoHide.setToolTip(tr("Toggle auto hide mode"));
    actEditHide.setToolTip(tr("Toggle auto hide on edit mode"));
    actEditShow.setToolTip(tr("Toggle auto show on edit mode"));
    actIncrease.setToolTip(tr("Increase window size, either width or height depending on the docking site.\n"
                              "Hold CTRL key while pressing the button to change size in the other dimension.\n"
                              "Hold SHIFT key while pressing the button to move the window.\n"
                              "Hold CTRL + SHIFT key to move the window in the other direction."));
    actDecrease.setToolTip(tr("Decrease window size, either width or height depending on the docking site.\n"
                              "Hold CTRL key while pressing to change size in the other dimension.\n"
                              "Hold SHIFT key while pressing the button to move the window.\n"
                              "Hold CTRL + SHIFT key to move the window in the other direction."));
    actOverlay.setToolTip(tr("Toggle overlay"));
}

void OverlayTabWidget::syncAction()
{
    QAction *action = &actAutoHide;
    if (actEditShow.isChecked())
        action = &actEditShow;
    else if (actEditHide.isChecked())
        action = &actEditHide;
    actAutoMode.setIcon(action->icon());
    actAutoMode.setToolTip(action->toolTip());
    QSignalBlocker blocker(&actAutoMode);
    actAutoMode.setChecked(action->isChecked());
}

void OverlayTabWidget::onAction(QAction *action)
{
    if (action == &actAutoMode) {
        QMenu menu;
        menu.addAction(&actAutoHide);
        menu.addAction(&actEditShow);
        menu.addAction(&actEditHide);
        menu.exec(QCursor::pos());
        return;
    }
    if(action == &actEditHide) {
        if(hGrp)
            hGrp->SetBool("EditHide", actEditHide.isChecked());
        if(action->isChecked()) {
            setAutoHide(false);
            setEditShow(false);
        }
    } else if(action == &actAutoHide) {
        if(hGrp)
            hGrp->SetBool("AutoHide", actAutoHide.isChecked());
        if(action->isChecked()) {
            setEditHide(false);
            setEditShow(false);
        }
    } else if(action == &actEditShow) {
        if(hGrp)
            hGrp->SetBool("EditShow", actEditShow.isChecked());
        if(action->isChecked()) {
            setEditHide(false);
            setAutoHide(false);
        }
    } else if(action == &actIncrease)
        changeSize(5);
    else if(action == &actDecrease)
        changeSize(-5);
    else if(action == &actOverlay) {
        OverlayManager::instance()->setOverlayMode(OverlayManager::ToggleActive);
        return;
    } else if(action == &actTransparent) {
        if(hGrp)
            hGrp->SetBool("Transparent", actTransparent.isChecked());
    }
    OverlayManager::instance()->refresh(this);
}

void OverlayTabWidget::setState(State state)
{
    if (_state == state)
        return;
    switch(state) {
    case State_Normal:
        _state = state;
        hide();
        if (dockArea == Qt::RightDockWidgetArea)
            setTabPosition(East);
        else if (dockArea == Qt::BottomDockWidgetArea)
            setTabPosition(South);
        if (count() == 1)
            tabBar()->hide();
        _graphicsEffectTab->setEnabled(false);
        titleBar->show();
        splitter->show();
        break;
    case State_Hint:
        if (_state == State_HintHidden)
            break;
        _state = state;
        if (ViewParams::getDockOverlayHintTabBar()) {
            tabBar()->show();
            titleBar->hide();
            splitter->hide();
            _graphicsEffectTab->setEnabled(true);
            show();
            raise();
            proxyWidget->raise();
            if (dockArea == Qt::RightDockWidgetArea)
                setTabPosition(West);
            else if (dockArea == Qt::BottomDockWidgetArea)
                setTabPosition(North);
            OverlayManager::instance()->refresh(this);
        }
        break;
    case State_HintHidden:
        _state = state;
        hide();
        _graphicsEffectTab->setEnabled(true);
        break;
    }
}

bool OverlayTabWidget::checkAutoHide() const
{
    if(isAutoHide())
        return true;

    if(ViewParams::getDockOverlayAutoView()) {
        auto view = getMainWindow()->activeWindow();
        if(!view || (!view->isDerivedFrom(View3DInventor::getClassTypeId())
                        && !view->isDerivedFrom(SplitView3DInventor::getClassTypeId())))
            return true;
    }

    if(isEditShow()) {
        return !Application::Instance->editDocument() 
            && (!Control().taskPanel() || Control().taskPanel()->isEmpty());
    }

    if(isEditHide() && Application::Instance->editDocument())
        return true;

    return false;
}

static inline OverlayTabWidget *findTabWidget(QWidget *widget=nullptr, bool filterDialog=false)
{
    if(!widget)
        widget = qApp->focusWidget();
    for(auto w=widget; w; w=w->parentWidget()) {
        auto tabWidget = qobject_cast<OverlayTabWidget*>(w);
        if(tabWidget) 
            return tabWidget;
        auto proxy = qobject_cast<OverlayProxyWidget*>(w);
        if(proxy)
            return proxy->getOwner();
        if(filterDialog && w->windowType() != Qt::Widget)
            break;
    }
    return nullptr;
}

void OverlayTabWidget::leaveEvent(QEvent*)
{
    if (titleBar && QWidget::mouseGrabber() == titleBar)
        return;
    OverlayManager::instance()->refresh();
}

void OverlayTabWidget::enterEvent(QEvent*)
{
    revealTime = QTime();
    OverlayManager::instance()->refresh();
}

void OverlayTabWidget::setRevealTime(const QTime &time)
{
    revealTime = time;
}

class OverlayStyleSheet: public ParameterGrp::ObserverType {
public:

    OverlayStyleSheet() {
        handle = App::GetApplication().GetParameterGroupByPath(
                "User parameter:BaseApp/Preferences/MainWindow");
        update();
        handle->Attach(this);
    }

    static OverlayStyleSheet *instance() {
        static OverlayStyleSheet *inst;
        if(!inst)
            inst = new OverlayStyleSheet;
        return inst;
    }

    void OnChange(Base::Subject<const char*> &, const char* sReason) {
        if(!sReason)
            return;
        if(strcmp(sReason, "StyleSheet")==0
                || strcmp(sReason, "OverlayActiveStyleSheet")==0
                || strcmp(sReason, "OverlayOnStyleSheet")==0
                || strcmp(sReason, "OverlayOffStyleSheet")==0)
        {
            OverlayManager::instance()->refresh(nullptr, true);
        }
    }

    void update() {
        QString mainstyle = QString::fromLatin1(handle->GetASCII("StyleSheet").c_str());

        QString prefix;
       
        if(!mainstyle.isEmpty()) {
            int dark = mainstyle.indexOf(QLatin1String("dark"),0,Qt::CaseInsensitive);
            prefix = QString::fromLatin1("overlay:%1").arg(
                    dark<0 ? QLatin1String("Light") : QLatin1String("Dark"));
        }

        QString name;

        onStyleSheet.clear();
        if(ViewParams::getDockOverlayExtraState()) {
            name = QString::fromUtf8(handle->GetASCII("OverlayOnStyleSheet").c_str());
            if(name.isEmpty() && !prefix.isEmpty())
                name = prefix + QLatin1String("-on.qss");
            else if (!QFile::exists(name))
                name = QString::fromLatin1("overlay:%1").arg(name);
            if(QFile::exists(name)) {
                QFile f(name);
                if(f.open(QFile::ReadOnly)) {
                    QTextStream str(&f);
                    onStyleSheet = str.readAll();
                }
            }
            if(onStyleSheet.isEmpty()) {
                static QLatin1String _default(
                    "* { background-color: transparent;"
                        "border: 1px solid palette(dark);"
                        "alternate-background-color: rgba(255,255,255,100)}"
                    "QTreeView, QListView { background: rgba(255,255,255,50) }"
                    "QToolTip { background-color: palette(base) }"
                    // Both background and border are necessary to make this work.
                    // And this spare us to have to call QTabWidget::setDocumentMode(true).
                    "QTabWidget:pane { background-color: rgba(255,255,255,50); border: transparent }"
                );
                onStyleSheet = _default;
            }
        }

        name = QString::fromUtf8(handle->GetASCII("OverlayOffStyleSheet").c_str());
        if(name.isEmpty() && !prefix.isEmpty())
            name = prefix + QLatin1String("-off.qss");
        else if (!QFile::exists(name))
            name = QString::fromLatin1("overlay:%1").arg(name);
        offStyleSheet.clear();
        if(QFile::exists(name)) {
            QFile f(name);
            if(f.open(QFile::ReadOnly)) {
                QTextStream str(&f);
                offStyleSheet = str.readAll();
            }
        }
        if(offStyleSheet.isEmpty()) {
            static QLatin1String _default(
                "Gui--OverlayToolButton { background: transparent; padding: 0px; border: none }"
                "Gui--OverlayToolButton:hover { background: palette(light); border: 1px solid palette(dark) }"
                "Gui--OverlayToolButton:focus { background: palette(dark); border: 1px solid palette(dark) }"
                "Gui--OverlayToolButton:pressed { background: palette(dark); border: 1px inset palette(dark) }"
                "Gui--OverlayToolButton:checked { background: palette(dark); border: 1px inset palette(dark) }"
                "Gui--OverlayToolButton:checked:hover { background: palette(light); border: 1px inset palette(dark) }"
            );
            offStyleSheet = _default;
        }

        name = QString::fromUtf8(handle->GetASCII("OverlayActiveStyleSheet").c_str());
        if(name.isEmpty() && !prefix.isEmpty())
            name = prefix + QLatin1String(".qss");
        else if (!QFile::exists(name))
            name = QString::fromLatin1("overlay:%1").arg(name);
        activeStyleSheet.clear();
        if(QFile::exists(name)) {
            QFile f(name);
            if(f.open(QFile::ReadOnly)) {
                QTextStream str(&f);
                activeStyleSheet = str.readAll();
            }
        }
        if(activeStyleSheet.isEmpty()) {
            static QLatin1String _default(
                "* {alternate-background-color: rgba(250,250,250,120);}"

                "QComboBox, QComboBox:editable, QComboBox:!editable, QLineEdit,"
                "QTextEdit, QPlainTextEdit, QAbstractSpinBox, QDateEdit, QDateTimeEdit,"
                "Gui--PropertyEditor--PropertyEditor QLabel "
                    "{background : palette(base);}"

                "QScrollBar { background: transparent;}"
                "QTabWidget::pane { background-color: transparent; border: transparent }"
                "Gui--OverlayTabWidget { qproperty-effectColor: rgba(0,0,0,0) }"
                "Gui--OverlayTabWidget::pane { background-color: rgba(250,250,250,80) }"

                "QTabBar {border : none;}"
                "QTabBar::tab {color: palette(text);"
                              "background-color: rgba(100,100,100,50);"
                              "padding: 5px}"
                "QTabBar::tab:selected {background-color: rgba(250,250,250,80);}"
                "QTabBar::tab:hover {background-color: rgba(250,250,250,200);}"

                "QHeaderView { background:transparent }"
                "QHeaderView::section {color: palette(text);"
                                      "background-color: rgba(250,250,250,50);"
                                      "border: 1px solid palette(dark);"
                                      "padding: 2px}"

                "QTreeView, QListView, QTableView {"
                            "background: rgb(250,250,250);"
                            "selection-background-color: rgba(94, 144, 250, 0.7);}"
                "QListView::item:selected, QTreeView::item:selected {"
                            "background-color: rgba(94, 144, 250, 0.7);}"

                "Gui--PropertyEditor--PropertyEditor {"
                            "border: 1px solid palette(dark);"
                            "qproperty-groupTextColor: rgb(100, 100, 100);"
                            "qproperty-groupBackground: rgba(180, 180, 180, 0.7);}"

                "QToolTip {background-color: rgba(250,250,250,180);}"

                "Gui--CallTipsList::item { background-color: rgba(200,200,200,200);}"
                "Gui--CallTipsList::item::selected { background-color: palette(highlight);}"

                "QAbstractButton { background: rgba(250,250,250,80);"
                                  "padding: 2px 4px;}"
                "QAbstractButton::hover { background: rgba(250,250,250,200);}"
                "QAbstractButton::focus { background: rgba(250,250,250,255);}"
                "QAbstractButton::pressed { background: rgba(100,100,100,100);"
                                           "border: 1px inset palette(dark) }"
                "QAbstractButton::checked { background: rgba(100,100,100,100);"
                                           "border: 1px inset palette(dark) }"
                "QAbstractButton::checked:hover { background: rgba(150,150,150,200);"
                                                 "border: 1px inset palette(dark) }"
                "Gui--OverlayToolButton { background: transparent; padding: 0px; border: none }"
                );
            activeStyleSheet = _default;
        }

        if(onStyleSheet.isEmpty()) {
            onStyleSheet = activeStyleSheet;
            hideTab = false;
        } else {
            hideTab = (onStyleSheet.indexOf(QLatin1String("QTabBar")) < 0);
        }
    }

    ParameterGrp::handle handle;
    QString onStyleSheet;
    QString offStyleSheet;
    QString activeStyleSheet;
    bool hideTab = false;
};

void OverlayTabWidget::_setOverlayMode(QWidget *widget, int enable)
{
    if(!widget)
        return;

#if QT_VERSION>QT_VERSION_CHECK(5,12,2) && QT_VERSION < QT_VERSION_CHECK(5,12,6)
    // Work around Qt bug https://bugreports.qt.io/browse/QTBUG-77006
    if(enable < 0)
        widget->setStyleSheet(OverlayStyleSheet::instance()->activeStyleSheet);
    else if(enable)
        widget->setStyleSheet(OverlayStyleSheet::instance()->onStyleSheet);
    else
        widget->setStyleSheet(OverlayStyleSheet::instance()->offStyleSheet);
#endif

    auto tabbar = qobject_cast<QTabBar*>(widget);
    if(tabbar) {
        // Stylesheet QTabWidget::pane make the following two calls unnecessary
        //
        // tabbar->setDrawBase(enable>0);
        // tabbar->setDocumentMode(enable!=0);

        if(!tabbar->autoHide() || tabbar->count()>1) {
            if(!OverlayStyleSheet::instance()->hideTab)
                tabbar->setVisible(true);
            else
                tabbar->setVisible(enable==0 || (enable<0 && tabbar->count()>1));
            return;
        }
    }
    if(enable!=0) {
        widget->setWindowFlags(widget->windowFlags() | Qt::FramelessWindowHint);
    } else {
        widget->setWindowFlags(widget->windowFlags() & ~Qt::FramelessWindowHint);
    }
    widget->setAttribute(Qt::WA_NoSystemBackground, enable!=0);
    widget->setAttribute(Qt::WA_TranslucentBackground, enable!=0);
}

void OverlayTabWidget::setOverlayMode(QWidget *widget, int enable)
{
    if(!widget || qobject_cast<QDialog*>(widget)
               || qobject_cast<TaskView::TaskPanel*>(widget))
        return;

    if(widget != tabBar()) {
        if((ViewParams::getDockOverlayMouseThrough()
                    || ViewParams::getDockOverlayAutoMouseThrough())
                && enable == -1)
        {
            widget->setMouseTracking(true);
        }
    }

    _setOverlayMode(widget, enable);

    if(qobject_cast<QComboBox*>(widget)) {
        // do not set child QAbstractItemView of QComboBox, otherwise the drop down box
        // won't be shown
        return;
    }
    for(auto child : widget->children())
        setOverlayMode(qobject_cast<QWidget*>(child), enable);
}

void OverlayTabWidget::setAutoHide(bool enable)
{
    if(actAutoHide.isChecked() == enable)
        return;
    if(hGrp)
        hGrp->SetBool("AutoHide", enable);
    actAutoHide.setChecked(enable);
    syncAction();
    if(enable) {
        setEditHide(false);
        setEditShow(false);
    }
    OverlayManager::instance()->refresh(this);
}

void OverlayTabWidget::setTransparent(bool enable)
{
    if(actTransparent.isChecked() == enable)
        return;
    if(hGrp)
        hGrp->SetBool("Transparent", enable);
    actTransparent.setChecked(enable);
    OverlayManager::instance()->refresh(this);
}

void OverlayTabWidget::setEditHide(bool enable)
{
    if(actEditHide.isChecked() == enable)
        return;
    if(hGrp)
        hGrp->SetBool("EditHide", enable);
    actEditHide.setChecked(enable);
    syncAction();
    if(enable) {
        setAutoHide(false);
        setEditShow(false);
    }
    OverlayManager::instance()->refresh(this);
}

void OverlayTabWidget::setEditShow(bool enable)
{
    if(actEditShow.isChecked() == enable)
        return;
    if(hGrp)
        hGrp->SetBool("EditShow", enable);
    actEditShow.setChecked(enable);
    syncAction();
    if(enable) {
        setAutoHide(false);
        setEditHide(false);
    }
    OverlayManager::instance()->refresh(this);
}

QDockWidget *OverlayTabWidget::currentDockWidget() const
{
    int index = -1;
    for(int size : splitter->sizes()) {
        ++index;
        if(size>0)
            return dockWidget(index);
    }
    return dockWidget(currentIndex());
}

QDockWidget *OverlayTabWidget::dockWidget(int index) const
{
    if(index < 0 || index >= splitter->count())
        return nullptr;
    return qobject_cast<QDockWidget*>(splitter->widget(index));
}

void OverlayTabWidget::setOverlayMode(bool enable)
{
    overlayed = enable;

    if(!isVisible() || !count())
        return;

    touched = false;

    if (_state == State_Normal)
        titleBar->setVisible(!enable);

    QString stylesheet;
    int mode;

    if(!enable && isTransparent()) {
        stylesheet = OverlayStyleSheet::instance()->activeStyleSheet;
        mode = -1;
    } else if (enable && !isTransparent() && (isEditShow() || isAutoHide())) {
        stylesheet = OverlayStyleSheet::instance()->offStyleSheet;
        mode = 0;
    } else {
        if(enable)
            stylesheet = OverlayStyleSheet::instance()->onStyleSheet;
        else
            stylesheet = OverlayStyleSheet::instance()->offStyleSheet;
        mode = enable?1:0;
    }

    proxyWidget->setStyleSheet(stylesheet);
    this->setStyleSheet(stylesheet);
    setOverlayMode(this, mode);

    _graphicsEffect->setEnabled(effectEnabled() && (enable || isTransparent()));

    if (_state == State_Hint && ViewParams::getDockOverlayHintTabBar()) {
        tabBar()->show();
    } else if (ViewParams::getDockOverlayHideTabBar() || count()==1) {
        tabBar()->hide();
    } else
        tabBar()->setVisible(!enable || !OverlayStyleSheet::instance()->hideTab);

    setRect(rectOverlay);
}

const QRect &OverlayTabWidget::getRect()
{
    return rectOverlay;
}

bool OverlayTabWidget::getAutoHideRect(QRect &rect) const
{
    rect = rectOverlay;
    int hintWidth = ViewParams::getDockOverlayHintSize();
    switch(dockArea) {
    case Qt::LeftDockWidgetArea:
    case Qt::RightDockWidgetArea:
        if (_TopOverlay->isVisible())
            rect.setTop(std::max(rect.top(), _TopOverlay->rectOverlay.bottom()));
        if (dockArea == Qt::RightDockWidgetArea)
            rect.setLeft(rect.left() + std::max(rect.width()-hintWidth,0));
        else
            rect.setRight(rect.right() - std::max(rect.width()-hintWidth,0));
        break;
    case Qt::TopDockWidgetArea:
    case Qt::BottomDockWidgetArea:
        if (_LeftOverlay->isVisible())
            rect.setLeft(std::max(rect.left(),_LeftOverlay->rectOverlay.right()));
        if (dockArea == Qt::TopDockWidgetArea)
            rect.setBottom(rect.bottom() - std::max(rect.height()-hintWidth,0));
        else {
            rect.setTop(rect.top() + std::max(rect.height()-hintWidth,0));
            if (_RightOverlay->isVisible())
                rect.setRight(std::min(rect.right(), _RightOverlay->x()));
        }
        break;
    default:
        break;
    }
    return overlayed && checkAutoHide();
}

void OverlayTabWidget::setOffset(const QSize &ofs)
{
    if(offset != ofs) {
        offset = ofs;
        if(hGrp) {
            hGrp->SetInt("Offset1", ofs.width());
            hGrp->SetInt("Offset3", ofs.height());
        }
    }
}

void OverlayTabWidget::setSizeDelta(int delta)
{
    if(sizeDelta != delta) {
        if(hGrp)
            hGrp->SetInt("Offset2", delta);
        sizeDelta = delta;
    }
}

void OverlayTabWidget::setRect(QRect rect)
{
    if(busy || rect.width()<=0 || rect.height()<=0)
        return;

    switch(dockArea) {
    case Qt::LeftDockWidgetArea:
        if (rect.width() < _MinimumOverlaySize)
            rect.setWidth(_MinimumOverlaySize);
        break;
    case Qt::RightDockWidgetArea:
        if (rect.width() < _MinimumOverlaySize)
            rect.setLeft(rect.right()-_MinimumOverlaySize);
        break;
    case Qt::TopDockWidgetArea:
        if (rect.height() < _MinimumOverlaySize)
            rect.setHeight(_MinimumOverlaySize);
        break;
    default:
        if (rect.height() < _MinimumOverlaySize)
            rect.setTop(rect.bottom()-_MinimumOverlaySize);
        break;
    }

    if(hGrp && rect.size() != rectOverlay.size()) {
        hGrp->SetInt("Width", rect.width());
        hGrp->SetInt("Height", rect.height());
    }
    rectOverlay = rect;

    if(getAutoHideRect(rect) || _state == State_Hint) {
        QRect rectHint = rect;
        if (_state != State_Hint)
            startHide();
        else if (count() && ViewParams::getDockOverlayHintTabBar()) {
            switch(dockArea) {
            case Qt::LeftDockWidgetArea: 
            case Qt::RightDockWidgetArea: 
                rectHint.setBottom(rect.bottom());
                if (dockArea == Qt::LeftDockWidgetArea)
                    rect.setWidth(tabBar()->width());
                else
                    rect.setLeft(rect.left() + rect.width() - tabBar()->width());
                rect.setHeight(std::min(rect.height(), 
                            tabBar()->y() + tabBar()->sizeHint().height() + 5));
                break;
            case Qt::BottomDockWidgetArea: 
            case Qt::TopDockWidgetArea: 
                rectHint.setRight(rect.right());
                if (dockArea == Qt::TopDockWidgetArea)
                    rect.setHeight(tabBar()->height());
                else
                    rect.setTop(rect.top() + rect.height() - tabBar()->height());
                rect.setWidth(std::min(rect.width(),
                            tabBar()->x() + tabBar()->sizeHint().width() + 5));
                break;
            default:
                break;
            }

            setGeometry(rect);
        }
        proxyWidget->setGeometry(rectHint);
        proxyWidget->show();
        proxyWidget->raise();

    } else {
        setGeometry(rectOverlay);

        for(int i=0, count=splitter->count(); i<count; ++i)
            splitter->widget(i)->show();

        if(!isVisible() && count()) {
            proxyWidget->hide();
            startShow();
            Base::StateLocker guard(busy);
            setOverlayMode(overlayed);
        }
    }
}

void OverlayTabWidget::addWidget(QDockWidget *dock, const QString &title)
{
    QRect rect = dock->geometry();

    getMainWindow()->removeDockWidget(dock);

    auto titleWidget = dock->titleBarWidget();
    if(titleWidget && titleWidget->objectName()==QLatin1String("OverlayTitle")) {
        // replace the title bar with an invisible widget to hide it. The
        // OverlayTabWidget uses its own title bar for all docks.
        auto w = new QWidget();
        w->setObjectName(QLatin1String("OverlayTitle"));
        dock->setTitleBarWidget(w);
        w->hide();
        titleWidget->deleteLater();
    }

    dock->show();
    splitter->addWidget(dock);
    addTab(new QWidget(this), title);

    dock->setFeatures(dock->features() & ~QDockWidget::DockWidgetFloatable);
    if(count() == 1)
        setRect(rect);

    saveTabs();
}

int OverlayTabWidget::dockWidgetIndex(QDockWidget *dock) const
{
    return splitter->indexOf(dock);
}

void OverlayTabWidget::removeWidget(QDockWidget *dock, QDockWidget *lastDock)
{
    int index = dockWidgetIndex(dock);
    if(index < 0)
        return;

    dock->show();
    if(lastDock)
        getMainWindow()->tabifyDockWidget(lastDock, dock);
    else
        getMainWindow()->addDockWidget(dockArea, dock);

    auto w = this->widget(index);
    removeTab(index);
    w->deleteLater();

    if(!count())
        hide();

    w = dock->titleBarWidget();
    if(w && w->objectName() == QLatin1String("OverlayTitle")) {
        dock->setTitleBarWidget(nullptr);
        w->deleteLater();
    }
    OverlayManager::instance()->setupTitleBar(dock);

    dock->setFeatures(dock->features() | QDockWidget::DockWidgetFloatable);

    setOverlayMode(dock, 0);

    saveTabs();
}

void OverlayTabWidget::resizeEvent(QResizeEvent *ev)
{
    QTabWidget::resizeEvent(ev);
    if (_state == State_Normal)
        timer.start(10);
}

void OverlayTabWidget::setupLayout()
{
    if (_state != State_Normal)
        return;

    if(count() == 1)
        tabSize = 0;
    else {
        int tsize;
        if(dockArea==Qt::LeftDockWidgetArea || dockArea==Qt::RightDockWidgetArea)
            tsize = tabBar()->width();
        else
            tsize = tabBar()->height();
        tabSize = tsize;
    }
    int titleBarSize = _TitleButtonSize + 1;
    QRect rect, rectTitle;
    switch(tabPosition()) {
    case West:
        rectTitle = QRect(tabSize, 0, this->width()-tabSize, titleBarSize);
        rect = QRect(rectTitle.left(), rectTitle.bottom(),
                     rectTitle.width(), this->height()-rectTitle.height());
        break;
    case East:
        rectTitle = QRect(0, 0, this->width()-tabSize, titleBarSize);
        rect = QRect(rectTitle.left(), rectTitle.bottom(),
                     rectTitle.width(), this->height()-rectTitle.height());
        break;
    case North:
        rectTitle = QRect(0, tabSize, titleBarSize, this->height()-tabSize);
        rect = QRect(rectTitle.right(), rectTitle.top(),
                     this->width()-rectTitle.width(), rectTitle.height());
        break;
    case South:
        rectTitle = QRect(0, 0, titleBarSize, this->height()-tabSize);
        rect = QRect(rectTitle.right(), rectTitle.top(),
                     this->width()-rectTitle.width(), rectTitle.height());
        break;
    }
    if (_animation != 0.0) {
        switch(dockArea) {
        case Qt::LeftDockWidgetArea:
            rect.moveLeft(rect.left() - _animation * rect.width());
            break;
        case Qt::RightDockWidgetArea:
            rect.moveLeft(rect.left() + _animation * rect.width());
            break;
        case Qt::TopDockWidgetArea:
            rect.moveTop(rect.top() - _animation * rect.height());
            break;
        case Qt::BottomDockWidgetArea:
            rect.moveTop(rect.top() + _animation * rect.height());
            break;
        default:
            break;
        }
    }
    splitter->setGeometry(rect);
    titleBar->setGeometry(rectTitle);
}

void OverlayTabWidget::setCurrent(QDockWidget *widget)
{
    int index = dockWidgetIndex(widget);
    if(index >= 0) 
        setCurrentIndex(index);
}

void OverlayTabWidget::onSplitterMoved()
{
    int index = -1;
    for(int size : splitter->sizes()) {
        ++index;
        if(size) {
            if (index != currentIndex())
                setCurrentIndex(index);
            break;
        }
    }
    saveTabs();
}

void OverlayTabWidget::onCurrentChanged(int index)
{
    setState(State_Normal);
    startShow();

    auto sizes = splitter->sizes();
    int i=0;
    int size = splitter->orientation()==Qt::Vertical ? 
                    height()-tabBar()->height() : width()-tabBar()->width();
    for(auto &s : sizes) {
        if(i++ == index)
            s = size;
        else
            s = 0;
    }
    splitter->setSizes(sizes);
    saveTabs();
}

void OverlayTabWidget::changeSize(int changes, bool checkModify)
{
    auto modifier = checkModify ? QApplication::queryKeyboardModifiers() : Qt::NoModifier;
    if(modifier== Qt::ShiftModifier) {
        setOffset(QSize(std::max(offset.width()+changes, 0), offset.height()));
        return;
    } else if ((modifier == (Qt::ShiftModifier | Qt::AltModifier))
            || (modifier == (Qt::ShiftModifier | Qt::ControlModifier))) {
        setOffset(QSize(offset.width(), std::max(offset.height()+changes, 0)));
        return;
    } else if (modifier == Qt::ControlModifier || modifier == Qt::AltModifier) {
        setSizeDelta(sizeDelta - changes);
        return;
    }

    QRect rect = rectOverlay;
    switch(dockArea) {
    case Qt::LeftDockWidgetArea:
        rect.setRight(rect.right() + changes);
        break;
    case Qt::RightDockWidgetArea:
        rect.setLeft(rect.left() - changes);
        break;
    case Qt::TopDockWidgetArea:
        rect.setBottom(rect.bottom() + changes);
        break;
    case Qt::BottomDockWidgetArea:
        rect.setTop(rect.top() - changes);
        break;
    default:
        break;
    }
    setRect(rect);
}

void OverlayTabWidget::onSizeGripMove(const QPoint &p)
{
    QPoint pos = mapFromGlobal(p) + this->pos();
    QRect rect = this->rectOverlay;

    switch(dockArea) {
    case Qt::LeftDockWidgetArea:
        if (pos.x() - rect.left() < _MinimumOverlaySize)
            return;
        rect.setRight(pos.x());
        break;
    case Qt::RightDockWidgetArea:
        if (rect.right() - pos.x() < _MinimumOverlaySize)
            return;
        rect.setLeft(pos.x());
        break;
    case Qt::TopDockWidgetArea:
        if (pos.y() - rect.top() < _MinimumOverlaySize)
            return;
        rect.setBottom(pos.y());
        break;
    default:
        if (rect.bottom() - pos.y() < _MinimumOverlaySize)
            return;
        rect.setTop(pos.y());
        break;
    }
    this->setRect(rect);
    OverlayManager::instance()->refresh();
}

// -----------------------------------------------------------

OverlayTitleBar::OverlayTitleBar(QWidget * parent)
    :QWidget(parent)
{
    setMouseTracking(true);
    setCursor(Qt::OpenHandCursor);
}

void OverlayTitleBar::mouseMoveEvent(QMouseEvent *me)
{
    if (QWidget::mouseGrabber() != this) {
        if (qobject_cast<QDockWidget*>(parentWidget()))
            me->ignore();
        return;
    }
    auto mdi = getMainWindow()->getMdiArea();
    if (!_DragFrame || !mdi) {
        setCursor(Qt::OpenHandCursor);
        releaseMouse();
        return;
    }

    QPoint pos = me->globalPos();
    auto dock = qobject_cast<QDockWidget*>(parentWidget());
    if (dock && dock->isFloating())
        dock->move(pos - dragOffset);

    OverlayTabWidget *tabWidget = nullptr;
    int index = -1;
    QRect rect;
    QRect rectMain(getMainWindow()->mapToGlobal(QPoint()),
                   getMainWindow()->size());

    for (OverlayTabWidget *overlay : _Overlays) {
        rect = QRect(mdi->mapToGlobal(overlay->rectOverlay.topLeft()),
                                      overlay->rectOverlay.size());
        int dockArea = overlay->getDockArea();
        switch(dockArea) {
        case Qt::LeftDockWidgetArea:
            rect.setWidth(rect.width()*2/3);
            if (rect.width() < _MinimumOverlaySize)
                rect.setWidth(_MinimumOverlaySize);
            break;
        case Qt::RightDockWidgetArea:
            rect.setLeft(rect.right() - rect.width()*2/3);
            if (rect.width() < _MinimumOverlaySize)
                rect.setLeft(rect.right() - _MinimumOverlaySize);
            break;
        case Qt::TopDockWidgetArea:
            rect.setHeight(rect.height()*2/3);
            if (rect.height() < _MinimumOverlaySize)
                rect.setHeight(_MinimumOverlaySize);
            break;
        default:
            rect.setTop(rect.bottom() - rect.height()*2/3);
            if (rect.height() < _MinimumOverlaySize)
                rect.setTop(rect.bottom() - _MinimumOverlaySize);
            break;
        }

        if (!rect.contains(pos))
            continue;

        index = -2;
        tabWidget = overlay;
        if (dockArea == Qt::LeftDockWidgetArea) {
            if (pos.x() - rect.left() < rect.width()/2) {
                if (rect.left() - rectMain.left() > _MinimumOverlaySize)
                    rect.setRight(rect.left());
                else
                    rect.setRight(rect.left() + _MinimumOverlaySize);
                rect.setLeft(rectMain.left());
                break;
            }
        }
        else if (dockArea == Qt::RightDockWidgetArea) {
            if (rect.right() - pos.x() < rect.width()/2) {
                if (rectMain.right() - rect.right() > _MinimumOverlaySize)
                    rect.setLeft(rect.right());
                else
                    rect.setLeft(rect.right() - _MinimumOverlaySize);
                rect.setRight(rectMain.right());
                break;
            }
        }
        else if (dockArea == Qt::TopDockWidgetArea) {
            if (pos.y() - rect.top() < rect.height()/2) {
                if (rect.top() - rectMain.top() > _MinimumOverlaySize)
                    rect.setBottom(rect.top());
                else
                    rect.setBottom(rect.top() + _MinimumOverlaySize);
                rect.setTop(rectMain.top());
                break;
            }
        }
        else {
            if (rect.bottom() - pos.y() < rect.height()/2) {
                if (rectMain.bottom() - rect.bottom() > _MinimumOverlaySize)
                    rect.setTop(rect.bottom());
                else
                    rect.setTop(rect.bottom() - _MinimumOverlaySize);
                rect.setBottom(rectMain.bottom());
                break;
            }
        }

        index = -1;
        int i = -1;
        for (int size : overlay->getSplitter()->sizes()) {
            ++i;
            if (!size)
                continue;
            QWidget *w  = overlay->dockWidget(i);
            if (w && w->rect().contains(w->mapFromGlobal(pos))) {
                QPoint pt = overlay->getSplitter()->mapToGlobal(w->pos());
                rect = QRect(pt, w->size());
                index = i;
                break;
            }
        }
        break;
    };

    if (!tabWidget) {
        dragOverlay = nullptr;
        rect = QRect(pos-dragOffset, dragSize);

        for(auto dockWidget : getMainWindow()->findChildren<QDockWidget*>()) {
            if (dockWidget == dock || !dockWidget->isVisible())
                continue;
            if (dockWidget->rect().contains(dockWidget->mapFromGlobal(pos))) {
                rect = QRect(dockWidget->mapToGlobal(QPoint()),
                             dockWidget->size());
                break;
            }
        }
    }
    else {
        dragOverlay = tabWidget;
        dragDockIndex = index;
        if (dragDockIndex == -1)
            rect = QRect(mdi->mapToGlobal(tabWidget->rectOverlay.topLeft()),
                         tabWidget->rectOverlay.size());
    }
    _DragFrame->setGeometry(
            QRect(getMainWindow()->mapFromGlobal(rect.topLeft()), rect.size()));
}

void OverlayTitleBar::mousePressEvent(QMouseEvent *me)
{
    QWidget *parent = parentWidget();
    if (!parent || !getMainWindow())
        return;

    dragOverlay = nullptr;
    if (!_DragFrame) {
        if (!getMainWindow())
            return;
        _DragFrame = new OverlayDragFrame(getMainWindow());
    }

    dragSize = parent->size();
    OverlayTabWidget *tabWidget = qobject_cast<OverlayTabWidget*>(parent);
    if (!tabWidget) {
        if(QApplication::queryKeyboardModifiers() == Qt::ShiftModifier) {
            me->ignore();
            return;
        }
    }
    else {
        for (int s : tabWidget->getSplitter()->sizes()) {
            if (!s)
                continue;
            if (tabWidget == _TopOverlay || tabWidget == _BottomOverlay) {
                dragSize.setWidth(s + this->width());
                dragSize.setHeight(tabWidget->height());
            }
            else {
                dragSize.setHeight(s + this->height());
                dragSize.setWidth(tabWidget->width());
            }
        }
    }

    QRect rect(getMainWindow()->mapFromGlobal(parent->mapToGlobal(QPoint())), dragSize);
    _DragFrame->setGeometry(rect);
    _DragFrame->raise();
    _DragFrame->show();
    dragOffset = me->pos();
    grabMouse();
    setCursor(Qt::ClosedHandCursor);

    mouseMoveEvent(me);
}

void OverlayTitleBar::mouseReleaseEvent(QMouseEvent *me)
{
    setCursor(Qt::OpenHandCursor);
    _DragFrame->hide();
    if (QWidget::mouseGrabber() == this)
        releaseMouse();
    else {
        if (qobject_cast<QDockWidget*>(parentWidget()))
            me->ignore();
        return;
    }

    OverlayManager::instance()->dropDockWidget(
            me->globalPos(), parentWidget(), dragOverlay, dragDockIndex);
}

// -----------------------------------------------------------

OverlayDragFrame::OverlayDragFrame(QWidget * parent)
    :QWidget(parent)
{
}

void OverlayDragFrame::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.drawRect(0, 0, this->width()-1, this->height()-1);
    painter.setOpacity(0.3);
    painter.setBrush(QBrush(Qt::blue));
    painter.drawRect(0, 0, this->width()-1, this->height()-1);
}

// -----------------------------------------------------------

OverlaySizeGrip::OverlaySizeGrip(QWidget * parent, bool vertical)
    :QWidget(parent)
{
    setMouseTracking(true);
    setCursor(vertical ? Qt::SizeHorCursor : Qt::SizeVerCursor);
}

void OverlaySizeGrip::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.setPen(Qt::transparent);
    painter.setBrush(QBrush(Qt::black, Qt::Dense6Pattern));
    QRect rect(this->rect());
    painter.drawRect(rect);
}

void OverlaySizeGrip::mouseMoveEvent(QMouseEvent *me)
{
    if (QWidget::mouseGrabber() != this)
        return;

    Q_EMIT dragMove(me->globalPos());
}

void OverlaySizeGrip::mousePressEvent(QMouseEvent *)
{
    grabMouse();
}

void OverlaySizeGrip::mouseReleaseEvent(QMouseEvent *)
{
    if (QWidget::mouseGrabber() == this)
        releaseMouse();
}

// -----------------------------------------------------------

OverlayGraphicsEffect::OverlayGraphicsEffect(QObject *parent) :
    QGraphicsEffect(parent),
    _enabled(false),
    _size(1,1),
    _blurRadius(2.0f),
    _color(0, 0, 0, 80)
{
}

QT_BEGIN_NAMESPACE
  extern Q_WIDGETS_EXPORT void qt_blurImage(QPainter *p, QImage &blurImage, qreal radius, bool quality, bool alphaOnly, int transposed = 0 );
QT_END_NAMESPACE

void OverlayGraphicsEffect::draw(QPainter* painter)
{
    // if nothing to show outside the item, just draw source
    if (!_enabled || _blurRadius + _size.height() <= 0 || _blurRadius + _size.width() <= 0) {
        drawSource(painter);
        return;
    }

    PixmapPadMode mode = QGraphicsEffect::PadToEffectiveBoundingRect;
    QPoint offset;
    QPixmap px = sourcePixmap(Qt::DeviceCoordinates, &offset, mode);

    // return if no source
    if (px.isNull())
        return;

#if 0
    if (FC_LOG_INSTANCE.isEnabled(FC_LOGLEVEL_LOG)) {
        static int count;
        getMainWindow()->showMessage(
                QString::fromLatin1("dock overlay redraw %1").arg(count++));
    }
#endif

    QTransform restoreTransform = painter->worldTransform();
    painter->setWorldTransform(QTransform());

    // Calculate size for the background image
    QImage tmp(px.size(), QImage::Format_ARGB32_Premultiplied);
    tmp.setDevicePixelRatio(px.devicePixelRatioF());
    tmp.fill(0);
    QPainter tmpPainter(&tmp);
    tmpPainter.setCompositionMode(QPainter::CompositionMode_Source);
    if(_size.width() == 0 && _size.height() == 0)
        tmpPainter.drawPixmap(QPoint(0, 0), px);
    else {
        for (int x=-_size.width();x<=_size.width();++x) {
            for (int y=-_size.height();y<=_size.height();++y) {
                if (x || y) {
                    tmpPainter.drawPixmap(QPoint(x, y), px);
                    tmpPainter.setCompositionMode(QPainter::CompositionMode_SourceOver);
                }
            }
        }
    }
    tmpPainter.end();

    // blur the alpha channel
    QImage blurred(tmp.size(), QImage::Format_ARGB32_Premultiplied);
    blurred.setDevicePixelRatio(px.devicePixelRatioF());
    blurred.fill(0);
    QPainter blurPainter(&blurred);
    qt_blurImage(&blurPainter, tmp, blurRadius(), false, true);
    blurPainter.end();

    tmp = blurred;

    // blacken the image...
    tmpPainter.begin(&tmp);
    tmpPainter.setCompositionMode(QPainter::CompositionMode_SourceIn);
    tmpPainter.fillRect(tmp.rect(), color());
    tmpPainter.end();

    // draw the blurred shadow...
    painter->drawImage(QPointF(offset.x()+_offset.x(), offset.y()+_offset.y()), tmp);

    // draw the actual pixmap...
    painter->drawPixmap(offset, px, QRectF());

#if 0
    QWidget *focus = qApp->focusWidget();
    if (focus) {
        QWidget *widget = qobject_cast<QWidget*>(this->parent());
        if (auto *edit = qobject_cast<QPlainTextEdit*>(focus)) {
            if (!edit->isReadOnly() && edit->isEnabled()) {
                for(auto w=edit->parentWidget(); w; w=w->parentWidget()) {
                    if (w == widget) {
                        QRect r = edit->cursorRect();
                        QRect rect(edit->viewport()->mapTo(widget, r.topLeft()), 
                                edit->viewport()->mapTo(widget, r.bottomRight()));
                        // painter->fillRect(rect, edit->textColor());
                        // painter->fillRect(rect, edit->currentCharFormat().foreground());
                        painter->fillRect(rect.translated(offset), Qt::white);
                    }
                }
            }
        }
    }
#endif

    // restore world transform
    painter->setWorldTransform(restoreTransform);
}

QRectF OverlayGraphicsEffect::boundingRectFor(const QRectF& rect) const
{
    if (!_enabled)
        return rect;
    return rect.united(rect.adjusted(-_blurRadius - _size.width() + _offset.x(), 
                                     -_blurRadius - _size.height()+ _offset.y(), 
                                     _blurRadius + _size.width() + _offset.x(),
                                     _blurRadius + _size.height() + _offset.y()));
}

// -----------------------------------------------------------

struct OverlayInfo {
    const char *name;
    OverlayTabWidget *tabWidget;
    Qt::DockWidgetArea dockArea;
    std::unordered_map<QDockWidget*, OverlayInfo*> &overlayMap;
    ParameterGrp::handle hGrp;

    OverlayInfo(QWidget *parent,
                const char *name,
                Qt::DockWidgetArea pos,
                std::unordered_map<QDockWidget*, OverlayInfo*> &map)
        : name(name), dockArea(pos), overlayMap(map)
    {
        tabWidget = new OverlayTabWidget(parent, dockArea);
        tabWidget->setObjectName(QString::fromLatin1(name));
        tabWidget->getProxyWidget()->setObjectName(tabWidget->objectName() + QString::fromLatin1("Proxy"));
        tabWidget->setMovable(true);
        hGrp = App::GetApplication().GetUserParameter().GetGroup("BaseApp")
                            ->GetGroup("MainWindow")->GetGroup("DockWindows")->GetGroup(name);
    }

    bool addWidget(QDockWidget *dock, bool forced=true) {
        if(!dock)
            return false;
        if(tabWidget->dockWidgetIndex(dock) >= 0)
            return false;
        overlayMap[dock] = this;
        bool visible = dock->isVisible();

        auto focus = qApp->focusWidget();
        if(focus && findTabWidget(focus) != tabWidget)
            focus = nullptr;

        tabWidget->addWidget(dock, dock->windowTitle());

        if(focus) {
            tabWidget->setCurrent(dock);
            focus = qApp->focusWidget();
            if(focus)
                focus->clearFocus();
        }

        if(forced) {
            auto mw = getMainWindow();
            for(auto d : mw->findChildren<QDockWidget*>()) {
                if(mw->dockWidgetArea(d) == dockArea
                        && d->toggleViewAction()->isChecked())
                {
                    addWidget(d, false);
                }
            }
            if(visible) {
                dock->show();
                tabWidget->setCurrent(dock);
            }
        } else
            tabWidget->saveTabs();
        return true;
    }

    void removeWidget() {
        if(!tabWidget->count())
            return;

        tabWidget->hide();

        QPointer<QWidget> focus = qApp->focusWidget();

        QDockWidget *lastDock = tabWidget->currentDockWidget();
        if(lastDock)
            tabWidget->removeWidget(lastDock);
        while(tabWidget->count()) {
            QDockWidget *dock = tabWidget->dockWidget(0);
            if(!dock) {
                tabWidget->removeTab(0);
                continue;
            }
            tabWidget->removeWidget(dock, lastDock);
            lastDock = dock;
        }

        if(focus)
            focus->setFocus();

        tabWidget->saveTabs();
    }

    void save()
    {
    }

    void restore()
    {
        tabWidget->restore(hGrp);
        for(int i=0,c=tabWidget->count();i<c;++i) {
            auto dock = tabWidget->dockWidget(i);
            if(dock)
                overlayMap[dock] = this;
        }
    }

};

#endif // FC_HAS_DOCK_OVERLAY

enum OverlayToggleMode {
    OverlayUnset,
    OverlaySet,
    OverlayToggle,
    OverlayToggleAutoHide,
    OverlayToggleTransparent,
    OverlayCheck,
};

class OverlayManager::Private {
public:

#ifdef FC_HAS_DOCK_OVERLAY
    QTimer _timer;

    bool mouseTransparent = false;

    std::unordered_map<QDockWidget*, OverlayInfo*> _overlayMap;
    OverlayInfo _left;
    OverlayInfo _right;
    OverlayInfo _top;
    OverlayInfo _bottom;
    std::array<OverlayInfo*,4> _overlayInfos;
    QCursor _cursor;

    QPoint _lastPos;

    QAction _actClose;
    QAction _actFloat;
    QAction _actOverlay;
    std::array<QAction*, 3> _actions;

    QList<QPointer<View3DInventorViewer> > _3dviews;
    int _trackingView = -1;
    OverlayTabWidget *_trackingOverlay = nullptr;

    bool updateStyle = false;

    Private(OverlayManager *host, QWidget *parent)
        :_left(parent,"OverlayLeft", Qt::LeftDockWidgetArea,_overlayMap)
        ,_right(parent,"OverlayRight", Qt::RightDockWidgetArea,_overlayMap)
        ,_top(parent,"OverlayTop", Qt::TopDockWidgetArea,_overlayMap)
        ,_bottom(parent,"OverlayBottom",Qt::BottomDockWidgetArea,_overlayMap)
        ,_overlayInfos({&_left,&_right,&_top,&_bottom})
        ,_actions({&_actOverlay,&_actFloat,&_actClose})
    {
        _Overlays = {_LeftOverlay, _RightOverlay, _TopOverlay, _BottomOverlay};

        connect(&_timer, SIGNAL(timeout()), host, SLOT(onTimer()));
        _timer.setSingleShot(true);

        connect(qApp, SIGNAL(focusChanged(QWidget*,QWidget*)),
                host, SLOT(onFocusChanged(QWidget*,QWidget*)));

        qApp->installEventFilter(host);

        Application::Instance->signalActivateView.connect([this](const MDIView *) {
            refresh();
        });
        Application::Instance->signalInEdit.connect([this](const ViewProviderDocumentObject &) {
            refresh();
        });
        Application::Instance->signalResetEdit.connect([this](const ViewProviderDocumentObject &) {
            refresh();
        });

        _actOverlay.setIcon(QPixmap(_PixmapOverlay));
        _actOverlay.setData(QString::fromLatin1("OBTN Overlay"));

        const char * const pixmapFloat[]={
            "10 10 2 1",
            ". c None",
            TITLE_BUTTON_COLOR,
            "...#######",
            "...#.....#",
            "...#.....#",
            "#######..#",
            "#.....#..#",
            "#.....#..#",
            "#.....####",
            "#.....#...",
            "#.....#...",
            "#######...",
        };
        _actFloat.setIcon(QPixmap(pixmapFloat));
        _actFloat.setData(QString::fromLatin1("OBTN Float"));

        const char * const pixmapClose[]={
            "10 10 2 1",
            ". c None",
            TITLE_BUTTON_COLOR,
            "##......##",
            "###....###",
            ".###..###.",
            "..######..",
            "...####...",
            "...####...",
            "..######..",
            ".###..###.",
            "###....###",
            "##......##",
        };
        _actClose.setIcon(QPixmap(pixmapClose));
        _actClose.setData(QString::fromLatin1("OBTN Close"));

        retranslate();

        for(auto action : _actions) {
            QObject::connect(action, SIGNAL(triggered(bool)), host, SLOT(onAction()));
        }
        for(auto o : _overlayInfos) {
            for(auto action : o->tabWidget->actions()) {
                QObject::connect(action, SIGNAL(triggered(bool)), host, SLOT(onAction()));
            }
            o->tabWidget->setTitleBar(createTitleBar(o->tabWidget));
        }

        QIcon px = BitmapFactory().pixmap("cursor-through");
        _cursor = QCursor(px.pixmap(32,32), 10, 9);
    }

    void setMouseTransparent(bool enabled)
    {
        if (mouseTransparent == enabled)
            return;
        mouseTransparent = enabled;
        for (OverlayTabWidget *tabWidget : _Overlays) {
            tabWidget->getProxyWidget()->setAttribute(
                    Qt::WA_TransparentForMouseEvents, enabled);
            tabWidget->setAttribute(
                    Qt::WA_TransparentForMouseEvents, enabled);
        }
        if(!enabled)
            qApp->setOverrideCursor(QCursor());
        else
            qApp->setOverrideCursor(_cursor);
    }

    bool toggleOverlay(QDockWidget *dock, int toggle, int dockPos=Qt::NoDockWidgetArea)
    {
        if(!dock)
            return false;

        auto it = _overlayMap.find(dock);
        if(it != _overlayMap.end()) {
            auto o = it->second;
            switch(toggle) {
            case OverlayToggleAutoHide:
                o->tabWidget->setAutoHide(!o->tabWidget->isAutoHide());
                break;
            case OverlayToggleTransparent:
                o->tabWidget->setTransparent(!o->tabWidget->isTransparent());
                break;
            case OverlayUnset:
            case OverlayToggle:
                _overlayMap.erase(it);
                o->removeWidget();
                return false;
            default:
                break;
            }
            return true;
        }

        if(toggle == OverlayUnset)
            return false;

        if(dockPos == Qt::NoDockWidgetArea)
            dockPos = getMainWindow()->dockWidgetArea(dock);
        OverlayInfo *o;
        switch(dockPos) {
        case Qt::LeftDockWidgetArea:
            o = &_left;
            break;
        case Qt::RightDockWidgetArea:
            o = &_right;
            break;
        case Qt::TopDockWidgetArea:
            o = &_top;
            break;
        case Qt::BottomDockWidgetArea:
            o = &_bottom;
            break;
        default:
            return false;
        }
        if(toggle == OverlayCheck && !o->tabWidget->count())
            return false;
        if(o->addWidget(dock)) {
            if(toggle == OverlayToggleAutoHide)
                o->tabWidget->setAutoHide(true);
            else if(toggle == OverlayToggleTransparent)
                o->tabWidget->setTransparent(true);
        }
        refresh();
        return true;
    }

    void refresh(QWidget *widget=nullptr, bool refreshStyle=false)
    {
        if(refreshStyle) {
            OverlayStyleSheet::instance()->update();
            updateStyle = true;
        }

        if(widget) {
            auto tabWidget = findTabWidget(widget);
            if(tabWidget && tabWidget->count()) {
                for(auto o : _overlayInfos) {
                    if(tabWidget == o->tabWidget) {
                        tabWidget->touch();
                        onTimer();
                        return;
                    }
                }
            }
        }
        _timer.start(ViewParams::getDockOverlayDelay());
    }

    void save()
    {
        _left.save();
        _right.save();
        _top.save();
        _bottom.save();
    }

    void restore()
    {
        _left.restore();
        _right.restore();
        _top.restore();
        _bottom.restore();
        refresh();
    }

    void onTimer()
    {
        auto mdi = getMainWindow() ? getMainWindow()->getMdiArea() : nullptr;
        if(!mdi)
            return;

        auto focus = findTabWidget(qApp->focusWidget());
        if (focus && !focus->getSplitter()->isVisible())
            focus = nullptr;
        auto active = findTabWidget(qApp->widgetAt(QCursor::pos()));
        if (active && !active->getSplitter()->isVisible())
            active = nullptr;
        OverlayTabWidget *reveal = nullptr;

        bool updateFocus = false;
        bool updateActive = false;

        for(auto o : _overlayInfos) {
            if(o->tabWidget->isTouched() || updateStyle) {
                if(o->tabWidget == focus)
                    updateFocus = true;
                else if(o->tabWidget == active)
                    updateActive = true;
                else 
                    o->tabWidget->setOverlayMode(true);
            }
            if(!o->tabWidget->getRevealTime().isNull()) {
                if(o->tabWidget->getRevealTime()<= QTime::currentTime())
                    o->tabWidget->setRevealTime(QTime());
                else
                    reveal = o->tabWidget;
            }
        }
        updateStyle = false;

        if(focus && (focus->isOverlayed() || updateFocus)) {
            focus->setOverlayMode(false);
            focus->raise();
            if(reveal == focus)
                reveal = nullptr;
        }

        if(active) {
            if(active != focus && (active->isOverlayed() || updateActive)) 
                active->setOverlayMode(false);
            active->raise();
            if(reveal == active)
                reveal = nullptr;
        }

        if(reveal) {
            reveal->setOverlayMode(false);
            reveal->raise();
        }

        for(auto o : _overlayInfos) {
            if(o->tabWidget != focus 
                    && o->tabWidget != active
                    && o->tabWidget != reveal
                    && o->tabWidget->count()
                    && !o->tabWidget->isOverlayed())
            {
                o->tabWidget->setOverlayMode(true);
            }
        }

        int w = mdi->geometry().width();
        int h = mdi->geometry().height();
        auto tabbar = mdi->findChild<QTabBar*>();
        if(tabbar)
            h -= tabbar->height();

        int naviCubeSize = ViewParams::getNaviWidgetSize()+10;
        int naviCorner = ViewParams::getDockOverlayCheckNaviCube() ?
            ViewParams::getCornerNaviCube() : -1;

        QRect rect;
        QRect rectBottom(0,0,0,0);
        if(_bottom.tabWidget->count()) {
            rect = _bottom.tabWidget->getRect();

            QSize ofs = _bottom.tabWidget->getOffset();
            int delta = _bottom.tabWidget->getSizeDelta();
            h -= ofs.height();
            if(naviCorner == 2)
                ofs.setWidth(ofs.width()+naviCubeSize);
            int bw = w-10-ofs.width()-delta;
            if(naviCorner == 3)
                bw -= naviCubeSize;
            if(bw < 10)
                bw = 10;

            // Bottom width is maintain the same to reduce QTextEdit re-layout
            // which may be expensive if there are lots of text, e.g. for
            // ReportView or PythonConsole.
            _bottom.tabWidget->setRect(QRect(ofs.width(),h-rect.height(),bw,rect.height()));

            if (_bottom.tabWidget->isVisible() 
                    && _bottom.tabWidget->getState() == OverlayTabWidget::State_Normal)
                rectBottom = _bottom.tabWidget->getRect();
        }

        QRect rectLeft(0,0,0,0);
        if(_left.tabWidget->count()) {
            rect = _left.tabWidget->getRect();

            auto ofs = _left.tabWidget->getOffset();
            if(naviCorner == 0)
                ofs.setWidth(ofs.width()+naviCubeSize);
            int delta = _left.tabWidget->getSizeDelta()+rectBottom.height();
            if(naviCorner == 2 && naviCubeSize > rectBottom.height())
                delta += naviCubeSize - rectBottom.height();
            int lh = std::max(h-ofs.width()-delta, 10);

            _left.tabWidget->setRect(QRect(ofs.height(),ofs.width(),rect.width(),lh));

            if (_left.tabWidget->isVisible() 
                    && _left.tabWidget->getState() == OverlayTabWidget::State_Normal)
                rectLeft = _left.tabWidget->getRect();
        }

        QRect rectRight(0,0,0,0);
        if(_right.tabWidget->count()) {
            rect = _right.tabWidget->getRect();

            auto ofs = _right.tabWidget->getOffset();
            if(naviCorner == 1)
                ofs.setWidth(ofs.width()+naviCubeSize);
            int delta = _right.tabWidget->getSizeDelta()+rectBottom.height();
            if(naviCorner == 3 && naviCubeSize > rectBottom.height())
                delta += naviCubeSize - rectBottom.height();
            int rh = std::max(h-ofs.width()-delta, 10);
            w -= ofs.height();

            _right.tabWidget->setRect(QRect(w-rect.width(),ofs.width(),rect.width(),rh));

            if (_right.tabWidget->isVisible() 
                    && _right.tabWidget->getState() == OverlayTabWidget::State_Normal)
                rectRight = _right.tabWidget->getRect();
        }

        if(_top.tabWidget->count()) {
            rect = _top.tabWidget->getRect();

            auto ofs = _top.tabWidget->getOffset();
            int delta = _top.tabWidget->getSizeDelta();
            if(naviCorner == 0)
                rectLeft.setWidth(std::max(rectLeft.width(), naviCubeSize));
            else if(naviCorner == 1)
                rectRight.setWidth(std::max(rectRight.width(), naviCubeSize));
            int tw = w-rectLeft.width()-rectRight.width()-ofs.width()-delta;

            _top.tabWidget->setRect(QRect(rectLeft.width()-ofs.width(),ofs.height(),tw,rect.height()));
        }
    }

    void setOverlayMode(OverlayMode mode)
    {
        switch(mode) {
        case OverlayManager::DisableAll:
        case OverlayManager::EnableAll: {
            auto docks = getMainWindow()->findChildren<QDockWidget*>();
            // put visible dock widget first
            std::sort(docks.begin(),docks.end(),
                [](const QDockWidget *a, const QDockWidget *) {
                    return !a->visibleRegion().isEmpty();
                });
            for(auto dock : docks) {
                if(mode == OverlayManager::DisableAll)
                    toggleOverlay(dock, OverlayUnset);
                else
                    toggleOverlay(dock, OverlaySet);
            }
            return;
        }
        case OverlayManager::ToggleAll:
            for(auto o : _overlayInfos) {
                if(o->tabWidget->count()) {
                    setOverlayMode(OverlayManager::DisableAll);
                    return;
                }
            }
            setOverlayMode(OverlayManager::EnableAll);
            return;
        case OverlayManager::AutoHideAll: {
            bool found = false;
            for(auto o : _overlayInfos) {
                if(o->tabWidget->count())
                    found = true;
            }
            if(!found)
                setOverlayMode(OverlayManager::EnableAll);
        }
        // fall through
        case OverlayManager::AutoHideNone:
            for(auto o : _overlayInfos)
                o->tabWidget->setAutoHide(mode == OverlayManager::AutoHideAll);
            refresh();
            return;
        case OverlayManager::ToggleAutoHideAll:
            for(auto o : _overlayInfos) {
                if(o->tabWidget->count() && o->tabWidget->isAutoHide()) {
                    setOverlayMode(OverlayManager::AutoHideNone);
                    return;
                }
            }
            setOverlayMode(OverlayManager::AutoHideAll);
            return;
        case OverlayManager::TransparentAll: {
            bool found = false;
            for(auto o : _overlayInfos) {
                if(o->tabWidget->count())
                    found = true;
            }
            if(!found)
                setOverlayMode(OverlayManager::EnableAll);
        }
        // fall through
        case OverlayManager::TransparentNone:
            for(auto o : _overlayInfos)
                o->tabWidget->setTransparent(mode == OverlayManager::TransparentAll);
            refresh();
            return;
        case OverlayManager::ToggleTransparentAll:
            for(auto o : _overlayInfos) {
                if(o->tabWidget->count() && o->tabWidget->isTransparent()) {
                    setOverlayMode(OverlayManager::TransparentNone);
                    return;
                }
            }
            setOverlayMode(OverlayManager::TransparentAll);
            return;
        default:
            break;
        }

        OverlayToggleMode m;
        QDockWidget *dock = nullptr;
        for(auto w=qApp->widgetAt(QCursor::pos()); w; w=w->parentWidget()) {
            dock = qobject_cast<QDockWidget*>(w);
            if(dock)
                break;
            auto tabWidget = qobject_cast<OverlayTabWidget*>(w);
            if(tabWidget) {
                dock = tabWidget->currentDockWidget();
                if(dock)
                    break;
            }
        }
        if(!dock) {
            for(auto w=qApp->focusWidget(); w; w=w->parentWidget()) {
                dock = qobject_cast<QDockWidget*>(w);
                if(dock)
                    break;
            }
        }

        switch(mode) {
        case OverlayManager::ToggleActive:
            m = OverlayToggle;
            break;
        case OverlayManager::ToggleAutoHide:
            m = OverlayToggleAutoHide;
            break;
        case OverlayManager::ToggleTransparent:
            m = OverlayToggleTransparent;
            break;
        case OverlayManager::EnableActive:
            m = OverlaySet;
            break;
        case OverlayManager::DisableActive:
            m = OverlayUnset;
            break;
        default:
            return;
        }
        toggleOverlay(dock, m);
    }

    void onToggleDockWidget(QDockWidget *dock, int checked)
    {
        if(!dock)
            return;

        auto it = _overlayMap.find(dock);
        if(it == _overlayMap.end()) {
            if(!checked)
                return;
            toggleOverlay(dock, OverlayCheck);
            it = _overlayMap.find(dock);
            if(it == _overlayMap.end())
                return;
        }
        OverlayTabWidget *tabWidget = it->second->tabWidget;
        if(checked) {
            int index = tabWidget->dockWidgetIndex(dock);
            if(index >= 0) {
                auto sizes = tabWidget->getSplitter()->sizes();
                if(index >= sizes.size() || sizes[index]==0) {
                    if (checked > 0) {
                        tabWidget->setCurrent(dock);
                        tabWidget->onCurrentChanged(tabWidget->dockWidgetIndex(dock));
                    }
                } else if (checked < 0) {
                    if (sizes[index] > 0 && sizes.size() > 1) {
                        bool expand = true;
                        for (int i=0; i<sizes.size(); ++i) {
                            if (i != index && sizes[i] > 0) {
                                expand = false;
                                break;
                            }
                        }
                        if (expand) {
                            int next = index == sizes.size()-1 ? 0 : index+1;
                            tabWidget->setCurrentIndex(next);
                            tabWidget->onCurrentChanged(next);
                        }
                    }
                } else if (sizes[index] == 0) {
                    tabWidget->setCurrent(dock);
                    tabWidget->onCurrentChanged(tabWidget->dockWidgetIndex(dock));
                }
            }
            if (checked > 0)
                tabWidget->setRevealTime(QTime::currentTime().addMSecs(
                        ViewParams::getDockOverlayRevealDelay()));
            refresh();
        } else if (checked < 0) {
            refresh();
        } else {
            tabWidget->removeWidget(dock);
            getMainWindow()->addDockWidget(it->second->dockArea, dock);
            _overlayMap.erase(it);
        }
    }

    void changeOverlaySize(int changes)
    {
        auto tabWidget = findTabWidget(qApp->widgetAt(QCursor::pos()));
        if(tabWidget) {
            tabWidget->changeSize(changes, false);
            refresh();
        }
    }

    void onFocusChanged(QWidget *, QWidget *) {
        refresh();
    }

    void setupTitleBar(QDockWidget *dock)
    {
        if(!dock->titleBarWidget())
            dock->setTitleBarWidget(createTitleBar(dock));
    }

    QWidget *createTitleBar(QWidget *parent)
    {
        OverlayTitleBar *widget = new OverlayTitleBar(parent);
        widget->setObjectName(QLatin1String("OverlayTitle"));
        bool vertical = false;
        QBoxLayout *layout = nullptr;
        auto tabWidget = qobject_cast<OverlayTabWidget*>(parent);
        if(!tabWidget) {
            layout = new QBoxLayout(QBoxLayout::LeftToRight, widget); 
        } else {
            switch(tabWidget->getDockArea()) {
            case Qt::LeftDockWidgetArea:
                layout = new QBoxLayout(QBoxLayout::LeftToRight, widget); 
                break;
            case Qt::RightDockWidgetArea:
                layout = new QBoxLayout(QBoxLayout::RightToLeft, widget); 
                break;
            case Qt::TopDockWidgetArea:
                layout = new QBoxLayout(QBoxLayout::TopToBottom, widget); 
                vertical = true;
                break;
            case Qt::BottomDockWidgetArea:
                layout = new QBoxLayout(QBoxLayout::BottomToTop, widget); 
                vertical = true;
                break;
            default:
                break;
            }
        }
        layout->addSpacing(5);
        layout->setContentsMargins(1,1,1,1);
        if(tabWidget) {
            for(auto action : tabWidget->actions())
                layout->addWidget(createTitleButton(action));
        } else {
            for(auto action : _actions)
                layout->addWidget(createTitleButton(action));
        }
        // layout->addStretch(2);
        layout->addSpacerItem(new QSpacerItem(_TitleButtonSize,_TitleButtonSize,
                    vertical?QSizePolicy::Minimum:QSizePolicy::Expanding,
                    vertical?QSizePolicy::Expanding:QSizePolicy::Minimum));

        if (tabWidget) {
            auto grip = new OverlaySizeGrip(tabWidget, !vertical);
            if (vertical) {
                grip->setFixedHeight(6);
                grip->setMinimumWidth(_TitleButtonSize);
            } else {
                grip->setFixedWidth(6);
                grip->setMinimumHeight(_TitleButtonSize);
            }
            QObject::connect(grip, SIGNAL(dragMove(QPoint)),
                    tabWidget, SLOT(onSizeGripMove(QPoint)));
            layout->addWidget(grip);
            grip->raise();
        }
        return widget;
    }

    QWidget *createTitleButton(QAction *action)
    {
        auto button = new OverlayToolButton(nullptr);
        button->setObjectName(action->data().toString());
        button->setDefaultAction(action);
        button->setAutoRaise(true);
        button->setContentsMargins(0,0,0,0);
        button->setFixedSize(_TitleButtonSize,_TitleButtonSize);
        return button;
    }

    void onAction(QAction *action) {
        if(action == &_actOverlay) {
            OverlayManager::instance()->setOverlayMode(OverlayManager::ToggleActive);
        } else if(action == &_actFloat || action == &_actClose) {
            for(auto w=qApp->widgetAt(QCursor::pos());w;w=w->parentWidget()) {
                auto dock = qobject_cast<QDockWidget*>(w);
                if(!dock)
                    continue;
                if(action == &_actClose) {
                    dock->toggleViewAction()->activate(QAction::Trigger);
                } else {
                    auto it = _overlayMap.find(dock);
                    if(it != _overlayMap.end()) {
                        it->second->tabWidget->removeWidget(dock);
                        getMainWindow()->addDockWidget(it->second->dockArea, dock);
                        _overlayMap.erase(it);
                        dock->show();
                        dock->setFloating(true);
                        refresh();
                    } else 
                        dock->setFloating(!dock->isFloating());
                }
                return;
            }
        } else {
            auto tabWidget = qobject_cast<OverlayTabWidget*>(action->parent());
            if(tabWidget)
                tabWidget->onAction(action);
        }
    }

    void retranslate()
    {
        _actOverlay.setToolTip(QObject::tr("Toggle overlay"));
        _actFloat.setToolTip(QObject::tr("Toggle floating window"));
        _actClose.setToolTip(QObject::tr("Close dock window"));
    }

    void dropDockWidget(const QPoint &pos,
                        QWidget *srcWidget,
                        OverlayTabWidget *dst,
                        int dropIndex)
    {
        QDockWidget *dock = qobject_cast<QDockWidget*>(srcWidget);
        OverlayTabWidget *src = nullptr;

        int index = -1;
        if (!dock) {
            src = qobject_cast<OverlayTabWidget*>(srcWidget);
            if (!src)
                return;
            for(int size : src->getSplitter()->sizes()) {
                ++index;
                if (size) {
                    dock = src->dockWidget(index);
                    break;
                }
            }
            if (!dock)
                return;
        }

        if (src && src == dst && dropIndex != -2){
            auto splitter = src->getSplitter();
            if (dropIndex == -1) {
                src->tabBar()->moveTab(index, 0);
                src->setCurrentIndex(0);
                src->onCurrentChanged(0);
            }
            else if (index != dropIndex) {
                auto sizes = splitter->sizes();
                src->tabBar()->moveTab(index, dropIndex);
                splitter->setSizes(sizes);
            }
            return;
        }

        if (src) {
            _overlayMap.erase(dock);
            src->removeWidget(dock);
        }

        if (!dst) {
            QDockWidget *lastDock = nullptr;
            for(auto dockWidget : getMainWindow()->findChildren<QDockWidget*>()) {
                if (!dockWidget->isVisible())
                    continue;
                if (dockWidget->rect().contains(dockWidget->mapFromGlobal(pos))) {
                    lastDock = dockWidget;
                    break;
                }
            }
            if (lastDock) {
                if (lastDock != dock) {
                    dock->setFloating(false);
                    getMainWindow()->tabifyDockWidget(lastDock, dock);
                }
            }
            else {
                dock->setFloating(true);
                if(_DragFrame)
                    dock->setGeometry(QRect(_DragFrame->mapToGlobal(QPoint()),
                                            _DragFrame->size()));
            }
            dock->show();
        }
        else if (dropIndex == -2) {
            getMainWindow()->addDockWidget(dst->getDockArea(), dock);
            dock->setFloating(false);
        }
        else {
            for (auto o : _overlayInfos) {
                if (o->tabWidget == dst) {
                    o->addWidget(dock, false);
                    break;
                }
            }
            index = dst->dockWidgetIndex(dock);
            if (index >= 0) {
                if (dropIndex < 0) {
                    dst->tabBar()->moveTab(index, 0);
                    dst->setCurrentIndex(0);
                    dst->onCurrentChanged(0);
                }
                else {
                    dst->tabBar()->moveTab(index, dropIndex);
                    dst->setCurrentIndex(dropIndex);
                    dst->onCurrentChanged(dropIndex);
                }
                dst->setRevealTime(QTime::currentTime().addMSecs(
                            ViewParams::getDockOverlayRevealDelay()));
            }
        }

        refresh();
    }

#else // FC_HAS_DOCK_OVERLAY

    Private(OverlayManager *, QWidget *) {}
    void refresh(QWidget *, bool) {}
    void setMouseTransparent(bool) {}
    void save() {}
    void restore() {}
    void onTimer() {}
    void setOverlayMode(OverlayMode) {}
    void onToggleDockWidget(QDockWidget *, bool) {}
    void changeOverlaySize(int) {}
    void onFocusChanged(QWidget *, QWidget *) {}
    void onAction(QAction *) {}
    void setupTitleBar(QDockWidget *) {}
    void retranslate() {}
    void dropDockWidget(OverlayTabWidget *, OverlayTabWidget *, int) {}

    bool toggleOverlay(QDockWidget *,
                       OverlayToggleMode,
                       Qt::DockWidgetArea dockPos=Qt::NoDockWidgetArea)
    {
        (void)dockPos;
        return false;
    }

#endif // FC_HAS_DOCK_OVERLAY
};


static OverlayManager * _instance;

OverlayManager* OverlayManager::instance()
{
    if ( _instance == 0 )
        _instance = new OverlayManager;
    return _instance;
}

void OverlayManager::destruct()
{
    delete _instance;
    _instance = 0;
}

OverlayManager::OverlayManager()
{
    auto mdi = getMainWindow()->getMdiArea();
    assert(mdi);
    d = new Private(this, mdi);
}

OverlayManager::~OverlayManager()
{
    delete d;
}

void OverlayManager::setOverlayMode(OverlayMode mode)
{
    d->setOverlayMode(mode);
}


void OverlayManager::initDockWidget(QDockWidget *dw, QWidget *widget)
{
#ifdef FC_HAS_DOCK_OVERLAY
    connect(dw->toggleViewAction(), SIGNAL(triggered(bool)), this, SLOT(onToggleDockWidget(bool)));
    connect(widget, SIGNAL(windowTitleChanged(QString)), this, SLOT(onDockWidgetTitleChange(QString)));
#endif
}

void OverlayManager::setupDockWidget(QDockWidget *dw, int dockArea)
{
    d->toggleOverlay(dw, OverlayCheck, dockArea);
}

void OverlayManager::unsetupDockWidget(QDockWidget *dw)
{
    d->toggleOverlay(dw, OverlayUnset);
}

void OverlayManager::onToggleDockWidget(bool checked)
{
    auto action = qobject_cast<QAction*>(sender());
    if(!action)
        return;
    d->onToggleDockWidget(qobject_cast<QDockWidget*>(action->parent()), checked?1:0);
}

void OverlayManager::onTaskViewUpdate()
{
#ifdef FC_HAS_DOCK_OVERLAY
    auto taskview = qobject_cast<TaskView::TaskView*>(sender());
    if (!taskview)
        return;
    QDockWidget *dock = nullptr;
    DockWnd::ComboView *comboview = nullptr;
    for (QWidget *w=taskview; w; w=w->parentWidget()) {
        if ((dock = qobject_cast<QDockWidget*>(w)))
            break;
        if (!comboview)
            comboview = qobject_cast<DockWnd::ComboView*>(w);
    }
    if (dock) {
        if (comboview && comboview->hasTreeView())
            refresh();
        else
            d->onToggleDockWidget(dock, taskview->isEmpty() ? -1 : 1);
    }
#endif
}

void OverlayManager::onDockWidgetTitleChange(const QString &title)
{
    if (title.isEmpty())
        return;
#ifdef FC_HAS_DOCK_OVERLAY
    auto widget = qobject_cast<QWidget*>(sender());
    QDockWidget *dock = nullptr;
    for (QWidget *w=widget; w; w=w->parentWidget()) {
        if ((dock = qobject_cast<QDockWidget*>(w)))
            break;
    }
    if(!dock)
        return;
    auto tabWidget = findTabWidget(dock);
    if (!tabWidget)
        return;
    int index = tabWidget->dockWidgetIndex(dock);
    if (index >= 0)
        tabWidget->setTabText(index, title);
#endif
}

void OverlayManager::retranslate()
{
    d->retranslate();
}

void OverlayManager::onTimer()
{
    d->onTimer();
}

bool OverlayManager::eventFilter(QObject *o, QEvent *ev)
{
    switch(ev->type()) {
    case QEvent::Resize: {
        if(getMainWindow() && o == getMainWindow()->getMdiArea())
            refresh();
        return false;
    }
#ifdef FC_HAS_DOCK_OVERLAY
    case QEvent::KeyPress: {
        QKeyEvent *ke = static_cast<QKeyEvent*>(ev);
        bool accepted = false;
        if (ke->modifiers() == Qt::NoModifier && ke->key() == Qt::Key_Escape) {
            if (d->mouseTransparent) {
                d->setMouseTransparent(false);
                accepted = true;
            } else {
                for (OverlayTabWidget *tabWidget : _Overlays) {
                    if (tabWidget->getState() == OverlayTabWidget::State_Hint) {
                        tabWidget->setState(OverlayTabWidget::State_HintHidden);
                        accepted = true;
                    }
                }
            }
        }
        if (accepted) {
            ke->accept();
            return true;
        }
        break;
    }
    case QEvent::Paint:
        if (auto widget = qobject_cast<QWidget*>(o)) {
            // QAbstractItemView optimize redraw using its item delegate's
            // visualRect(). However, if we are using QGraphicsEffects, the
            // effect may touch areas outside of visualRect(), so
            // OverlayTabWidget offers a timer for a delayed redraw.
            widget = qobject_cast<QAbstractItemView*>(widget->parentWidget());
            if(widget) {
                auto tabWidget = findTabWidget(widget, true);
                if (tabWidget)
                    tabWidget->scheduleRepaint();
            }
        }
        break;
    // case QEvent::MouseButtonDblClick:
    // case QEvent::NativeGesture:
    case QEvent::MouseButtonRelease:
    case QEvent::MouseButtonPress:
    case QEvent::MouseMove:
    case QEvent::Wheel:
    case QEvent::ContextMenu: {
        QWidget *grabber = QWidget::mouseGrabber();
        if (d->mouseTransparent || (grabber && grabber != d->_trackingOverlay))
            return false;
        if (d->_trackingView >= 0) {
            View3DInventorViewer *view = nullptr;
            if(!TreeWidget::isDragging() && d->_trackingView < d->_3dviews.size())
                view = d->_3dviews[d->_trackingView];
            if(view)
                view->callEventFilter(ev);
            if(!view || ev->type() == QEvent::MouseButtonRelease
                     || QApplication::mouseButtons() == Qt::NoButton)
            {
                d->_trackingView = -1;
                if (d->_trackingOverlay == grabber
                        && ev->type() == QEvent::MouseButtonRelease)
                {
                    d->_trackingOverlay = nullptr;
                    // Must not release mouse here, because otherwise the event
                    // will find its way to the actual widget under cursor.
                    // Instead, return false here to let OverlayTabWidget::event()
                    // release the mouse.
                    return false;
                }
                if(d->_trackingOverlay && grabber == d->_trackingOverlay)
                    d->_trackingOverlay->releaseMouse();
                d->_trackingOverlay = nullptr;
            }
            // Must return true here to filter the event, otherwise ContextMenu
            // event may be routed to the actual widget. Other types of event
            // probably do not matter.
            return true;
        } else if (ev->type() != QEvent::MouseButtonPress 
                && QApplication::mouseButtons()!=Qt::NoButton)
            return false;

        if(TreeWidget::isDragging())
            return false;

        int hit = 0;
        QPoint pos = QCursor::pos();
        if ((ViewParams::getDockOverlayAutoMouseThrough()
                    && ev->type() != QEvent::Wheel
                    && pos == d->_lastPos)
                || (ViewParams::getDockOverlayMouseThrough()
                    && (QApplication::queryKeyboardModifiers() & Qt::AltModifier)))
        {
            hit = 1;
        } else if (ev->type() != QEvent::Wheel) {
            for(auto widget=qApp->widgetAt(pos); widget ; widget=widget->parentWidget()) {
                int type = widget->windowType();
                if (type != Qt::Widget && type != Qt::Window) {
                    if (type != Qt::SubWindow)
                        hit = -1;
                    break;
                }
                if (qobject_cast<QAbstractButton*>(widget))
                    break;
                auto tabWidget = qobject_cast<OverlayTabWidget*>(widget);
                if (tabWidget) {
                    if (tabWidget->testAlpha(pos) == 0) {
                        hit = ViewParams::getDockOverlayAutoMouseThrough();
                        d->_lastPos = pos;
                    }
                    break;
                }
            }
        }
        if (hit == 0) {
            for (OverlayTabWidget *tabWidget : _Overlays)
                tabWidget->getProxyWidget()->hitTest(pos);
        }
        if (hit <= 0) {
            d->_lastPos.setX(INT_MAX);
            d->_3dviews.clear();
            return false;
        }
        if(d->_3dviews.isEmpty()) {
            for(auto w : getMainWindow()->windows(QMdiArea::StackingOrder)) {
                if(!w->isVisible())
                    continue;
                // It is possible to support mouse through for all MDIView.
                // But then we would have to copy all types of intercepted
                // event and manually map the local position inside. For
                // View3DInventorViewer, we use its backdoor function
                // callEventFilter() to pass event directly to
                // Quarter::EventFilter, and subsequently to
                // Quarter::Mouse, which we have modified to use the global
                // position of the event instead of local one.
                for(auto view : w->findChildren<View3DInventorViewer*>()) {
                    if(view->isVisible())
                        d->_3dviews.insert(0,view);
                }
            }
            if(d->_3dviews.isEmpty())
                return false;
        }

        auto widget = qobject_cast<QWidget*>(o);
        if(!widget) {
            QWindow* window = qobject_cast<QWindow*>(o);
            if (window) {
                widget = QWidget::find(window->winId());
                if (!widget)
                    return false;
            }
        }
        auto tabWidget = findTabWidget(widget, true);
        if(!tabWidget || tabWidget->isOverlayed() || !tabWidget->isTransparent())
            return false;
        if(o != tabWidget) {
            ev->ignore();
            return true;
        }
        ev->accept();
        int i = -1;
        for(auto &view : d->_3dviews) {
            ++i;
            if(!view || !view->isVisible())
                continue;
            auto p = view->mapFromGlobal(pos);
            if(p.x()<0 || p.y()<0 || p.x()>view->width() || p.y()>view->height())
                continue;

            // We could have used sendEvent() here, but it won't work for Wheel
            // event. It is (probably) filtered out by some unknown Qt event
            // filter if the target widget is not under focus.  Calling
            // setFocus() here does not seem to have any effect. So we have to
            // use some kind of backdoor here.
            view->callEventFilter(ev);

            if (ev->type() == QEvent::MouseButtonPress) {
                d->_trackingView = i;
                d->_trackingOverlay = tabWidget;
                d->_trackingOverlay->grabMouse();
            }
            break;
        }
        break;
    }
#endif
    default:
        break;
    }
    return false;
}

void OverlayManager::refresh(QWidget *widget, bool refreshStyle)
{
    d->refresh(widget, refreshStyle);
}

void OverlayManager::setMouseTransparent(bool enabled)
{
    d->setMouseTransparent(enabled);
}

bool OverlayManager::isMouseTransparent() const
{
    return d->mouseTransparent;
}

bool OverlayManager::isUnderOverlay() const
{
#ifdef FC_HAS_DOCK_OVERLAY
    return ViewParams::getDockOverlayAutoMouseThrough()
        && findTabWidget(qApp->widgetAt(QCursor::pos()), true);
#else
    return false;
#endif
}

void OverlayManager::save()
{
    d->save();
}

void OverlayManager::restore()
{
    d->restore();

    if (Control().taskPanel())
        connect(Control().taskPanel(), SIGNAL(taskUpdate()), this, SLOT(onTaskViewUpdate()));
}

void OverlayManager::changeOverlaySize(int changes)
{
    d->changeOverlaySize(changes);
}

void OverlayManager::onFocusChanged(QWidget *old, QWidget *now)
{
    d->onFocusChanged(old, now);
}

void OverlayManager::setupTitleBar(QDockWidget *dock)
{
    d->setupTitleBar(dock);
}

void OverlayManager::onAction()
{
    QAction *action = qobject_cast<QAction*>(sender());
    if(action)
        d->onAction(action);
}

void OverlayManager::dropDockWidget(const QPoint &pos,
                                    QWidget *src,
                                    OverlayTabWidget *dst,
                                    int index)
{
    d->dropDockWidget(pos, src, dst, index);
}

#include "moc_OverlayWidgets.cpp"