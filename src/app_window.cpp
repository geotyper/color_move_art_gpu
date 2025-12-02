#include "app_window.h"

#include "mpm_renderer.h"

#include <QExposeEvent>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QSurfaceFormat>

AppWindow::AppWindow()
{
    QSurfaceFormat fmt;
    fmt.setRenderableType(QSurfaceFormat::OpenGL);
    fmt.setVersion(4, 3);
    fmt.setProfile(QSurfaceFormat::CoreProfile);
    fmt.setDepthBufferSize(24);
    fmt.setStencilBufferSize(8);
    setFormat(fmt);
    setSurfaceType(QSurface::OpenGLSurface);
    m_renderer = std::make_unique<MpmRenderer>(this);
    m_timer.setTimerType(Qt::PreciseTimer);
    m_timer.setSingleShot(false);
    connect(&m_timer, &QTimer::timeout, this, &AppWindow::onFrame);
    start();
}

AppWindow::~AppWindow()
{
    stop();
    if (m_renderer) {
        m_renderer->shutdown();
        m_renderer.reset();
    }
}

void AppWindow::start()
{
    if (m_running)
        return;
    m_running = true;
    m_frameTimer.restart();
    m_timer.start(0); // drive as fast as possible; vsync handled by swapchain
}

void AppWindow::stop()
{
    if (!m_running)
        return;
    m_running = false;
    m_timer.stop();
}

void AppWindow::exposeEvent(QExposeEvent *event)
{
    Q_UNUSED(event);
    if (!isExposed())
        stop();
}

void AppWindow::resizeEvent(QResizeEvent *event)
{
    if (m_renderer)
        m_renderer->resize(event->size());
    QWindow::resizeEvent(event);
}

void AppWindow::onFrame()
{
    if (m_paused)
        return;
    const float dt = m_frameTimer.isValid() ? float(m_frameTimer.restart()) / 1000.0f : 0.016f;
    m_renderer->renderFrame(dt);
}

void AppWindow::mousePressEvent(QMouseEvent *event)
{
    m_lastMousePos = event->position();
    m_hasLastPos = true;
    if (m_renderer)
        m_renderer->onImpulse(event->position(), QPointF(0, 0), 1.0f);
}

void AppWindow::mouseMoveEvent(QMouseEvent *event)
{
    const QPointF pos = event->position();
    if (m_hasLastPos && m_renderer) {
        QPointF delta = pos - m_lastMousePos;
        m_renderer->onImpulse(pos, delta * 4.0, 1.0f);
    }
    m_lastMousePos = pos;
    m_hasLastPos = true;
}

void AppWindow::play()
{
    m_paused = false;
    if (!m_running)
        start();
}

void AppWindow::pause()
{
    m_paused = true;
}
