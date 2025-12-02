#include "app_window.h"

#include <QApplication>
#include <QHBoxLayout>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("color_move_art"));
    QApplication::setOrganizationName(QStringLiteral("color_move_art"));

    auto *window = new AppWindow;
    window->resize(1280, 720);

    QWidget mainWidget;
    mainWidget.setWindowTitle(QStringLiteral("Color Move Art"));
    auto *container = QWidget::createWindowContainer(window);
    container->setMinimumSize(640, 360);
    container->setFocusPolicy(Qt::StrongFocus);

    QPushButton *toggleBtn = new QPushButton(QObject::tr("Pause"));
    QObject::connect(toggleBtn, &QPushButton::clicked, [window, toggleBtn]() {
        if (window->isPaused()) {
            window->play();
            toggleBtn->setText(QObject::tr("Pause"));
        } else {
            window->pause();
            toggleBtn->setText(QObject::tr("Resume"));
        }
    });

    QHBoxLayout *controls = new QHBoxLayout;
    controls->addWidget(toggleBtn);
    controls->addStretch();

    QVBoxLayout *layout = new QVBoxLayout;
    layout->addWidget(container, 1);
    layout->addLayout(controls);

    mainWidget.setLayout(layout);
    mainWidget.resize(1280, 780);
    mainWidget.show();

    return app.exec();
}
