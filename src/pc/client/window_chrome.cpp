#include "window_chrome.h"

#include <QEvent>
#include <QObject>
#include <QWidget>
#include <QtGlobal>

#if defined(Q_OS_WIN)
#include "win_window_chrome.h"
#elif defined(Q_OS_MACOS)
#include "mac_window_chrome.h"
#endif

#if defined(Q_OS_LINUX) && QT_VERSION >= QT_VERSION_CHECK(6, 2, 0)
#include <QGuiApplication>
#include <QLibrary>
#include <QtGui/qguiapplication_platform.h>
#include <QWindow>
#if QT_CONFIG(xcb)
#define MIRRORSHIFT_HAS_QX11_APPLICATION 1
#endif
#endif

namespace {

#if defined(MIRRORSHIFT_HAS_QX11_APPLICATION)
void applyLinuxGtkDarkVariant(QWidget *window)
{
    QWindow *handle = window->windowHandle();
    if (handle == nullptr) {
        handle = window->window()->windowHandle();
    }
    if (handle == nullptr) {
        return;
    }

    auto *x11App = qGuiApp->nativeInterface<QNativeInterface::QX11Application>();
    if (x11App == nullptr) {
        return;
    }

    void *display = x11App->display();
    if (display == nullptr) {
        return;
    }

    QLibrary x11(QStringLiteral("X11"));
    if (!x11.load()) {
        x11.setFileName(QStringLiteral("libX11.so.6"));
        if (!x11.load()) {
            return;
        }
    }

    using XInternAtomFn = unsigned long (*)(void *, const char *, int);
    using XChangePropertyFn = int (*)(void *, unsigned long, unsigned long,
                                      unsigned long, int, int,
                                      const unsigned char *, int);
    const auto internAtom = reinterpret_cast<XInternAtomFn>(
        x11.resolve("XInternAtom"));
    const auto changeProperty = reinterpret_cast<XChangePropertyFn>(
        x11.resolve("XChangeProperty"));
    if (internAtom == nullptr || changeProperty == nullptr) {
        return;
    }

    const unsigned long variant = internAtom(display, "_GTK_THEME_VARIANT", 0);
    const unsigned long utf8 = internAtom(display, "UTF8_STRING", 0);
    static const unsigned char dark[] = "dark";
    const unsigned long xid = static_cast<unsigned long>(handle->winId());
    changeProperty(display, xid, variant, utf8, 8, 0, dark, 4);
}
#endif

void applyPlatformWindowChrome(QWidget *window, const QColor &background,
                               const QColor &foreground)
{
#ifdef Q_OS_WIN
    Q_UNUSED(foreground);
    applyWinWindowChrome(window, background);
#elif defined(Q_OS_MACOS)
    Q_UNUSED(foreground);
    applyMacWindowChrome(window, background);
#elif defined(MIRRORSHIFT_HAS_QX11_APPLICATION)
    Q_UNUSED(background);
    Q_UNUSED(foreground);
    applyLinuxGtkDarkVariant(window);
#else
    Q_UNUSED(window);
    Q_UNUSED(background);
    Q_UNUSED(foreground);
#endif
}

class WindowChromeFilter final : public QObject {
public:
    WindowChromeFilter(const QColor &background, const QColor &foreground,
                       QObject *parent)
        : QObject(parent)
        , background_(background)
        , foreground_(foreground)
    {
    }

    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (event->type() != QEvent::Show) {
            return false;
        }

        auto *widget = qobject_cast<QWidget *>(watched);
        if (widget == nullptr || !widget->isWindow()) {
            return false;
        }

        const Qt::WindowType type = widget->windowType();
        if (type != Qt::Window && type != Qt::Dialog) {
            return false;
        }

        applyPlatformWindowChrome(widget, background_, foreground_);
        return false;
    }

private:
    QColor background_;
    QColor foreground_;
};

} // namespace

void installWindowChrome(QObject *app, const QColor &background,
                         const QColor &foreground)
{
    if (app == nullptr) {
        return;
    }

    app->installEventFilter(
        new WindowChromeFilter(background, foreground, app));
}
