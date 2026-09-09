#pragma once

#include <QColor>

class QObject;

// Apply the product chrome after every top-level window or dialog is shown.
void installWindowChrome(QObject *app, const QColor &background,
                         const QColor &foreground);
