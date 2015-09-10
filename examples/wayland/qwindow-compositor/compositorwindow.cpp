/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the examples of the Qt Wayland module
**
** $QT_BEGIN_LICENSE:BSD$
** You may use this file under the terms of the BSD license as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "compositorwindow.h"

#include <QMouseEvent>
#include <QOpenGLWindow>
#include <QOpenGLTexture>
#include <QOpenGLFunctions>
#include <QMatrix4x4>

#include "windowcompositor.h"
#include <QtWaylandCompositor/qwaylandinput.h>

void CompositorWindow::initializeGL()
{
    QImage backgroundImage = QImage(QLatin1String(":/background.jpg"));
    m_backgroundTexture = new QOpenGLTexture(backgroundImage, QOpenGLTexture::DontGenerateMipMaps);
    m_backgroundImageSize = backgroundImage.size();
    m_textureBlitter.create();
}

void CompositorWindow::drawBackground()
{
    for (int y = 0; y < height(); y += m_backgroundImageSize.height()) {
        for (int x = 0; x < width(); x += m_backgroundImageSize.width()) {
            QMatrix4x4 targetTransform = QOpenGLTextureBlitter::targetTransform(QRect(QPoint(x,y), m_backgroundImageSize), QRect(QPoint(0,0), size()));
            m_textureBlitter.blit(m_backgroundTexture->textureId(),
                              targetTransform,
                              QOpenGLTextureBlitter::OriginTopLeft);
        }
    }
}

void CompositorWindow::paintGL()
{
    m_compositor->startRender();
    QOpenGLFunctions *functions = context()->functions();
    functions->glClearColor(1.f, .6f, .0f, 0.5f);
    functions->glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    m_textureBlitter.bind();
    drawBackground();

    functions->glEnable(GL_BLEND);
    functions->glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    Q_FOREACH (WindowCompositorView *view, m_compositor->views()) {
        GLuint textureId = view->getTexture();
        QWaylandSurface *surface = view->surface();
        if (surface && surface->isMapped()) {
            QSize s = surface->size();
            if (!s.isEmpty()) {
                QRectF surfaceGeometry(view->requestedPosition(), s);
                //qDebug() << surface << surface->views().first() << view << s;
                QOpenGLTextureBlitter::Origin surfaceOrigin =
                    view->currentBuffer().origin() == QWaylandSurface::OriginTopLeft
                    ? QOpenGLTextureBlitter::OriginTopLeft
                    : QOpenGLTextureBlitter::OriginBottomLeft;
                QMatrix4x4 targetTransform = QOpenGLTextureBlitter::targetTransform(surfaceGeometry, QRect(QPoint(), size()));
                m_textureBlitter.blit(textureId, targetTransform, surfaceOrigin);
            }
        }
    }
    functions->glDisable(GL_BLEND);

    m_textureBlitter.release();
    m_compositor->endRender();
}

void resizeGL(int w, int h)
{
}

WindowCompositorView *CompositorWindow::viewAt(const QPointF &point)
{
    WindowCompositorView *ret = 0;
    Q_FOREACH (WindowCompositorView *view, m_compositor->views()) {
        QPointF topLeft = view->requestedPosition();
        QWaylandSurface *surface = view->surface();
        QRectF geo(topLeft, surface->size());
        if (geo.contains(point))
            ret = view;
    }
    return ret;
}

void CompositorWindow::mousePressEvent(QMouseEvent *e)
{
    if (m_mouseView.isNull()) {
        m_mouseView = viewAt(e->localPos());
        QMouseEvent moveEvent(QEvent::MouseMove, e->localPos(), e->globalPos(), Qt::NoButton, Qt::NoButton, e->modifiers());
        sendMouseEvent(&moveEvent, m_mouseView);
    }
    sendMouseEvent(e, m_mouseView);
}

void CompositorWindow::mouseReleaseEvent(QMouseEvent *e)
{
    sendMouseEvent(e, m_mouseView);
    if (e->buttons() == Qt::NoButton)
        m_mouseView = 0;
}

void CompositorWindow::mouseMoveEvent(QMouseEvent *e)
{
    sendMouseEvent(e, m_mouseView);
}

void CompositorWindow::sendMouseEvent(QMouseEvent *e, QWaylandView *target)
{
    if (!target)
        return;

    QPointF mappedPos = e->localPos() - target->requestedPosition();
    QMouseEvent viewEvent(e->type(), mappedPos, e->localPos(), e->button(), e->buttons(), e->modifiers());
    m_compositor->handleMouseEvent(target, &viewEvent);
}

void CompositorWindow::keyPressEvent(QKeyEvent *e)
{
    m_compositor->defaultInputDevice()->sendKeyPressEvent(e->nativeScanCode());
}

void CompositorWindow::keyReleaseEvent(QKeyEvent *e)
{
    m_compositor->defaultInputDevice()->sendKeyReleaseEvent(e->nativeScanCode());
}
