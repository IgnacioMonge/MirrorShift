#ifndef MIRRORSHIFT_UI_THEME_H
#define MIRRORSHIFT_UI_THEME_H

// Desktop theme for Mirror Shift: cooler navy and ice cyan so the chrome
// matches the glass Lattice rather than inherited Shatranj slate. Tokens are
// preprocessor literals so the style sheets stay compile-time concatenated
// strings with no runtime formatting.

#include <QApplication>
#include <QColor>
#include <QFont>
#include <QFontDatabase>
#include <QPalette>
#include <QString>
#include <QStringList>
#include <QtGlobal>
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
#include <QStyleHints>
#endif

// --- Surfaces ---------------------------------------------------------------
#define MSH_INK          "#0c121c"  // window ground — abyss behind the Lattice
#define MSH_SURFACE      "#182230"  // inputs, wells, menus
#define MSH_SURFACE_ALT  "#243448"  // raised: buttons
#define MSH_HOVER        "#2e445c"
#define MSH_PRESSED      "#1a2838"
#define MSH_ROW_ALT      "#16202c"  // alternate table rows
#define MSH_CARD         "#111a26"  // grouped panel behind controls
#define MSH_WELL         "#080e16"  // status bar
#define MSH_QUIET        "#141c28"  // neutral fill: disabled inputs, idle status
#define MSH_BORDER       "#3a5a70"  // visible hairline
#define MSH_BORDER_SOFT  "#243848"  // internal separators
#define MSH_DISABLED     "#1a2430"
#define MSH_SCROLL       "#3a5870"

// --- Ink --------------------------------------------------------------------
#define MSH_TEXT         "#e8f4fa"
#define MSH_TEXT_DIM     "#b4c8d6"
#define MSH_TEXT_MUTED   "#7a92a4"

// --- Accents ----------------------------------------------------------------
#define MSH_ACCENT       "#40e8ff"  // captions, live state, links
#define MSH_ACCENT_SOFT  "#8ef4ff"
#define MSH_ACCENT_DEEP  "#18b8d4"  // selection fill, hover edge
#define MSH_ACCENT_FILL  "#1a4a62"  // filled emphasis: your turn, primary action
#define MSH_ACCENT_FILL_HOVER "#226078"
#define MSH_SEND         "#4ec8e8"  // Echoes send, ready
#define MSH_SEND_HOVER   "#68d8f4"
#define MSH_SEND_TEXT    "#061018"
#define MSH_GO           "#1f9d47"  // alignment ready to send
#define MSH_GO_HOVER     "#28b956"
#define MSH_GO_TEXT      "#f4fff8"
#define MSH_ALERT        "#743838"  // failure fill
#define MSH_ALERT_TEXT   "#fff0f0"
#define MSH_ALERT_BRIGHT "#ff5a5a"
#define MSH_WAIT         "#163848"  // alignment in transit
#define MSH_WAIT_TEXT    "#b8f4ff"
#define MSH_WAIT_BRIGHT  "#8ef4ff"
#define MSH_SUCCESS      "#7dff8a"

// --- Geometry ---------------------------------------------------------------
#define MSH_R            "6px"      // inputs, buttons
#define MSH_R_LG         "10px"     // cards, board frame

// --- Board fallbacks (plain squares when a texture is not in use) -----------
#define MSH_BOARD_WELL   "#050a12"
#define MSH_BOARD_EDGE   "#40e8ff"
#define MSH_SQ_LIGHT     "#f0f0ec"
#define MSH_SQ_DARK      "#5f6870"
#define MSH_SQ_EDGE      "#2c3034"
#define MSH_SQ_SEL       "#c9b56b"
#define MSH_SQ_SEL_EDGE  "#5d4b1d"
#define MSH_SQ_TGT       "#9fb8d9"
#define MSH_SQ_TGT_EDGE  "#2f5f9f"
#define MSH_SQ_HIT       "#8ef4ff"
#define MSH_SQ_HIT_EDGE  "#18b8d4"
#define MSH_SQ_HINT_L    "#d5ebd5"
#define MSH_SQ_HINT_D    "#486648"
#define MSH_SQ_HINT_EDGE "#6fa86f"
#define MSH_SQ_INK       "#1e1e1e"

namespace UiTheme {

// Proportional UI family, per platform, with a graceful fallback.
inline QString uiFamily()
{
    static const QString cached = []() {
        const QStringList families = QFontDatabase::families();
        for (const char *candidate : {"Segoe UI Variable Text", "Segoe UI",
                                      "SF Pro Text", "Helvetica Neue",
                                      "Inter", "Noto Sans"}) {
            const QString name = QString::fromLatin1(candidate);
            if (families.contains(name)) {
                return name;
            }
        }
        return QFontDatabase::systemFont(QFontDatabase::GeneralFont).family();
    }();
    return cached;
}

// Tabular family for the move list and the raw log.
inline QString monoFamily()
{
    static const QString cached = []() {
        const QStringList families = QFontDatabase::families();
        for (const char *candidate : {"Cascadia Mono", "Consolas", "SF Mono",
                                      "Menlo", "DejaVu Sans Mono"}) {
            const QString name = QString::fromLatin1(candidate);
            if (families.contains(name)) {
                return name;
            }
        }
        return QFontDatabase::systemFont(QFontDatabase::FixedFont).family();
    }();
    return cached;
}

// Fusion plus an explicit palette so native chrome (menus, tooltips, combo
// popups, message boxes) matches the style sheet on every platform.
inline void apply(QApplication &app)
{
    app.setStyle(QStringLiteral("Fusion"));
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
    // Keep native title bars and dialogs aligned with the dark product theme.
    app.styleHints()->setColorScheme(Qt::ColorScheme::Dark);
#endif

    QFont base(uiFamily());
    base.setPointSizeF(10.0);
    app.setFont(base);

    QPalette pal;
    pal.setColor(QPalette::Window, QColor(MSH_INK));
    pal.setColor(QPalette::WindowText, QColor(MSH_TEXT));
    pal.setColor(QPalette::Base, QColor(MSH_SURFACE));
    pal.setColor(QPalette::AlternateBase, QColor(MSH_ROW_ALT));
    pal.setColor(QPalette::Text, QColor(MSH_TEXT));
    pal.setColor(QPalette::Button, QColor(MSH_SURFACE_ALT));
    pal.setColor(QPalette::ButtonText, QColor(MSH_TEXT));
    pal.setColor(QPalette::BrightText, QColor(MSH_ACCENT_SOFT));
    pal.setColor(QPalette::Highlight, QColor(MSH_ACCENT_DEEP));
    pal.setColor(QPalette::HighlightedText, QColor(MSH_WELL));
    pal.setColor(QPalette::Link, QColor(MSH_ACCENT));
    pal.setColor(QPalette::ToolTipBase, QColor(MSH_SURFACE));
    pal.setColor(QPalette::ToolTipText, QColor(MSH_TEXT));
    pal.setColor(QPalette::PlaceholderText, QColor(MSH_TEXT_MUTED));
    pal.setColor(QPalette::Disabled, QPalette::Text, QColor(MSH_TEXT_MUTED));
    pal.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(MSH_TEXT_MUTED));
    pal.setColor(QPalette::Disabled, QPalette::WindowText, QColor(MSH_TEXT_MUTED));
    app.setPalette(pal);
}

} // namespace UiTheme

#endif // MIRRORSHIFT_UI_THEME_H
