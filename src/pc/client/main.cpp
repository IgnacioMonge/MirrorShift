#include <QApplication>

#include "main_window.h"
#include "ui_theme.h"
#include "window_chrome.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName("Mirror Shift");
    QApplication::setOrganizationName("Mirror Shift");
#ifdef Q_OS_LINUX
    QApplication::setDesktopFileName(
        "io.github.ignaciomonge.mirrorshift");
#endif
    UiTheme::apply(app);
    installWindowChrome(&app, QColor(QStringLiteral(MSH_INK)),
                        QColor(QStringLiteral(MSH_TEXT)));
    const QIcon appIcon = MainWindow::appIcon();
    QApplication::setWindowIcon(appIcon);

    MainWindow window;
    window.setWindowIcon(appIcon);
    window.showNormal();

    return app.exec();
}
