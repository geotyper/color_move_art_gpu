#pragma once

#include <QElapsedTimer>
#include <QPointF>
#include <QTimer>
#include <QWindow>
#include <functional>
#include <memory>

class MpmRenderer;

class AppWindow : public QWindow
{
public:
    AppWindow();
    ~AppWindow() override;

    void play();
    void pause();
    bool isPaused() const { return m_paused; }

protected:
    void exposeEvent(QExposeEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;

private:
    void start();
    void stop();
    void onFrame();

    std::unique_ptr<MpmRenderer> m_renderer;
    QTimer m_timer;
    QElapsedTimer m_frameTimer;
    bool m_running {false};
    bool m_paused {false};
    QPointF m_lastMousePos;
    bool m_hasLastPos {false};
};
