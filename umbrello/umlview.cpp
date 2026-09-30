/*
    SPDX-License-Identifier: GPL-2.0-or-later
    SPDX-FileCopyrightText: 2002-2022 Umbrello UML Modeller Authors <umbrello-devel@kde.org>
*/

// own header
#include "umlview.h"

// application specific includes
#include "debug_utils.h"
#include "docwindow.h"
#include "model_utils.h"
#include "notewidget.h"
#include "umlapp.h"
#include "umldoc.h"
#include "umldragdata.h"
#include "umlscene.h"
#include "umlviewdialog.h"
#include "umlwidget.h"

#include <QApplication>
#include <QContextMenuEvent>
#include <QPointer>
#include <QScrollBar>

DEBUG_REGISTER(UMLView)

/**
 * Constructor.
 */
UMLView::UMLView(UMLFolder *parentFolder)
  : QGraphicsView(UMLApp::app()->mainViewWidget()),
    m_handPanning(false),
    m_handPanMoved(false),
    m_openContextMenuOnRelease(false),
    m_handPanScroll()
{
    setAcceptDrops(true);
    setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing | QPainter::SmoothPixmapTransform);
    setDragMode(NoDrag); //:TODO: RubberBandDrag);
    setViewportUpdateMode(QGraphicsView::BoundingRectViewportUpdate);
    setOptimizationFlags(QGraphicsView::DontSavePainterState | QGraphicsView::DontAdjustForAntialiasing);
    setCacheMode(QGraphicsView::CacheBackground);
    setScene(new UMLScene(parentFolder, this));
    setBackgroundBrush(Qt::black);
    setResizeAnchor(AnchorUnderMouse);
    setTransformationAnchor(AnchorUnderMouse);
}

/**
 * Destructor.
 */
UMLView::~UMLView()
{
    delete umlScene();
}

/**
 * Getter for the uml scene.
 */
UMLScene* UMLView::umlScene() const
{
    return static_cast<UMLScene*>(scene());
}

/**
 * Returns the zoom of the diagram.
 */
qreal UMLView::zoom() const
{
    return transform().m11()*100.0;
}

/**
 * Sets the zoom of the diagram.
 */
void UMLView::setZoom(qreal zoom)
{
    if (zoom < 10) {
        zoom = 10;
    } else if (zoom > 500) {
        zoom = 500;
    }

    QPointF keepCenter;
    const bool haveViewport = viewport() && viewport()->width() > 0 && viewport()->height() > 0;
    if (haveViewport)
        keepCenter = mapToScene(viewport()->rect().center());

    logDebug1("UMLView::setZoom %1", zoom);
    QTransform wm;
    wm.scale(zoom / 100.0, zoom / 100.0);
    setTransform(wm);
    if (haveViewport)
        centerOn(keepCenter);
}

void UMLView::fitSceneRect(const QRectF &sceneRect, qreal minZoom, const QPointF *preferCenter)
{
    if (!viewport() || !sceneRect.isValid() || sceneRect.isEmpty())
        return;

    const QSize vp = viewport()->size();
    if (vp.width() < 8 || vp.height() < 8)
        return;

    const qreal pad = qMax(48.0, qMax(sceneRect.width(), sceneRect.height()) * 0.08);
    const QRectF padded = sceneRect.normalized().adjusted(-pad, -pad, pad, pad);
    if (padded.width() < 1.0 || padded.height() < 1.0)
        return;

    qreal z = qMin(100.0 * vp.width() / padded.width(),
                   100.0 * vp.height() / padded.height());
    z *= 0.96;
    const bool clamped = (z < minZoom);
    if (z < minZoom)
        z = minZoom;
    else if (z > 500.0)
        z = 500.0;

    setZoom(z);
    if (clamped && preferCenter)
        centerOn(*preferCenter);
    else
        centerOn(padded.center());
    UMLApp::app()->setZoom(qRound(z), false);
}

/**
 * Shows the properties dialog for the view.
 */
bool UMLView::showPropertiesDialog(QWidget *parent)
{
    QPointer<UMLViewDialog> dlg = new UMLViewDialog(parent, umlScene());
    bool success = dlg->exec() == QDialog::Accepted;
    delete dlg;
    return success;
}

void UMLView::zoomIn()
{
    QTransform wm = transform();
    wm.scale(1.5, 1.5); // adjust zooming step here
    setZoom(wm.m11()*100.0);
}

void UMLView::zoomOut()
{
    QTransform wm = transform();
    wm.scale(2.0 / 3.0, 2.0 / 3.0); //adjust zooming step here
    setZoom(wm.m11()*100.0);
}

/**
 * Overrides standard method from QWidget for possible additional actions.
 * TBC can we remove this?
 */
void UMLView::show()
{
    QWidget::show();
}

/**
 * Zoom the view in and out.
 */
void UMLView::wheelEvent(QWheelEvent* event)
{
    // get the position of the mouse before scaling, in scene coords
    QPointF pointBeforeScale(mapToScene(event->position().toPoint()));

    // scale the view ie. do the zoom
    double scaleFactor = 1.15;
    if (event->angleDelta().y() > 0) {
        // zoom in
        if (zoom() < 500) {
            setZoom(zoom() * scaleFactor);
        } else {
            return;
        }
    } else {
        // zooming out
        if (zoom() > 10) {
            setZoom(zoom() / scaleFactor);
        } else {
            return;
        }
    }

    // get the position after scaling, in scene coords
    QPointF pointAfterScale(mapToScene(event->position().toPoint()));

    // get the offset of how the screen moved
    QPointF offset = pointBeforeScale - pointAfterScale;

    // adjust to the new center for correct zooming
    QPointF newCenter = mapToScene(viewport()->rect().center()) + offset;

   centerOn(newCenter);

    UMLApp::app()->setZoom(zoom(), false);
}

