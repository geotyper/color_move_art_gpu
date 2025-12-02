#include "app_window.h"

#include <QGuiApplication>

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("color_move_art"));
    QGuiApplication::setOrganizationName(QStringLiteral("color_move_art"));

    AppWindow window;
    window.resize(1280, 720);
    window.show();

    return app.exec();
}
