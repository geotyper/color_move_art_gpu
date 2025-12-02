#pragma once

#include <QElapsedTimer>
#include <QPointF>
#include <QTimer>
#include <QWindow>
#include <memory>

class MpmRenderer;

class AppWindow : public QWindow
{
public:
    AppWindow();
    ~AppWindow() override;

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
    QPointF m_lastMousePos;
    bool m_hasLastPos {false};
};