/**
 * Overrides the standard operation.
 */
void UMLView::showEvent(QShowEvent* se)
{
    UMLApp* theApp = UMLApp::app();
    WorkToolBar* tb = theApp->workToolBar();
    UMLScene *us = umlScene();
    connect(tb, SIGNAL(sigButtonChanged(int)), us, SLOT(slotToolBarChanged(int)));
    connect(us, SIGNAL(sigResetToolBar()), tb, SLOT(slotResetToolBar()));

    umlScene()->showEvent(se);
    us->resetToolbar();
}

/**
 * Overrides the standard operation.
 */
void UMLView::hideEvent(QHideEvent* he)
{
    UMLApp* theApp = UMLApp::app();
    WorkToolBar* tb = theApp->workToolBar();
    UMLScene *us = umlScene();
    disconnect(tb, SIGNAL(sigButtonChanged(int)), us, SLOT(slotToolBarChanged(int)));
    disconnect(us, SIGNAL(sigResetToolBar()), tb, SLOT(slotResetToolBar()));

    us->hideEvent(he);
}

/**
 * Override standard method.
 * Middle or right button drag pans the diagram with a grab-hand cursor.
 * A right click without a drag still opens the context menu.
 */
void UMLView::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::MiddleButton || event->button() == Qt::RightButton) {
        beginHandPan(event);
        event->accept();
        return;
    }
    QGraphicsView::mousePressEvent(event);
}

void UMLView::mouseMoveEvent(QMouseEvent* event)
{
    if (m_handPanning) {
        const QPoint delta = event->pos() - m_handPanStart;
        if (delta.manhattanLength() >= QApplication::startDragDistance())
            m_handPanMoved = true;
        viewport()->setCursor(Qt::ClosedHandCursor);
        if (horizontalScrollBar())
            horizontalScrollBar()->setValue(m_handPanScroll.x() - delta.x());
        if (verticalScrollBar())
            verticalScrollBar()->setValue(m_handPanScroll.y() - delta.y());
        event->accept();
        return;
    }
    QGraphicsView::mouseMoveEvent(event);
}

void UMLView::mouseReleaseEvent(QMouseEvent* event)
{
    if (m_handPanning && (event->button() == Qt::MiddleButton || event->button() == Qt::RightButton)) {
        endHandPan(event);
        event->accept();
        return;
    }
    QGraphicsView::mouseReleaseEvent(event);
}

void UMLView::expandSceneRectForPan()
{
    UMLScene *s = umlScene();
    if (!s)
        return;
    const QRectF bounds = s->itemsBoundingRect();
    if (!bounds.isValid())
        return;
    qreal padX = 400.0;
    qreal padY = 400.0;
    if (viewport() && viewport()->width() > 0 && viewport()->height() > 0) {
        const QRectF vp = mapToScene(viewport()->rect()).boundingRect();
        padX = qMax(padX, vp.width());
        padY = qMax(padY, vp.height());
    }
    s->setSceneRect(bounds.adjusted(-padX, -padY, padX, padY));
}

void UMLView::beginHandPan(QMouseEvent* event)
{
    m_handPanning = true;
    m_handPanMoved = false;
    m_openContextMenuOnRelease = (event->button() == Qt::RightButton);
    m_handPanStart = event->pos();
    // Expand first — setSceneRect changes scrollbar range/value.
    expandSceneRectForPan();
    m_handPanScroll = QPoint(horizontalScrollBar() ? horizontalScrollBar()->value() : 0,
                             verticalScrollBar() ? verticalScrollBar()->value() : 0);
    viewport()->setCursor(Qt::ClosedHandCursor);
    viewport()->grabMouse();
    setInteractive(false);
    setRenderHint(QPainter::Antialiasing, false);
    setRenderHint(QPainter::SmoothPixmapTransform, false);
}

void UMLView::endHandPan(QMouseEvent* event)
{
    if (QWidget::mouseGrabber() == viewport())
        viewport()->releaseMouse();
    setInteractive(true);
    setRenderHint(QPainter::Antialiasing, true);
    setRenderHint(QPainter::SmoothPixmapTransform, true);
    viewport()->unsetCursor();

    const bool showMenu = m_openContextMenuOnRelease && !m_handPanMoved;
    m_handPanning = false;
    m_handPanMoved = false;
    m_openContextMenuOnRelease = false;

    if (showMenu) {
        QContextMenuEvent menuEvent(QContextMenuEvent::Mouse, event->pos(), event->globalPos());
        QGraphicsView::contextMenuEvent(&menuEvent);
    }
}

void UMLView::contextMenuEvent(QContextMenuEvent* event)
{
    // Qt sends this on right-press. Swallow while panning; click-without-drag
    // still opens the menu from endHandPan().
    if (m_handPanning || m_openContextMenuOnRelease) {
        event->accept();
        return;
    }
    QGraphicsView::contextMenuEvent(event);
}

/**
 * Override standard method.
 */
void UMLView::resizeEvent(QResizeEvent *event)
{
    bool oldState1 = verticalScrollBar()->blockSignals(true);
    bool oldState2 = horizontalScrollBar()->blockSignals(true);
    QGraphicsView::resizeEvent(event);
    verticalScrollBar()->blockSignals(oldState1);
    horizontalScrollBar()->blockSignals(oldState2);
}
