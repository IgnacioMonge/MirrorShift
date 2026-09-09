#include <QAbstractSocket>
#include <QAction>
#include <QApplication>
#include <QByteArray>
#include <QButtonGroup>
#include <QCheckBox>
#include <QColor>
#include <QComboBox>
#include <QCloseEvent>
#include <QCoreApplication>
#include <QDateTime>
#include <QDialog>
#include <QElapsedTimer>
#include <QEasingCurve>
#include <QEvent>
#include <QEventLoop>
#include <QFont>
#include <QFrame>
#include <QFontMetricsF>
#include <QGridLayout>
#include <QGraphicsOpacityEffect>
#include <QHash>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QIcon>
#include <QImage>
#include <QKeyEvent>
#include <QLabel>
#include <QLinearGradient>
#include <QLineEdit>
#include <QLocale>
#include <QMainWindow>
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QNetworkInterface>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QRadialGradient>
#include <QPlainTextEdit>
#include <QPropertyAnimation>
#include <QVariantAnimation>
#include <QPointer>
#include <QPushButton>
#include <QRadioButton>
#include <QRandomGenerator>
#include <QScrollBar>
#include <QSettings>
#include <QSet>
#include <QSize>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStackedWidget>
#include <QStatusBar>
#include <QString>
#include <QStringList>
#include <QAbstractItemView>
#include <QDesktopServices>
#include <QTableWidget>
#include <QUrl>
#include <QVector>
#include <QPair>
#include <QStyle>
#include <QTableWidgetItem>
#include <QHostAddress>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTextCharFormat>
#include <QTextEdit>
#include <QToolButton>
#include <QTime>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>
#include <QWidgetAction>
#include <QtGlobal>
#include <QtMath>

#include <cstring>
#include <cmath>
#include <functional>

#include "main_window.h"

extern "C" {
#include "common/reversi/reversi.h"
#include "common/protocol/platform_protocol.h"
#include "common/savegame/savegame_wire.h"
#include "common/session/session.h"
}
#include "common/ui_messages.h"
#include "app_banner.h"
#include "input_helpers.h"
#include "desktop_session_controller.h"
#include "desktop_transport_codec.h"
#ifdef Q_OS_MACOS
#include "mac_window_chrome.h"
#endif
#ifdef Q_OS_WIN
#include "win_window_chrome.h"
#endif
#include "piece_renderer.h"
#include "save_game_store.h"
#include "ui_theme.h"

namespace {
using namespace PieceRenderer;

constexpr int kBoardSquareSize = 52;
constexpr int kBoardCoordSize = 24;
constexpr int kPieceIconSize = 52;
constexpr int kActionButtonWidth = 100;
constexpr int kSidePanelWidth = 340;
constexpr int kTurnSlotHeight = 36;
constexpr int kChatTextMax = SESSION_CHAT_TEXT_MAX;
constexpr int kSpectrumFrameMs = 20;
constexpr int kPieceRevealStepMs = 5 * kSpectrumFrameMs;
constexpr int kPieceRevealMiddlePauseMs = 3 * kSpectrumFrameMs;
constexpr int kPieceFlipFrames = 20;
constexpr int kPieceFlipFrameMs = 28;
constexpr int kGameOverAbyssDelayMs = kPieceFlipFrames * kPieceFlipFrameMs + 80;
// A TCP listener may exist before the remote app is ready for its DIRECT handshake.
constexpr const char *kAppVersion = NETCHESSZX_APP_VERSION;
constexpr const char *kDirectHostBusyStatus = "Host busy";
constexpr const char *kDirectPortSettingsKey = "connection/directPort";
constexpr const char *kMqttPortSettingsKey = "connection/mqttPort";
constexpr qint64 kUiStallWarnMs = 2500;
constexpr qint64 kMoveSendWarnMs = 250;
constexpr qint64 kMaxClockSeconds = 359999; // 99h59m59s ceiling for save-slot elapsed clock
constexpr uint8_t kMqttLinkId = 1u;
constexpr int kMqttBufferedPublishMax = 8;

struct MqttBufferedPublish {
    QString suffix;
    QByteArray topic;
    QByteArray payload;
    bool retained = false;
};

static QStringList directIpHistoryWithImpl(const QStringList &history,
                                           const QString &host)
{
    QStringList result;
    const auto append = [&result](const QString &candidate) {
        const QString ip = candidate.trimmed();
        if (InputHelpers::isDirectIpSyntaxOk(ip) && !result.contains(ip)) {
            result.append(ip);
        }
    };

    append(host);
    for (const QString &ip : history) {
        if (result.size() >= kDirectIpHistoryMax) {
            break;
        }
        append(ip);
    }
    return result;
}

static QIcon directIpHistoryIcon()
{
    QPixmap pixmap(16, 16);
    const QColor color(QStringLiteral(MSH_ACCENT));

    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.fillRect(2, 3, 8, 2, color);
    painter.fillRect(2, 7, 8, 2, color);
    painter.fillRect(2, 11, 8, 2, color);
    painter.fillRect(11, 6, 3, 2, color);
    painter.fillRect(12, 8, 1, 2, color);
    return QIcon(pixmap);
}

static QByteArray mqttClientIdForImpl(bool host, quint64 nonce)
{
    return QStringLiteral("PC%1%2")
        .arg(host ? QLatin1Char('H') : QLatin1Char('J'))
        .arg(nonce, 16, 16, QLatin1Char('0'))
        .toUpper()
        .toLatin1();
}

static QIcon sessionActionIcon(const char *kind)
{
    QPixmap pixmap(16, 16);
    const QColor color(QStringLiteral(MSH_ACCENT));
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QPen(color, 1.6, Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin));
    painter.setBrush(Qt::NoBrush);
    if (std::strcmp(kind, "takeback") == 0) {
        painter.drawLine(QPointF(12.5, 4.5), QPointF(12.5, 10.5));
        painter.drawLine(QPointF(12.5, 10.5), QPointF(4.5, 10.5));
        QPolygonF head;
        head << QPointF(4.5, 10.5) << QPointF(8.0, 7.5) << QPointF(8.0, 13.5);
        painter.setBrush(color);
        painter.setPen(Qt::NoPen);
        painter.drawPolygon(head);
    } else if (std::strcmp(kind, "reset") == 0) {
        QRectF arc(3.0, 3.0, 10.0, 10.0);
        painter.drawArc(arc, 50 * 16, 260 * 16);
        QPolygonF head;
        head << QPointF(11.5, 3.0) << QPointF(15.0, 6.5) << QPointF(9.5, 6.5);
        painter.setBrush(color);
        painter.setPen(Qt::NoPen);
        painter.drawPolygon(head);
    }
    return QIcon(pixmap);
}

static uint8_t localMachinePlatform()
{
#if defined(Q_OS_MACOS)
    return NETCHESS_PLAT_MAC;
#elif defined(Q_OS_LINUX)
    return NETCHESS_PLAT_LNX;
#elif defined(Q_OS_WIN)
    return NETCHESS_PLAT_PC;
#else
    return NETCHESS_PLAT_UNKNOWN;
#endif
}

static QByteArray localMachPayload()
{
    char text[16];

    if (!netchess_proto_format_mach(text, sizeof(text),
                                    localMachinePlatform())) {
        return QByteArray();
    }
    return QByteArray(text);
}

static bool parseMachPayload(const QByteArray &payload, uint8_t *platform)
{
    if (platform == nullptr || payload.contains('\0')) {
        return false;
    }
    return netchess_proto_parse_mach(payload.constData(), platform) != 0u;
}

static bool isSessionControlCommand(const QString &command)
{
    return command == QStringLiteral("/resign") ||
           command == QStringLiteral("/takeback");
}

static bool chatCanSharePendingControl(const QString &command,
                                       uint8_t pending,
                                       bool promptOpen,
                                       bool decisionOpen)
{
    return !isSessionControlCommand(command) && !promptOpen && !decisionOpen &&
           (pending == SESSION_REQUEST_RESET ||
            pending == SESSION_REQUEST_RESIGN ||
             pending == SESSION_REQUEST_TAKEBACK);
}

static QMessageBox::StandardButton askQuestion(
    QWidget *parent, const QString &title, const QString &message,
    QMessageBox::StandardButtons buttons,
    QMessageBox::StandardButton defaultButton = QMessageBox::NoButton)
{
    QMessageBox box(QMessageBox::NoIcon, title, message, buttons, parent);
    if (defaultButton != QMessageBox::NoButton) {
        box.setDefaultButton(defaultButton);
    }
    return static_cast<QMessageBox::StandardButton>(box.exec());
}

static QString appStyleSheet()
{
    return QStringLiteral(
        "QMainWindow, QDialog { background:" MSH_INK "; color:" MSH_TEXT "; }"
        "QWidget { color:" MSH_TEXT "; }"
        // Exact-class selector: plain layout containers must not paint the
        // window ground over the card they sit on. Real controls keep theirs.
        ".QWidget { background:transparent; }"
        "QWidget#topCard, QWidget#sidePanel { background:" MSH_CARD ";"
        " border:1px solid " MSH_BORDER_SOFT "; border-radius:" MSH_R_LG "; }"
        "QLabel { color:" MSH_TEXT "; background:transparent; }"
        "QLabel#caption { color:" MSH_ACCENT "; font-weight:700; font-size:10px;"
        " letter-spacing:1px; padding:0; background:transparent; }"
        "QLineEdit, QSpinBox, QComboBox, QPlainTextEdit, QTextEdit {"
        " background:" MSH_SURFACE "; color:" MSH_TEXT ";"
        " border:1px solid " MSH_BORDER "; border-radius:" MSH_R ";"
        " padding:4px 8px; selection-background-color:" MSH_ACCENT_DEEP ";"
        " selection-color:" MSH_WELL "; }"
        "QLineEdit:hover, QSpinBox:hover, QComboBox:hover {"
        " border-color:" MSH_ACCENT_DEEP "; }"
        "QLineEdit:focus, QSpinBox:focus, QComboBox:focus,"
        " QPlainTextEdit:focus, QTextEdit:focus { border-color:" MSH_ACCENT "; }"
        "QLineEdit:disabled, QSpinBox:disabled, QComboBox:disabled,"
        " QPlainTextEdit:disabled, QTextEdit:disabled { background:" MSH_QUIET ";"
        " color:" MSH_TEXT_MUTED "; border-color:" MSH_BORDER_SOFT "; }"
        "QPlainTextEdit, QTextEdit { padding:6px; }"
        "QComboBox::drop-down { border:0; width:18px; }"
        "QComboBox QAbstractItemView { background:" MSH_SURFACE ";"
        " color:" MSH_TEXT "; border:1px solid " MSH_BORDER ";"
        " selection-background-color:" MSH_ACCENT_DEEP "; selection-color:" MSH_WELL ";"
        " outline:0; }"
        "QPushButton { background:" MSH_SURFACE_ALT "; color:" MSH_TEXT ";"
        " border:1px solid " MSH_SURFACE_ALT "; border-radius:" MSH_R ";"
        " font-weight:600; font-size:9pt; padding:4px 12px; min-height:20px; }"
        "QPushButton:hover { background:" MSH_HOVER "; border-color:" MSH_HOVER "; }"
        "QPushButton:pressed { background:" MSH_PRESSED ";"
        " border-color:" MSH_PRESSED "; }"
        "QPushButton:disabled { background:" MSH_DISABLED ";"
        " color:" MSH_TEXT_MUTED "; border-color:" MSH_DISABLED "; }"
        "QRadioButton, QCheckBox { color:" MSH_TEXT "; spacing:6px;"
        " background:transparent; }"
        "QRadioButton::indicator { width:11px; height:11px; border-radius:6px;"
        " border:1px solid " MSH_TEXT_MUTED "; background:" MSH_SURFACE "; }"
        "QRadioButton::indicator:hover { border-color:" MSH_ACCENT "; }"
        "QRadioButton::indicator:checked { background:" MSH_ACCENT ";"
        " border:1px solid " MSH_ACCENT "; }"
        "QRadioButton::indicator:disabled { background:" MSH_DISABLED ";"
        " border-color:" MSH_BORDER "; }"
        "QRadioButton::indicator:checked:disabled { background:" MSH_TEXT_MUTED ";"
        " border:1px solid " MSH_TEXT_MUTED "; }"
        "QRadioButton:checked:disabled { color:" MSH_TEXT_DIM "; }"
        "QScrollBar:vertical { background:transparent; width:10px; margin:2px; }"
        "QScrollBar::handle:vertical { background:" MSH_SCROLL ";"
        " border-radius:3px; min-height:28px; }"
        "QScrollBar::handle:vertical:hover { background:" MSH_ACCENT_DEEP "; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height:0; }"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {"
        " background:transparent; }"
        "QToolTip { background:" MSH_SURFACE "; color:" MSH_TEXT ";"
        " border:1px solid " MSH_BORDER "; padding:4px 7px; }"
        "QStatusBar { background:" MSH_WELL ";"
        " border-top:1px solid " MSH_BORDER_SOFT "; }"
        "QStatusBar::item { border:0; }"
        "QStatusBar QLabel { color:" MSH_ACCENT "; font-weight:700;"
        " font-size:11px; letter-spacing:0.5px; }");
}

static QString boardFrameStyle()
{
    return QStringLiteral(
        "QWidget#boardFrame { background:%1;"
        " border:1px solid %2; border-radius:" MSH_R_LG "; }")
        .arg(PieceRenderer::boardWellColor(kBoardSquareSize).name(),
             PieceRenderer::glassBoardFrameColor(
                 PieceRenderer::boardTexture()).name());
}

static QLabel *captionLabel(const QString &text, QWidget *parent)
{
    auto *label = new QLabel(text.toUpper(), parent);
    label->setObjectName(QStringLiteral("caption"));
    return label;
}

static void configureActionButton(QPushButton *button)
{
    if (button != nullptr) {
        button->setAutoDefault(false);
        button->setDefault(false);
        button->setMinimumSize(kActionButtonWidth, 32);
    }
}

static void setWidgetStyle(QWidget *widget, const QString &style)
{
    if (widget != nullptr && widget->styleSheet() != style) {
        widget->setStyleSheet(style);
    }
}

} // namespace

QByteArray mqttClientIdFor(bool host, quint64 nonce)
{
    return mqttClientIdForImpl(host, nonce);
}

QStringList directIpHistoryWith(const QStringList &history, const QString &host)
{
    return directIpHistoryWithImpl(history, host);
}


// Paradox abyss FX: lattice squares fall into a central vanishing point.
// Frame/coords stay; overlay covers only the 8x8 playfield.
// Triggers: CONVERGENCE / PARADOX / RESIGN.


class BoardCollapseOverlay final : public QWidget {
public:
    struct Tile {
        QPixmap pixmap;
        QRectF start;
        int delayMs = 0;
        int fallMs = 900;
        qreal spinDeg = 0.0;
        qreal tipDeg = 14.0;     // pre-fall rock amplitude (degrees)
        qreal rockPhase = 0.0;   // radians
        qreal rockHz = 1.6;      // cycles during pre-fall
    };

    explicit BoardCollapseOverlay(QWidget *parent)
        : QWidget(parent)
    {
        setAttribute(Qt::WA_OpaquePaintEvent, true);
        hide();
        connect(&tick_, &QTimer::timeout, this, [this]() {
            update();
            if (!clock_.isValid()) {
                return;
            }
            if (clock_.elapsed() >= totalMs_) {
                tick_.stop();
                tiles_.clear();
                if (keepAbyss_) {
                    update(); // pure abyss remains inside the frame
                } else {
                    hide();
                }
            }
        });
    }

    bool isRunning() const { return tick_.isActive(); }
    bool isHoldingAbyss() const { return keepAbyss_ && isVisible() && !tick_.isActive(); }

    void begin(QVector<Tile> tiles, bool keepAbyss)
    {
        tiles_ = std::move(tiles);
        keepAbyss_ = keepAbyss;
        totalMs_ = 0;
        for (const Tile &tile : tiles_) {
            // +1500 ms hold on the empty abyss after the last tile vanishes.
            totalMs_ = qMax(totalMs_, tile.delayMs + tile.fallMs + 1500);
        }
        // Crack beat: tiles rock on their hinge before peeling off.
        totalMs_ += 420;
        crackMs_ = 420;
        clock_.restart();
        show();
        raise();
        tick_.start(16);
        update();
    }

    void abort()
    {
        tick_.stop();
        tiles_.clear();
        keepAbyss_ = false;
        hide();
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setRenderHint(QPainter::SmoothPixmapTransform, true);

        const QPointF focal(width() * 0.5, height() * 0.5);
        const qreal span = qMin(width(), height());
        const qreal msBg = clock_.isValid() ? static_cast<qreal>(clock_.elapsed()) : 0.0;

        // Procedural abyss (no desktop capture).
        {
            QRadialGradient base(focal, span * 0.98);
            base.setColorAt(0.00, QColor(12, 26, 44));
            base.setColorAt(0.28, QColor(8, 18, 34));
            base.setColorAt(0.60, QColor(5, 11, 22));
            base.setColorAt(1.00, QColor(2, 5, 12));
            p.fillRect(rect(), base);
        }

        // Volumetric haze.
        p.setPen(Qt::NoPen);
        for (int i = 0; i < 6; ++i) {
            const qreal t = (i + 1) / 6.0;
            const qreal r = span * (0.10 + t * 0.58);
            QRadialGradient fog(focal, r);
            const int a = static_cast<int>(32 - i * 4);
            fog.setColorAt(0.0, QColor(28, 68, 118, a));
            fog.setColorAt(0.55, QColor(14, 36, 68, a / 2));
            fog.setColorAt(1.0, QColor(6, 12, 24, 0));
            p.setBrush(fog);
            p.drawEllipse(focal, r, r * 0.93);
        }

        // Depth rings that slowly drift inward (living well).
        {
            p.setBrush(Qt::NoBrush);
            const qreal phase = std::fmod(msBg * 0.00022, 1.0);
            for (int i = 0; i < 9; ++i) {
                qreal t = (i + phase) / 9.0;
                if (t > 1.0) {
                    t -= 1.0;
                }
                const qreal r = span * (0.06 + t * t * 0.68);
                const int alpha = static_cast<int>((1.0 - t) * 55.0 + 12.0);
                p.setPen(QPen(QColor(48, 108, 168, alpha), 1.0 + (1.0 - t) * 1.1));
                p.drawEllipse(focal, r, r * 0.94);
            }
        }

        // Faint spokes.
        {
            p.setPen(QPen(QColor(70, 140, 200, 14), 1.0));
            for (int i = 0; i < 12; ++i) {
                const qreal a = i * (M_PI / 6.0) + msBg * 0.00012;
                p.drawLine(focal, QPointF(focal.x() + qCos(a) * span * 0.48,
                                          focal.y() + qSin(a) * span * 0.48));
            }
        }

        // Throat: dimmer, more hole than lamp.
        {
            QRadialGradient throat(focal, span * 0.18);
            throat.setColorAt(0.00, QColor(90, 160, 210, 42));
            throat.setColorAt(0.35, QColor(40, 100, 160, 22));
            throat.setColorAt(0.70, QColor(16, 48, 90, 10));
            throat.setColorAt(1.00, QColor(8, 20, 40, 0));
            p.setPen(Qt::NoPen);
            p.setBrush(throat);
            p.drawEllipse(focal, span * 0.18, span * 0.18);
        }
        {
            QRadialGradient iris(focal, span * 0.26);
            iris.setColorAt(0.00, QColor(0, 0, 0, 0));
            iris.setColorAt(0.50, QColor(0, 0, 0, 0));
            iris.setColorAt(0.78, QColor(0, 4, 12, 70));
            iris.setColorAt(1.00, QColor(0, 0, 0, 0));
            p.setBrush(iris);
            p.drawEllipse(focal, span * 0.26, span * 0.26);
        }

        // Dust motes.
        {
            p.setPen(Qt::NoPen);
            for (int i = 0; i < 48; ++i) {
                const qreal u = qFabs(qSin(i * 12.9898 + 78.233));
                const qreal v = qFabs(qSin(i * 93.989 + 11.13));
                const qreal ang = u * 2.0 * M_PI;
                const qreal rad = (0.15 + v * 0.55) * span * 0.5;
                const QPointF m(focal.x() + qCos(ang) * rad,
                                focal.y() + qSin(ang) * rad * 0.94);
                p.setBrush(QColor(140, 200, 255, 18 + static_cast<int>(v * 45)));
                p.drawEllipse(m, 0.7 + u * 1.5, 0.7 + u * 1.5);
            }
        }

        // Outer vignette — pit mouth.
        {
            QRadialGradient vig(focal, span * 0.82);
            vig.setColorAt(0.00, QColor(0, 0, 0, 0));
            vig.setColorAt(0.42, QColor(0, 0, 0, 0));
            vig.setColorAt(0.75, QColor(0, 0, 0, 90));
            vig.setColorAt(1.00, QColor(0, 0, 0, 210));
            p.setBrush(vig);
            p.setPen(Qt::NoPen);
            p.drawRect(rect());
        }

        if (!clock_.isValid() || tiles_.isEmpty()) {
            return;
        }

        const qreal ms = static_cast<qreal>(clock_.elapsed());
        for (const Tile &tile : tiles_) {
            const qreal local = ms - static_cast<qreal>(crackMs_ + tile.delayMs);
            if (local < 0.0) {
                // Suspended rock: minimal random flip as if hinged, then they fall.
                const qreal preTotal = static_cast<qreal>(crackMs_ + tile.delayMs);
                const qreal preT = preTotal > 1.0 ? qBound(0.0, ms / preTotal, 1.0) : 1.0;
                // Ease into the rock, stronger near peel-off.
                const qreal amp = tile.tipDeg * (0.45 + 0.55 * preT);
                const qreal angDeg = amp * qSin(
                    preT * tile.rockHz * 2.0 * M_PI + tile.rockPhase);
                const qreal sx = qMax(0.52, qAbs(qCos(qDegreesToRadians(angDeg))));
                const qreal lift = 1.2 * qAbs(qSin(
                    preT * tile.rockHz * M_PI + tile.rockPhase * 0.5));
                const qreal wobble = 0.4 * qSin(ms * 0.07 + tile.rockPhase);

                p.setOpacity(1.0);
                p.save();
                p.translate(tile.start.center().x() + wobble,
                            tile.start.center().y() - lift);
                p.scale(sx, 1.0);
                p.translate(-tile.start.width() * 0.5, -tile.start.height() * 0.5);
                p.drawPixmap(QPointF(0, 0), tile.pixmap);
                p.restore();
                continue;
            }
            qreal u = local / static_cast<qreal>(qMax(1, tile.fallMs));
            if (u >= 1.0) {
                continue;
            }

            // Slow detach, then rush into the focal point.
            const qreal e = 1.0 - qPow(1.0 - u, 2.55);
            const QPointF startC = tile.start.center();
            const QPointF pos = startC + (focal - startC) * e;

            // Semi-flip to edge-on early, then recede (scale to infinity).
            const qreal flip = qMin(1.0, e * 1.75);
            const qreal sxFlip = qMax(0.07, qAbs(qCos(flip * M_PI * 0.5)));
            const qreal recede = qMax(0.03, qPow(1.0 - e, 1.40));
            const qreal opacity = (e > 0.80) ? qMax(0.0, 1.0 - (e - 0.80) / 0.20) : 1.0;

            p.setOpacity(opacity);
            p.save();
            p.translate(pos);
            p.rotate(tile.spinDeg * e);
            p.scale(sxFlip * recede, recede);
            p.translate(-tile.start.width() * 0.5, -tile.start.height() * 0.5);
            p.drawPixmap(QPointF(0, 0), tile.pixmap);
            p.restore();
        }
        p.setOpacity(1.0);
    }

private:
    QVector<Tile> tiles_;
    QElapsedTimer clock_;
    QTimer tick_;
    int totalMs_ = 0;
    int crackMs_ = 280;
    bool keepAbyss_ = false;
};

static PieceRenderer::GlassMeterPalette chromeMeterPalette()
{
    const auto tint = [](const QString &hex, int alpha) {
        QColor color(hex);
        color.setAlpha(alpha);
        return color;
    };
    PieceRenderer::GlassMeterPalette palette;
    palette.selfFar = tint(QStringLiteral(MSH_ACCENT_FILL), 90);
    palette.selfMid = tint(QStringLiteral(MSH_ACCENT_DEEP), 160);
    palette.selfNear = tint(QStringLiteral(MSH_ACCENT), 230);
    palette.echoFar = tint(QStringLiteral(MSH_SURFACE), 220);
    palette.echoMid = tint(QStringLiteral(MSH_TEXT_MUTED), 180);
    palette.echoNear = tint(QStringLiteral(MSH_TEXT_DIM), 150);
    palette.seam = QColor(QStringLiteral(MSH_ACCENT));
    palette.well = QColor(QStringLiteral(MSH_WELL));
    return palette;
}

// Two realities pressing on a shared aperture. The counts only move the
// luminous seam where their light leaks between them — not a progress bar.
class RealityMeter final : public QWidget {
public:
    explicit RealityMeter(QWidget *parent = nullptr)
        : QWidget(parent)
        , palette_(chromeMeterPalette())
    {
        setFixedHeight(kTurnSlotHeight);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        setAccessibleName(QStringLiteral("Fragment count"));
        anim_ = new QVariantAnimation(this);
        anim_->setDuration(280);
        anim_->setEasingCurve(QEasingCurve::InOutCubic);
        connect(anim_, &QVariantAnimation::valueChanged, this,
                [this](const QVariant &value) {
                    displayRatio_ = value.toReal();
                    update();
                });
    }

    void setStatusText(const QString &text)
    {
        if (statusText_ == text) {
            return;
        }
        statusText_ = text;
        update();
    }

    void setCounts(int self, int echo)
    {
        self = qMax(0, self);
        echo = qMax(0, echo);
        const qreal target =
            static_cast<qreal>(self) / static_cast<qreal>(qMax(1, self + echo));
        if (self_ == self && echo_ == echo &&
            qAbs(displayRatio_ - target) < 0.0005) {
            return;
        }
        self_ = self;
        echo_ = echo;
        setAccessibleName(
            QStringLiteral("SELF %1 fragments, ECHO %2 fragments")
                .arg(self_).arg(echo_));
        if (!hasRatio_) {
            hasRatio_ = true;
            displayRatio_ = target;
            update();
            return;
        }
        anim_->stop();
        anim_->setStartValue(displayRatio_);
        anim_->setEndValue(target);
        anim_->start();
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);

        QFont labelFont = font();
        labelFont.setPixelSize(10);
        labelFont.setWeight(QFont::DemiBold);
        labelFont.setLetterSpacing(QFont::AbsoluteSpacing, 0.8);
        p.setFont(labelFont);

        const QRect textRect(0, 0, width(), 12);
        const QString selfText = QStringLiteral("SELF %1").arg(self_);
        const QString echoText = QStringLiteral("%1 ECHO").arg(echo_);
        const QFontMetricsF metrics(labelFont);
        p.setPen(QColor(QStringLiteral(MSH_TEXT)));
        p.drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, selfText);
        p.setPen(QColor(QStringLiteral(MSH_TEXT_DIM)));
        p.drawText(textRect, Qt::AlignRight | Qt::AlignVCenter, echoText);
        if (!statusText_.isEmpty()) {
            const qreal side = qMax(metrics.horizontalAdvance(selfText),
                                    metrics.horizontalAdvance(echoText)) + 8.0;
            const QRectF mid(side, 0.0, qMax(0.0, width() - side * 2.0), 12.0);
            p.setPen(QColor(QStringLiteral(MSH_TEXT)));
            p.drawText(mid, Qt::AlignCenter,
                       metrics.elidedText(statusText_, Qt::ElideRight, mid.width()));
        }

        const QRectF well(0.5, 15.0, qMax(1.0, width() - 1.0), 18.0);
        const qreal radius = well.height() * 0.45;
        const qreal seamX = well.left() + well.width() * displayRatio_;
        QColor seam = palette_.seam;

        QPainterPath trough;
        trough.addRoundedRect(well, radius, radius);

        p.setPen(Qt::NoPen);
        p.setBrush(palette_.well);
        p.drawPath(trough);

        p.save();
        p.setClipPath(trough);

        {
            QLinearGradient selfBody(well.topLeft(), QPointF(seamX, well.center().y()));
            selfBody.setColorAt(0.00, palette_.selfFar);
            selfBody.setColorAt(0.45, palette_.selfMid);
            selfBody.setColorAt(0.88, palette_.selfNear);
            selfBody.setColorAt(1.00, palette_.selfNear.lighter(115));
            p.setBrush(selfBody);
            p.drawRect(QRectF(well.left(), well.top(),
                              qMax(0.0, seamX - well.left()), well.height()));
        }

        {
            QLinearGradient echoBody(well.topRight(), QPointF(seamX, well.center().y()));
            echoBody.setColorAt(0.00, palette_.echoFar);
            echoBody.setColorAt(0.45, palette_.echoMid);
            echoBody.setColorAt(0.88, palette_.echoNear);
            echoBody.setColorAt(1.00, palette_.echoNear.lighter(130));
            p.setBrush(echoBody);
            p.drawRect(QRectF(seamX, well.top(),
                              qMax(0.0, well.right() - seamX), well.height()));
        }

        p.setBrush(Qt::NoBrush);
        for (int i = 1; i <= 7; ++i) {
            const qreal x = well.left() + well.width() * i / 8.0;
            const bool selfSide = x < seamX;
            p.setPen(QPen(QColor(255, 255, 255, selfSide ? 18 : 10), 0.7));
            p.drawLine(QPointF(x, well.top() + 1.5),
                       QPointF(x, well.bottom() - 1.5));
        }

        p.restore();

        {
            QColor bloomCore = seam.lighter(160);
            bloomCore.setAlpha(210);
            QColor bloomMid = seam;
            bloomMid.setAlpha(140);
            QColor bloomEdge = seam;
            bloomEdge.setAlpha(50);
            QColor bloomGone = seam;
            bloomGone.setAlpha(0);
            QRadialGradient bloom(QPointF(seamX, well.center().y()),
                                  well.height() * 1.35);
            bloom.setColorAt(0.00, QColor(255, 255, 255, 210));
            bloom.setColorAt(0.22, bloomCore);
            bloom.setColorAt(0.55, bloomEdge);
            bloom.setColorAt(1.00, bloomGone);
            p.setPen(Qt::NoPen);
            p.setBrush(bloom);
            p.drawEllipse(QPointF(seamX, well.center().y()),
                          well.height() * 0.95, well.height() * 0.85);

            QLinearGradient blade(QPointF(seamX, well.top() - 1.0),
                                  QPointF(seamX, well.bottom() + 1.0));
            blade.setColorAt(0.00, QColor(255, 255, 255, 0));
            blade.setColorAt(0.50, QColor(255, 255, 255, 230));
            blade.setColorAt(1.00, QColor(255, 255, 255, 0));
            p.setPen(QPen(QBrush(blade), 1.6, Qt::SolidLine, Qt::RoundCap));
            p.drawLine(QPointF(seamX, well.top() + 0.8),
                       QPointF(seamX, well.bottom() - 0.8));
        }

        p.setBrush(Qt::NoBrush);
        QColor rim = seam;
        rim.setAlpha(90);
        p.setPen(QPen(rim, 1.0));
        p.drawRoundedRect(well, radius, radius);
        p.setPen(QPen(QColor(255, 255, 255, 35), 1.0));
        p.drawLine(QPointF(well.left() + radius, well.top() + 1.0),
                   QPointF(well.right() - radius, well.top() + 1.0));
    }

private:
    PieceRenderer::GlassMeterPalette palette_;
    QVariantAnimation *anim_ = nullptr;
    qreal displayRatio_ = 0.5;
    bool hasRatio_ = false;
    int self_ = 0;
    int echo_ = 0;
    QString statusText_;
};

class MainWindowImpl final : public QMainWindow {
    Q_DISABLE_COPY_MOVE(MainWindowImpl)
public:
#ifdef NETCHESSZX_PC_MQTT_TX_FAILURE_TEST
    QTcpSocket *testSocket() const
    {
        return socket_;
    }

    bool testPrepareMqttGuestSession()
    {
        const QSignalBlocker directBlocker(directRadio_);
        const QSignalBlocker mqttBlocker(mqttRadio_);
        directRadio_->setChecked(false);
        mqttRadio_->setChecked(true);
        pcIsHost_ = false;
        mqttRoom_ = QStringLiteral("room");
        mqttSessionId_ = 0u;
        clearMqttSubscriptionState();
        mqttActiveSubscriptions_.insert(QStringLiteral("meta"));
        mqttTargetSubscriptions_ = mqttActiveSubscriptions_;
        mqttSubscribed_ = true;
        testMqttWriteFailure_ = false;
        if (!initializeMqttSession()) {
            return false;
        }
        mqttSessionLinked_ = true;
        (void)sessionController_.linkUp(kMqttLinkId);
        return true;
    }

    bool testPrepareMqttHostSession()
    {
        const QSignalBlocker directBlocker(directRadio_);
        const QSignalBlocker mqttBlocker(mqttRadio_);
        directRadio_->setChecked(false);
        mqttRadio_->setChecked(true);
        pcIsHost_ = true;
        hostPlaysWhite_ = true;
        pcPlaysWhite_ = true;
        mqttRoom_ = QStringLiteral("room");
        mqttSessionId_ = 77u;
        clearMqttSubscriptionState();
        testMqttWriteFailure_ = false;
        if (!initializeMqttSession()) {
            return false;
        }
        mqttActiveSubscriptions_.insert(QStringLiteral("meta"));
        mqttActiveSubscriptions_.insert(mqttInSuffix());
        mqttActiveSubscriptions_.insert(mqttInAckSuffix());
        mqttActiveSubscriptions_.insert(mqttPresenceSuffix());
        mqttActiveSubscriptions_.insert(mqttPeerPresenceSuffix());
        mqttTargetSubscriptions_ = mqttActiveSubscriptions_;
        mqttSubscribed_ = true;
        mqttSessionLinked_ = true;
        setConnectedUi(true);
        (void)sessionController_.linkUp(kMqttLinkId);
        return true;
    }

    bool testBlackHostOpeningContract()
    {
        roleHostRadio_->setChecked(true);
        hostSelfFirstRadio_->setChecked(true);
        configureSessionFromUi();
        startGameFromAck();
        const bool hostOk = pcIsHost_ && !hostPlaysWhite_ && !pcPlaysWhite_ &&
                            pcTurn_ && ms_rules_side() == MS_SIDE_B &&
                            ms_rules_can_play("d3") == MS_OK;

        stopGameClock();
        ++pieceRevealGeneration_;
        pieceRevealTimer_.stop();
        roleGuestRadio_->setChecked(true);
        configureSessionFromUi();
        return hostOk && !pcIsHost_ && !hostPlaysWhite_ && pcPlaysWhite_;
    }

    bool testEndAndRelinkMqttSession()
    {
        (void)sessionController_.linkDown(kMqttLinkId);
        if (!sessionController_.initialized()) {
            return false;
        }
        (void)sessionController_.linkUp(kMqttLinkId);
        return sessionController_.initialized();
    }

    bool testPrepareMqttGuestBootstrap()
    {
        const QSignalBlocker directBlocker(directRadio_);
        const QSignalBlocker mqttBlocker(mqttRadio_);
        directRadio_->setChecked(false);
        mqttRadio_->setChecked(true);
        pcIsHost_ = false;
        mqttRoom_ = QStringLiteral("room");
        mqttSessionId_ = 0u;
        clearMqttSubscriptionState();
        testMqttWriteFailure_ = false;
        return initializeMqttSession();
    }

    bool testBeginMqttRestore()
    {
        const QByteArray snapshot(SESSION_RESTORE_BYTES, 'A');

        return submitSessionLocalRequest(SESSION_REQUEST_RESTORE, 0u,
                                         snapshot, SESSION_PHASE_READY) &&
               directUiBusy_ == SESSION_REQUEST_RESTORE;
    }

    bool testRestoreUiIdle() const
    {
        return directUiBusy_ == 0u && directDecisionRequestId_ == 0u &&
               directDecisionControl_ == 0u && directDecisionBox_ == nullptr;
    }

    bool testTakebackModalRechecksState()
    {
        const TakebackSnapshot originalSnapshot = takebackSnapshot_;
        const bool originalClockRunning = gameClockRunning_;
        const bool originalGameOver = gameOver_;
        const int originalNextPly = nextPly_;

        gameClockRunning_ = true;
        gameOver_ = false;
        nextPly_ = 2;
        takebackSnapshot_ = TakebackSnapshot{};
        takebackSnapshot_.valid = true;
        takebackSnapshot_.localMove = true;
        takebackSnapshot_.ply = 1;
        chatEdit_->setText(QStringLiteral("/takeback"));
        QTimer::singleShot(0, this, [this]() {
            clearTakebackState();
            if (auto *box = qobject_cast<QMessageBox *>(
                    QApplication::activeModalWidget())) {
                box->done(QMessageBox::Yes);
            }
        });
        sendChat();
        const bool rejected = directUiBusy_ == 0u &&
                              statusMessage_ == QStringLiteral("No move to take back");

        takebackSnapshot_ = originalSnapshot;
        gameClockRunning_ = originalClockRunning;
        gameOver_ = originalGameOver;
        nextPly_ = originalNextPly;
        chatEdit_->clear();
        refreshChatButton();
        return rejected;
    }

    bool testUiRecoveryContracts()
    {
        resetGame(QStringLiteral("test reset"));
        boardPiecesVisible_ = true;
        pcTurn_ = true;
        refreshLegalMoves();
        const bool hadLegalTargets = !legalTargets_.isEmpty();
        clearSelection();
        showDestinationFeedback(2, 3);
        directUiBusy_ = SESSION_REQUEST_MOVE;
        handleDirectControlResult(SESSION_REQUEST_MOVE,
                                  SESSION_CONTROL_REJECTED);
        const bool rejectedMoveRecovered =
            !legalTargets_.isEmpty() && !feedbackTimer_.isActive() &&
            !feedbackOn_ && feedbackRow_ < 0 && feedbackCol_ < 0;

        chatEdit_->setText(QStringLiteral("/save"));
        const bool saveGated = !canSendChat();
        chatEdit_->setText(QStringLiteral("/load"));
        const bool loadGated = !canSendChat();

        startGameFromAck();
        const bool revealStarted = pieceRevealTimer_.isActive();
        showDestinationFeedback(2, 3);
        const bool played = applyMoveToBoard(QStringLiteral("c4"));
        const bool moveCancelledOtherEffects =
            !pieceRevealTimer_.isActive() && !feedbackTimer_.isActive() &&
            !feedbackOn_;
        refreshBoard(true);
        animateMirrorlockFlips();
        const bool flipStarted = pieceFlipTimer_.isActive();
        startGameFromAck();
        const bool startCancelledFlip = !pieceFlipTimer_.isActive() &&
                                        pieceFlipCells_.isEmpty();

        ++pieceRevealGeneration_;
        pieceRevealTimer_.stop();
        stopGameClock();
        resetBoard();
        boardPiecesVisible_ = true;
        pcTurn_ = true;
        nextPly_ = 1;
        const bool snapshotSaved = saveTakebackSnapshot(1, true);
        const bool takebackMovePlayed = applyMoveToBoard(QStringLiteral("c4"));
        refreshBoard(true);
        animateMirrorlockFlips();
        const bool takebackFlipStarted = pieceFlipTimer_.isActive();
        restoreTakebackSnapshot();
        const bool takebackCancelledFlip = !pieceFlipTimer_.isActive() &&
                                           pieceFlipCells_.isEmpty();

        resetPromptOpen_ = true;
        takebackSnapshot_.valid = true;
        closeGameInteractions();
        const bool gameOverInteractionsClosed =
            !resetPromptOpen_ && !takebackSnapshot_.valid;
        const bool abyssWaitsForFlip =
            kGameOverAbyssDelayMs >= kPieceFlipFrames * kPieceFlipFrameMs;

        resetGame(QStringLiteral("test complete"));
        chatEdit_->clear();
        const bool ok = hadLegalTargets && rejectedMoveRecovered &&
                        saveGated && loadGated && revealStarted && played &&
                        moveCancelledOtherEffects && flipStarted &&
                        startCancelledFlip && snapshotSaved &&
                        takebackMovePlayed && takebackFlipStarted &&
                        takebackCancelledFlip && gameOverInteractionsClosed &&
                        abyssWaitsForFlip;
        if (!ok) {
            qWarning("UI recovery contracts failed");
        }
        return ok;
    }

    bool testTransportPortPersistence()
    {
        directRadio_->setChecked(true);
        portSpin_->setValue(5001);
        mqttRadio_->setChecked(true);
        portSpin_->setValue(1884);
        directRadio_->setChecked(true);
        const bool directRestored = portSpin_->value() == 5001;
        mqttRadio_->setChecked(true);
        const bool mqttRestored = portSpin_->value() == 1884;
        return directRestored && mqttRestored;
    }

    void testFeedMqtt(const QByteArray &suffix,
                      const QByteArray &payload,
                      bool retained)
    {
        handleMqttPayload(topicFor(QString::fromLatin1(suffix)).toLatin1(),
                          payload, retained);
    }

    void testSetMqttWriteFailure(bool enabled)
    {
        testMqttWriteFailure_ = enabled;
    }

    bool testSessionReady() const
    {
        return directSessionReady_;
    }

    QString testStatusContextText() const
    {
        return statusContextText();
    }

    bool testStatusBarAligned()
    {
        const bool wasVisible = isVisible();
        if (!wasVisible) {
            show();
        }
        QCoreApplication::processEvents();
        alignStatusBarToControls();
        QCoreApplication::processEvents();

        const int leftAnchor =
            flipBoardButton_->mapToGlobal(QPoint(0, 0)).x();
        const int leftStatus =
            statusStateLabel_->mapToGlobal(QPoint(0, 0)).x();
        const int rightAnchor = logToggleButton_->mapToGlobal(
            QPoint(logToggleButton_->width(), 0)).x();
        const int rightStatus = moveClockLabel_->mapToGlobal(
            QPoint(moveClockLabel_->width(), 0)).x();
        const bool aligned = qAbs(leftStatus - leftAnchor) <= 1 &&
                             qAbs(rightStatus - rightAnchor) <= 1;
        if (!wasVisible) {
            hide();
        }
        return aligned;
    }

    bool testDisconnectButtonAvailable() const
    {
        return connectButton_ != nullptr && connectButton_->isEnabled() &&
               connectButton_->text() == QStringLiteral("Disconnect");
    }

    bool testSharedBoardOrientation()
    {
        boardOrientationManual_ = false;
        boardStandardOrientation_ = true;
        refreshBoard();
        const unsigned refreshes = testBoardRefreshes_;
        handleDirectSideChanged(SESSION_COLOR_BLACK);
        const bool blackSideStandard = boardStandardOrientation_;
        handleDirectSideChanged(SESSION_COLOR_WHITE);
        mqttSideReady_ = true;
        handleMqttSideChanged(SESSION_COLOR_WHITE, mqttSessionId_);
        const bool unchanged = blackSideStandard && boardStandardOrientation_ &&
            testBoardRefreshes_ == refreshes && coordinatesInitialized_ &&
            displayFileLabel(0) == QStringLiteral("A") &&
            displayRankLabel(0) == QStringLiteral("1") &&
            boardRowForDisplay(0) == 7 && boardColForDisplay(0) == 0;

        boardOrientationManual_ = true;
        boardStandardOrientation_ = false;
        refreshBoard();
        handleDirectSideChanged(SESSION_COLOR_BLACK);
        const bool manualPreserved = !boardStandardOrientation_ &&
            testBoardRefreshes_ == refreshes + 1;
        boardOrientationManual_ = false;
        handleDirectSideChanged(SESSION_COLOR_WHITE);
        return unchanged && manualPreserved && boardStandardOrientation_ &&
               testBoardRefreshes_ == refreshes + 2;
    }

    QByteArray testMqttClientId(bool host) const
    {
        return mqttClientIdFor(host, mqttClientNonce_);
    }

    void testHandleMqttPacket(const QByteArray &packet)
    {
        handleMqttPacket(packet);
    }

    QHash<uint16_t, QString> testMqttPendingSubacks() const
    {
        return mqttSubackPending_;
    }

    QHash<uint16_t, QString> testMqttPendingUnsubacks() const
    {
        return mqttUnsubackPending_;
    }

    QSet<QString> testMqttActiveSubscriptions() const
    {
        return mqttActiveSubscriptions_;
    }

    bool testMqttOperational() const
    {
        return mqttSubscribed_ && mqttSideReady_;
    }

    void testStartDirectGuestConnection(const QString &host, quint16 port)
    {
        directRadio_->setChecked(true);
        roleGuestRadio_->setChecked(true);
        hostEdit_->setText(host);
        portSpin_->setValue(port);
        connectToOpponent();
    }

    bool testDirectRetryPending() const
    {
        return directConnectRetryActive_ && directConnectRetryCount_ == 1 &&
               directConnectRetryTimer_->isActive();
    }

    bool testStartDirectHostListener(quint16 port)
    {
        directRadio_->setChecked(true);
        roleHostRadio_->setChecked(true);
        portSpin_->setValue(port);
        connectToOpponent();
        return directServer_ != nullptr && directServer_->isListening();
    }

    bool testDirectListenerActive() const
    {
        return directServer_ != nullptr && directServer_->isListening();
    }

    void testClickConnectButton()
    {
        connectButton_->click();
    }

    bool testReplaceDirectClientBeforeDisconnect()
    {
        QTcpSocket *oldSocket = socket_;
        if (oldSocket == nullptr || directServer_ == nullptr) {
            return false;
        }

        const QSignalBlocker serverBlocker(directServer_);
        if (!directServer_->hasPendingConnections() &&
            !directServer_->waitForNewConnection(2000)) {
            return false;
        }
        {
            const QSignalBlocker socketBlocker(oldSocket);
            if (oldSocket->bytesAvailable() == 0 &&
                !oldSocket->waitForReadyRead(2000)) {
                return false;
            }
            consumeReadyRead(oldSocket);
        }
        if (directPrimaryLinkId_ != SESSION_LINK_NONE) {
            return false;
        }
        acceptDirectClient();
        QCoreApplication::sendPostedEvents(oldSocket, QEvent::DeferredDelete);
        return directSockets_.size() == 1 &&
               directSockets_.constBegin().value() == socket_;
    }

    bool testResignRestartUiProjection()
    {
        gameOver_ = true;
        gameClockRunning_ = false;
        directLocalResignPending_ = true;
        directResignRestartPending_ = true;
        directUiBusy_ = SESSION_REQUEST_RESIGN;
        chatEdit_->setText(QStringLiteral("/resign"));
        sendChat();
        const bool pendingBlocked =
            statusMessage_ ==
            QString::fromLatin1(NETCHESSZX_UI_NOTICE_WAITING_RESIGN_ACK);
        handleDirectControlResult(SESSION_REQUEST_RESIGN,
                                  SESSION_CONTROL_ACCEPTED);
        chatEdit_->setText(QStringLiteral("/resign"));
        sendChat();
        const bool completedBlocked =
            statusMessage_ ==
            QString::fromLatin1(NETCHESSZX_UI_NOTICE_RESIGN_ALREADY_APPLIED);
        handleDirectControlResult(SESSION_REQUEST_RESET,
                                  SESSION_CONTROL_REJECTED);
        const bool failedRestart =
            statusMessage_ ==
            QString::fromLatin1(NETCHESSZX_UI_ERROR_RESTART_FAILED_GAME_OVER);
        ms_rules_reset();
        netchesszx_save_state_t resignedSave = {};
        const bool resignedSaveBlocked = !currentSaveState(&resignedSave, false) &&
                                          !canSaveGameFile();
        gameOver_ = false;
        gameClockRunning_ = true;
        directResignRestartPending_ = false;
        directUiBusy_ = 0u;
        applyDirectResignTransition();
        const bool remoteResign =
            statusMessage_ ==
                QString::fromLatin1(NETCHESSZX_UI_EVENT_OPPONENT_RESIGN) &&
            directUiBusy_ == SESSION_REQUEST_RESET &&
            startGameButton_->text() == QStringLiteral("Restarting...") &&
            !startGameButton_->isEnabled();
        const bool controlHighlighted =
            chatLogEdit_->currentCharFormat().foreground().color() ==
            QColor(0xff, 0x5a, 0x5a);
        appendChat(pcChatName(), QStringLiteral("hello"));
        const bool regularChatNormal =
            chatLogEdit_->currentCharFormat().foreground().color() ==
            QColor(0xe8, 0xee, 0xf6);
        const bool chatSharesControls =
            !isSessionControlCommand(QStringLiteral("/draw")) &&
            chatCanSharePendingControl(QStringLiteral("hello"),
                                       SESSION_REQUEST_RESET, false, false) &&
            chatCanSharePendingControl(QStringLiteral("hello"),
                                       SESSION_REQUEST_RESIGN, false, false) &&
            !chatCanSharePendingControl(QStringLiteral("/resign"),
                                         SESSION_REQUEST_RESET, false, false);
        directLocalResignPending_ = false;
        directResignRestartPending_ = false;
        directUiBusy_ = 0u;
        gameOver_ = false;
        chatEdit_->clear();
        setConnectedUi(false);
        return pendingBlocked && completedBlocked && failedRestart &&
               resignedSaveBlocked && remoteResign && controlHighlighted && regularChatNormal &&
               chatSharesControls;
    }

    bool testDesktopPresentation()
    {
        chatEdit_->setText(QStringLiteral("draft echo"));
        refreshChatButton();
        const bool controls = !takebackButton_->isEnabled() &&
                              !resetIconButton_->isEnabled();
        takebackButton_->click();
        const bool draft = chatEdit_->text() == QStringLiteral("draft echo");
        layoutChatCountLabel();
        const bool counter = chatCountLabel_->parentWidget() == chatEdit_ &&
            chatCountLabel_->testAttribute(Qt::WA_TransparentForMouseEvents) &&
            !chatCountLabel_->isHidden();
        chatEdit_->setText(QString(kChatTextMax, QLatin1Char('W')));
        const bool overlap = chatCountLabel_->isHidden();
        chatEdit_->clear();

        ms_rules_reset();
        syncBoardFromRules();
        clearSelection();
        boardPiecesVisible_ = true;
        refreshBoard();
        const bool played = applyMoveToBoard(QStringLiteral("c4"));
        const unsigned before = testSquareRefreshes_;
        refreshBoard(true);
        const unsigned changed = testSquareRefreshes_ - before;
        QImage projected[8][8];
        for (int r = 0; r < 8; ++r) {
            for (int c = 0; c < 8; ++c) {
                projected[r][c] = squares_[r][c]->icon().pixmap(kBoardSquareSize).toImage();
            }
        }
        refreshBoard();
        bool matches = true;
        for (int r = 0; r < 8; ++r) {
            for (int c = 0; c < 8; ++c) {
                matches = matches && projected[r][c] ==
                    squares_[r][c]->icon().pixmap(kBoardSquareSize).toImage();
            }
        }
        const bool flipEndpointMatches =
            PieceRenderer::boardSquareIconFlip(
                MS_CELL_A, MS_CELL_B, 4, 3,
                kBoardSquareSize, kPieceIconSize, M_PI)
                .pixmap(kBoardSquareSize).toImage() ==
            PieceRenderer::boardSquareIcon(
                MS_CELL_B, 4, 3, kBoardSquareSize, kPieceIconSize,
                true, false, false)
                .pixmap(kBoardSquareSize).toImage();
        animateMirrorlockFlips();
        const bool animationStarted = pieceFlipTimer_.isActive();
        for (int frame = 0; frame < kPieceFlipFrames; ++frame) {
            advanceMirrorlockFlip();
        }
        const bool animationStopped = !pieceFlipTimer_.isActive();
        resetBoard();
        clearSelection();
        refreshBoard();
        const QString capture = qEnvironmentVariable("MIRRORSHIFT_UI_CAPTURE");
        if (!capture.isEmpty()) {
            grab().save(capture);
        }
        const bool ok = controls && draft && counter && overlap && played &&
                        changed == 2 && matches && flipEndpointMatches &&
                        animationStarted && animationStopped;
        if (!ok) {
            qWarning("Presentation: controls=%d draft=%d counter=%d overlap=%d played=%d changed=%u pixels=%d endpoint=%d start=%d stop=%d",
                     controls, draft, counter, overlap, played, changed, matches,
                     flipEndpointMatches, animationStarted, animationStopped);
        }
        return ok;
    }

    bool testRestoredMoveProjection()
    {
        bool ok = true;
        const auto cellText = [this](int row, int col) {
            QTableWidgetItem *item = moveTable_->item(row, col);
            return item != nullptr ? item->text() : QString();
        };
        // A saved placement count cannot identify the last player after Silence.
        // Restore adds a neutral log event; only actual moves enter the table.
        for (uint8_t side : {MS_SIDE_A, MS_SIDE_B}) {
            ms_rules_reset();
            netchesszx_save_state_t state = {};
            std::memcpy(state.cells, ms_rules_cells(), sizeof(state.cells));
            state.side = side;
            state.ply = 58u;
            state.flags = NETCHESSZX_SAVE_FLAG_ACTIVE;
            if (!restoreApplyState(state) || !moveHistoryRecords_.isEmpty()) {
                ok = false;
                break;
            }
            appendMoveRecord(59, side, QStringLiteral("d3"), QString());
            ok = moveTable_->rowCount() == 1 &&
                 cellText(0, side == MS_SIDE_B ? 1 : 2) == QStringLiteral("D3") &&
                 cellText(0, side == MS_SIDE_B ? 2 : 1).isEmpty();
            if (!ok) {
                break;
            }
        }
        if (ok) {
            clearMoveHistory();
            appendMoveRecord(17, MS_SIDE_A, QStringLiteral("d3"), QString());
            appendMoveRecord(18, MS_SIDE_A, QStringLiteral("c4"), QString());
            ok = moveTable_->rowCount() == 2 &&
                 cellText(0, 1).isEmpty() &&
                 cellText(0, 2) == QStringLiteral("D3") &&
                 cellText(1, 1).isEmpty() &&
                 cellText(1, 2) == QStringLiteral("C4");
        }
        clearMoveHistory();
        setConnectedUi(false);
        return ok;
    }
#endif

    MainWindowImpl()
        : sessionController_(this)
    {
        setWindowTitle(QStringLiteral("Mirror Shift %1").arg(
            QString::fromLatin1(kAppVersion)));
        setStyleSheet(appStyleSheet());
        pieceRevealTimer_.setSingleShot(true);
        connect(&pieceRevealTimer_, &QTimer::timeout, this, [this]() {
            revealBoardRing(pieceRevealGeneration_, pieceRevealRing_);
            if (++pieceRevealRing_ < 4) {
                pieceRevealTimer_.start(kPieceRevealStepMs +
                    (pieceRevealRing_ == 1 ? kPieceRevealMiddlePauseMs : 0));
            }
        });
        connect(&pieceFlipTimer_, &QTimer::timeout, this, [this]() {
            advanceMirrorlockFlip();
        });
        connect(&feedbackTimer_, &QTimer::timeout, this, [this]() {
            feedbackOn_ = (++feedbackStep_ % 2) == 0;
            const int row = feedbackRow_, col = feedbackCol_;
            if (feedbackStep_ == 5) {
                feedbackTimer_.stop();
                feedbackRow_ = feedbackCol_ = -1;
                feedbackOn_ = false;
            }
            refreshBoardSquareVisual(row, col);
        });
        resetBoard();
        PieceRenderer::prewarmPieceIcons();
        auto *root = new QWidget(this);
        auto *layout = new QVBoxLayout(root);
        layout->setContentsMargins(10, 10, 10, 6);
        layout->setSpacing(10);
        auto *banner = new AppBanner(root);
        banner->clicked = [this]() {
            showAboutDialog();
        };
        layout->addWidget(banner);
        auto *topCard = new QWidget(root);
        topCard->setObjectName(QStringLiteral("topCard"));
        auto *topRows = new QVBoxLayout(topCard);
        topRows->setContentsMargins(10, 8, 10, 8);
        topRows->setSpacing(6);
        auto *connectionRow = new QHBoxLayout();
        connectionRow->setContentsMargins(0, 0, 0, 0);
        connectionRow->setSpacing(0);
        auto *sessionRow = new QHBoxLayout();
        sessionRow->setContentsMargins(0, 0, 0, 0);
        sessionRow->setSpacing(6);

        QSettings settings;

        directRadio_ = new QRadioButton("Direct", root);
        mqttRadio_ = new QRadioButton("MQTT", root);
        auto *transportGroup = new QButtonGroup(root);
        transportGroup->addButton(directRadio_);
        transportGroup->addButton(mqttRadio_);
        const bool useMqtt = settings.value("connection/mqtt", false).toBool();
        directRadio_->setChecked(!useMqtt);
        mqttRadio_->setChecked(useMqtt);

        hostEdit_ = new QLineEdit(root);
        hostEdit_->setPlaceholderText(useMqtt ? "MQTT broker" : "Opponent IP");
        QString savedHost = settings.value("connection/host",
                                           useMqtt ? "broker.hivemq.com" : "192.168.0.").toString();
        if (useMqtt && savedHost == "test.mosquitto.org") {
            savedHost = "broker.hivemq.com";
        }
        const QStringList savedDirectIpHistory =
            settings.value(kDirectIpHistorySettingsKey).toStringList();
        const QStringList directIpHistory = directIpHistoryWith(
            savedDirectIpHistory, useMqtt ? QString() : savedHost);
        if (directIpHistory != savedDirectIpHistory) {
            settings.setValue(kDirectIpHistorySettingsKey, directIpHistory);
        }
        if (useMqtt) {
            mqttBrokerCache_ = savedHost;
            directIpCache_ = directIpHistory.value(0, QStringLiteral("192.168.0."));
        } else {
            directIpCache_ = directIpHistory.value(0, savedHost);
        }
        hostEdit_->setText(useMqtt ? savedHost : directIpCache_);
        hostEdit_->setAccessibleName(QStringLiteral("Host"));
        hostEdit_->setClearButtonEnabled(true);
        hostEdit_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        directIpHistory_ = directIpHistory;

        directIpHistoryAction_ = hostEdit_->addAction(
            directIpHistoryIcon(), QLineEdit::LeadingPosition);
        directIpHistoryAction_->setText(QStringLiteral("Saved Direct IPs"));
        directIpHistoryAction_->setToolTip(QStringLiteral("Saved Direct IPs"));
        hostEdit_->setMinimumWidth(hostEdit_->sizeHint().width());
        directIpHistoryMenu_ = new QMenu(hostEdit_);
        directIpHistoryMenu_->setStyleSheet(
            "QMenu { background:" MSH_SURFACE "; color:" MSH_TEXT ";"
            " border:1px solid " MSH_BORDER "; border-radius:" MSH_R "; padding:4px; }"
            "QMenu::item { min-width:140px; padding:7px 12px; border-radius:4px; }"
            "QMenu::item:selected { background:" MSH_ACCENT_DEEP "; color:" MSH_WELL "; }");

        portSpin_ = new QSpinBox(root);
        portSpin_->setAccessibleName(QStringLiteral("Port"));
        portSpin_->setRange(1, 65535);
        const int savedPort = settings.value("connection/port",
                                             useMqtt ? 1883 : 5000).toInt();
        directPortCache_ = settings.value(kDirectPortSettingsKey,
                                          useMqtt ? 5000 : savedPort).toInt();
        mqttPortCache_ = settings.value(kMqttPortSettingsKey,
                                        useMqtt ? savedPort : 1883).toInt();
        portSpin_->setValue(useMqtt ? mqttPortCache_ : directPortCache_);
        portSpin_->setMinimumWidth(76);

        roomEdit_ = new QLineEdit(root);
        roomEdit_->setAccessibleName(QStringLiteral("Room"));
        roomEdit_->setPlaceholderText("MS0000");
        // Preserve legacy room text so validation cannot silently join a
        // different room by truncating a stored eight-character name.
        roomEdit_->setMaxLength(8);
        roomEdit_->setText(settings.value("connection/room", "MS0000").toString());
        roomEdit_->setClearButtonEnabled(true);
        roomEdit_->setMinimumWidth(100);

        connectButton_ = new QPushButton("Connect", root);
        startGameButton_ = new QPushButton("Start Game", root);
        startGameButton_->setEnabled(false);
        resetButton_ = new QPushButton("Reset Game", root);
        restoreButton_ = new QPushButton("Load Game", root);
        restoreButton_->setVisible(false);
        restoreButton_->setEnabled(false);
        configureActionButton(connectButton_);
        configureActionButton(startGameButton_);
        configureActionButton(resetButton_);
        configureActionButton(restoreButton_);

        roleHostRadio_ = new QRadioButton("Host", root);
        roleGuestRadio_ = new QRadioButton("Guest", root);
        auto *roleGroup = new QButtonGroup(root);
        roleGroup->addButton(roleHostRadio_);
        roleGroup->addButton(roleGuestRadio_);
        const bool pcIsHost = settings.value("connection/pcHost", false).toBool();
        roleHostRadio_->setChecked(pcIsHost);
        roleGuestRadio_->setChecked(!pcIsHost);

        hostSelfFirstRadio_ = new QRadioButton("SELF FIRST", root);
        hostEchoFirstRadio_ = new QRadioButton("ECHO FIRST", root);
        auto *colorGroup = new QButtonGroup(root);
        colorGroup->addButton(hostSelfFirstRadio_);
        colorGroup->addButton(hostEchoFirstRadio_);
        const bool hostWhite = settings.value("connection/hostWhite", false).toBool();
        hostEchoFirstRadio_->setChecked(hostWhite);
        hostSelfFirstRadio_->setChecked(!hostWhite);
        auto *connectionWidget = new QWidget(root);
        connectionWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        auto *connectionControls = new QHBoxLayout(connectionWidget);
        connectionControls->setContentsMargins(0, 0, 0, 0);
        connectionControls->setSpacing(8);
        connectionControls->addWidget(directRadio_);
        connectionControls->addWidget(mqttRadio_);
        hostCaptionLabel_ = captionLabel("LOCAL IP", root);
        hostCaptionLabel_->setFixedWidth(hostCaptionLabel_->fontMetrics().horizontalAdvance(QStringLiteral("LOCAL IP")));
        roomCaptionLabel_ = captionLabel("Room", root);
        connectionControls->addWidget(hostCaptionLabel_);
        connectionControls->addWidget(hostEdit_);
        connectionControls->addWidget(captionLabel("Port", root));
        connectionControls->addWidget(portSpin_);
        connectionControls->addWidget(roomCaptionLabel_);
        connectionControls->addWidget(roomEdit_);
        connectionRow->addWidget(connectionWidget, 1);
        connectionRow->addStretch(1);
        connectionRow->addWidget(connectButton_);
        auto *actionWidget = new QWidget(root);
        actionWidget->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        auto *actionRow = new QHBoxLayout(actionWidget);
        actionRow->setContentsMargins(0, 0, 0, 0);
        actionRow->setSpacing(6);
        actionRow->addWidget(startGameButton_);
        actionRow->addWidget(resetButton_);
        actionRow->addWidget(restoreButton_);

        auto *roleWidget = new QWidget(root);
        roleWidget->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        auto *roleRow = new QHBoxLayout(roleWidget);
        roleRow->setContentsMargins(0, 0, 0, 0);
        roleRow->setSpacing(8);
        roleRow->addWidget(captionLabel("Role", root), 0, Qt::AlignBaseline);
        roleRow->addWidget(roleHostRadio_, 0, Qt::AlignBaseline);
        roleRow->addWidget(roleGuestRadio_, 0, Qt::AlignBaseline);
        roleRow->addSpacing(10);

        hostColorLabel_ = captionLabel("Align", root);
        roleRow->addWidget(hostColorLabel_, 0, Qt::AlignBaseline);
        roleRow->addWidget(hostSelfFirstRadio_, 0, Qt::AlignBaseline);
        roleRow->addWidget(hostEchoFirstRadio_, 0, Qt::AlignBaseline);
        sessionRow->addWidget(roleWidget);
        sessionRow->addStretch(1);
        sessionRow->addWidget(actionWidget);
        topRows->addLayout(connectionRow);
        topRows->addLayout(sessionRow);
        layout->addWidget(topCard);

        auto *mainRow = new QHBoxLayout();
        mainRow->setContentsMargins(0, 0, 0, 0);
        mainRow->setSpacing(10);

        auto *boardWidget = new QWidget(root);
        boardWidget->setObjectName("boardFrame");
        boardFrame_ = boardWidget;
        boardWidget->setFixedSize(kBoardCoordSize * 2 + kBoardSquareSize * 8 + 2,
                                  kBoardCoordSize * 2 + kBoardSquareSize * 8 + 2);
        boardWidget->setStyleSheet(boardFrameStyle());
        auto *boardLayout = new QGridLayout(boardWidget);
        boardLayout->setContentsMargins(1, 1, 1, 1);
        boardLayout->setSpacing(0);
        addBoardCoordinates(boardLayout, boardWidget);
        for (int row = 0; row < 8; ++row) {
            for (int col = 0; col < 8; ++col) {
                auto *button = new QPushButton(boardWidget);
                button->setFixedSize(kBoardSquareSize, kBoardSquareSize);
                button->setIconSize(QSize(kBoardSquareSize, kBoardSquareSize));
                button->setFocusPolicy(Qt::StrongFocus);
                connect(button, &QPushButton::clicked, this, [this, row, col]() {
                    squareClicked(row, col);
                });
                squares_[row][col] = button;
                setSquareStyle(row, col, squareStyle(row, col, false, false, false, false));
                boardLayout->addWidget(button, row + 1, col + 1);
            }
        }
        flipBoardButton_ = new QPushButton(QString(QChar(0x21bb)), boardWidget);
        flipBoardButton_->setAccessibleName(QStringLiteral("Flip board"));
        flipBoardButton_->setToolTip("Flip board");
        flipBoardButton_->setFixedSize(kBoardCoordSize - 4, kBoardCoordSize - 4);
        flipBoardButton_->setFocusPolicy(Qt::StrongFocus);
        flipBoardButton_->setGeometry(2,
                                      boardWidget->height() - kBoardCoordSize + 2,
                                      kBoardCoordSize - 4,
                                      kBoardCoordSize - 4);
        flipBoardButton_->setStyleSheet(
            "QPushButton { background:" MSH_QUIET "; color:" MSH_ACCENT ";"
            " border:1px solid " MSH_BORDER "; border-radius:" MSH_R ";"
            " font-weight:700; font-size:11px; padding:0; min-height:0; }"
            "QPushButton:hover { background:" MSH_SURFACE_ALT ";"
            " border-color:" MSH_ACCENT "; color:" MSH_TEXT "; }");
        flipBoardButton_->raise();

        // Paradox abyss overlay (PARADOX / RESIGN).
        collapseOverlay_ = new BoardCollapseOverlay(boardWidget);
        collapseOverlay_->setGeometry(0, 0, boardWidget->width(), boardWidget->height());
        collapseOverlay_->hide();

        auto *sideWidget = new QWidget(root);
        sideWidget->setObjectName(QStringLiteral("sidePanel"));
        sideWidget->setFixedSize(kSidePanelWidth, boardWidget->height());
        auto *sidePanel = new QVBoxLayout(sideWidget);
        sidePanel->setContentsMargins(12, 8, 12, 8);
        sidePanel->setSpacing(5);

        sidePanel->addWidget(captionLabel("Status", root));

        turnCard_ = new QWidget(root);
        turnCard_->setObjectName(QStringLiteral("turnCard"));
        turnCard_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        auto *turnCol = new QVBoxLayout(turnCard_);
        turnCol->setContentsMargins(8, 4, 8, 4);
        turnCol->setSpacing(0);
        turnStack_ = new QStackedWidget(turnCard_);
        turnStack_->setFrameShape(QFrame::NoFrame);
        turnStack_->setContentsMargins(0, 0, 0, 0);
        turnStack_->setFixedHeight(kTurnSlotHeight);
        turnStack_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        turnLabel_ = new QLabel("OFFLINE", turnStack_);
        turnLabel_->setAlignment(Qt::AlignCenter);
        turnLabel_->setWordWrap(false);
        turnLabel_->setFixedHeight(kTurnSlotHeight);
        realityMeter_ = new RealityMeter(turnStack_);
        turnStack_->addWidget(turnLabel_);
        turnStack_->addWidget(realityMeter_);
        turnCol->addWidget(turnStack_);
        // Style before the first sizeHint so the 1 px card border is already
        // in the layout; swapping the meter later must not move ECHOES/MOVES.
        setWidgetStyle(turnCard_, turnCardStyle(MSH_DISABLED, MSH_TEXT_DIM));
        sidePanel->addWidget(turnCard_);

        selectedLabel_ = new QLabel(root);
        selectedLabel_->hide();
        selectedLabel_->setStyleSheet(
            "QLabel { color:" MSH_TEXT_DIM "; font-size:10px; }");

        showHintsCheck_ = new QCheckBox("Enabled", root);
        showHintsCheck_->setAccessibleName(QStringLiteral("Show legal move hints"));
        showHintsCheck_->setChecked(settings.value("ui/showHints", true).toBool());
        showHintsCheck_->setStyleSheet(
            "QCheckBox { color:" MSH_TEXT_DIM "; font-size:10px; spacing:6px; }"
            "QCheckBox::indicator { width:10px; height:10px; border-radius:3px;"
            " border:1px solid " MSH_TEXT_MUTED "; background:" MSH_SURFACE "; }"
            "QCheckBox::indicator:hover { border-color:" MSH_ACCENT "; }"
            "QCheckBox::indicator:checked { background:" MSH_ACCENT ";"
            " border:1px solid " MSH_ACCENT "; }"
            "QCheckBox::indicator:disabled { background:" MSH_DISABLED ";"
            " border-color:" MSH_BORDER "; }"
            "QCheckBox::indicator:checked:disabled { background:" MSH_TEXT_MUTED ";"
            " border:1px solid " MSH_TEXT_MUTED "; }"
            "QCheckBox:checked:disabled { color:" MSH_TEXT_DIM "; }"
        );

        auto *settingsButton = new QToolButton(root);
        settingsButton->setText(QString(QChar(0x2699)));
        settingsButton->setAccessibleName(QStringLiteral("Settings"));
        settingsButton->setToolTip("Settings");
        settingsButton->setPopupMode(QToolButton::InstantPopup);
        settingsButton->setFixedSize(32, 32);
        settingsButton->setFocusPolicy(Qt::StrongFocus);
        settingsButton->setStyleSheet(
            "QToolButton { background:" MSH_QUIET "; color:" MSH_ACCENT ";"
            " border:1px solid " MSH_BORDER "; border-radius:" MSH_R ";"
            " font-weight:700; font-size:13px; padding:0; }"
            "QToolButton::menu-indicator { image:none; width:0; }"
            "QToolButton:hover { background:" MSH_SURFACE_ALT ";"
            " border-color:" MSH_ACCENT "; color:" MSH_TEXT "; }");

        auto *chatHeaderRow = new QHBoxLayout();
        chatHeaderRow->setContentsMargins(0, 0, 0, 0);
        chatHeaderRow->setSpacing(6);
        chatHeaderRow->addWidget(captionLabel("ECHOES", root));
        chatHeaderRow->addStretch(1);
        actionRow->insertWidget(2, settingsButton);
        sidePanel->addLayout(chatHeaderRow);
        chatLogEdit_ = new QPlainTextEdit(root);
        chatLogEdit_->setAccessibleName(QStringLiteral("Echoes log"));
        chatLogEdit_->setReadOnly(true);
        chatLogEdit_->setFixedHeight(126);
        chatLogEdit_->setLineWrapMode(QPlainTextEdit::WidgetWidth);
        chatLogEdit_->setStyleSheet(
            "QPlainTextEdit { background:" MSH_SURFACE "; color:" MSH_TEXT ";"
            " font-size:10pt; border:1px solid " MSH_BORDER ";"
            " border-radius:" MSH_R "; padding:7px; }");
        chatLogEdit_->document()->setMaximumBlockCount(200);
        sidePanel->addWidget(chatLogEdit_);

        auto *chatControls = new QVBoxLayout();
        chatControls->setSpacing(4);

        chatEdit_ = new QLineEdit(root);
        chatEdit_->setAccessibleName(QStringLiteral("Echo or alignment"));
        chatEdit_->setPlaceholderText("Message or alignment (d3)");
        chatEdit_->setClearButtonEnabled(true);
        chatEdit_->setMaxLength(kChatTextMax);
        chatEdit_->setMinimumHeight(32);
        chatEdit_->setStyleSheet(
            "QLineEdit { background:" MSH_QUIET "; color:" MSH_TEXT ";"
            " border:1px solid " MSH_ACCENT "; border-radius:" MSH_R ";"
            " padding:4px 8px; selection-background-color:" MSH_ACCENT_DEEP ";"
            " selection-color:" MSH_WELL "; }"
            "QLineEdit:focus { background:" MSH_SURFACE ";"
            " border:1px solid " MSH_ACCENT_SOFT "; }");
        chatEdit_->installEventFilter(this);
        moveEdit_ = chatEdit_;
        chatButton_ = new QPushButton("SEND", root);
        chatButton_->setEnabled(false);
        configureActionButton(chatButton_);
        chatButton_->setMinimumSize(72, 32);
        setWidgetStyle(chatButton_, chatButtonStyle(false));

        chatCountLabel_ = new QLabel(chatEdit_);
        chatCountLabel_->setAccessibleName(QStringLiteral("Echo character count"));
        chatCountLabel_->setAttribute(Qt::WA_TransparentForMouseEvents, true);
        chatCountLabel_->setStyleSheet(
            "QLabel { color:" MSH_TEXT_MUTED "; font-size:9px;"
            " background:transparent; padding:0; }");
        chatCountLabel_->setText(QStringLiteral("0/%1").arg(kChatTextMax));
        takebackButton_ = new QPushButton(root);
        resetIconButton_ = new QPushButton(root);
        takebackButton_->setIcon(sessionActionIcon("takeback"));
        resetIconButton_->setIcon(sessionActionIcon("reset"));
        takebackButton_->setAccessibleName(QStringLiteral("Request takeback"));
        resetIconButton_->setAccessibleName(QStringLiteral("Reset game"));
        takebackButton_->setToolTip(QStringLiteral("Takeback"));
        resetIconButton_->setToolTip(QStringLiteral("Reset game"));
        const QString iconButtonStyle =
            "QPushButton { background:" MSH_QUIET "; color:" MSH_ACCENT ";"
            " border:1px solid " MSH_BORDER "; border-radius:" MSH_R ";"
            " padding:0; min-height:0; min-width:0; }"
            "QPushButton:hover { background:" MSH_SURFACE_ALT ";"
            " border-color:" MSH_ACCENT "; }"
            "QPushButton:disabled { background:" MSH_DISABLED ";"
            " border-color:" MSH_DISABLED "; }";
        for (QPushButton *button : {takebackButton_, resetIconButton_}) {
            button->setIconSize(QSize(16, 16));
            button->setFixedSize(32, 32);
            button->setFocusPolicy(Qt::StrongFocus);
            button->setAutoDefault(false);
            button->setDefault(false);
            button->setStyleSheet(iconButtonStyle);
        }
        connect(takebackButton_, &QPushButton::clicked, this, [this]() {
            (void)requestTakeback();
            refreshSessionCommandButtons();
        });
        connect(resetIconButton_, &QPushButton::clicked, this, [this]() {
            if (resetButton_ != nullptr) {
                resetButton_->click();
            }
        });
        auto *chatActionRow = new QHBoxLayout();
        chatActionRow->setSpacing(6);
        chatActionRow->addWidget(takebackButton_);
        chatActionRow->addWidget(resetIconButton_);
        chatActionRow->addStretch(1);
        chatActionRow->addWidget(chatButton_);
        chatControls->addWidget(chatEdit_);
        chatControls->addLayout(chatActionRow);
        sidePanel->addLayout(chatControls);
        boardCombo_ = new QComboBox(root);
        boardCombo_->setAccessibleName(QStringLiteral("Lattice surface"));
        boardCombo_->setToolTip("Lattice surface");
        for (const QString &id : PieceRenderer::glassBoardThemeIds()) {
            boardCombo_->addItem(PieceRenderer::glassBoardThemeTitle(id), id);
        }

        auto *settingsMenu = new QMenu(settingsButton);
        settingsMenu->setStyleSheet(
            "QMenu { background:" MSH_SURFACE "; color:" MSH_TEXT ";"
            " border:1px solid " MSH_BORDER "; border-radius:" MSH_R "; }");
        auto *settingsPanel = new QWidget(settingsMenu);
        auto *settingsLayout = new QGridLayout(settingsPanel);
        settingsLayout->setContentsMargins(8, 8, 8, 8);
        settingsLayout->setHorizontalSpacing(8);
        settingsLayout->setVerticalSpacing(6);
        settingsLayout->addWidget(captionLabel("Lattice", settingsPanel), 0, 0);
        settingsLayout->addWidget(boardCombo_, 0, 1);
        settingsLayout->addWidget(captionLabel("Hints", settingsPanel), 1, 0);
        settingsLayout->addWidget(showHintsCheck_, 1, 1);
        settingsLayout->setColumnStretch(1, 1);
        auto *settingsAction = new QWidgetAction(settingsMenu);
        settingsAction->setDefaultWidget(settingsPanel);
        settingsMenu->addAction(settingsAction);
        settingsButton->setMenu(settingsMenu);
        connect(boardCombo_, &QComboBox::currentIndexChanged, this, [this]() { applyBoardTexture(); });
        // Restore saved preferences
        {
            const QSignalBlocker boardBlocker(boardCombo_);
            // Prefer a saved glass theme; fall back to Glass Ice.
            {
                const QString savedBoard = settings.value(
                    QStringLiteral("appearance/board"),
                    QStringLiteral("glass-lattice")).toString();
                QString boardId = savedBoard;
                if (!PieceRenderer::isGlassBoardTheme(boardId)) {
                    boardId = QStringLiteral("glass-lattice");
                }
                const int idx = boardCombo_->findData(boardId);
                if (idx >= 0) {
                    boardCombo_->setCurrentIndex(idx);
                }
            }
        }
        applyBoardTexture();

        logTitleLabel_ = captionLabel("Moves", root);
        sidePanel->addWidget(logTitleLabel_);

        logStack_ = new QStackedWidget(root);
        logStack_->setMinimumHeight(96);
        logStack_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

        moveTable_ = new QTableWidget(logStack_);
        moveTable_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Ignored);
        moveTable_->setColumnCount(3);
        moveTable_->setHorizontalHeaderLabels({QString(), "SELF", "ECHO"});
        moveTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
        moveTable_->setAlternatingRowColors(true);
        moveTable_->setFocusPolicy(Qt::NoFocus);
        moveTable_->setSelectionMode(QAbstractItemView::NoSelection);
        moveTable_->setShowGrid(false);
        moveTable_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        moveTable_->verticalHeader()->hide();
        moveTable_->verticalHeader()->setDefaultSectionSize(18);
        moveTable_->horizontalHeader()->setFixedHeight(21);
        moveTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
        moveTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
        moveTable_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
        moveTable_->setStyleSheet(
            QStringLiteral(
                "QTableWidget { background:" MSH_SURFACE "; color:" MSH_TEXT ";"
                " alternate-background-color:" MSH_ROW_ALT ";"
                " font-family:\"%1\"; font-size:9pt;"
                " border:1px solid " MSH_BORDER "; border-radius:" MSH_R "; }"
                "QTableWidget::item { padding:0 6px;"
                " border-bottom:1px solid " MSH_BORDER_SOFT "; border-right:0; }"
                "QHeaderView::section { background:" MSH_SURFACE ";"
                " color:" MSH_TEXT "; font-family:\"%1\"; font-weight:700;"
                " font-size:8pt; letter-spacing:1px; border:0;"
                " border-bottom:1px solid " MSH_BORDER "; padding:0 6px; }")
                .arg(UiTheme::monoFamily()));
        logStack_->addWidget(moveTable_);

        logEdit_ = new QTextEdit(logStack_);
        logEdit_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Ignored);
        logEdit_->setReadOnly(true);
        logEdit_->setLineWrapMode(QTextEdit::WidgetWidth);
        logEdit_->setStyleSheet(
            QStringLiteral(
                "QTextEdit { background:" MSH_SURFACE "; color:" MSH_TEXT ";"
                " font-family:\"%1\"; font-size:9pt;"
                " border:1px solid " MSH_BORDER "; border-radius:" MSH_R ";"
                " padding:6px; }")
                .arg(UiTheme::monoFamily()));
        logEdit_->document()->setDocumentMargin(0);
        logEdit_->document()->setMaximumBlockCount(400);
        logStack_->addWidget(logEdit_);
        sidePanel->addWidget(logStack_, 1);

        auto *logActionRow = new QHBoxLayout();
        logActionRow->setContentsMargins(0, 0, 0, 0);
        logActionRow->setSpacing(6);
        saveGameButton_ = new QPushButton(root);
        saveGameButton_->setAccessibleName(QStringLiteral("Save game"));
        saveGameButton_->setToolTip("Save game");
        saveGameButton_->setIcon(style()->standardIcon(QStyle::SP_DialogSaveButton));
        saveGameButton_->setIconSize(QSize(16, 16));
        saveGameButton_->setMinimumSize(32, 32);
        saveGameButton_->setFocusPolicy(Qt::StrongFocus);
        saveGameButton_->setEnabled(false);
        loadGameButton_ = new QPushButton(root);
        loadGameButton_->setAccessibleName(QStringLiteral("Load game"));
        loadGameButton_->setToolTip("Load game");
        loadGameButton_->setIcon(style()->standardIcon(QStyle::SP_DialogOpenButton));
        loadGameButton_->setIconSize(QSize(16, 16));
        loadGameButton_->setMinimumSize(32, 32);
        loadGameButton_->setFocusPolicy(Qt::StrongFocus);
        loadGameButton_->setEnabled(false);
        logToggleButton_ = new QPushButton("Log", root);
        logToggleButton_->setMinimumSize(kActionButtonWidth, 32);
        logActionRow->addStretch(1);
        logActionRow->addWidget(saveGameButton_);
        logActionRow->addWidget(loadGameButton_);
        logActionRow->addWidget(logToggleButton_);
        sidePanel->addLayout(logActionRow);

        mainRow->addWidget(boardWidget, 0, Qt::AlignTop);
        mainRow->addWidget(sideWidget, 1, Qt::AlignTop);
        layout->addLayout(mainRow);
        setCentralWidget(root);
        socket_ = new QTcpSocket(this);
        directServer_ = new QTcpServer(this);
        statusBarContents_ = new QWidget(statusBar());
        statusBarLayout_ = new QHBoxLayout(statusBarContents_);
        statusBarLayout_->setContentsMargins(0, 0, 0, 0);
        statusBarLayout_->setSpacing(6);
        statusStateLabel_ = new QLabel("DISCONNECTED", statusBarContents_);
        statusContextLabel_ = new QLabel(QString(), statusBarContents_);
        gameClockLabel_ = new QLabel("GAME --:--", statusBarContents_);
        moveClockLabel_ = new QLabel("MOVE --:--", statusBarContents_);
        statusStateLabel_->setMinimumWidth(120);
        statusContextLabel_->setSizePolicy(QSizePolicy::Expanding,
                                           QSizePolicy::Preferred);
        gameClockLabel_->setMinimumWidth(80);
        moveClockLabel_->setMinimumWidth(80);
        statusContextLabel_->setStyleSheet(
            "QLabel { color:" MSH_ACCENT "; font-weight:600; font-size:11px; }");
        statusContextLabel_->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        gameClockLabel_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        moveClockLabel_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        statusBar()->setSizeGripEnabled(false);
        statusBarLayout_->addWidget(statusStateLabel_);
        statusBarLayout_->addWidget(statusContextLabel_, 1);
        statusBarLayout_->addWidget(gameClockLabel_);
        statusBarLayout_->addWidget(moveClockLabel_);
        statusBar()->addWidget(statusBarContents_, 1);

        configureSessionFromUi();
        if (isMqttMode() && pcIsHost_) {
            roomEdit_->setText(generateMqttRoomCode());
        }
        updateSessionControlsEnabled();
        refreshTurnLabel();
        updateConnectionModeUi();
        renderLogView();
        refreshStatusBar();
        resizeToContent();

        clockTimer_ = new QTimer(this);
        connect(clockTimer_, &QTimer::timeout, this, [this]() {
            checkUiStall();
            updateClockLabels();
            checkConnectionHealth();
        });
        uiTickTimer_.start();
        clockTimer_->start(1000);

        directConnectRetryTimer_ = new QTimer(this);
        directConnectRetryTimer_->setSingleShot(true);
        directConnectRetryTimer_->setInterval(kDirectConnectRetryIntervalMs);
        connect(directConnectRetryTimer_, &QTimer::timeout, this, [this]() {
            retryDirectConnection();
        });

        configureSessionControllerCallbacks();

        connect(connectButton_, &QPushButton::clicked, this, [this]() {
            if (isConnected()) {
                const QMessageBox::StandardButton answer = askQuestion(
                    this, QStringLiteral("Disconnect"),
                    QStringLiteral("Disconnect?"),
                    QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
                if (answer != QMessageBox::Yes || !isConnected()) {
                    return;
                }
                if (!isMqttMode() && pcIsHost_ && directServer_ != nullptr) {
                    directServer_->close();
                }
                if (isMqttMode()) {
                    localDisconnectPending_ = true;
                    directEndStatus_ = NETCHESSZX_UI_PHASE_DISCONNECTED;
                    if (!submitSessionLocalRequest(SESSION_REQUEST_BYE)) {
                        socket_->disconnectFromHost();
                    }
                } else {
                    directEndStatus_ = NETCHESSZX_UI_PHASE_DISCONNECTED;
                    if (!submitSessionLocalRequest(SESSION_REQUEST_BYE)) {
                        socket_->disconnectFromHost();
                    }
                }
                return;
            }
            if (isDirectListening()) {
                directServer_->close();
                setStatusText(NETCHESSZX_UI_NOTICE_LISTEN_CANCELLED);
                setConnectedUi(false);
                return;
            }
            if (isConnecting()) {
                cancelDirectConnectRetry();
                if (!isMqttMode() && pcIsHost_ && directServer_ != nullptr) {
                    directServer_->close();
                }
                if (socket_ != nullptr) {
                    if (!isMqttMode()) {
                        directEndStatus_ = NETCHESSZX_UI_PHASE_DISCONNECTED;
                    }
                    socket_->abort();
                }
                setStatusText(NETCHESSZX_UI_PHASE_DISCONNECTED);
                setConnectedUi(false);
                return;
            }
            connectToOpponent();
        });
        connect(flipBoardButton_, &QPushButton::clicked, this, [this]() {
            boardOrientationManual_ = true;
            boardStandardOrientation_ = !boardStandardOrientation_;
            coordinatesInitialized_ = false;
            refreshBoard();
        });
        connect(directRadio_, &QRadioButton::toggled, this, [this](bool checked) {
            if (checked) {
                cancelDirectConnectRetry();
                mqttPortCache_ = portSpin_->value();
                const QString host = hostEdit_->text().trimmed();
                if (!host.isEmpty() && looksLikeMqttHost(host)) {
                    mqttBrokerCache_ = host;
                }
                if (directIpCache_.isEmpty()) {
                    directIpCache_ = "192.168.0.";
                }
                hostEdit_->setPlaceholderText("Opponent IP");
                if (!pcIsHost_) {
                    hostEdit_->setText(directIpCache_);
                }
                portSpin_->setValue(directPortCache_);
                configureSessionFromUi();
                updateSessionControlsEnabled();
                updateConnectionModeUi();
                refreshStatusBar();
            }
        });
        connect(mqttRadio_, &QRadioButton::toggled, this, [this](bool checked) {
            if (checked) {
                cancelDirectConnectRetry();
                directPortCache_ = portSpin_->value();
                const QString host = hostEdit_->text().trimmed();
                if (!directShowingLocalHost_ && !host.isEmpty() && !looksLikeMqttHost(host)) {
                    directIpCache_ = host;
                }
                if (mqttBrokerCache_.isEmpty()) {
                    mqttBrokerCache_ = "broker.hivemq.com";
                }
                hostEdit_->setPlaceholderText("MQTT broker");
                hostEdit_->setText(mqttBrokerCache_);
                portSpin_->setValue(mqttPortCache_);
                configureSessionFromUi();
                if (pcIsHost_) {
                    roomEdit_->setText(generateMqttRoomCode());
                }
                updateSessionControlsEnabled();
                updateConnectionModeUi();
                refreshStatusBar();
            }
        });
        connect(roleHostRadio_, &QRadioButton::toggled, this, [this](bool checked) {
            cancelDirectConnectRetry();
            configureSessionFromUi();
            if (checked && isMqttMode()) {
                roomEdit_->setText(generateMqttRoomCode());
            }
            updateConnectionModeUi();
            refreshBoard();
            setConnectedUi(isConnected());
        });
        connect(roleGuestRadio_, &QRadioButton::toggled, this, [this]() {
            cancelDirectConnectRetry();
            configureSessionFromUi();
            updateConnectionModeUi();
            refreshBoard();
            setConnectedUi(isConnected());
        });
        connect(hostEchoFirstRadio_, &QRadioButton::toggled, this, [this]() {
            configureSessionFromUi();
            refreshBoard();
            setConnectedUi(isConnected());
        });
        connect(hostSelfFirstRadio_, &QRadioButton::toggled, this, [this]() {
            configureSessionFromUi();
            refreshBoard();
            setConnectedUi(isConnected());
        });
        #if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
        connect(showHintsCheck_, &QCheckBox::checkStateChanged, this, [this](Qt::CheckState state) {
        #else
        connect(showHintsCheck_, &QCheckBox::stateChanged, this, [this](int state) {
        #endif
            QSettings settings;
            settings.setValue("ui/showHints", state == Qt::Checked);
            refreshBoard();
        });
        connect(hostEdit_, &QLineEdit::textChanged, this, [this]() {
            cancelDirectConnectRetry();
            setConnectedUi(isConnected());
            refreshStatusBar();
        });
        connect(hostEdit_, &QLineEdit::returnPressed, this, [this]() {
            if (connectButton_->isEnabled()) {
                connectButton_->animateClick();
            }
        });
        connect(directIpHistoryAction_, &QAction::triggered, this, [this]() {
            if (!directIpHistory_.isEmpty()) {
                rebuildDirectIpHistoryMenu();
                directIpHistoryMenu_->popup(
                    hostEdit_->mapToGlobal(QPoint(0, hostEdit_->height())));
            }
        });
        connect(portSpin_, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int) {
            cancelDirectConnectRetry();
            setConnectedUi(isConnected());
            refreshStatusBar();
        });
        connect(roomEdit_, &QLineEdit::textChanged, this, [this]() {
            setConnectedUi(isConnected());
            refreshStatusBar();
        });
        connect(roomEdit_, &QLineEdit::returnPressed, this, [this]() {
            if (connectButton_->isEnabled()) {
                connectButton_->animateClick();
            }
        });
        connect(startGameButton_, &QPushButton::clicked, this, [this]() {
            if (gameOver_) {
                if (restoreBusy()) {
                    setStatusText("Load in progress");
                    return;
                }
                setStatusText(NETCHESSZX_UI_CONFIRM_RESTART_GAME);
                resetPromptOpen_ = true;
                const QMessageBox::StandardButton answer =
                    askQuestion(this, NETCHESSZX_UI_CONFIRM_PC_RESTART_TITLE,
                                NETCHESSZX_UI_CONFIRM_RESTART_GAME,
                                QMessageBox::Yes | QMessageBox::No);
                if (!resetPromptOpen_) {
                    return;
                }
                resetPromptOpen_ = false;
                if (answer != QMessageBox::Yes) {
                    disconnectToSetup();
                    return;
                }
                if (submitSessionLocalRequest(SESSION_REQUEST_RESET)) {
                    setStatusText(NETCHESSZX_UI_NOTICE_WAITING_RESTART_ACK);
                    setConnectedUi(true);
                }
                return;
            }
            sendGameStart();
        });
        connect(resetButton_, &QPushButton::clicked, this, [this]() {
            const bool connected = isConnected();
            if (connected) {
                if (restoreBusy()) {
                    setStatusText("Load in progress");
                    return;
                }
                resetPromptOpen_ = true;
                const QMessageBox::StandardButton answer =
                    askQuestion(this, NETCHESSZX_UI_CONFIRM_PC_RESET_TITLE,
                                NETCHESSZX_UI_CONFIRM_PC_RESET_REQUEST,
                                QMessageBox::Yes | QMessageBox::No);
                if (!resetPromptOpen_) {
                    return;
                }
                resetPromptOpen_ = false;
                if (answer != QMessageBox::Yes) {
                    return;
                }
                if (submitSessionLocalRequest(SESSION_REQUEST_RESET)) {
                    setStatusText(NETCHESSZX_UI_NOTICE_RESET_REQUESTED_ACK);
                    setConnectedUi(true);
                }
                return;
            }
            resetGame("Game reset");
            stopGameClock();
            setConnectedUi(connected);
        });
        connect(chatButton_, &QPushButton::clicked, this, [this]() {
            sendChat();
        });
        connect(chatEdit_, &QLineEdit::textChanged, this,
                [this](const QString &text) {
            chatInputHistoryIndex_ = static_cast<int>(chatInputHistory_.size());
            chatCountLabel_->setText(
                QStringLiteral("%1/%2").arg(text.size()).arg(kChatTextMax));
            layoutChatCountLabel();
            refreshChatButton();
        });
        connect(chatEdit_, &QLineEdit::returnPressed, this, [this]() {
            if (chatButton_->isEnabled()) {
                sendChat();
            }
        });
        connect(saveGameButton_, &QPushButton::clicked, this, [this]() {
            saveGameWithDialog();
        });
        connect(loadGameButton_, &QPushButton::clicked, this, [this]() {
            loadGameWithDialog();
        });
        connect(logToggleButton_, &QPushButton::clicked, this, [this]() {
            toggleLogView();
        });
        attachSocket(socket_);
        connect(directServer_, &QTcpServer::newConnection, this, [this]() {
            acceptDirectClient();
        });

        setConnectedUi(false);
        updateClockLabels();
        refreshBoard();
    }

    ~MainWindowImpl() override
    {
        for (QTcpSocket *sock : findChildren<QTcpSocket *>()) {
            QObject::disconnect(sock, nullptr, this, nullptr);
        }
    }

private:
    struct MoveRecord {
        int ply = 0;
        uint8_t side = MS_SIDE_B;
        QString move;
        QString notation;
    };

    struct TakebackSnapshot {
        char board[8][8] = {};
        ms_state_t rules = {};
        QString lastMove;
        int ply = 0;
        int nextPly = 1;
        int historyCount = 0;
        bool pcTurn = false;
        bool localMove = false;
        bool valid = false;
    };


    void setBoardCellsVisible(bool visible)
    {
        for (int row = 0; row < 8; ++row) {
            for (int col = 0; col < 8; ++col) {
                if (squares_[row][col] != nullptr) {
                    squares_[row][col]->setVisible(visible);
                }
            }
        }
        for (int i = 0; i < 8; ++i) {
            if (fileLabelsTop_[i] != nullptr) fileLabelsTop_[i]->setVisible(visible);
            if (fileLabelsBottom_[i] != nullptr) fileLabelsBottom_[i]->setVisible(visible);
            if (rankLabelsLeft_[i] != nullptr) rankLabelsLeft_[i]->setVisible(visible);
            if (rankLabelsRight_[i] != nullptr) rankLabelsRight_[i]->setVisible(visible);
        }
        if (flipBoardButton_ != nullptr) {
            flipBoardButton_->setVisible(visible);
        }
    }

    void setLatticeSquaresVisible(bool visible)
    {
        for (int row = 0; row < 8; ++row) {
            for (int col = 0; col < 8; ++col) {
                if (squares_[row][col] != nullptr) {
                    squares_[row][col]->setVisible(visible);
                }
            }
        }
    }

    QVector<QWidget *> latticeChromeWidgets() const
    {
        QVector<QWidget *> chrome;
        for (int i = 0; i < 8; ++i) {
            if (fileLabelsTop_[i] != nullptr) {
                chrome.append(fileLabelsTop_[i]);
            }
            if (fileLabelsBottom_[i] != nullptr) {
                chrome.append(fileLabelsBottom_[i]);
            }
            if (rankLabelsLeft_[i] != nullptr) {
                chrome.append(rankLabelsLeft_[i]);
            }
            if (rankLabelsRight_[i] != nullptr) {
                chrome.append(rankLabelsRight_[i]);
            }
        }
        if (flipBoardButton_ != nullptr) {
            chrome.append(flipBoardButton_);
        }
        return chrome;
    }

    void showLatticeChrome()
    {
        ++chromeFadeGeneration_; // invalidate in-flight fade-outs
        for (QWidget *w : latticeChromeWidgets()) {
            if (w == nullptr) {
                continue;
            }
            w->setGraphicsEffect(nullptr);
            w->setVisible(true);
        }
    }

    // Fade coords + flip icon as soon as the abyss starts.
    void fadeOutLatticeChrome()
    {
        constexpr int kFadeMs = 320;
        const int generation = ++chromeFadeGeneration_;
        for (QWidget *w : latticeChromeWidgets()) {
            if (w == nullptr || !w->isVisible()) {
                continue;
            }
            auto *effect = new QGraphicsOpacityEffect(w);
            effect->setOpacity(1.0);
            w->setGraphicsEffect(effect);
            auto *anim = new QPropertyAnimation(effect, "opacity", w);
            anim->setDuration(kFadeMs);
            anim->setStartValue(1.0);
            anim->setEndValue(0.0);
            anim->setEasingCurve(QEasingCurve::OutCubic);
            QObject::connect(anim, &QPropertyAnimation::finished, w,
                             [this, w, generation]() {
                if (generation != chromeFadeGeneration_) {
                    return;
                }
                w->setVisible(false);
                w->setGraphicsEffect(nullptr);
            });
            anim->start(QAbstractAnimation::DeleteWhenStopped);
        }
    }

    // Stop abyss (running or held) and restore lattice so Start/Reset/restore
    // never stack FX over a new board.
    void cancelParadoxAbyss()
    {
        ++abyssGeneration_;
        if (collapseOverlay_ != nullptr) {
            collapseOverlay_->abort();
        }
        setLatticeSquaresVisible(true);
        showLatticeChrome();
    }

    QRect latticePlayfieldRect() const
    {
        if (squares_[0][0] == nullptr || squares_[7][7] == nullptr) {
            return QRect();
        }
        return squares_[0][0]->geometry().united(squares_[7][7]->geometry());
    }

    // Keep abyss inside the frame after tiles vanish (until reset/start/restore).
    void playParadoxAbyss(int generation)
    {
        if (generation != abyssGeneration_) {
            return; // Start/Reset/restore already superseded this delayed trigger
        }
        if (boardFrame_ == nullptr || collapseOverlay_ == nullptr) {
            return;
        }
        // Replace any held abyss rather than stacking.
        if (collapseOverlay_->isRunning() || collapseOverlay_->isHoldingAbyss()) {
            collapseOverlay_->abort();
        }
        if (generation != abyssGeneration_) {
            return;
        }

        const QRect grid = latticePlayfieldRect();
        if (!grid.isValid() || grid.isEmpty()) {
            return;
        }

        finishMirrorlockFlip();
        collapseOverlay_->setGeometry(grid);

        QVector<BoardCollapseOverlay::Tile> tiles;
        tiles.reserve(64);
        auto *rng = QRandomGenerator::global();
        for (int row = 0; row < 8; ++row) {
            for (int col = 0; col < 8; ++col) {
                QPushButton *btn = squares_[row][col];
                if (btn == nullptr) {
                    continue;
                }
                BoardCollapseOverlay::Tile tile;
                tile.pixmap = btn->grab();
                tile.start = QRectF(btn->geometry().translated(-grid.topLeft()));
                const qreal dist = qHypot(col - 3.5, row - 3.5);
                // Outer tiles peel first; center hangs a breath longer.
                tile.delayMs = static_cast<int>(55.0 * (6.2 - dist))
                    + static_cast<int>(rng->bounded(0, 200));
                tile.fallMs = 920 + static_cast<int>(rng->bounded(0, 680));
                tile.spinDeg = rng->bounded(-16, 17);
                const qreal sign = rng->bounded(0, 2) == 0 ? -1.0 : 1.0;
                tile.tipDeg = sign * (10.0 + rng->bounded(0, 14));
                tile.rockPhase = rng->generateDouble() * 2.0 * M_PI;
                tile.rockHz = 1.15 + rng->generateDouble() * 1.1;
                tiles.append(tile);
            }
        }

        if (generation != abyssGeneration_) {
            return;
        }
        setLatticeSquaresVisible(false);
        fadeOutLatticeChrome();
        // Keep abyss inside the frame after tiles vanish (until reset/start/restore).
        collapseOverlay_->begin(std::move(tiles), true);
    }





    void closeEvent(QCloseEvent *event) override
    {
        if (isConnected()) {
            if (isMqttMode()) {
                directEndStatus_ = NETCHESSZX_UI_PHASE_DISCONNECTED;
                (void)submitSessionLocalRequest(SESSION_REQUEST_BYE);
            } else {
                directEndStatus_ = NETCHESSZX_UI_PHASE_DISCONNECTED;
                (void)submitSessionLocalRequest(SESSION_REQUEST_BYE);
            }
        }
        if (directServer_ != nullptr) {
            directServer_->close();
        }
        QMainWindow::closeEvent(event);
    }

    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (watched == chatEdit_ && event->type() == QEvent::Resize) {
            layoutChatCountLabel();
        }
        if (watched == chatEdit_ && event->type() == QEvent::KeyPress) {
            auto *keyEvent = static_cast<QKeyEvent *>(event);
            if ((keyEvent->key() == Qt::Key_Up || keyEvent->key() == Qt::Key_Down) &&
                !chatInputHistory_.isEmpty()) {
                const int delta = keyEvent->key() == Qt::Key_Up ? -1 : 1;
                const int historySize = static_cast<int>(chatInputHistory_.size());
                const int target = qBound(0, chatInputHistoryIndex_ + delta,
                                          historySize);
                chatEdit_->setText(target == historySize
                                       ? QString()
                                       : chatInputHistory_.at(target));
                chatInputHistoryIndex_ = target;
                chatEdit_->selectAll();
                return true;
            }
            const bool isReturn = keyEvent->key() == Qt::Key_Return ||
                                  keyEvent->key() == Qt::Key_Enter;
            if (isReturn && keyEvent->modifiers().testFlag(Qt::ControlModifier)) {
                if (chatButton_->isEnabled()) {
                    sendChat();
                }
                return true;
            }
        }
        return QMainWindow::eventFilter(watched, event);
    }

    static QString buildStamp()
    {
        return QStringLiteral(__DATE__ " " __TIME__);
    }

    void showAboutDialog()
    {
        static constexpr int kAboutWidth = 363;
        QDialog dialog(this);
        dialog.setWindowTitle("About Mirror Shift");
        dialog.setModal(true);
        dialog.setFixedWidth(kAboutWidth);
        dialog.setStyleSheet(
            "QDialog { background:" MSH_INK "; color:" MSH_TEXT "; }"
            "QLabel { color:" MSH_TEXT "; background:" MSH_INK "; }"
            "QLabel#muted { color:" MSH_TEXT_DIM "; }"
            "QLabel#link { color:" MSH_ACCENT "; }"
            "QPushButton { background:" MSH_SURFACE_ALT "; color:" MSH_TEXT ";"
            " border:1px solid " MSH_BORDER "; border-radius:" MSH_R ";"
            " padding:6px 18px; }"
            "QPushButton:hover { background:" MSH_HOVER "; }");

        auto *layout = new QVBoxLayout(&dialog);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(10);
        const QPixmap aboutPixmap(PieceRenderer::assetPath(
            QStringLiteral("assets/pc-client/about/mirrorshift-about.png")));
        if (!aboutPixmap.isNull()) {
            auto *artwork = new QLabel(&dialog);
            artwork->setAccessibleName(QStringLiteral("Mirror Shift artwork"));
            artwork->setAlignment(Qt::AlignCenter);
            artwork->setPixmap(aboutPixmap.scaledToWidth(
                kAboutWidth, Qt::SmoothTransformation));
            layout->addWidget(artwork);
        }

        auto *info = new QLabel(
            QString("<div style=\"color:" MSH_ACCENT "; font-weight:700; font-size:10pt;\">"
                    "Qt Client for Mirror Shift<br>"
                    "(C) 2026 M. Ignacio Monge Garcia<br>"
                    "Version %1 (Build %2)<br>"
                    "<a style=\"color:" MSH_ACCENT "; text-decoration:none;\" "
                    "href=\"https://github.com/IgnacioMonge/MirrorShift\">"
                    "github.com/IgnacioMonge/MirrorShift</a>"
                    "</div><br>"
                    "<div style=\"color:" MSH_TEXT_DIM "; font-size:9pt;\">"
                    "Mirror Shift, built on the Shatranj runtime: GNU GPL v2.0.<br>"
                    "Qt 6: LGPLv3 / GPLv2 / GPLv3.<br>"
                    "Board artwork: original Mirror Shift artwork."
                    "</div>")
                .arg(QString::fromLatin1(kAppVersion), buildStamp()),
            &dialog);
        info->setAlignment(Qt::AlignCenter);
        info->setTextFormat(Qt::RichText);
        info->setOpenExternalLinks(true);
        info->setWordWrap(true);
        info->setStyleSheet(
            "QLabel { color:" MSH_TEXT_DIM "; font-size:9pt;"
            " padding-left:28px; padding-right:28px; }");
        layout->addWidget(info);

        auto *buttonRow = new QHBoxLayout();
        buttonRow->setContentsMargins(0, 0, 0, 8);
        auto *closeButton = new QPushButton("Close", &dialog);
        connect(closeButton, &QPushButton::clicked, &dialog, &QDialog::accept);
        buttonRow->addWidget(closeButton, 0, Qt::AlignCenter);
        layout->addLayout(buttonRow);

        dialog.adjustSize();
#ifdef Q_OS_WIN
        dialog.createWinId();
        applyWinWindowChrome(&dialog, QColor(QStringLiteral(MSH_INK)));
#endif
        dialog.exec();
    }

    // The Lattice mirrors the rules core; the cell view is the single source of
    // truth for every renderer.
    void syncBoardFromRules()
    {
        const char *cells = ms_rules_cells();

        for (int row = 0; row < 8; ++row) {
            for (int col = 0; col < 8; ++col) {
                board_[row][col] = cells[row * 8 + col];
            }
        }
    }

    void resetBoard()
    {
        ms_rules_reset();
        syncBoardFromRules();
        refreshLegalMoves();
    }

    // Mirror Shift has no piece selection: every legal square is a one-click
    // Alignment, so the hint list is the whole move list for the side to move.
    void refreshLegalMoves()
    {
        char list[MS_MOVE_LIST_CAP];

        legalTargets_.clear();
        if (ms_rules_is_over() || !pcTurn_) {
            return;
        }
        if (ms_rules_legal_moves(list, sizeof(list)) <= 0) {
            return;
        }
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
        legalTargets_ = QString::fromLatin1(list).split(' ', Qt::SkipEmptyParts);
#else
        legalTargets_ = QString::fromLatin1(list).split(' ', QString::SkipEmptyParts);
#endif
    }

    quint16 squareVisualState(int row, int col) const
    {
        return static_cast<unsigned char>(board_[row][col]) |
            ((row == selectedRow_ && col == selectedCol_) << 8) |
            ((row == targetRow_ && col == targetCol_) << 9) |
            (isLegalTarget(row, col) << 10) |
            (isFeedbackSquare(row, col) << 11) |
            ((showHintsCheck_ == nullptr || showHintsCheck_->isChecked()) << 12) |
            (boardPiecesVisible_ << 13);
    }

    void refreshBoard(bool changedOnly = false)
    {
#ifdef NETCHESSZX_PC_MQTT_TX_FAILURE_TEST
        ++testBoardRefreshes_;
#endif
        refreshBoardCoordinates();
        for (int displayRow = 0; displayRow < 8; ++displayRow) {
            for (int displayCol = 0; displayCol < 8; ++displayCol) {
                const int boardRow = boardRowForDisplay(displayRow);
                const int boardCol = boardColForDisplay(displayCol);
                const quint16 visual = squareVisualState(boardRow, boardCol);
                if (changedOnly && squareVisualCache_[displayRow][displayCol] == visual) {
                    continue;
                }
                squareVisualCache_[displayRow][displayCol] = visual;
#ifdef NETCHESSZX_PC_MQTT_TX_FAILURE_TEST
                ++testSquareRefreshes_;
#endif
                const bool selected = (boardRow == selectedRow_ && boardCol == selectedCol_);
                const bool target = (boardRow == targetRow_ && boardCol == targetCol_);
                const bool legalTarget = isLegalTarget(boardRow, boardCol);
                const bool feedback = isFeedbackSquare(boardRow, boardCol);
                squares_[displayRow][displayCol]->setAccessibleName(
                    QStringLiteral("Board square %1").arg(InputHelpers::squareName(boardRow, boardCol).toUpper()));
                squares_[displayRow][displayCol]->setText(QString());
                const bool hasPiece = (board_[boardRow][boardCol] != '.');
                const bool showHints = showHintsCheck_ ? showHintsCheck_->isChecked() : true;
                const bool styledBackground = selected || feedback ||
                    (target && hasPiece) ||
                    (legalTarget && showHints && hasPiece);
                const bool textured = !PieceRenderer::boardTexture().isEmpty() && !styledBackground;
                squares_[displayRow][displayCol]->setIconSize(
                    QSize(textured ? kBoardSquareSize : kPieceIconSize,
                          textured ? kBoardSquareSize : kPieceIconSize));
                squares_[displayRow][displayCol]->setIcon(
                    textured ? PieceRenderer::boardSquareIcon(board_[boardRow][boardCol], boardRow,
                                                              boardCol, kBoardSquareSize,
                                                              kPieceIconSize, boardPiecesVisible_,
                                                              legalTarget && showHints && !hasPiece,
                                                              target && !hasPiece)
                             : (boardPiecesVisible_
                                    ? PieceRenderer::pieceIcon(board_[boardRow][boardCol])
                                    : QIcon()));
                setSquareStyle(displayRow, displayCol,
                               squareStyle(boardRow, boardCol, selected, target,
                                           legalTarget, feedback, hasPiece, showHints));
            }
        }
    }

    static QLabel *coordLabel(const QString &text, QWidget *parent, const QSize &size)
    {
        auto *label = new QLabel(text, parent);
        label->setAlignment(Qt::AlignCenter);
        label->setFixedSize(size);
        label->setStyleSheet(
            "QLabel { color:" MSH_TEXT_DIM "; background:transparent;"
            " font-weight:700; font-size:12px; padding:0; margin:0; }");
        return label;
    }

    void addBoardCoordinates(QGridLayout *layout, QWidget *parent)
    {
        for (int col = 0; col < 8; ++col) {
            fileLabelsTop_[col] = coordLabel(QString(), parent,
                                             QSize(kBoardSquareSize, kBoardCoordSize));
            fileLabelsBottom_[col] = coordLabel(QString(), parent,
                                                QSize(kBoardSquareSize, kBoardCoordSize));
            layout->addWidget(fileLabelsTop_[col], 0, col + 1, Qt::AlignCenter);
            layout->addWidget(fileLabelsBottom_[col], 9, col + 1, Qt::AlignCenter);
        }

        for (int row = 0; row < 8; ++row) {
            rankLabelsLeft_[row] = coordLabel(QString(), parent,
                                              QSize(kBoardCoordSize, kBoardSquareSize));
            rankLabelsRight_[row] = coordLabel(QString(), parent,
                                               QSize(kBoardCoordSize, kBoardSquareSize));
            layout->addWidget(rankLabelsLeft_[row], row + 1, 0, Qt::AlignCenter);
            layout->addWidget(rankLabelsRight_[row], row + 1, 9, Qt::AlignCenter);
        }
        refreshBoardCoordinates(true);
    }

    // Mirror Shift notation: the aligned square, the Mirrorlock size, "!" when
    // the rival falls into Silence and "#" at the Convergence.
    QString moveNotation(const QString &move) const
    {
        QString notation = move;
        const uint8_t flips = ms_rules_last_flip_count();

        if (flips > 0u) {
            notation += QStringLiteral("x") + QString::number(flips);
        }
        if (ms_rules_last_silences() == 1u) {
            notation += QStringLiteral("!");
        }
        if (ms_rules_is_over()) {
            notation += QStringLiteral("#");
        }
        return notation;
    }

    QString scoreText() const
    {
        uint8_t a = 0;
        uint8_t b = 0;

        ms_rules_score(&a, &b);
        const uint8_t self = pcPlaysWhite_ ? a : b;
        const uint8_t echo = pcPlaysWhite_ ? b : a;
        return QString("SELF %1 - %2 ECHO").arg(self).arg(echo);
    }

    int boardRowForDisplay(int displayRow) const
    {
        return boardStandardOrientation_ ? 7 - displayRow : displayRow;
    }

    int boardColForDisplay(int displayCol) const
    {
        return boardStandardOrientation_ ? displayCol : 7 - displayCol;
    }

    int displayRowForBoard(int boardRow) const
    {
        return boardStandardOrientation_ ? 7 - boardRow : boardRow;
    }

    int displayColForBoard(int boardCol) const
    {
        return boardStandardOrientation_ ? boardCol : 7 - boardCol;
    }

    QString displayFileLabel(int displayCol) const
    {
        return QString(QChar('A' + boardColForDisplay(displayCol)));
    }

    QString displayRankLabel(int displayRow) const
    {
        return QString(QChar('8' - boardRowForDisplay(displayRow)));
    }

    void refreshMoveLogHeaders()
    {
        if (moveTable_ == nullptr) {
            return;
        }
        // Left column is the first mover (B). Label it SELF when this
        // client holds that side.
        if (pcPlaysWhite_) {
            moveTable_->setHorizontalHeaderLabels({QString(), "ECHO", "SELF"});
        } else {
            moveTable_->setHorizontalHeaderLabels({QString(), "SELF", "ECHO"});
        }
    }

    QString pcSideLetter() const
    {
        return pcPlaysWhite_ ? "A" : "B";
    }

    QString pcChatName() const
    {
        return QStringLiteral("SELF");
    }

    QString opponentChatName() const
    {
        return QStringLiteral("ECHO");
    }

    bool syncSharedBoardOrientation()
    {
        if (boardOrientationManual_ || boardStandardOrientation_) {
            return false;
        }
        boardStandardOrientation_ = true;
        coordinatesInitialized_ = false;
        return true;
    }

    void refreshBoardCoordinates(bool force = false)
    {
        if (!force && coordinatesInitialized_ &&
            lastCoordinateStandard_ == boardStandardOrientation_) {
            return;
        }
        coordinatesInitialized_ = true;
        lastCoordinateStandard_ = boardStandardOrientation_;

        for (int col = 0; col < 8; ++col) {
            const QString file = displayFileLabel(col);
            if (fileLabelsTop_[col] != nullptr) {
                fileLabelsTop_[col]->setText(file);
            }
            if (fileLabelsBottom_[col] != nullptr) {
                fileLabelsBottom_[col]->setText(file);
            }
        }
        for (int row = 0; row < 8; ++row) {
            const QString rank = displayRankLabel(row);
            if (rankLabelsLeft_[row] != nullptr) {
                rankLabelsLeft_[row]->setText(rank);
            }
            if (rankLabelsRight_[row] != nullptr) {
                rankLabelsRight_[row]->setText(rank);
            }
        }
    }

    bool isLegalTarget(int row, int col) const
    {
        const QString square = InputHelpers::squareName(row, col);

        for (const QString &target : legalTargets_) {
            if (target == square) {
                return true;
            }
        }

        return false;
    }

    void setSquareStyle(int displayRow, int displayCol, const QString &style)
    {
        if (displayRow < 0 || displayRow >= 8 || displayCol < 0 || displayCol >= 8 ||
            squares_[displayRow][displayCol] == nullptr ||
            squareStyleCache_[displayRow][displayCol] == style) {
            return;
        }

        squareStyleCache_[displayRow][displayCol] = style;
        squares_[displayRow][displayCol]->setStyleSheet(style);
    }

    void refreshBoardSquareStyle(int row, int col)
    {
        if (row < 0 || row >= 8 || col < 0 || col >= 8) {
            return;
        }

        const int displayRow = displayRowForBoard(row);
        const int displayCol = displayColForBoard(col);
        const bool selected = (row == selectedRow_ && col == selectedCol_);
        const bool target = (row == targetRow_ && col == targetCol_);
        const bool legalTarget = isLegalTarget(row, col);
        const bool feedback = isFeedbackSquare(row, col);
        const bool hasPiece = (board_[row][col] != '.');
        const bool showHints = showHintsCheck_ ? showHintsCheck_->isChecked() : true;
        setSquareStyle(displayRow, displayCol,
                       squareStyle(row, col, selected, target, legalTarget, feedback, hasPiece, showHints));
    }

    void refreshBoardSquareVisual(int row, int col)
    {
        if (row < 0 || row >= 8 || col < 0 || col >= 8) {
            return;
        }
        squareVisualCache_[displayRowForBoard(row)][displayColForBoard(col)] = squareVisualState(row, col);
        refreshBoardSquareStyle(row, col);
        refreshBoardSquareIcon(row, col, true);
    }

    void refreshBoardSquareIcon(int row, int col, bool visible)
    {
        if (row < 0 || row >= 8 || col < 0 || col >= 8) {
            return;
        }

        const int displayRow = displayRowForBoard(row);
        const int displayCol = displayColForBoard(col);
        const bool selected = (row == selectedRow_ && col == selectedCol_);
        const bool target = (row == targetRow_ && col == targetCol_);
        const bool legalTarget = isLegalTarget(row, col);
        const bool feedback = isFeedbackSquare(row, col);
        const bool hasPiece = (board_[row][col] != '.');
        const bool showHints = showHintsCheck_ ? showHintsCheck_->isChecked() : true;
        const bool styledBackground = selected || feedback ||
            (target && hasPiece) ||
            (legalTarget && showHints && hasPiece);
        const bool textured = !PieceRenderer::boardTexture().isEmpty() && !styledBackground;
        squares_[displayRow][displayCol]->setIconSize(
            QSize(textured ? kBoardSquareSize : kPieceIconSize,
                  textured ? kBoardSquareSize : kPieceIconSize));
        squares_[displayRow][displayCol]->setIcon(
            textured ? PieceRenderer::boardSquareIcon(board_[row][col], row, col,
                                                      kBoardSquareSize, kPieceIconSize,
                                                      boardPiecesVisible_ && visible,
                                                      legalTarget && showHints && !hasPiece,
                                                      target && !hasPiece)
                     : (boardPiecesVisible_ && visible
                            ? PieceRenderer::pieceIcon(board_[row][col])
                            : QIcon()));
    }

    void revealBoardRing(int generation, int ring)
    {
        if (generation != pieceRevealGeneration_) {
            return;
        }
        for (int row = 0; row < 8; ++row) {
            for (int col = 0; col < 8; ++col) {
                const int depth = qMin(qMin(row, 7 - row), qMin(col, 7 - col));
                if (3 - depth == ring) {
                    refreshBoardSquareIcon(row, col, true);
                }
            }
        }
    }

    // The Lattice settles from its centre outwards: the four seed fragments
    // first, then each surrounding ring of empty squares.
    void animateBoardPiecesIn()
    {
        ++pieceRevealGeneration_;
        pieceRevealTimer_.stop();
        pieceRevealRing_ = 0;

        boardPiecesVisible_ = true;
        for (int row = 0; row < 8; ++row) {
            for (int col = 0; col < 8; ++col) {
                refreshBoardSquareIcon(row, col, false);
            }
        }

        pieceRevealTimer_.start(0);
    }

    struct FlipAnim {
        int row = 0;
        int col = 0;
        char start = MS_CELL_EMPTY;
        char end = MS_CELL_EMPTY;
    };

    void stopMirrorlockFlip()
    {
        pieceFlipTimer_.stop();
        pieceFlipCells_.clear();
        pieceFlipStep_ = 0;
    }

    void finishMirrorlockFlip()
    {
        pieceFlipTimer_.stop();
        for (const FlipAnim &cell : pieceFlipCells_) {
            refreshBoardSquareIcon(cell.row, cell.col, true);
        }
        pieceFlipCells_.clear();
        pieceFlipStep_ = 0;
    }

    void animateMirrorlockFlips()
    {
        QVector<FlipAnim> cells;
        if (ms_rules_last_flip_count() > 0u) {
            const uint8_t *rows = ms_rules_last_flip_rows();
            for (int row = 0; row < 8; ++row) {
                for (int col = 0; col < 8; ++col) {
                    if ((rows[row] & static_cast<uint8_t>(1u << col)) == 0u) {
                        continue;
                    }
                    const char end = board_[row][col];
                    if (end == '.' || end == MS_CELL_EMPTY) {
                        continue;
                    }
                    cells.append(FlipAnim{
                        row, col,
                        (end == MS_CELL_A) ? MS_CELL_B : MS_CELL_A,
                        end});
                }
            }
        }
        const int8_t placed = ms_rules_last_square();
        if (placed >= 0 && placed < MS_CELLS) {
            const int row = placed / 8;
            const int col = placed % 8;
            const char end = board_[row][col];
            bool already = false;
            for (const FlipAnim &cell : cells) {
                if (cell.row == row && cell.col == col) {
                    already = true;
                    break;
                }
            }
            if (!already && end != '.' && end != MS_CELL_EMPTY) {
                cells.append(FlipAnim{row, col, MS_CELL_EMPTY, end});
            }
        }
        if (cells.isEmpty() || !boardPiecesVisible_) {
            return;
        }

        // refreshBoard() already committed final cells. Restore the pre-move
        // look in this same turn: vacant for the new disc, old face for
        // captures. Skip angle 0 so the first squash frame does not replace
        // a matching static icon.
        for (const FlipAnim &cell : cells) {
            board_[cell.row][cell.col] = cell.start;
            refreshBoardSquareIcon(cell.row, cell.col, true);
            board_[cell.row][cell.col] = cell.end;
        }

        stopMirrorlockFlip();
        pieceFlipCells_ = std::move(cells);
        pieceFlipStep_ = 0;
        pieceFlipTimer_.start(kPieceFlipFrameMs);
    }

    void advanceMirrorlockFlip()
    {
        ++pieceFlipStep_;
        if (pieceFlipStep_ == kPieceFlipFrames) {
            finishMirrorlockFlip();
            return;
        }
        const qreal t = static_cast<qreal>(pieceFlipStep_) /
                        static_cast<qreal>(kPieceFlipFrames);
        const qreal angle = t * t * (3.0 - 2.0 * t) * M_PI;

        for (const FlipAnim &cell : pieceFlipCells_) {
            const int displayRow = displayRowForBoard(cell.row);
            const int displayCol = displayColForBoard(cell.col);
            if (squares_[displayRow][displayCol] == nullptr) {
                continue;
            }
            squares_[displayRow][displayCol]->setIconSize(
                QSize(kBoardSquareSize, kBoardSquareSize));
            squares_[displayRow][displayCol]->setIcon(
                PieceRenderer::boardSquareIconFlip(
                    cell.start, cell.end, cell.row, cell.col,
                    kBoardSquareSize, kPieceIconSize, angle));
        }

    }

    void refreshBoardSquareStyleByName(const QString &square)
    {
        if (square.size() < 2) {
            return;
        }
        refreshBoardSquareVisual('8' - square[1].unicode(),
                                 square[0].unicode() - 'a');
    }

    void refreshSelectionFootprint(int oldSelectedRow,
                                   int oldSelectedCol,
                                   int oldTargetRow,
                                   int oldTargetCol,
                                   const QStringList &oldLegalTargets)
    {
        for (const QString &target : oldLegalTargets) {
            refreshBoardSquareStyleByName(target);
        }
        refreshBoardSquareVisual(oldSelectedRow, oldSelectedCol);
        refreshBoardSquareVisual(oldTargetRow, oldTargetCol);

        for (const QString &target : legalTargets_) {
            refreshBoardSquareStyleByName(target);
        }
        refreshBoardSquareVisual(selectedRow_, selectedCol_);
        refreshBoardSquareVisual(targetRow_, targetCol_);
        refreshTurnLabel();
    }

    void configureSessionControllerCallbacks()
    {
        DesktopSessionController::Callbacks callbacks;
        callbacks.mqttTransportReady = [this]() {
            return mqttSubscribed_ && mqttSideReady_;
        };

        callbacks.send = [this](DesktopSessionController::Mode mode,
                                const SessionAction &action,
                                const QByteArray &payload) {
            if (mode == DesktopSessionController::Mode::Mqtt) {
                const QByteArray suffix = sessionController_.mqttTopicSuffixForRoute(
                    action.data.send.route);
                return !suffix.isEmpty() &&
                    mqttPublish(QString::fromLatin1(suffix),
                                QString::fromLatin1(payload),
                                action.data.send.retained != 0u);
            }

            const QPointer<QTcpSocket> sock =
                directSocketForLink(action.data.send.link_id);
            QByteArray frame = payload;
            frame.append('\n');
            bool sent = sock != nullptr &&
                        sock->state() == QAbstractSocket::ConnectedState;
            if (sent) {
                const qint64 written = sock->write(frame);
                sent = written == frame.size();
                if (sent) {
                    sock->flush();
                    appendLog("TX: " + QString::fromLatin1(payload));
                } else {
                    appendLog(QString("ERROR: direct write %1/%2")
                                  .arg(written)
                                  .arg(frame.size()));
                }
            } else {
                appendLog("ERROR: direct link unavailable");
            }
            if (!sent && sock != nullptr) {
                sock->abort();
            }
            return sent;
        };
        callbacks.closeLink = [this](DesktopSessionController::Mode mode,
                                     uint8_t linkId) {
            if (mode == DesktopSessionController::Mode::Mqtt) {
                if (socket_ != nullptr) {
                    socket_->flush();
                    socket_->disconnectFromHost();
                }
                return;
            }
            const QPointer<QTcpSocket> sock = directSocketForLink(linkId);
            if (sock != nullptr) {
                sock->disconnectFromHost();
            }
        };
        callbacks.decision = [this](uint8_t requestId, uint8_t control,
                                    uint16_t value) {
            showDirectDecision(requestId, control, value);
        };
        callbacks.game = [this](DesktopSessionController::Mode,
                                uint8_t kind, uint8_t deliveryId,
                                uint16_t value, const QByteArray &payload,
                                QVector<DesktopSessionFollowup> &followups) {
            handleDirectGameAction(kind, deliveryId, value, payload, followups);
        };
        callbacks.sessionChanged = [this](uint8_t status, uint8_t reason) {
            handleDirectSessionChanged(status, reason);
        };
        callbacks.sideChanged = [this](DesktopSessionController::Mode mode,
                                       uint8_t color, uint16_t sessionId) {
            if (mode == DesktopSessionController::Mode::Mqtt) {
                handleMqttSideChanged(color, sessionId);
            } else {
                handleDirectSideChanged(color);
            }
        };
        callbacks.error = [this](const QString &message) {
            appendLog("ERROR: " + message);
        };
        sessionController_.setCallbacks(callbacks);
    }

    bool initializeDirectSession()
    {
        closeDirectDecisionPrompt();
        directSockets_.clear();
        transportCodec_.clear();
        directLinkUpSeen_.clear();
        directNextLinkId_ = 0u;
        directPrimaryLinkId_ = SESSION_LINK_NONE;
        directSessionReady_ = false;
        directUiBusy_ = false;
        directStartTransitionApplied_ = false;
        directLocalResignPending_ = false;
        directResignRestartPending_ = false;
        directEndStatus_.clear();
        resetPeerMachineState();

        const uint8_t role = pcIsHost_ ? SESSION_ROLE_HOST : SESSION_ROLE_GUEST;
        const uint8_t hostColor = pcIsHost_
                                      ? (hostPlaysWhite_ ? SESSION_COLOR_WHITE
                                                        : SESSION_COLOR_BLACK)
                                      : SESSION_COLOR_UNKNOWN;
        return sessionController_.initializeDirect(role, hostColor);
    }

    bool mqttSessionActive() const
    {
        return sessionController_.initialized() &&
               sessionController_.mode() == DesktopSessionController::Mode::Mqtt;
    }

    bool initializeMqttSession()
    {
        closeDirectDecisionPrompt();
        transportCodec_.clear();
        directSessionReady_ = false;
        directUiBusy_ = false;
        directStartTransitionApplied_ = false;
        directLocalResignPending_ = false;
        directResignRestartPending_ = false;
        mqttSessionLinked_ = false;
        mqttSideReady_ = pcIsHost_;
        resetPeerMachineState();
        const uint8_t role = pcIsHost_ ? SESSION_ROLE_HOST : SESSION_ROLE_GUEST;
        const uint8_t hostColor = pcIsHost_
                                      ? (hostPlaysWhite_ ? SESSION_COLOR_WHITE
                                                        : SESSION_COLOR_BLACK)
                                      : SESSION_COLOR_UNKNOWN;
        return sessionController_.initializeMqtt(role, hostColor, mqttSessionId_);
    }

    void resetPeerMachineState()
    {
        peerPlatform_ = NETCHESS_PLAT_UNKNOWN;
        localMachSent_ = false;
    }

    void setPeerPlatform(uint8_t platform)
    {
        const char *code = netchess_proto_mach_code(platform);

        if (code == nullptr || peerPlatform_ == platform) {
            return;
        }
        peerPlatform_ = platform;
        appendLog("PEER MACHINE: " + QString::fromLatin1(code));
        refreshStatusBar();
    }

    void sendLocalMachAnnouncement()
    {
        if (localMachSent_ || !directSessionReady_) {
            return;
        }
        localMachSent_ = true;

        const QByteArray payload = localMachPayload();
        if (payload.isEmpty()) {
            appendLog("NOTICE: local machine identity unavailable");
            return;
        }

        if (isMqttMode()) {
            const QByteArray suffix =
                sessionController_.mqttTopicSuffixForRoute(SESSION_ROUTE_GAME);
            if (suffix.isEmpty() ||
                !mqttPublish(QString::fromLatin1(suffix),
                             QString::fromLatin1(payload), false)) {
                appendLog("NOTICE: MACH announcement not sent");
            }
            return;
        }

        const QPointer<QTcpSocket> sock =
            directSocketForLink(directPrimaryLinkId_);
        if (sock == nullptr ||
            sock->state() != QAbstractSocket::ConnectedState) {
            appendLog("NOTICE: MACH announcement link unavailable");
            return;
        }

        QByteArray frame = payload;
        frame.append('\n');
        const qint64 written = sock->write(frame);
        if (written == frame.size()) {
            sock->flush();
            appendLog("TX: " + QString::fromLatin1(payload));
            return;
        }

        appendLog(QString("NOTICE: MACH write %1/%2")
                      .arg(written)
                      .arg(frame.size()));
        if (written > 0) {
            /* A partial TCP frame corrupts later line framing. */
            sock->abort();
        }
    }

    bool submitSessionLocalRequest(uint8_t request,
                                   uint16_t value = 0u,
                                   const QByteArray &payload = QByteArray(),
                                   uint8_t phase = SESSION_PHASE_IDLE)
    {
        if (!sessionController_.initialized() ||
            (isMqttMode() && (!mqttSubscribed_ || !mqttSideReady_))) {
            return false;
        }
        if (request != SESSION_REQUEST_CHAT) {
            directUiBusy_ = request;
        }
        const bool accepted =
            sessionController_.submitLocalRequest(request, value, payload, phase);
        if (!accepted && request != SESSION_REQUEST_CHAT) {
            directUiBusy_ = false;
        }
        if (request != SESSION_REQUEST_CHAT) {
            setConnectedUi(isMqttMode() ? isConnected() : directSessionReady_);
        }
        return accepted;
    }

    void submitSessionUserDecision(uint8_t requestId, uint8_t decision)
    {
        if (decision == SESSION_DECISION_REJECT) {
            directUiBusy_ = false;
        }
        sessionController_.submitUserDecision(requestId, decision);
    }

    void submitSessionGameResult(uint8_t deliveryId,
                                 uint16_t value,
                                 uint8_t result,
                                 const QByteArray &detail = QByteArray())
    {
        directUiBusy_ = false;
        sessionController_.submitGameResult(deliveryId, value, result, detail);
        setConnectedUi(isMqttMode() ? isConnected() : directSessionReady_);
    }
    uint8_t directLinkForSocket(QTcpSocket *sock) const
    {
        if (sock == nullptr) {
            return SESSION_LINK_NONE;
        }
        for (auto it = directSockets_.constBegin(); it != directSockets_.constEnd(); ++it) {
            if (it.value() == sock) {
                return it.key();
            }
        }
        return SESSION_LINK_NONE;
    }

    QPointer<QTcpSocket> directSocketForLink(uint8_t linkId) const
    {
        return directSockets_.value(linkId);
    }

    uint8_t registerDirectSocket(QTcpSocket *sock)
    {
        const uint8_t existing = directLinkForSocket(sock);
        if (existing != SESSION_LINK_NONE) {
            return existing;
        }
        for (int attempt = 0; attempt < 255; ++attempt) {
            const uint8_t linkId = directNextLinkId_++;
            if (linkId != SESSION_LINK_NONE && !directSockets_.contains(linkId)) {
                directSockets_.insert(linkId, QPointer<QTcpSocket>(sock));
                directLinkUpSeen_.insert(linkId, false);
                return linkId;
            }
        }
        return SESSION_LINK_NONE;
    }

    void forgetDirectSocket(uint8_t linkId)
    {
        transportCodec_.clearDirect(linkId);
        directLinkUpSeen_.remove(linkId);
        directSockets_.remove(linkId);
        if (directPrimaryLinkId_ == linkId) {
            directPrimaryLinkId_ = SESSION_LINK_NONE;
        }
    }

    void handleDirectSideChanged(uint8_t color)
    {
        if (color != SESSION_COLOR_WHITE && color != SESSION_COLOR_BLACK) {
            return;
        }
        pcPlaysWhite_ = color == SESSION_COLOR_WHITE;
        hostPlaysWhite_ = pcIsHost_ ? pcPlaysWhite_ : !pcPlaysWhite_;
        refreshMoveLogHeaders();
        if (syncSharedBoardOrientation()) {
            refreshBoard();
        }
        setConnectedUi(directSessionReady_);
    }

    void handleMqttSideChanged(uint8_t color, uint16_t sessionId)
    {
        if (color != SESSION_COLOR_WHITE && color != SESSION_COLOR_BLACK) {
            return;
        }
        const bool changed = pcPlaysWhite_ != (color == SESSION_COLOR_WHITE);
        mqttSessionId_ = sessionId;
        pcPlaysWhite_ = color == SESSION_COLOR_WHITE;
        hostPlaysWhite_ = pcIsHost_ ? pcPlaysWhite_ : !pcPlaysWhite_;
        refreshMoveLogHeaders();
        if (!mqttSideReady_ || changed) {
            if (mqttSideTransitionPending_ ||
                !mqttSubackPending_.isEmpty() ||
                !mqttUnsubackPending_.isEmpty()) {
                failMqttConnection("MQTT side transition overlapped");
                return;
            }

            QSet<QString> target;
            target.insert(QStringLiteral("meta"));
            target.insert(mqttInSuffix());
            target.insert(mqttInAckSuffix());
            target.insert(mqttPeerPresenceSuffix());
            target.insert(mqttPresenceSuffix());

            QSet<QString> additions = target;
            additions.subtract(mqttActiveSubscriptions_);
            mqttObsoleteSubscriptions_ = mqttActiveSubscriptions_;
            mqttObsoleteSubscriptions_.subtract(target);
            mqttTargetSubscriptions_ = target;
            mqttSideTransitionPending_ = true;
            mqttSubscribed_ = false;
            mqttSideReady_ = false;

            for (const QString &suffix : additions) {
                if (!mqttSubscribe(suffix)) {
                    failMqttConnection("MQTT side subscribe send failed");
                    return;
                }
            }
            advanceMqttSubscriptionTransition();
            return;
        }
        if (syncSharedBoardOrientation()) {
            refreshBoard();
        }
        setConnectedUi(isConnected());
    }

    void handleDirectSessionChanged(uint8_t status, uint8_t reason)
    {
        if (status == SESSION_CHANGED_READY) {
            cancelDirectConnectRetry();
            directSessionReady_ = true;
            directUiBusy_ = false;
            directEndStatus_.clear();
            setStatusText(pcIsHost_ ? NETCHESSZX_UI_NOTICE_OPPONENT_READY_START
                                    : NETCHESSZX_UI_NOTICE_OPPONENT_READY_WAIT_START);
            setConnectedUi(true);
            sendLocalMachAnnouncement();
            return;
        }
        if (status == SESSION_CHANGED_BUSY) {
            cancelDirectConnectRetry();
            directSessionReady_ = false;
            resetPeerMachineState();
            directEndStatus_ = QString::fromLatin1(kDirectHostBusyStatus);
            setStatusText(directEndStatus_);
            setConnectedUi(false);
            return;
        }
        if (status == SESSION_CHANGED_STARTED) {
            directSessionReady_ = true;
            directUiBusy_ = false;
            if (!directStartTransitionApplied_) {
                applyDirectStartTransition();
            }
            return;
        }
        if (status == SESSION_CHANGED_ENDED) {
            if (reason == SESSION_END_REASON_LOCAL_BYE) {
                directEndStatus_ =
                    QString::fromLatin1(NETCHESSZX_UI_PHASE_DISCONNECTED);
            } else if (reason == SESSION_END_REASON_REMOTE_BYE) {
                directEndStatus_ =
                    QString::fromLatin1(NETCHESSZX_UI_ERROR_OPPONENT_DISCONNECTED);
            } else if (reason == SESSION_END_REASON_TRANSPORT_LOST &&
                       directEndStatus_.isEmpty()) {
                directEndStatus_ =
                    QString::fromLatin1(NETCHESSZX_UI_ERROR_CONNECTION_LOST);
            }
            directSessionReady_ = false;
            resetPeerMachineState();
            directUiBusy_ = false;
            directStartTransitionApplied_ = false;
            directLocalResignPending_ = false;
            directResignRestartPending_ = false;
            directPrimaryLinkId_ = SESSION_LINK_NONE;
            closeDirectDecisionPrompt();
            const QString ended = directEndStatus_.isEmpty()
                                      ? QString::fromLatin1(NETCHESSZX_UI_ERROR_CONNECTION_LOST)
                                      : directEndStatus_;
            directEndStatus_.clear();
            resetGame(ended);
            clearChatLog();
            setStatusText(ended);
            const bool restartMqttSession =
                isMqttMode() && !localDisconnectPending_ &&
                mqttSessionLinked_ && mqttSubscribed_ && mqttSideReady_ &&
                isConnected();
            if (restartMqttSession) {
                (void)sessionController_.linkUp(kMqttLinkId);
            }
            setConnectedUi(restartMqttSession);
        }
    }

    void handleDirectControlResult(uint16_t control, uint8_t result)
    {
        if (result == SESSION_CONTROL_CANCELLED ||
            result == SESSION_CONTROL_EXPIRED) {
            if (control == SESSION_REQUEST_RESTORE) {
                directUiBusy_ = false;
                closeDirectDecisionPrompt();
                setStatusText(QStringLiteral("Load cancelled"));
                setConnectedUi(directSessionReady_);
                return;
            }
            if (control == SESSION_REQUEST_RESET &&
                directResignRestartPending_) {
                directUiBusy_ = false;
                directResignRestartPending_ = false;
                closeDirectDecisionPrompt();
                appendLog(NETCHESSZX_UI_ERROR_RESTART_FAILED_GAME_OVER);
                setStatusText(NETCHESSZX_UI_ERROR_RESTART_FAILED_GAME_OVER);
                setConnectedUi(directSessionReady_);
                return;
            }
            const QString message = result == SESSION_CONTROL_CANCELLED
                                        ? QStringLiteral("RESET cancelled: no response")
                                        : QStringLiteral("RESET request expired");

            directUiBusy_ = false;
            closeDirectDecisionPrompt();
            appendLog(message);
            setStatusText(message);
            setConnectedUi(directSessionReady_);
            return;
        }
        const bool rejected = result == SESSION_CONTROL_REJECTED;
        switch (control) {
        case SESSION_REQUEST_START:
            directUiBusy_ = false;
            if (rejected) {
                setStatusText(NETCHESSZX_UI_ERROR_START_REJECTED_BY_OPPONENT);
            }
            break;
        case SESSION_REQUEST_MOVE:
            directUiBusy_ = false;
            if (rejected) {
                pcTurn_ = true;
                clearDestinationFeedback();
                refreshLegalMoves();
                refreshBoard(true);
                setStatusText(QStringLiteral("Move rejected by opponent"));
            }
            break;
        case SESSION_REQUEST_RESET:
            if (rejected) {
                directUiBusy_ = false;
                if (directResignRestartPending_) {
                    directResignRestartPending_ = false;
                    appendLog(NETCHESSZX_UI_ERROR_RESTART_FAILED_GAME_OVER);
                    setStatusText(NETCHESSZX_UI_ERROR_RESTART_FAILED_GAME_OVER);
                } else {
                    setStatusText(gameOver_ ? NETCHESSZX_UI_ERROR_RESTART_REJECTED
                                            : NETCHESSZX_UI_ERROR_RESET_REJECTED);
                }
            } else {
                applyDirectStartTransition();
            }
            break;
        case SESSION_REQUEST_RESIGN:
            directLocalResignPending_ = false;
            if (rejected) {
                directUiBusy_ = false;
                directResignRestartPending_ = false;
            } else {
                directUiBusy_ = SESSION_REQUEST_RESET;
                directResignRestartPending_ = true;
                setStatusText(NETCHESSZX_UI_NOTICE_RESTARTING_GAME);
            }
            break;
        case SESSION_REQUEST_TAKEBACK:
            directUiBusy_ = false;
            setStatusText(rejected ? QStringLiteral("Takeback rejected")
                                   : QStringLiteral("Takeback accepted"));
            break;
        case SESSION_REQUEST_RESTORE:
            directUiBusy_ = false;
            closeDirectDecisionPrompt();
            if (rejected) {
                setStatusText(QStringLiteral("Load declined"));
            }
            break;
        default:
            break;
        }
        setConnectedUi(directSessionReady_);
    }

    void handleDirectGameAction(uint8_t kind,
                                uint8_t deliveryId,
                                uint16_t value,
                                const QByteArray &payload,
                                QVector<DesktopSessionFollowup> &followups)
    {
        switch (kind) {
        case SESSION_DELIVER_REMOTE_MOVE: {
            directUiBusy_ = true;
            setConnectedUi(directSessionReady_);
            QByteArray failure;
            if (!applyDirectRemoteMoveAnimated(deliveryId, value,
                                               QString::fromLatin1(payload).toLower(),
                                               &failure)) {
                followups.append(DesktopSessionFollowup{
                    SESSION_EV_GAME_RESULT, deliveryId, SESSION_GAME_REJECTED,
                    value, failure});
                directUiBusy_ = false;
                setConnectedUi(directSessionReady_);
            }
            break;
        }
        case SESSION_DELIVER_LOCAL_MOVE:
            directUiBusy_ = false;
            applyDirectLocalMove(value, QString::fromLatin1(payload).toLower());
            break;
        case SESSION_DELIVER_CHAT:
            appendChat(value == SESSION_CHAT_LOCAL ? pcChatName()
                                                   : opponentChatName(),
                       QString::fromLatin1(payload));
            break;
        case SESSION_DELIVER_CONTROL:
            if (value == SESSION_REQUEST_RESET) {
                appendControlEvent(false, QStringLiteral("RESET"));
                applyDirectStartTransition();
            } else if (value == SESSION_REQUEST_RESIGN) {
                applyDirectResignTransition();
            }
            break;
        case SESSION_DELIVER_CONTROL_RESULT:
            handleDirectControlResult(value, deliveryId);
            break;
        case SESSION_DELIVER_RESTORE:
            if (deliveryId != 0u) {
                uint16_t restoredPly = 0u;
                uint8_t restoredPhase = SESSION_PHASE_IDLE;
                const bool accepted = applyDirectRestore(
                    payload, &restoredPly, &restoredPhase);
                if (accepted) {
                    appendControlEvent(false, QStringLiteral("RESTORE"));
                }
                followups.append(DesktopSessionFollowup{
                    SESSION_EV_GAME_RESULT, deliveryId,
                    static_cast<uint8_t>(accepted ? SESSION_GAME_ACCEPTED
                                                  : SESSION_GAME_REJECTED),
                    restoredPly,
                    accepted
                        ? QByteArray(1, static_cast<char>(restoredPhase))
                        : QByteArray("INVALID")});
            } else {
                uint16_t restoredPly = 0u;
                const bool accepted = applyDirectRestore(
                    payload, &restoredPly, nullptr);
                if (accepted) {
                    appendControlEvent(true, QStringLiteral("RESTORE"));
                }
                if (!accepted || restoredPly != value) {
                    appendLog("ERROR: restored ply differs from session core");
                }
            }
            directUiBusy_ = false;
            setConnectedUi(directSessionReady_);
            break;
        case SESSION_DELIVER_TAKEBACK: {
            const bool local = takebackSnapshot_.valid &&
                               takebackSnapshot_.localMove;
            const bool accepted = applyDirectTakeback(value);
            if (accepted) {
                appendControlEvent(local, QStringLiteral("TAKEBACK"));
            }
            followups.append(DesktopSessionFollowup{
                SESSION_EV_GAME_RESULT, deliveryId,
                static_cast<uint8_t>(accepted ? SESSION_GAME_ACCEPTED
                                              : SESSION_GAME_REJECTED),
                value,
                QByteArray()});
            directUiBusy_ = false;
            setConnectedUi(directSessionReady_);
            break;
        }
        default:
            appendLog(QString("ERROR: unknown direct game action %1").arg(kind));
            break;
        }
    }

    void closeDirectDecisionPrompt()
    {
        directDecisionRequestId_ = 0u;
        directDecisionControl_ = 0u;
        if (directDecisionBox_ != nullptr) {
            QMessageBox *box = directDecisionBox_;
            directDecisionBox_.clear();
            box->close();
        }
    }

    void applyDirectResignTransition()
    {
        const bool local = directLocalResignPending_;
        ++pieceRevealGeneration_;
        pieceRevealTimer_.stop();
        stopMirrorlockFlip();
        clearDestinationFeedback();
        closeDirectDecisionPrompt();
        directResignRestartPending_ = true;
        directUiBusy_ = local ? SESSION_REQUEST_RESIGN : SESSION_REQUEST_RESET;
        appendControlEvent(local, QStringLiteral("RESIGN"));
        endGameOver(QStringLiteral("RESIGN"));
        setStatusText(local ? NETCHESSZX_UI_NOTICE_WAITING_RESIGN_ACK
                            : NETCHESSZX_UI_EVENT_OPPONENT_RESIGN);
    }

    void applyDirectStartTransition()
    {
        if (directStartTransitionApplied_) {
            return;
        }
        directStartTransitionApplied_ = true;
        directUiBusy_ = false;
        startGameFromAck();
        QTimer::singleShot(0, this, [this]() {
            directStartTransitionApplied_ = false;
        });
    }

    void showDirectDecision(uint8_t requestId, uint8_t control, uint16_t value)
    {
        if (resetPromptOpen_ ||
            (control == SESSION_REQUEST_TAKEBACK &&
            (!takebackSnapshot_.valid || takebackSnapshot_.localMove ||
             takebackSnapshot_.ply != value || value != nextPly_ - 1))) {
            QTimer::singleShot(0, this, [this, requestId]() {
                submitSessionUserDecision(requestId, SESSION_DECISION_REJECT);
            });
            return;
        }

        closeDirectDecisionPrompt();
        directUiBusy_ = true;
        directDecisionRequestId_ = requestId;
        directDecisionControl_ = control;

        QString title;
        QString message;
        if (control == SESSION_REQUEST_RESET) {
            title = gameOver_ ? NETCHESSZX_UI_CONFIRM_PC_RESTART_TITLE
                              : NETCHESSZX_UI_CONFIRM_PC_RESET_TITLE;
            message = gameOver_ ? NETCHESSZX_UI_CONFIRM_PC_RESTART_REQUEST
                                : NETCHESSZX_UI_CONFIRM_PC_RESET_REQUEST_LONG;
            setStatusText(gameOver_ ? NETCHESSZX_UI_CONFIRM_RESTART_REQUEST
                                    : NETCHESSZX_UI_CONFIRM_RESET_REQUEST);
        } else if (control == SESSION_REQUEST_TAKEBACK) {
            title = NETCHESSZX_UI_CONFIRM_PC_TAKEBACK_TITLE;
            message = NETCHESSZX_UI_CONFIRM_PC_ACCEPT_TAKEBACK;
            setStatusText(NETCHESSZX_UI_CONFIRM_TAKEBACK_REQUEST);
        } else if (control == SESSION_REQUEST_RESTORE) {
            title = QStringLiteral("Load Game");
            message = QStringLiteral("ECHO wants to load a saved game. Accept?");
            setStatusText(QStringLiteral("Load requested"));
        } else {
            QTimer::singleShot(0, this, [this, requestId]() {
                submitSessionUserDecision(requestId, SESSION_DECISION_REJECT);
            });
            return;
        }

        setConnectedUi(directSessionReady_);
        auto *box = new QMessageBox(QMessageBox::NoIcon, title, message,
                                    QMessageBox::Yes | QMessageBox::No, this);
        box->setAttribute(Qt::WA_DeleteOnClose);
        directDecisionBox_ = box;
        connect(box, &QDialog::finished, this, [this, requestId, control](int result) {
            if (directDecisionRequestId_ != requestId ||
                directDecisionControl_ != control) {
                return;
            }
            directDecisionBox_.clear();
            directDecisionRequestId_ = 0u;
            directDecisionControl_ = 0u;
            const bool accepted = result == QMessageBox::Yes;
            submitSessionUserDecision(requestId, accepted ? SESSION_DECISION_ACCEPT
                                                          : SESSION_DECISION_REJECT);
            if (!accepted) {
                setStatusText(control == SESSION_REQUEST_RESTORE
                                  ? QStringLiteral("Load declined")
                                  : QStringLiteral("Request rejected"));
                setConnectedUi(directSessionReady_);
            }
        });
        box->open();
    }

    void attachSocket(QTcpSocket *sock)
    {
        if (sock == nullptr) {
            return;
        }
        connect(sock, &QTcpSocket::connected, this, [this, sock]() {
            if (directLinkForSocket(sock) != SESSION_LINK_NONE || sock == socket_) {
                handleSocketConnected(sock);
            }
        });
        connect(sock, &QTcpSocket::disconnected, this, [this, sock]() {
            if (directLinkForSocket(sock) != SESSION_LINK_NONE) {
                handleSocketDisconnected(sock);
            } else if (sock == socket_) {
                if (ignoreNextDisconnect_) {
                    ignoreNextDisconnect_ = false;
                    return;
                }
                handleSocketDisconnected(sock);
            }
        });
        connect(sock, &QTcpSocket::readyRead, this, [this, sock]() {
            if (directLinkForSocket(sock) != SESSION_LINK_NONE || sock == socket_) {
                consumeReadyRead(sock);
            }
        });
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
        connect(sock, &QTcpSocket::errorOccurred, this, [this, sock](QAbstractSocket::SocketError) {
            if (directLinkForSocket(sock) != SESSION_LINK_NONE || sock == socket_) {
                handleSocketError(sock);
            }
        });
#else
        connect(sock, QOverload<QAbstractSocket::SocketError>::of(&QTcpSocket::error),
                this, [this, sock](QAbstractSocket::SocketError) {
                    if (directLinkForSocket(sock) != SESSION_LINK_NONE || sock == socket_) {
                        handleSocketError(sock);
                    }
                });
#endif
    }

    void cancelDirectConnectRetry()
    {
        if (directConnectRetryTimer_ != nullptr) {
            directConnectRetryTimer_->stop();
        }
        directConnectRetryActive_ = false;
        directConnectRetryCount_ = 0;
    }

    bool scheduleDirectConnectRetry(QTcpSocket *sock)
    {
        if (sock != socket_ || !directConnectRetryActive_ ||
            isMqttMode() || pcIsHost_ || directSessionReady_) {
            return false;
        }
        if (directConnectRetryTimer_->isActive()) {
            return true;
        }

        ++directConnectRetryCount_;
        directConnectRetryTimer_->start();
        const uint8_t directLink = directLinkForSocket(sock);
        if (directLink != SESSION_LINK_NONE) {
            if (directLinkUpSeen_.value(directLink, false)) {
                (void)sessionController_.linkDown(directLink);
            }
            forgetDirectSocket(directLink);
        }
        sock->abort();
        directEndStatus_.clear();
        lastSocketError_.clear();
        setStatusText(QString("Opponent not ready - retry %1 in 2 s")
                          .arg(directConnectRetryCount_));
        setConnectedUi(false);
        return true;
    }

    void retryDirectConnection()
    {
        if (!directConnectRetryActive_ || isMqttMode() || pcIsHost_ ||
            socket_->state() != QAbstractSocket::UnconnectedState) {
            cancelDirectConnectRetry();
            setConnectedUi(isConnected());
            return;
        }

        const uint8_t linkId = registerDirectSocket(socket_);
        if (linkId == SESSION_LINK_NONE) {
            cancelDirectConnectRetry();
            resetGame(NETCHESSZX_UI_PHASE_CONNECTION_FAILED);
            clearChatLog();
            setStatusText(NETCHESSZX_UI_PHASE_CONNECTION_FAILED);
            setConnectedUi(false);
            return;
        }

        directPrimaryLinkId_ = linkId;
        appendLog(QString("RETRY %1 CONNECT %2:%3")
                      .arg(directConnectRetryCount_)
                      .arg(hostEdit_->text().trimmed())
                      .arg(portSpin_->value()));
        setStatusText(NETCHESSZX_UI_NOTICE_CONNECTING_PC);
        socket_->connectToHost(hostEdit_->text().trimmed(),
                               static_cast<quint16>(portSpin_->value()));
        setConnectedUi(false);
    }

    void handleSocketError(QTcpSocket *sock)
    {
        if (sock == nullptr) {
            return;
        }
        const QString socketError = sock->errorString();
        appendLog("ERROR: " + socketError);
        if (scheduleDirectConnectRetry(sock)) {
            return;
        }
        const uint8_t directLink = directLinkForSocket(sock);
        if (directLink != SESSION_LINK_NONE) {
            const bool primary = directLink == directPrimaryLinkId_ ||
                                 sock == socket_;
            if (!primary) {
                return;
            }
            lastSocketError_ = socketError;
            const bool wasLinked = directLinkUpSeen_.value(directLink, false);
            const QString status = wasLinked
                                       ? QString::fromLatin1(
                                             NETCHESSZX_UI_ERROR_CONNECTION_LOST)
                                       : socketError.contains("refused", Qt::CaseInsensitive)
                                             ? "Connection refused - opponent not ready"
                                             : "Connection failed - " + socketError;
            directEndStatus_ = status;
            setStatusText(status);
            if (!wasLinked && sock == socket_) {
                sock->abort();
                resetGame(status);
                clearChatLog();
                setConnectedUi(false);
            }
            return;
        }
        if (!isMqttMode()) {
            return;
        }
        lastSocketError_ = socketError;
        setStatusText(lastSocketError_);
        if (!isConnected()) {
            setConnectedUi(false);
        }
    }

    void handleSocketConnected(QTcpSocket *sock)
    {
        const uint8_t directLink = directLinkForSocket(sock);
        if (directLink != SESSION_LINK_NONE) {
            directLinkUpSeen_[directLink] = true;
            if (directLink == directPrimaryLinkId_) {
                pcTurn_ = false;
                clearSelection();
                chatLogEdit_->clear();
                setStatusText(NETCHESSZX_UI_NOTICE_WAITING_OPPONENT_APP);
                setConnectedUi(false);
                refreshBoard();
                appendLog("CONNECTED TCP");
            }
            (void)sessionController_.linkUp(directLink);
            return;
        }
        if (sock != socket_ || !isMqttMode()) {
            return;
        }
        linkWatch_.restart();
        pcTurn_ = false;
        clearSelection();
        chatLogEdit_->clear();
        setConnectedUi(true);
        mqttHandshake();
    }

    void handleSocketDisconnected(QTcpSocket *sock)
    {
        if (scheduleDirectConnectRetry(sock)) {
            return;
        }
        const uint8_t directLink = directLinkForSocket(sock);
        if (directLink != SESSION_LINK_NONE) {
            const bool wasLinked = directLinkUpSeen_.value(directLink, false);
            const bool primary = directLink == directPrimaryLinkId_ ||
                                 sock == socket_;
            if (primary && directEndStatus_.isEmpty()) {
                directEndStatus_ = lastSocketError_.isEmpty()
                                       ? QString::fromLatin1(NETCHESSZX_UI_ERROR_CONNECTION_LOST)
                                       : lastSocketError_;
            }
            if (wasLinked) {
                (void)sessionController_.linkDown(directLink);
            }
            forgetDirectSocket(directLink);
            if (!primary) {
                sock->deleteLater();
            } else if (pcIsHost_ && directServer_ != nullptr &&
                       directServer_->isListening()) {
                setConnectedUi(false);
            }
            lastSocketError_.clear();
            appendLog("DISCONNECTED");
            return;
        }
        if (sock != socket_ || !isMqttMode()) {
            return;
        }
        QString status;

        if (localDisconnectPending_) {
            status = NETCHESSZX_UI_PHASE_DISCONNECTED;
            localDisconnectPending_ = false;
        } else if (!lastSocketError_.isEmpty()) {
            status = lastSocketError_;
        } else {
            status = NETCHESSZX_UI_ERROR_CONNECTION_LOST;
        }
        clearMqttSubscriptionState();
        bool handled = false;
        if (mqttSessionLinked_) {
            mqttSessionLinked_ = false;
            directEndStatus_ = status;
            handled = sessionController_.linkDown(kMqttLinkId);
        }
        if (handled) {
            lastSocketError_.clear();
            appendLog("DISCONNECTED");
            return;
        }
        resetGame(status);
        clearChatLog();
        setConnectedUi(false);
        setStatusText(status);
        lastSocketError_.clear();
        appendLog("DISCONNECTED");
    }

    void disconnectToSetup()
    {
        cancelDirectConnectRetry();
        if (!isMqttMode() && pcIsHost_ && directServer_ != nullptr) {
            directServer_->close();
        }
        if (isConnected()) {
            if (isMqttMode()) {
                localDisconnectPending_ = true;
                directEndStatus_ = NETCHESSZX_UI_PHASE_DISCONNECTED;
                if (!submitSessionLocalRequest(SESSION_REQUEST_BYE)) {
                    socket_->disconnectFromHost();
                }
            } else {
                directEndStatus_ = NETCHESSZX_UI_PHASE_DISCONNECTED;
                if (!submitSessionLocalRequest(SESSION_REQUEST_BYE)) {
                    socket_->disconnectFromHost();
                }
                return;
            }
        } else if (isDirectListening()) {
            directServer_->close();
        } else if (isConnecting() && socket_ != nullptr) {
            socket_->abort();
        }
        resetGame(NETCHESSZX_UI_PHASE_DISCONNECTED);
        stopGameClock();
        clearChatLog();
        setConnectedUi(false);
        setStatusText(NETCHESSZX_UI_PHASE_DISCONNECTED);
    }

    void failMqttConnection(const QString &status)
    {
        const bool linked = mqttSessionLinked_;
        clearMqttSubscriptionState();
        mqttSessionLinked_ = false;
        if (linked) {
            directEndStatus_ = status;
            (void)sessionController_.linkDown(kMqttLinkId);
        }
        ignoreNextDisconnect_ = true;
        appendLog("ERROR: " + status);
        if (socket_ != nullptr) {
            socket_->abort();
        }
        resetGame(status);
        clearChatLog();
        setStatusText(status);
        setConnectedUi(false);
    }

    void acceptDirectClient()
    {
        QTcpSocket *accepted = directServer_->nextPendingConnection();
        if (accepted == nullptr) {
            return;
        }
        const uint8_t linkId = registerDirectSocket(accepted);
        if (linkId == SESSION_LINK_NONE) {
            accepted->disconnectFromHost();
            accepted->deleteLater();
            return;
        }
        attachSocket(accepted);
        if (accepted->state() != QAbstractSocket::ConnectedState) {
            forgetDirectSocket(linkId);
            accepted->deleteLater();
            return;
        }
        if (directPrimaryLinkId_ == SESSION_LINK_NONE) {
            if (socket_ != nullptr && socket_ != accepted) {
                const uint8_t oldLink = directLinkForSocket(socket_);
                if (oldLink != SESSION_LINK_NONE) {
                    forgetDirectSocket(oldLink);
                }
                socket_->deleteLater();
            }
            socket_ = accepted;
            directPrimaryLinkId_ = linkId;
        }
        appendLog(QString("ACCEPT %1:%2")
                      .arg(accepted->peerAddress().toString())
                      .arg(accepted->peerPort()));
        handleSocketConnected(accepted);
    }

    bool isFeedbackSquare(int row, int col) const
    {
        return feedbackOn_ && row == feedbackRow_ && col == feedbackCol_;
    }

    static QString squareStyle(int row, int col, bool selected, bool target,
                               bool legalTarget, bool feedback, bool hasPiece = false,
                               bool showHints = true)
    {
        const bool light = ((row + col) % 2) == 0;
        const bool texturedBoard = !PieceRenderer::boardTexture().isEmpty();
        QString bg;
        QString border;

        if (feedback) {
            bg = MSH_SQ_HIT;
            border = "4px solid " MSH_SQ_HIT_EDGE;
        } else if (selected) {
            bg = MSH_SQ_SEL;
            border = "3px solid " MSH_SQ_SEL_EDGE;
        } else if (target) {
            if (legalTarget && showHints && !hasPiece && !texturedBoard) {
                const QString dotColor = "rgba(0, 0, 0, 0.28)";
                bg = QString("qradialgradient(cx:0.5, cy:0.5, radius:0.12, fx:0.5, fy:0.5, stop:0 %1, stop:0.85 %1, stop:0.9 " MSH_SQ_TGT ", stop:1.0 " MSH_SQ_TGT ")").arg(dotColor);
            } else {
                bg = MSH_SQ_TGT;
            }
            border = "3px solid " MSH_SQ_TGT_EDGE;
        } else if (legalTarget && showHints) {
            if (hasPiece) {
                bg = light ? MSH_SQ_HINT_L : MSH_SQ_HINT_D;
                border = "2px solid " MSH_SQ_HINT_EDGE;
            } else {
                const QString baseBg = light ? MSH_SQ_LIGHT : MSH_SQ_DARK;
                const QString dotColor = "rgba(0, 0, 0, 0.25)";
                bg = QString("qradialgradient(cx:0.5, cy:0.5, radius:0.12, fx:0.5, fy:0.5, stop:0 %1, stop:0.85 %1, stop:0.9 %2, stop:1.0 %2)").arg(dotColor, baseBg);
                border = "1px solid " MSH_SQ_EDGE;
            }
        } else {
            bg = light ? MSH_SQ_LIGHT : MSH_SQ_DARK;
            border = "1px solid " MSH_SQ_EDGE;
        }

        const QString fg = light || selected || target || feedback ? MSH_SQ_INK : "#ffffff";
        return PieceRenderer::boardSquareStyle(row, col, kBoardSquareSize, bg, fg, border);
    }

    void showDestinationFeedback(int row, int col)
    {
        clearDestinationFeedback();
        feedbackRow_ = row;
        feedbackCol_ = col;
        feedbackOn_ = true;
        refreshBoardSquareVisual(row, col);

        feedbackStep_ = 0;
        feedbackTimer_.start(90);
    }

    void clearDestinationFeedback()
    {
        feedbackTimer_.stop();
        feedbackRow_ = -1;
        feedbackCol_ = -1;
        feedbackStep_ = 0;
        feedbackOn_ = false;
    }

    void clearSelection()
    {
        selectedRow_ = -1;
        selectedCol_ = -1;
        targetRow_ = -1;
        targetCol_ = -1;
        legalTargets_.clear();
    }

    void squareClicked(int displayRow, int displayCol)
    {
        const int row = boardRowForDisplay(displayRow);
        const int col = boardColForDisplay(displayCol);
        const QString clicked = InputHelpers::squareName(row, col);
        const int oldSelectedRow = selectedRow_;
        const int oldSelectedCol = selectedCol_;
        const int oldTargetRow = targetRow_;
        const int oldTargetCol = targetCol_;
        const QStringList oldLegalTargets = legalTargets_;

        if (!canPcMove()) {
            clearSelection();
            selectedLabel_->setText("Alignment: none");
            if (socket_->state() != QAbstractSocket::ConnectedState) {
                setStatusText(NETCHESSZX_UI_ERROR_NOT_CONNECTED);
                appendLog(QString("CLICK: %1 ignored, not connected").arg(clicked));
            } else if (directUiBusy_) {
                setStatusText(NETCHESSZX_UI_PHASE_WAITING_OPPONENT);
                appendLog(QString("CLICK: %1 ignored, ACK pending").arg(clicked));
            } else if (!gameClockRunning_) {
                if (!gameOver_) {
                    setStatusBarText(NETCHESSZX_UI_NOTICE_GAME_NOT_STARTED);
                }
                appendLog(QString("CLICK: %1 ignored, game not active").arg(clicked));
            } else {
                setStatusBarText("Not your turn");
                appendLog(QString("CLICK: %1 ignored, opponent turn").arg(clicked));
            }
            refreshSelectionFootprint(oldSelectedRow, oldSelectedCol,
                                      oldTargetRow, oldTargetCol,
                                      oldLegalTargets);
            return;
        }

        // One click is one Alignment: the clicked square is either legal or it
        // is not, there is nothing to pick up first.
        if (!isLegalTarget(row, col)) {
            const bool occupied = board_[row][col] != '.';
            selectedLabel_->setText("Alignment: none");
            setStatusText(occupied
                              ? QString("%1 already holds a fragment").arg(clicked.toUpper())
                              : QString("%1 locks nothing").arg(clicked.toUpper()));
            appendLog(QString("CLICK: %1 is not a legal alignment").arg(clicked));
            return;
        }

        if (targetRow_ == row && targetCol_ == col) {
            targetRow_ = -1;
            targetCol_ = -1;
            moveEdit_->clear();
            selectedLabel_->setText("Alignment: none");
            setStatusText(NETCHESSZX_UI_NOTICE_SELECTION_CLEARED);
            appendLog(QString("CLICK: cleared %1").arg(clicked));
            refreshSelectionFootprint(oldSelectedRow, oldSelectedCol,
                                      oldTargetRow, oldTargetCol,
                                      oldLegalTargets);
            return;
        }

        targetRow_ = row;
        targetCol_ = col;
        // moveEdit_ and chatEdit_ are the same line edit: never clear it after
        // writing the square, or SEND stays disabled on an empty field.
        moveEdit_->setText(clicked);
        refreshChatButton();
        selectedLabel_->setText(QString("Alignment: %1").arg(clicked.toUpper()));
        setStatusText(QString("Alignment ready: %1 - press SEND").arg(clicked.toUpper()));
        appendLog(QString("CLICK: alignment %1").arg(clicked));
        refreshSelectionFootprint(oldSelectedRow, oldSelectedCol,
                                  oldTargetRow, oldTargetCol,
                                  oldLegalTargets);
        refreshTurnLabel();
    }

    void connectToOpponent()
    {
        cancelDirectConnectRetry();
        const QString host = hostEdit_->text().trimmed();
        const quint16 port = static_cast<quint16>(portSpin_->value());
        const QString room = roomEdit_->text().trimmed().toUpper();

        configureSessionFromUi();

        if (host.isEmpty() && (isMqttMode() || !pcIsHost_)) {
            appendLog("ERROR: empty host");
            setStatusText(isMqttMode() ? NETCHESSZX_UI_ERROR_EMPTY_BROKER : NETCHESSZX_UI_ERROR_INVALID_IP);
            return;
        }
        if (!isMqttMode() && !pcIsHost_ && !InputHelpers::isDirectIpSyntaxOk(host)) {
            appendLog("ERROR: invalid direct IP");
            setStatusText(NETCHESSZX_UI_ERROR_INVALID_IP);
            setConnectedUi(false);
            return;
        }
        if (isMqttMode() && room.isEmpty()) {
            appendLog("ERROR: empty MQTT room");
            setStatusText(NETCHESSZX_UI_ERROR_INVALID_ROOM);
            return;
        }
        if (isMqttMode() && !InputHelpers::isMqttRoomSyntaxOk(room)) {
            appendLog("ERROR: MQTT room must be MS plus four hexadecimal characters");
            setStatusText(NETCHESSZX_UI_ERROR_INVALID_MQTT_ROOM);
            return;
        }
        if (isMqttMode() && roomEdit_->text() != room) {
            roomEdit_->setText(room);
        }
        if (socket_->state() != QAbstractSocket::UnconnectedState) {
            socket_->abort();
        }
        if (directServer_ != nullptr && directServer_->isListening()) {
            directServer_->close();
        }

        QSettings settings;
        if (isMqttMode()) {
            mqttBrokerCache_ = host;
            mqttPortCache_ = port;
        } else if (!pcIsHost_) {
            directIpCache_ = host;
            directPortCache_ = port;
            rememberDirectGuestIp(settings, host);
        } else {
            directPortCache_ = port;
        }
        const QString savedConnectionHost =
            (!isMqttMode() && pcIsHost_) ?
                (directIpCache_.isEmpty() ? QStringLiteral("192.168.0.") : directIpCache_) :
                host;

        settings.setValue("connection/host", savedConnectionHost);
        settings.setValue("connection/port", port);
        settings.setValue(kDirectPortSettingsKey, directPortCache_);
        settings.setValue(kMqttPortSettingsKey, mqttPortCache_);
        settings.setValue("connection/mqtt", isMqttMode());
        settings.setValue("connection/room", room);
        settings.setValue("connection/pcHost", pcIsHost_);
        settings.setValue("connection/hostWhite", hostPlaysWhite_);

        transportCodec_.clearMqtt();
        clearMqttSubscriptionState();
        mqttSessionId_ = (isMqttMode() && pcIsHost_) ? newMqttSessionId() : 0;
        lastSocketError_.clear();
        mqttNextPacketId_ = 1;
        mqttRoom_ = room;

        if ((isMqttMode() && !initializeMqttSession()) ||
            (!isMqttMode() && !initializeDirectSession())) {
            appendLog("ERROR: session init failed");
            setStatusText(NETCHESSZX_UI_PHASE_CONNECTION_FAILED);
            setConnectedUi(false);
            return;
        }

        if (!isMqttMode() && pcIsHost_) {
            appendLog(QString("LISTEN :%1").arg(port));
            appendLog(QString("SESSION HOST host=%1 pc=%2")
                          .arg(hostSideLetter(), pcSideLetter()));
            if (!directServer_->listen(QHostAddress::Any, port)) {
                appendLog("ERROR: listen failed: " + directServer_->errorString());
                setStatusText(NETCHESSZX_UI_ERROR_LISTEN_FAILED);
                setConnectedUi(false);
                return;
            }
            setStatusText(NETCHESSZX_UI_NOTICE_LISTENING_OPPONENT);
            setConnectedUi(false);
            return;
        }

        appendLog(QString("CONNECT %1:%2").arg(host).arg(port));
        if (isMqttMode()) {
            appendLog(pcIsHost_
                          ? QString("SESSION HOST host=%1 pc=%2")
                                .arg(hostSideLetter(), pcSideLetter())
                          : QString("SESSION GUEST host=auto pc=auto"));
        }
        setStatusText(NETCHESSZX_UI_NOTICE_CONNECTING_PC);
        if (!isMqttMode()) {
            const uint8_t linkId = registerDirectSocket(socket_);
            if (linkId == SESSION_LINK_NONE) {
                appendLog("ERROR: no direct link id available");
                setStatusText(NETCHESSZX_UI_PHASE_CONNECTION_FAILED);
                setConnectedUi(false);
                return;
            }
            directPrimaryLinkId_ = linkId;
            directConnectRetryActive_ = !pcIsHost_;
        }
        socket_->connectToHost(host, port);
        setConnectedUi(false);
    }

    void resetGame(const QString &status)
    {
        cancelParadoxAbyss();
        ++gameGeneration_;
        ++pieceRevealGeneration_;
        pieceRevealTimer_.stop();
        stopMirrorlockFlip();
        clearDestinationFeedback();
        stopGameClock();
        clearTakebackState();
        closeControlPrompt();
        closeDirectDecisionPrompt();
        directUiBusy_ = false;
        gameOver_ = false;
        if (!isMqttMode()) {
            transportCodec_.clearMqtt();
            clearMqttSubscriptionState();
        }
        pcTurn_ = false;
        lastMove_.clear();
        clearMoveHistory();
        boardPiecesVisible_ = false;
        clearSelection();
        resetBoard();
        nextPly_ = 1;
        moveEdit_->clear();
        selectedLabel_->setText("Alignment: none");
        setStatusText(status);
        refreshBoard();
        refreshTurnLabel();
    }

    static void splitElapsed(qint64 elapsedMs, uint8_t *hour,
                             uint8_t *minute, uint8_t *second)
    {
        qint64 total = elapsedMs > 0 ? elapsedMs / 1000 : 0;
        if (total > kMaxClockSeconds) {
            total = kMaxClockSeconds;
        }
        *hour = static_cast<uint8_t>(total / 3600);
        *minute = static_cast<uint8_t>((total / 60) % 60);
        *second = static_cast<uint8_t>(total % 60);
    }

    static qint64 elapsedFromSave(uint8_t hour, uint8_t minute, uint8_t second)
    {
        return (static_cast<qint64>(hour) * 3600 +
                static_cast<qint64>(minute) * 60 +
                static_cast<qint64>(second)) * 1000;
    }

    bool currentSaveState(netchesszx_save_state_t *state, bool forPeer) const
    {
        if (state == nullptr || (gameOver_ && !ms_rules_is_over())) {
            return false;
        }
        std::memset(state, 0, sizeof(*state));
        for (int row = 0; row < 8; ++row) {
            for (int col = 0; col < 8; ++col) {
                state->cells[row * 8 + col] = board_[row][col];
            }
        }
        state->ply = static_cast<uint16_t>(nextPly_ > 0 ? nextPly_ - 1 : 0);
        state->side = ms_rules_side() == MS_SIDE_A ? NETCHESSZX_SAVE_SIDE_WHITE
                                                   : NETCHESSZX_SAVE_SIDE_BLACK;
        state->over = gameOver_ ? 1u : 0u;
        state->host_color = hostPlaysWhite_ ? NETCHESSZX_SAVE_HOST_WHITE
                                            : NETCHESSZX_SAVE_HOST_BLACK;
        state->flags = 0;
        if (gameClockRunning_) {
            state->flags |= NETCHESSZX_SAVE_FLAG_ACTIVE;
        }
        if (gameOver_) {
            state->flags |= NETCHESSZX_SAVE_FLAG_GAME_OVER;
        }
        splitElapsed(gameClockRunning_ ? gameTimerOffsetMs_ + gameTimer_.elapsed() : 0,
                      &state->game_hour, &state->game_minute, &state->game_second);
        splitElapsed(gameClockRunning_ ? moveTimerOffsetMs_ + moveTimer_.elapsed() : 0,
                      &state->move_hour, &state->move_minute, &state->move_second);
        state->view_flags = (!forPeer && !boardStandardOrientation_)
            ? NETCHESSZX_SAVE_VIEW_FLIPPED : 0u;
        return netchesszx_save_state_validate(state) == NETCHESSZX_SAVE_OK;
    }

    bool writeSaveFilePath(const QString &path)
    {
        if (!canSaveGameFile()) {
            setStatusText("Save failed");
            return false;
        }
        const QString fullPath = SaveGameStore::ensureExtension(path);
        netchesszx_save_state_t state;

        if (!currentSaveState(&state, false) ||
            !SaveGameStore::write(fullPath, state)) {
            setStatusText("Save failed");
            return false;
        }
        appendLog("SAVE: " + fullPath);
        setStatusText("Game saved");
        return true;
    }

    bool writeSaveFile(const QString &name)
    {
        return writeSaveFilePath(SaveGameStore::pathForName(
            SaveGameStore::defaultDirectory(), name));
    }

    bool restoreBusy() const
    {
        return directUiBusy_ || resetPromptOpen_ ||
               directDecisionBox_ != nullptr;
    }

    bool resignCanPreemptBusy() const
    {
        return directUiBusy_ == SESSION_REQUEST_MOVE &&
               !resetPromptOpen_ && directDecisionBox_ == nullptr;
    }

    bool restorePeerReady() const
    {
        if (!isConnected()) {
            return false;
        }
        return directSessionReady_;
    }

    static constexpr bool kSaveRestoreAvailable = true;

    bool canSaveGameFile() const
    {
        return kSaveRestoreAvailable && restorePeerReady() &&
               boardPiecesVisible_ && !restoreBusy() &&
               (!gameOver_ || ms_rules_is_over());
    }

    bool canLoadGameFile() const
    {
        return kSaveRestoreAvailable && restorePeerReady() &&
               !restoreBusy();
    }

    uint8_t currentSaveHostColor() const
    {
        return hostPlaysWhite_ ? NETCHESSZX_SAVE_HOST_WHITE
                               : NETCHESSZX_SAVE_HOST_BLACK;
    }

    bool restoreHostColorOk(const netchesszx_save_state_t &state) const
    {
        return state.host_color == currentSaveHostColor();
    }

    static uint8_t directRestorePhase(uint8_t flags)
    {
        if ((flags & NETCHESSZX_SAVE_FLAG_GAME_OVER) != 0u) {
            return SESSION_PHASE_OVER;
        }
        return (flags & NETCHESSZX_SAVE_FLAG_ACTIVE) != 0u
            ? SESSION_PHASE_ACTIVE : SESSION_PHASE_READY;
    }

    void refreshSaveLoadButtons()
    {
        if (saveGameButton_ != nullptr) {
            saveGameButton_->setEnabled(canSaveGameFile());
        }
        if (loadGameButton_ != nullptr) {
            loadGameButton_->setEnabled(canLoadGameFile());
        }
    }

    static void setPeerView(netchesszx_save_state_t *state, bool pcIsHost)
    {
        (void)pcIsHost;
        state->view_flags = 0u;
    }

    bool loadSaveFilePath(const QString &path)
    {
        if (!canLoadGameFile()) {
            setStatusText("Load failed");
            return false;
        }
        netchesszx_save_state_t state;
        if (!SaveGameStore::read(path, &state)) {
            setStatusText("Load failed");
            return false;
        }
        if (!restoreHostColorOk(state)) {
            MS_STATE_SWAP_SIDES(&state);
            state.host_color = currentSaveHostColor();
        }
        setPeerView(&state, pcIsHost_);
        uint8_t wire[NETCHESSZX_SAVE_WIRE_SIZE];
        char b64[NETCHESSZX_SAVE_WIRE_B64_SIZE];
        if (netchesszx_save_wire_pack(wire, sizeof(wire), &state) !=
                NETCHESSZX_SAVE_OK ||
            netchesszx_save_wire_b64_encode(b64, sizeof(b64), wire,
                                            sizeof(wire)) !=
                NETCHESSZX_SAVE_OK ||
            !submitSessionLocalRequest(
                SESSION_REQUEST_RESTORE, state.ply,
                QByteArray(b64, NETCHESSZX_SAVE_WIRE_B64_SIZE),
                directRestorePhase(state.flags))) {
            setStatusText(QStringLiteral("Load failed"));
            return false;
        }
        setStatusText(QStringLiteral("Waiting opponent approval"));
        refreshSaveLoadButtons();
        return true;
    }

    bool loadSaveFile(const QString &name)
    {
        return loadSaveFilePath(SaveGameStore::pathForName(
            SaveGameStore::defaultDirectory(), name));
    }

    /* Save button: Spectrum-style one-click save into the first free slot;
       the file name (NNYMDHMM.msh) carries the slot and timestamp exactly
       like the Spectrum FILE browser, no native dialog involved. */
    void saveGameWithDialog()
    {
        if (!canSaveGameFile()) {
            return;
        }
        const QString savesDirectory = SaveGameStore::defaultDirectory();
        const QVector<SaveSlotEntry> saveSlots =
            SaveGameStore::scanSlots(savesDirectory);

        for (int i = 0; i < saveSlots.size(); ++i) {
            if (saveSlots[i].used) {
                continue;
            }
            const QString base =
                SaveGameStore::slotBaseName(
                    i + 1, QDateTime::currentDateTimeUtc());
            if (writeSaveFilePath(
                    SaveGameStore::slotFilePath(savesDirectory, base))) {
                setStatusText(QStringLiteral("Saved GAME%1").arg(i + 1));
            }
            refreshSaveLoadButtons();
            return;
        }
        setStatusText(QStringLiteral("All save slots are full"));
        openSavedGamesDialog();
    }

    void loadGameWithDialog()
    {
        if (!canLoadGameFile()) {
            return;
        }
        openSavedGamesDialog();
    }

    void openSavedGamesDialog()
    {
        QDialog dialog(this);
        dialog.setWindowTitle(QStringLiteral("Saved games"));
        dialog.resize(380, 430);
        dialog.setMinimumSize(380, 430);
        const QString savesDirectory = SaveGameStore::defaultDirectory();

        auto *layout = new QVBoxLayout(&dialog);
        auto *table = new QTableWidget(SaveGameStore::kSlotCount, 3, &dialog);

        table->setHorizontalHeaderLabels(QStringList()
                                         << QStringLiteral("NAME")
                                         << QStringLiteral("DATE")
                                         << QStringLiteral("TIME"));
        table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
        table->verticalHeader()->setVisible(false);
        table->setSelectionBehavior(QAbstractItemView::SelectRows);
        table->setSelectionMode(QAbstractItemView::SingleSelection);
        table->setEditTriggers(QAbstractItemView::NoEditTriggers);
        table->setShowGrid(false);
        table->setStyleSheet(QStringLiteral(
            "QTableWidget { background:" MSH_SURFACE "; color:" MSH_TEXT ";"
            " border:1px solid " MSH_BORDER "; border-radius:" MSH_R ";"
            " selection-background-color:" MSH_ACCENT_DEEP ";"
            " selection-color:" MSH_WELL "; }"
            "QHeaderView::section { background:" MSH_SURFACE "; color:" MSH_TEXT ";"
            " border:0; border-bottom:1px solid " MSH_BORDER "; padding:4px; }"));
        layout->addWidget(table);

        auto *buttons = new QHBoxLayout();
        auto *loadButton = new QPushButton(QStringLiteral("Load"), &dialog);
        auto *eraseButton = new QPushButton(QStringLiteral("Erase"), &dialog);
        auto *folderButton =
            new QPushButton(QStringLiteral("Open folder"), &dialog);
        auto *closeButton = new QPushButton(QStringLiteral("Close"), &dialog);

        buttons->addWidget(loadButton);
        buttons->addWidget(eraseButton);
        buttons->addStretch(1);
        buttons->addWidget(folderButton);
        buttons->addWidget(closeButton);
        layout->addLayout(buttons);

        QVector<SaveSlotEntry> saveSlots;
        const bool canLoad = canLoadGameFile();
        const QLocale locale = QLocale::system();

        const auto refresh = [&]() {
            saveSlots = SaveGameStore::scanSlots(savesDirectory);
            for (int i = 0; i < SaveGameStore::kSlotCount; ++i) {
                const SaveSlotEntry &entry = saveSlots[i];
                auto *name = new QTableWidgetItem(
                    entry.used ? QStringLiteral("GAME%1%2").arg(i + 1).arg(entry.duplicate ? QStringLiteral(" !") : QString())
                               : QStringLiteral("- free -"));
                auto *date = new QTableWidgetItem(
                    entry.duplicate ? QStringLiteral("DUPLICATE") :
                    entry.used ? locale.toString(entry.when.date(), QLocale::ShortFormat)
                               : QString());
                auto *time = new QTableWidgetItem(
                    entry.used ? locale.toString(entry.when.time(), QLocale::ShortFormat)
                               : QString());

                name->setForeground(QBrush(entry.used
                                               ? QColor(0x40, 0xd0, 0xd0)
                                               : QColor(0x70, 0x70, 0x80)));
                table->setItem(i, 0, name);
                table->setItem(i, 1, date);
                table->setItem(i, 2, time);
            }
        };
        const auto selectionUsed = [&]() {
            const int row = table->currentRow();
            return row >= 0 && row < saveSlots.size() && saveSlots[row].used;
        };
        const auto refreshButtons = [&]() {
            loadButton->setEnabled(canLoad && selectionUsed());
            eraseButton->setEnabled(selectionUsed());
        };

        connect(table, &QTableWidget::itemSelectionChanged, &dialog,
                refreshButtons);
        connect(table, &QTableWidget::cellDoubleClicked, &dialog,
                [&](int, int) {
                    if (loadButton->isEnabled()) {
                        loadButton->click();
                    }
                });
        connect(loadButton, &QPushButton::clicked, &dialog, [&]() {
            const int row = table->currentRow();

            if (row >= 0 && saveSlots[row].used &&
                loadSaveFilePath(saveSlots[row].filePath)) {
                refreshSaveLoadButtons();
                dialog.accept();
            }
        });
        connect(eraseButton, &QPushButton::clicked, &dialog, [&]() {
            const int row = table->currentRow();

            if (row >= 0 && saveSlots[row].used) {
                const QString slotName = QStringLiteral("GAME%1").arg(row + 1);
                const bool erase =
                    askQuestion(&dialog,
                                QStringLiteral("Erase saved game"),
                                QStringLiteral("Erase %1?").arg(slotName),
                                QMessageBox::Yes | QMessageBox::No) ==
                    QMessageBox::Yes;
                if (!erase) {
                    return;
                }
                if (!SaveGameStore::remove(saveSlots[row].filePath)) {
                    QMessageBox::warning(&dialog,
                                         QStringLiteral("Erase saved game"),
                                         QStringLiteral("Could not erase %1.").arg(slotName));
                }
                refresh();
                refreshButtons();
            }
        });
        connect(folderButton, &QPushButton::clicked, &dialog, [&]() {
            QDesktopServices::openUrl(
                QUrl::fromLocalFile(savesDirectory));
        });
        connect(closeButton, &QPushButton::clicked, &dialog, &QDialog::reject);

        refresh();
        refreshButtons();
        (void)dialog.exec();
        refreshSaveLoadButtons();
    }

    void requestTakeback(bool clearInput = false)
    {
        if (directUiBusy_ || resetPromptOpen_ || directLocalResignPending_ ||
            directResignRestartPending_) {
            return;
        }
        if (!canRequestTakeback()) {
            appendLog("ERROR: No move to take back");
            setStatusText("No move to take back");
            if (clearInput) { chatEdit_->clear(); }
            refreshChatButton();
            return;
        }
        resetPromptOpen_ = true;
        const QMessageBox::StandardButton answer =
            askQuestion(this, NETCHESSZX_UI_CONFIRM_PC_TAKEBACK_TITLE,
                        NETCHESSZX_UI_CONFIRM_PC_TAKEBACK_REQUEST,
                        QMessageBox::Yes | QMessageBox::No);
        if (!resetPromptOpen_) {
            return;
        }
        resetPromptOpen_ = false;
        if (answer != QMessageBox::Yes) {
            return;
        }
        if (!canRequestTakeback()) {
            setStatusText(QStringLiteral("No move to take back"));
            return;
        }
        if (submitSessionLocalRequest(SESSION_REQUEST_TAKEBACK,
                                      takebackSnapshot_.ply)) {
            setStatusText(QStringLiteral("Takeback requested"));
            if (clearInput) { chatEdit_->clear(); }
        }
        return;
    }

    void sendChat()
    {
        QString text = chatEdit_->text().trimmed();
        if (text.isEmpty()) {
            return;
        }

        text.replace('\r', ' ');
        text.replace('\n', ' ');
        const QString cmd = text.toLower();
        if (InputHelpers::isMoveSyntaxOk(cmd)) {
            sendMove(true);
            return;
        }
        chatInputHistory_.append(text);
        while (chatInputHistory_.size() > 5) {
            chatInputHistory_.removeFirst();
        }
        chatInputHistoryIndex_ = static_cast<int>(chatInputHistory_.size());
        if (cmd == "/save" || cmd.startsWith(QStringLiteral("/save "))) {
            if (writeSaveFile(text.mid(5).trimmed())) {
                chatEdit_->clear();
                refreshChatButton();
            }
            return;
        }
        if (cmd == "/load" || cmd.startsWith(QStringLiteral("/load "))) {
            if (loadSaveFile(text.mid(5).trimmed())) {
                chatEdit_->clear();
                refreshChatButton();
            }
            return;
        }
        if (isSessionControlCommand(cmd) &&
            directLocalResignPending_) {
            setStatusText(NETCHESSZX_UI_NOTICE_WAITING_RESIGN_ACK);
            return;
        }
        if (isSessionControlCommand(cmd) &&
            directResignRestartPending_) {
            setStatusText(NETCHESSZX_UI_NOTICE_RESIGN_ALREADY_APPLIED);
            return;
        }
        if (isSessionControlCommand(cmd) &&
            directUiBusy_ != 0u &&
            !(cmd == QStringLiteral("/resign") &&
              resignCanPreemptBusy())) {
            setStatusText(NETCHESSZX_UI_NOTICE_WAITING_ACK);
            return;
        }
        if (cmd == "/resign") {
            if (!gameClockRunning_) {
                setStatusText(NETCHESSZX_UI_NOTICE_GAME_NOT_STARTED);
                return;
            }
            if (restoreBusy() && !resignCanPreemptBusy()) {
                setStatusText("Load in progress");
                return;
            }
            resetPromptOpen_ = true;
            const QMessageBox::StandardButton answer =
                askQuestion(this, NETCHESSZX_UI_CONFIRM_PC_RESIGN_TITLE,
                            NETCHESSZX_UI_CONFIRM_RESIGN,
                            QMessageBox::Yes | QMessageBox::No);
            if (!resetPromptOpen_) {
                return;
            }
            resetPromptOpen_ = false;
            if (answer != QMessageBox::Yes) {
                return;
            }
            directLocalResignPending_ = true;
            if (submitSessionLocalRequest(SESSION_REQUEST_RESIGN)) {
                setStatusText(NETCHESSZX_UI_NOTICE_WAITING_RESIGN_ACK);
                chatEdit_->clear();
            } else {
                directLocalResignPending_ = false;
            }
            return;
        }
        if (cmd == "/takeback") {
            requestTakeback(true);
            return;
        }
        if (!submitSessionLocalRequest(SESSION_REQUEST_CHAT, 0u,
                                       text.toLatin1())) {
            return;
        }
        chatEdit_->clear();
        refreshChatButton();
    }

    void sendGameStart()
    {
        if (socket_->state() != QAbstractSocket::ConnectedState) {
            setStatusText(NETCHESSZX_UI_ERROR_NOT_CONNECTED);
            return;
        }
        if (gameClockRunning_) {
            setStatusText(NETCHESSZX_UI_ERROR_GAME_ALREADY_RUNNING);
            return;
        }
        if (restoreBusy()) {
            setStatusText("Load in progress");
            return;
        }
        if (!pcIsHost_) {
            setStatusText(isMqttMode() ? NETCHESSZX_UI_PHASE_WAITING_HOST_START :
                                         NETCHESSZX_UI_PHASE_WAITING_OPPONENT_START);
            appendLog("START ignored: only host starts");
            return;
        }
        if (!directSessionReady_) {
            setStatusText(NETCHESSZX_UI_PHASE_WAITING_OPPONENT_SHORT);
            appendLog("START ignored: peer not ready");
            return;
        }
        if (submitSessionLocalRequest(SESSION_REQUEST_START)) {
            syncSharedBoardOrientation();
            gameOver_ = false;
            setStatusText(NETCHESSZX_UI_NOTICE_STARTING_GAME_PC);
        }
        setConnectedUi(true);
    }

    void sendMove(bool confirmed)
    {
        const QString move = moveEdit_->text().trimmed().toLower();
        if (!confirmed) {
            setStatusText(NETCHESSZX_UI_NOTICE_MOVE_READY);
            return;
        }
        if (!canPcMove()) {
            if (socket_->state() != QAbstractSocket::ConnectedState) {
                appendLog("ERROR: not connected");
                setStatusText(NETCHESSZX_UI_ERROR_NOT_CONNECTED);
            } else if (directUiBusy_) {
                appendLog(QStringLiteral("ERROR: waiting session core"));
                setStatusText(NETCHESSZX_UI_PHASE_WAITING_OPPONENT);
            } else if (!gameClockRunning_) {
                if (!gameOver_) {
                    setStatusBarText(NETCHESSZX_UI_NOTICE_GAME_NOT_STARTED);
                }
                appendLog(QStringLiteral("ERROR: game not active"));
            } else {
                appendLog("ERROR: opponent moves first; local side waits");
                setStatusBarText("Not your turn");
            }
            return;
        }
        if (!InputHelpers::isMoveSyntaxOk(move)) {
            appendLog("ERROR: an alignment is one Lattice square, e.g. d3");
            return;
        }
        const int toCol = move[0].unicode() - 'a';
        const int toRow = '8' - move[1].unicode();

        QElapsedTimer sendPrepTimer;
        sendPrepTimer.start();
        const QByteArray moveBytes = move.toLatin1();
        const int legalRc = ms_rules_can_play(moveBytes.constData());
        if (legalRc != MS_OK) {
            appendLog(QString("ERROR: %1: %2")
                          .arg(QString::fromLatin1(ms_rules_error_string(legalRc)), move));
            setStatusText(QString("Illegal alignment: %1").arg(move.toUpper()));
            return;
        }

        if (!submitSessionLocalRequest(SESSION_REQUEST_MOVE, 0u,
                                       moveBytes)) {
            return;
        }
        const qint64 prepMs = sendPrepTimer.elapsed();
        if (prepMs > kMoveSendWarnMs) {
            appendLog(QString("WARN: move send prep took %1 ms").arg(prepMs));
        }

        clearSelection();
        moveEdit_->clear();
        refreshBoard();
        showDestinationFeedback(toRow, toCol);
        setStatusText(NETCHESSZX_UI_PHASE_WAITING_OPPONENT);
        setConnectedUi(true);
    }

    void consumeReadyRead(QTcpSocket *sock)
    {
        if (sock == nullptr) {
            return;
        }
        const QByteArray data = sock->readAll();
        const uint8_t directLink = directLinkForSocket(sock);
        if (directLink == SESSION_LINK_NONE) {
            if (sock != socket_ || !isMqttMode()) {
                return;
            }
            linkWatch_.restart();
            consumeMqttBytes(data);
            return;
        }

        uint8_t machPlatform = NETCHESS_PLAT_UNKNOWN;
        bool machReceived = false;
        const DesktopTransportCodec::DirectFeedResult result =
            transportCodec_.feedDirect(directLink, data,
                [this, directLink, &machPlatform,
                 &machReceived](const QByteArray &lineBytes) {
                    appendLog("RX: " + QString::fromLatin1(lineBytes));
                    uint8_t platform;
                    if (parseMachPayload(lineBytes, &platform)) {
                        machPlatform = platform;
                        machReceived = true;
                    }
                    sessionController_.receiveDirect(directLink, lineBytes);
                });
        if (result.overflow) {
            appendLog("ERROR: direct RX line too long");
            if (directLink == directPrimaryLinkId_) {
                directEndStatus_ = NETCHESSZX_UI_ERROR_CONNECTION_LOST;
            }
            if (sock != nullptr) {
                sock->abort();
            }
            return;
        }
        if (result.delivered) {
            sessionController_.pump();
            if (machReceived && directSessionReady_ &&
                directLink == directPrimaryLinkId_) {
                setPeerPlatform(machPlatform);
            }
        }
    }

    bool isMqttMode() const
    {
        return mqttRadio_ != nullptr && mqttRadio_->isChecked();
    }

    void configureSessionFromUi()
    {
        pcIsHost_ = roleHostRadio_ != nullptr && roleHostRadio_->isChecked();
        if (pcIsHost_) {
            hostPlaysWhite_ = hostEchoFirstRadio_ == nullptr ||
                              hostEchoFirstRadio_->isChecked();
        } else if (!isConnected() && !isConnecting()) {
            hostPlaysWhite_ = false;
        }
        pcPlaysWhite_ = pcIsHost_ ? hostPlaysWhite_ : !hostPlaysWhite_;
        if (!isMqttMode()) {
            mqttSideReady_ = true;
        }
        syncSharedBoardOrientation();
        refreshMoveLogHeaders();
    }

    void updateSessionControlsEnabled()
    {
        const bool enabled = !isConnected() && !isConnecting();
        const bool colorVisible = pcIsHost_;
        const bool colorEnabled = enabled && pcIsHost_;

        if (roleHostRadio_) {
            roleHostRadio_->setEnabled(enabled);
        }
        if (roleGuestRadio_) {
            roleGuestRadio_->setEnabled(enabled);
        }
        if (hostColorLabel_) {
            hostColorLabel_->setVisible(colorVisible);
        }
        if (hostEchoFirstRadio_) {
            hostEchoFirstRadio_->setVisible(colorVisible);
            hostEchoFirstRadio_->setEnabled(colorEnabled);
        }
        if (hostSelfFirstRadio_) {
            hostSelfFirstRadio_->setVisible(colorVisible);
            hostSelfFirstRadio_->setEnabled(colorEnabled);
        }
    }

    QString hostSideLetter() const
    {
        return hostPlaysWhite_ ? "A" : "B";
    }

    static quint16 newMqttSessionId()
    {
        return static_cast<quint16>(
            QRandomGenerator::global()->bounded(1, 65536));
    }

    QString topicFor(const QString &suffix) const
    {
        // NOTE: mqttRoom_ is the pairing identifier and travels in cleartext
        // (MQTT over plain TCP, port 1883 — the Spectrum peer cannot do TLS).
        // The room code is NOT a security boundary: an on-path observer can read
        // it and inject frames. Do not treat it as a secret.
        return QString("netchesszx/v1/%1/%2").arg(mqttRoom_, suffix);
    }

    QString mqttInSuffix() const
    {
        return pcPlaysWhite_ ? "b2w" : "w2b";
    }

    QString mqttInAckSuffix() const
    {
        return pcPlaysWhite_ ? "ack_w" : "ack_b";
    }

    QString mqttPresenceSuffix() const
    {
        return pcPlaysWhite_ ? "pres_w" : "pres_b";
    }

    QString mqttPeerPresenceSuffix() const
    {
        return pcPlaysWhite_ ? "pres_b" : "pres_w";
    }

    uint16_t mqttPacketId()
    {
        if (mqttNextPacketId_ == 0) {
            mqttNextPacketId_ = 1;
        }
        return mqttNextPacketId_++;
    }

    bool writeMqttPacket(const QByteArray &packet, const QString &label)
    {
        if (packet.isEmpty()) {
            appendLog("ERROR: MQTT encode failed: " + label);
            return false;
        }
#ifdef NETCHESSZX_PC_MQTT_TX_FAILURE_TEST
        if (testMqttWriteFailure_) {
            appendLog("ERROR: forced MQTT write failure: " + label);
            return false;
        }
#endif
        const qint64 written = socket_->write(packet);
        if (written < 0) {
            appendLog("ERROR: MQTT write failed: " + socket_->errorString());
            return false;
        }
        if (written != packet.size()) {
            appendLog(QString("ERROR: MQTT partial write %1/%2")
                          .arg(written)
                          .arg(packet.size()));
            socket_->abort();
            return false;
        }
        socket_->flush();
        appendLog("MQTT TX: " + label);
        return true;
    }

    void mqttHandshake()
    {
        const QByteArray clientId = mqttClientIdFor(pcIsHost_, mqttClientNonce_);
        QByteArray willTopic;
        QByteArray willPayload;
        // Host-only will: it knows the session id at CONNECT time, so peers
        // can validate it. A guest will would be id-less and receivers drop
        // id-less F (any stray client could arm one and kill a live game);
        // guest death is detected by the session-core liveness timer instead.
        const bool useWill = pcIsHost_;

        if (useWill) {
            willTopic = topicFor(mqttPresenceSuffix()).toLatin1();
            willPayload = QString("F %1 %2")
                              .arg(pcPlaysWhite_ ? QStringLiteral("W")
                                                : QStringLiteral("B"))
                              .arg(mqttSessionId_)
                              .toLatin1();
        }
        const QByteArray packet = DesktopTransportCodec::encodeMqttConnect(
            clientId, 20, willTopic, willPayload, useWill);
        appendLog("CONNECTED TCP MQTT");
        setStatusText(NETCHESSZX_UI_NOTICE_MQTT_CONNECTING);
        (void)writeMqttPacket(packet, "CONNECT");
    }

    void clearMqttSubscriptionState()
    {
        mqttSubscribed_ = false;
        mqttSideReady_ = false;
        mqttSideTransitionPending_ = false;
        mqttSubackPending_.clear();
        mqttUnsubackPending_.clear();
        mqttActiveSubscriptions_.clear();
        mqttTargetSubscriptions_.clear();
        mqttObsoleteSubscriptions_.clear();
        mqttBufferedPublishes_.clear();
    }

    bool mqttSubscribe(const QString &suffix)
    {
        const QByteArray topic = topicFor(suffix).toLatin1();
        const uint16_t id = mqttPacketId();
        const QByteArray packet =
            DesktopTransportCodec::encodeMqttSubscribe(id, topic);
        if (!writeMqttPacket(packet, "SUB " + topicFor(suffix))) {
            return false;
        }
        mqttSubackPending_.insert(id, suffix);
        return true;
    }

    bool mqttUnsubscribe(const QString &suffix)
    {
        const QByteArray topic = topicFor(suffix).toLatin1();
        const uint16_t id = mqttPacketId();
        const QByteArray packet =
            DesktopTransportCodec::encodeMqttUnsubscribe(id, topic);
        if (!writeMqttPacket(packet, "UNSUB " + topicFor(suffix))) {
            return false;
        }
        mqttUnsubackPending_.insert(id, suffix);
        return true;
    }

    void replayMqttBufferedPublishes()
    {
        while (mqttSubscribed_ && sessionController_.initialized() &&
               !mqttBufferedPublishes_.isEmpty()) {
            const MqttBufferedPublish buffered =
                mqttBufferedPublishes_.takeFirst();
            if (!mqttActiveSubscriptions_.contains(buffered.suffix) ||
                !mqttTargetSubscriptions_.contains(buffered.suffix)) {
                continue;
            }
            (void)sessionController_.receiveMqtt(
                kMqttLinkId, buffered.topic, buffered.retained,
                buffered.payload);
        }
    }

    void advanceMqttSubscriptionTransition()
    {
        if (!mqttSubackPending_.isEmpty() ||
            !mqttUnsubackPending_.isEmpty()) {
            const int pending = mqttSubackPending_.size() +
                                mqttUnsubackPending_.size();
            setStatusText(QString("MQTT subscribing... %1").arg(pending));
            return;
        }

        if (mqttSideTransitionPending_ &&
            !mqttObsoleteSubscriptions_.isEmpty()) {
            const QSet<QString> obsolete = mqttObsoleteSubscriptions_;
            mqttObsoleteSubscriptions_.clear();
            for (const QString &suffix : obsolete) {
                if (!mqttUnsubscribe(suffix)) {
                    failMqttConnection("MQTT side unsubscribe send failed");
                    return;
                }
            }
            if (!mqttUnsubackPending_.isEmpty()) {
                setStatusText(QString("MQTT switching side... %1")
                                  .arg(mqttUnsubackPending_.size()));
                return;
            }
        }

        if (mqttActiveSubscriptions_ != mqttTargetSubscriptions_) {
            failMqttConnection("MQTT subscription transition incomplete");
            return;
        }

        mqttSubscribed_ = true;
        if (mqttSideTransitionPending_) {
            mqttSideTransitionPending_ = false;
            mqttSideReady_ = true;
            if (syncSharedBoardOrientation()) {
                refreshBoard();
            }
            appendLog("MQTT SIDE READY");
            setConnectedUi(isConnected());
            sessionController_.pump();
            replayMqttBufferedPublishes();
            return;
        }

        mqttSideReady_ = pcIsHost_;
        setConnectedUi(true);
        setStatusText(pcIsHost_ ? "MQTT link established - waiting opponent"
                                : "MQTT link established - waiting host");
        appendLog("MQTT READY");
        if (!mqttSessionLinked_) {
            mqttSessionLinked_ = true;
            (void)sessionController_.linkUp(kMqttLinkId);
        }
        replayMqttBufferedPublishes();
    }

    bool mqttPublish(const QString &suffix, const QString &payload,
                     bool retain = false)
    {
        if (!mqttSubscribed_ || !mqttSideReady_) {
            return false;
        }
        const QByteArray topic = topicFor(suffix).toLatin1();
        const QByteArray body = payload.toLatin1();
        const uint16_t id = mqttPacketId();
        const QByteArray packet = DesktopTransportCodec::encodeMqttPublish(
            id, topic, body, retain);
        return writeMqttPacket(packet, "PUB " + suffix + " " + payload);
    }

    void consumeMqttBytes(const QByteArray &data)
    {
        bool malformed = false;
        const QVector<QByteArray> packets = transportCodec_.feedMqtt(data, &malformed);
        for (const QByteArray &packet : packets) {
            handleMqttPacket(packet);
        }
        if (malformed) {
            appendLog("ERROR: malformed MQTT packet");
            socket_->abort();
        }
    }

    void handleMqttPacket(const QByteArray &raw)
    {
        DesktopTransportCodec::MqttPacket packet;
        if (!DesktopTransportCodec::decodeMqttPacket(raw, &packet)) {
            appendLog("ERROR: MQTT parse failed");
            return;
        }

        if (packet.type == DesktopTransportCodec::MqttPacketType::Connack) {
            if (packet.returnCode != 0) {
                failMqttConnection(QString("MQTT CONNACK %1").arg(packet.returnCode));
                return;
            }
            clearMqttSubscriptionState();
            mqttTargetSubscriptions_.insert(QStringLiteral("meta"));
            if (pcIsHost_) {
                mqttTargetSubscriptions_.insert(mqttInSuffix());
                mqttTargetSubscriptions_.insert(mqttInAckSuffix());
                mqttTargetSubscriptions_.insert(mqttPeerPresenceSuffix());
            }
            if ((pcIsHost_ &&
                 (!mqttSubscribe(mqttInSuffix()) ||
                  !mqttSubscribe(mqttInAckSuffix()) ||
                  !mqttSubscribe(mqttPeerPresenceSuffix()))) ||
                !mqttSubscribe(QStringLiteral("meta"))) {
                failMqttConnection("MQTT subscribe send failed");
                return;
            }
            setStatusText(NETCHESSZX_UI_NOTICE_MQTT_SUBSCRIBING);
            return;
        }

        if (packet.type == DesktopTransportCodec::MqttPacketType::Suback) {
            const auto pending = mqttSubackPending_.constFind(packet.packetId);
            if (pending == mqttSubackPending_.constEnd()) {
                appendLog(QString("IGNORE MQTT SUBACK id=%1 (not pending)")
                              .arg(packet.packetId));
                return;
            }
            if (packet.returnCode == 0x80) {
                failMqttConnection(QString("MQTT SUBACK failed id=%1")
                                       .arg(packet.packetId));
                return;
            }
            const QString suffix = pending.value();
            mqttSubackPending_.remove(packet.packetId);
            mqttActiveSubscriptions_.insert(suffix);
            advanceMqttSubscriptionTransition();
            return;
        }

        if (packet.type == DesktopTransportCodec::MqttPacketType::Unsuback) {
            const auto pending = mqttUnsubackPending_.constFind(packet.packetId);
            if (pending == mqttUnsubackPending_.constEnd()) {
                appendLog(QString("IGNORE MQTT UNSUBACK id=%1 (not pending)")
                              .arg(packet.packetId));
                return;
            }
            const QString suffix = pending.value();
            mqttUnsubackPending_.remove(packet.packetId);
            mqttActiveSubscriptions_.remove(suffix);
            advanceMqttSubscriptionTransition();
            return;
        }

        if (packet.type == DesktopTransportCodec::MqttPacketType::Publish) {
            appendLog(QString("MQTT RX %1%2: %3")
                          .arg(QString::fromLatin1(packet.topic),
                               packet.retained ? QString(" [retained]") : QString(),
                               QString::fromLatin1(packet.payload)));
            if (packet.packetId != 0) {
                writeMqttPacket(
                    DesktopTransportCodec::encodeMqttPuback(packet.packetId),
                    QString("PUBACK %1").arg(packet.packetId));
            }
            handleMqttPayload(packet.topic, packet.payload, packet.retained);
            return;
        }
    }

    void handleMqttPayload(const QByteArray &topic,
                           const QByteArray &payload,
                           bool retained = false)
    {
        const int slash = topic.lastIndexOf('/');
        const QString suffix = QString::fromLatin1(
            slash < 0 ? topic : topic.mid(slash + 1));
        const bool exactTopic = topic == topicFor(suffix).toLatin1();
        const bool metaReady =
            suffix == QStringLiteral("meta") &&
            mqttActiveSubscriptions_.contains(suffix);
        const bool sideReady =
            mqttSubscribed_ && mqttSideReady_ &&
            mqttActiveSubscriptions_.contains(suffix) &&
            mqttTargetSubscriptions_.contains(suffix);
        const bool subscriptionPending =
            !mqttSubackPending_.isEmpty() ||
            !mqttUnsubackPending_.isEmpty();

        if (exactTopic && mqttSessionActive() && subscriptionPending &&
            mqttTargetSubscriptions_.contains(suffix)) {
            if (mqttBufferedPublishes_.size() >= kMqttBufferedPublishMax) {
                failMqttConnection("MQTT subscription publish buffer full");
                return;
            }
            mqttBufferedPublishes_.append(
                MqttBufferedPublish{suffix, topic, payload, retained});
            appendLog("QUEUE MQTT topic " + QString::fromLatin1(topic));
            return;
        }

        if (!exactTopic || (!metaReady && !sideReady) ||
            !mqttSessionActive() ||
            !sessionController_.receiveMqtt(kMqttLinkId, topic,
                                            retained, payload)) {
            appendLog("IGNORE MQTT topic " + QString::fromLatin1(topic));
            return;
        }

        if (!retained && directSessionReady_ && suffix == mqttInSuffix()) {
            uint8_t platform;
            if (parseMachPayload(payload, &platform)) {
                setPeerPlatform(platform);
            }
        }
    }

    void endGameOver(const QString &message)
    {
        closeGameInteractions();
        gameOver_ = true;
        pcTurn_ = false;
        stopGameClock();
        appendLog(message);
        setStatusText(message);
        setConnectedUi(true);
        // Resignation and a natural PARADOX use the abyss animation.
        const QString upper = message.toUpper();
        if (upper.contains(QStringLiteral("RESIGN"))
            || upper.contains(QStringLiteral("PARADOX"))) {
            refreshBoard();
            const int gen = abyssGeneration_;
            QTimer::singleShot(120, this, [this, gen]() { playParadoxAbyss(gen); });
        }
    }

    void closeControlPrompt()
    {
        if (!resetPromptOpen_) {
            return;
        }
        resetPromptOpen_ = false;
        if (QMessageBox *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget())) {
            box->close();
        }
    }

    void closeGameInteractions()
    {
        closeControlPrompt();
        closeDirectDecisionPrompt();
        clearTakebackState();
    }

    bool restoreApplyState(const netchesszx_save_state_t &st)
    {
        ms_state_t rulesState = {};

        if (!kSaveRestoreAvailable ||
            netchesszx_save_state_validate(&st) != NETCHESSZX_SAVE_OK) {
            return false;
        }
        std::memcpy(rulesState.cells, st.cells, sizeof(rulesState.cells));
        rulesState.side = st.side;
        rulesState.over = st.over;
        if (ms_rules_restore(&rulesState) != MS_OK) {
            return false;
        }
        cancelParadoxAbyss();
        ++gameGeneration_;
        ++pieceRevealGeneration_;
        pieceRevealTimer_.stop();
        stopMirrorlockFlip();
        clearDestinationFeedback();
        for (int row = 0; row < 8; ++row) {
            for (int col = 0; col < 8; ++col) {
                board_[row][col] = st.cells[row * 8 + col];
            }
        }
        hostPlaysWhite_ = (st.host_color == NETCHESSZX_SAVE_HOST_WHITE);
        pcPlaysWhite_ = pcIsHost_ ? hostPlaysWhite_ : !hostPlaysWhite_;
        clearTakebackState();
        closeControlPrompt();
        closeDirectDecisionPrompt();
        directUiBusy_ = false;
        nextPly_ = static_cast<int>(st.ply) + 1;
        const bool restoredActive = (st.flags & NETCHESSZX_SAVE_FLAG_ACTIVE) != 0u;
        gameOver_ = st.over != 0u;
        pcTurn_ = restoredActive && !gameOver_ &&
            ((st.side == NETCHESSZX_SAVE_SIDE_WHITE) == pcPlaysWhite_);
        lastMove_.clear();
        clearMoveHistory();
        appendLog(QStringLiteral("RESTORED at alignment %1").arg(st.ply));
        boardPiecesVisible_ = true;
        clearSelection();
        moveEdit_->clear();
        selectedLabel_->setText("Alignment: none");
        syncSharedBoardOrientation();
        coordinatesInitialized_ = false;
        refreshMoveLogHeaders();
        refreshLegalMoves();
        refreshBoard();
        if (restoredActive && !gameOver_) {
            startGameClock(elapsedFromSave(st.game_hour, st.game_minute, st.game_second),
                           elapsedFromSave(st.move_hour, st.move_minute, st.move_second));
        } else if (!restoredActive || gameOver_) {
            stopGameClock();
        }
        refreshTurnLabel();
        setConnectedUi(true);
        return true;
    }

    bool applyDirectRestore(const QByteArray &payload,
                            uint16_t *restoredPly,
                            uint8_t *restoredPhase)
    {
        uint8_t wire[NETCHESSZX_SAVE_WIRE_SIZE];
        netchesszx_save_state_t state = {};
        if (payload.size() != NETCHESSZX_SAVE_WIRE_B64_SIZE ||
            netchesszx_save_wire_b64_decode(
                wire, sizeof(wire), payload.constData(),
                static_cast<size_t>(payload.size())) != NETCHESSZX_SAVE_OK ||
            netchesszx_save_wire_unpack(&state, wire, sizeof(wire)) !=
                NETCHESSZX_SAVE_OK ||
            !restoreHostColorOk(state) || !restoreApplyState(state)) {
            appendLog("RX: restore decode/apply failed");
            setStatusText(QStringLiteral("Load failed"));
            return false;
        }
        if (restoredPly != nullptr) {
            *restoredPly = state.ply;
        }
        if (restoredPhase != nullptr) {
            *restoredPhase = directRestorePhase(state.flags);
        }
        appendLog("RX: game restored");
        setStatusText(QStringLiteral("Game loaded"));
        return true;
    }

    void clearTakebackState()
    {
        takebackSnapshot_ = TakebackSnapshot{};
    }

    bool saveTakebackSnapshot(int ply, bool localMove)
    {
        TakebackSnapshot snapshot;

        std::memcpy(snapshot.board, board_, sizeof(board_));
        if (ms_rules_save(&snapshot.rules) != MS_OK) {
            appendLog("ERROR: could not save takeback snapshot");
            return false;
        }
        snapshot.lastMove = lastMove_;
        snapshot.ply = ply;
        snapshot.nextPly = nextPly_;
        snapshot.historyCount = moveHistoryRecords_.size();
        snapshot.pcTurn = pcTurn_;
        snapshot.localMove = localMove;
        snapshot.valid = true;
        takebackSnapshot_ = snapshot;
        return true;
    }

    void restoreTakebackSnapshot()
    {
        if (!takebackSnapshot_.valid) {
            return;
        }
        stopMirrorlockFlip();
        clearDestinationFeedback();
        std::memcpy(board_, takebackSnapshot_.board, sizeof(board_));
        (void)ms_rules_restore(&takebackSnapshot_.rules);
        while (moveHistoryRecords_.size() > takebackSnapshot_.historyCount) {
            moveHistoryRecords_.removeLast();
        }
        lastMove_ = takebackSnapshot_.lastMove;
        nextPly_ = takebackSnapshot_.nextPly;
        pcTurn_ = takebackSnapshot_.pcTurn;
        gameOver_ = false;
        clearSelection();
        selectedLabel_->setText("Alignment: none");
        clearTakebackState();
        refreshLegalMoves();
        refreshBoard();
        renderLogView();
        restartMoveClock();
        setConnectedUi(true);
    }

    bool applyDirectTakeback(uint16_t ply)
    {
        if (!takebackSnapshot_.valid || takebackSnapshot_.ply != ply ||
            ply != nextPly_ - 1) {
            appendLog(QString("ERROR: cannot apply TAKEBACK %1").arg(ply));
            return false;
        }
        restoreTakebackSnapshot();
        setStatusText(QStringLiteral("Takeback accepted"));
        return true;
    }

    bool canRequestTakeback() const
    {
        return gameClockRunning_ && !gameOver_ && !restoreBusy() &&
               takebackSnapshot_.valid && takebackSnapshot_.localMove &&
               takebackSnapshot_.ply == nextPly_ - 1;
    }

    // The rules core owns the Lattice; the widget grid is only a projection of
    // it, so applying a move is play-then-resync.
    bool applyMoveToBoard(const QString &move)
    {
        clearDestinationFeedback();
        if (pieceRevealTimer_.isActive()) {
            ++pieceRevealGeneration_;
            pieceRevealTimer_.stop();
            refreshBoard();
        }
        finishMirrorlockFlip();
        const QByteArray moveBytes = move.toLatin1();
        const int rc = ms_rules_play(moveBytes.constData());

        if (rc != MS_OK) {
            appendLog(QString("ERROR: alignment rejected by rules: %1 (%2)")
                          .arg(move, QString::fromLatin1(ms_rules_error_string(rc))));
            setStatusText(NETCHESSZX_UI_ERROR_RULES_REJECTED_MOVE);
            return false;
        }

        syncBoardFromRules();
        clearSelection();
        selectedLabel_->setText("Alignment: none");
        return true;
    }

    void finishAppliedMove(int ply, uint8_t side,
                           const QString &move, const QString &notation,
                           const QString &normalStatus)
    {
        lastMove_ = move;
        appendMoveRecord(ply, side, move, notation);
        nextPly_ = ply + 1;

        if (ms_rules_is_over()) {
            const uint8_t winner = ms_rules_winner();
            QString message;

            if (winner == MS_WINNER_PARADOX) {
                message = QStringLiteral("PARADOX - neither reality prevails");
            } else {
                const bool selfWon = (winner == MS_WINNER_A) == pcPlaysWhite_;
                message = selfWon
                              ? QStringLiteral("CONVERGENCE - your reality is the True World")
                              : QStringLiteral("CONVERGENCE - your reality fades into a reflection");
            }
            message += QString(" (%1)").arg(scoreText());
            closeGameInteractions();
            gameOver_ = true;
            pcTurn_ = false;
            stopGameClock();
            refreshLegalMoves();
            refreshBoard(true);
            appendLog(message);
            setStatusText(message);
            setConnectedUi(true);
            animateMirrorlockFlips();
            const int gen = abyssGeneration_;
            QTimer::singleShot(kGameOverAbyssDelayMs, this, [this, gen]() {
                playParadoxAbyss(gen);
            });
            return;
        }

        gameOver_ = false;
        pcTurn_ = ms_rules_side() == pcRulesSide();
        refreshLegalMoves();
        // Commit the lattice, then coin-turn the new disc from empty and the
        // captured discs from their previous face.
        refreshBoard(true);
        restartMoveClock();
        setStatusText(normalStatus);
        setConnectedUi(true);
        animateMirrorlockFlips();
    }

    void applyDirectLocalMove(uint16_t plyValue, const QString &move)
    {
        const int ply = static_cast<int>(plyValue);
        uint8_t side;

        (void)saveTakebackSnapshot(ply, true);
        if (!InputHelpers::isMoveSyntaxOk(move)) {
            pcTurn_ = false;
            setStatusText(NETCHESSZX_UI_ERROR_BOARD_REJECTED_MOVE);
            setConnectedUi(true);
            return;
        }
        side = ms_rules_side();
        if (!applyMoveToBoard(move)) {
            pcTurn_ = false;
            setStatusText(NETCHESSZX_UI_ERROR_BOARD_REJECTED_MOVE);
            setConnectedUi(true);
            return;
        }
        const QString notation = moveNotation(move);
        const bool echoSilenced = ms_rules_last_silences() == 1u;
        finishAppliedMove(ply, side, move, notation,
                          echoSilenced
                              ? QString("%1 - ECHO SILENCE, align again").arg(notation)
                              : QString("%1 confirmed - ECHO TO ALIGN").arg(notation));
    }

    bool applyDirectRemoteMoveAnimated(uint8_t deliveryId,
                                       uint16_t plyValue,
                                       const QString &move,
                                       QByteArray *failure)
    {
        const int ply = static_cast<int>(plyValue);
        uint8_t side;
        const auto reject = [failure](const char *reason) {
            if (failure != nullptr) {
                *failure = QByteArray(reason);
            }
            return false;
        };
        if (!gameClockRunning_ || !boardPiecesVisible_) {
            return reject("START");
        }
        if (!InputHelpers::isMoveSyntaxOk(move)) {
            return reject("SYNTAX");
        }
        if (pcTurn_) {
            return reject("TURN");
        }
        if (ply != nextPly_) {
            return reject("SYNC");
        }

        const QByteArray moveBytes = move.toLatin1();
        if (ms_rules_can_play(moveBytes.constData()) != MS_OK) {
            return reject("ILLEGAL");
        }

        (void)saveTakebackSnapshot(ply, false);
        side = ms_rules_side();
        if (!applyMoveToBoard(move)) {
            submitSessionGameResult(deliveryId, plyValue, SESSION_GAME_REJECTED,
                                    QByteArray("ILLEGAL"));
            return reject("ILLEGAL");
        }

        const QString notation = moveNotation(move);
        const bool selfSilenced = ms_rules_last_silences() == 1u;
        finishAppliedMove(ply, side, move, notation,
                          selfSilenced
                              ? QString("ECHO aligns %1 - SELF SILENCE").arg(notation)
                              : QString("ECHO aligns %1 - SELF TO ALIGN").arg(notation));
        submitSessionGameResult(deliveryId, plyValue, SESSION_GAME_ACCEPTED,
                                notation.toLatin1());
        return true;
    }

    void startGameFromAck()
    {
        cancelParadoxAbyss();
        ++pieceRevealGeneration_;
        pieceRevealTimer_.stop(); // kill leftover ring-reveal timers
        stopMirrorlockFlip();
        clearDestinationFeedback();
        directLocalResignPending_ = false;
        directResignRestartPending_ = false;
        clearTakebackState();
        nextPly_ = 1;
        gameOver_ = false;
        pcTurn_ = !pcPlaysWhite_;
        resetBoard();
        lastMove_.clear();
        clearMoveHistory();
        boardPiecesVisible_ = false;
        moveEdit_->clear();
        selectedLabel_->setText("Alignment: none");
        refreshBoard();
        startGameClock();
        animateBoardPiecesIn();
        refreshMoveLogHeaders();
        setStatusText(pcPlaysWhite_
                          ? QStringLiteral("The Convergence begins - ECHO TO ALIGN")
                          : QStringLiteral("The Convergence begins - SELF TO ALIGN"));
        setConnectedUi(true);
    }

    static QString generateMqttRoomCode()
    {
        return QString("MS%1")
            .arg(QRandomGenerator::global()->generate() & 0xffffu,
                 4,
                 16,
                 QLatin1Char('0'))
            .toUpper();
    }

    static QString formatElapsed(qint64 msecs)
    {
        const qint64 totalSeconds = msecs / 1000;
        const qint64 seconds = totalSeconds % 60;
        const qint64 minutes = (totalSeconds / 60) % 60;
        const qint64 hours = totalSeconds / 3600;

        if (hours > 0) {
            return QString("%1:%2:%3")
                .arg(hours)
                .arg(minutes, 2, 10, QLatin1Char('0'))
                .arg(seconds, 2, 10, QLatin1Char('0'));
        }
        return QString("%1:%2")
            .arg(minutes, 2, 10, QLatin1Char('0'))
            .arg(seconds, 2, 10, QLatin1Char('0'));
    }

    void startGameClock(qint64 gameOffsetMs = 0, qint64 moveOffsetMs = 0)
    {
        gameClockRunning_ = true;
        gameTimerOffsetMs_ = gameOffsetMs;
        moveTimerOffsetMs_ = moveOffsetMs;
        gameTimer_.restart();
        moveTimer_.restart();
        updateClockLabels();
    }

    void stopGameClock()
    {
        gameClockRunning_ = false;
        gameTimerOffsetMs_ = 0;
        moveTimerOffsetMs_ = 0;
        updateClockLabels();
    }

    void restartMoveClock()
    {
        if (gameClockRunning_) {
            moveTimerOffsetMs_ = 0;
            moveTimer_.restart();
            updateClockLabels();
        }
    }

    void updateClockLabels()
    {
        if (!gameClockLabel_ || !moveClockLabel_) {
            return;
        }
        if (!gameClockRunning_) {
            setLabelText(gameClockLabel_, "GAME --:--");
            setLabelText(moveClockLabel_, "MOVE --:--");
            return;
        }
        setLabelText(gameClockLabel_,
                     "GAME " + formatElapsed(gameTimerOffsetMs_ + gameTimer_.elapsed()));
        setLabelText(moveClockLabel_,
                     "MOVE " + formatElapsed(moveTimerOffsetMs_ + moveTimer_.elapsed()));
    }

    void checkUiStall()
    {
        if (!uiTickTimer_.isValid()) {
            uiTickTimer_.start();
            return;
        }

        const qint64 elapsed = uiTickTimer_.elapsed();
        uiTickTimer_.restart();
        if (elapsed > kUiStallWarnMs) {
            appendLog(QString("WARN: UI stalled %1 ms").arg(elapsed));
        }
    }

    void checkConnectionHealth()
    {
        if (!isMqttMode() || socket_ == nullptr ||
            socket_->state() != QAbstractSocket::ConnectedState ||
            !linkWatch_.isValid()) {
            return;
        }

        // Must stay well under the 20s MQTT keepalive or the broker drops us
        // and fires our own Last Will.
        if (linkWatch_.elapsed() <= 10000) {
            return;
        }

        (void)writeMqttPacket(DesktopTransportCodec::encodeMqttPing(), "PINGREQ");
        linkWatch_.restart();
    }

    static QString turnCardStyle(const char *background, const char *foreground)
    {
        return QStringLiteral(
                   "QWidget#turnCard { background:%1;"
                   " border:1px solid " MSH_BORDER_SOFT ";"
                   " border-radius:" MSH_R "; }"
                   "QWidget#turnCard QLabel { background:transparent; color:%2;"
                   " font-weight:700; font-size:13px; letter-spacing:1px; }")
            .arg(QLatin1String(background), QLatin1String(foreground));
    }

    void refreshTurnLabel()
    {
        if (!turnLabel_) {
            return;
        }

        const bool connected = socket_ != nullptr &&
                               socket_->state() == QAbstractSocket::ConnectedState;
        QString text;
        QString style;
        if (!connected && statusMessage_ == kDirectHostBusyStatus) {
            text = "HOST BUSY";
            style = turnCardStyle(MSH_ALERT, MSH_ALERT_TEXT);
        } else if (!connected && isConnectionErrorStatus()) {
            text = "CONNECT FAILED";
            style = turnCardStyle(MSH_ALERT, MSH_ALERT_TEXT);
        } else if (!connected) {
            text = isConnecting() ? "CONNECTING" : "OFFLINE";
            style = isConnecting() ? turnCardStyle(MSH_QUIET, MSH_ACCENT)
                                   : turnCardStyle(MSH_DISABLED, MSH_TEXT_DIM);
        } else if (gameOver_) {
            text = statusMessage_;
            style = turnCardStyle(MSH_ALERT, MSH_ALERT_TEXT);
        } else if (directUiBusy_) {
            text = "WAITING OPPONENT ACK";
            style = turnCardStyle(MSH_WAIT, MSH_WAIT_TEXT);
        } else if (!gameClockRunning_) {
            if (pcIsHost_ && !directSessionReady_) {
                text = "WAITING OPPONENT";
            } else if (!pcIsHost_ && !directSessionReady_) {
                text = "WAITING HOST";
            } else {
                text = pcIsHost_ ? "PRESS START GAME" : "WAITING OPPONENT START";
            }
            style = turnCardStyle(MSH_QUIET, MSH_ACCENT);
        } else if (pcTurn_) {
            text = QStringLiteral("SELF TO ALIGN");
            style = turnCardStyle(MSH_ACCENT_FILL, "#ffffff");
        } else {
            text = QStringLiteral("ECHO TO ALIGN");
            style = turnCardStyle(MSH_QUIET, MSH_ACCENT);
        }
        setLabelText(turnLabel_, text);
        setWidgetStyle(turnCard_ ? turnCard_ : turnLabel_, style);

        if (realityMeter_ != nullptr && turnStack_ != nullptr) {
            const bool showMeter = connected && (gameClockRunning_ || gameOver_);
            if (showMeter) {
                uint8_t a = 0;
                uint8_t b = 0;
                ms_rules_score(&a, &b);
                const int self = pcPlaysWhite_ ? a : b;
                const int echo = pcPlaysWhite_ ? b : a;
                realityMeter_->setCounts(self, echo);
                realityMeter_->setStatusText(text);
                turnStack_->setCurrentWidget(realityMeter_);
            } else {
                turnStack_->setCurrentWidget(turnLabel_);
            }
        }

        refreshChatButton();
    }

    static QString actionButtonStyle(const char *background, const char *hover,
                                     const char *foreground, const char *edge)
    {
        return QStringLiteral(
                   "QPushButton { background:%1; color:%3;"
                   " border:1px solid %4; border-radius:" MSH_R ";"
                   " font-weight:700; font-size:9pt; padding:4px 12px;"
                   " min-height:20px; }"
                   "QPushButton:hover { background:%2; }")
            .arg(QLatin1String(background), QLatin1String(hover),
                 QLatin1String(foreground), QLatin1String(edge));
    }

    static QString idleButtonStyle()
    {
        return actionButtonStyle(MSH_DISABLED, MSH_DISABLED, MSH_TEXT_MUTED,
                                 MSH_DISABLED);
    }

    static QString moveButtonStyle(bool ready, bool destinationReady)
    {
        if (destinationReady) {
            return actionButtonStyle(MSH_GO, MSH_GO_HOVER, MSH_GO_TEXT, MSH_GO);
        }
        if (ready) {
            return actionButtonStyle(MSH_ACCENT_FILL, MSH_ACCENT_FILL_HOVER,
                                     "#ffffff", MSH_ACCENT_FILL);
        }
        return idleButtonStyle();
    }

    static QString chatButtonStyle(bool ready)
    {
        return ready ? actionButtonStyle(MSH_SEND, MSH_SEND_HOVER,
                                         MSH_SEND_TEXT, MSH_SEND)
                     : idleButtonStyle();
    }

    static QString startButtonStyle(bool ready)
    {
        return ready ? actionButtonStyle(MSH_ACCENT_FILL, MSH_ACCENT_FILL_HOVER,
                                         "#ffffff", MSH_ACCENT_FILL)
                     : idleButtonStyle();
    }

    void setConnectedUi(bool connected)
    {
        const bool connecting = isConnecting();
        const bool canConnect = connected || connecting || isDirectListening() ||
                                canStartConnection();
        connectButton_->setEnabled(canConnect);
        connectButton_->setText(connected ? "Disconnect" :
                                connecting ? "Cancel" : "Connect");
        startGameButton_->setText(
            directResignRestartPending_ ? "Restarting..." :
            gameOver_ ? "Restart Game" : "Start Game");
        const bool startBaseReady = connected && !gameClockRunning_ &&
                                     !restoreBusy() && directSessionReady_;
        const bool startReady = startBaseReady && (pcIsHost_ || gameOver_);
        startGameButton_->setEnabled(startReady);
        setWidgetStyle(startGameButton_, startButtonStyle(startReady));
        const bool resetReady = connected && gameClockRunning_ && !restoreBusy();
        resetButton_->setEnabled(resetReady);
        setWidgetStyle(resetButton_, startButtonStyle(resetReady));
        if (restoreButton_ != nullptr) {
            restoreButton_->setEnabled(false);
            setWidgetStyle(restoreButton_, startButtonStyle(false));
        }
        refreshSaveLoadButtons();
        refreshChatButton();
        hostEdit_->setEnabled(!connected && !connecting);
        portSpin_->setEnabled(!connected && !connecting);
        roomEdit_->setEnabled(!connected && !connecting);
        directRadio_->setEnabled(!connected && !connecting);
        mqttRadio_->setEnabled(!connected && !connecting);
        updateSessionControlsEnabled();
        if (isDirectListening() &&
            statusMessage_ != NETCHESSZX_UI_ERROR_CONNECTION_LOST &&
            statusMessage_ != NETCHESSZX_UI_ERROR_OPPONENT_DISCONNECTED) {
            setStatusText(NETCHESSZX_UI_NOTICE_LISTENING_OPPONENT);
        } else if (!connected && !connecting && statusMessage_.isEmpty()) {
            setStatusText(NETCHESSZX_UI_PHASE_DISCONNECTED);
        } else if (connected && !directUiBusy_ &&
                   statusMessage_ == NETCHESSZX_UI_PHASE_DISCONNECTED) {
            setStatusText(pcTurn_ ? "Connected - SELF TO ALIGN" :
                                    "Connected - ECHO TO ALIGN");
        }
        refreshStatusBar();
        refreshTurnLabel();
    }

    bool canStartConnection() const
    {
        const QString host = hostEdit_ ? hostEdit_->text().trimmed() : QString();

        if (isMqttMode()) {
            const QString room = roomEdit_ ? roomEdit_->text().trimmed().toUpper() :
                                             QString();
            return !host.isEmpty() && InputHelpers::isMqttRoomSyntaxOk(room);
        }
        return pcIsHost_ || InputHelpers::isDirectIpSyntaxOk(host);
    }

    bool canPcMove() const
    {
        return socket_ != nullptr &&
               socket_->state() == QAbstractSocket::ConnectedState &&
               (!isMqttMode() || (mqttSubscribed_ && mqttSideReady_)) &&
               gameClockRunning_ &&
               pcTurn_ && !directUiBusy_ &&
               !restoreBusy();
    }

    bool hasSelectedMoveTarget() const
    {
        return targetRow_ >= 0 && targetCol_ >= 0 &&
               isLegalTarget(targetRow_, targetCol_);
    }

    bool canSendChat() const
    {
        if (chatEdit_ == nullptr) {
            return false;
        }
        const QString text = chatEdit_->text().trimmed();
        if (text.isEmpty()) {
            return false;
        }
        const QString cmd = text.toLower();
        if (InputHelpers::isMoveSyntaxOk(cmd)) {
            return canPcMove();
        }
        if (cmd == "/save" || cmd.startsWith(QStringLiteral("/save "))) {
            return canSaveGameFile();
        }
        if (cmd == "/load" || cmd.startsWith(QStringLiteral("/load "))) {
            return canLoadGameFile();
        }
        const bool chatDuringControl =
            chatCanSharePendingControl(cmd, directUiBusy_, resetPromptOpen_,
                                       directDecisionBox_ != nullptr);
        if (!isConnected() ||
            (restoreBusy() &&
             (cmd != "/resign" || !resignCanPreemptBusy()) &&
             !chatDuringControl)) {
            return false;
        }

        return directSessionReady_ &&
               (!isMqttMode() || (mqttSubscribed_ && mqttSideReady_)) &&
               statusMessage_ != NETCHESSZX_UI_ERROR_CONNECTION_LOST;
    }

    void refreshChatButton()
    {
        refreshSessionCommandButtons();
        if (chatButton_ == nullptr) {
            return;
        }
        const QString text = chatEdit_->text().trimmed().toLower();
        const bool moveText = InputHelpers::isMoveSyntaxOk(text);
        const bool ready = canSendChat();
        chatButton_->setEnabled(ready);
        setWidgetStyle(chatButton_, moveText
                                    ? moveButtonStyle(ready, ready && hasSelectedMoveTarget())
                                    : chatButtonStyle(ready));
    }

    void refreshSessionCommandButtons()
    {
        const bool inGame = isConnected() && gameClockRunning_ && !gameOver_;
        const bool canControl = inGame && !restoreBusy() &&
                                directUiBusy_ == 0u && !resetPromptOpen_;
        if (takebackButton_ != nullptr) {
            takebackButton_->setEnabled(canControl && canRequestTakeback());
        }
        if (resetIconButton_ != nullptr) {
            resetIconButton_->setEnabled(resetButton_ != nullptr && resetButton_->isEnabled());
        }
    }

    void layoutChatCountLabel()
    {
        if (chatEdit_ == nullptr || chatCountLabel_ == nullptr) {
            return;
        }
        chatCountLabel_->adjustSize();
        const int clearGutter = 22;
        const int pad = 8;
        const QSize hint = chatCountLabel_->sizeHint();
        const int x = chatEdit_->width() - hint.width() - clearGutter;
        const int y = (chatEdit_->height() - hint.height()) / 2;
        chatCountLabel_->move(qMax(pad, x), qMax(0, y));
        const QFontMetrics metrics(chatEdit_->font());
        const int textWidth = metrics.horizontalAdvance(chatEdit_->text());
        const int textRight = chatEdit_->textMargins().left() + pad + textWidth;
        chatCountLabel_->setVisible(textRight + 6 < chatCountLabel_->x());
    }

    void resizeToContent()
    {
        if (QWidget *root = centralWidget()) {
            root->layout()->activate();
            const QSize contentSize = root->sizeHint();
            const QSize chromeSize(0, statusBar()->sizeHint().height());
            const QSize windowSize = contentSize + chromeSize;
            setFixedSize(windowSize);
            QTimer::singleShot(0, this, [this]() {
                alignStatusBarToControls();
            });
        }
    }

    void alignStatusBarToControls()
    {
        if (statusBarContents_ == nullptr || statusBarLayout_ == nullptr ||
            flipBoardButton_ == nullptr || logToggleButton_ == nullptr) {
            return;
        }

        const int leftMargin = statusBarContents_->mapFromGlobal(
            flipBoardButton_->mapToGlobal(QPoint(0, 0))).x();
        const int logRight = statusBarContents_->mapFromGlobal(
            logToggleButton_->mapToGlobal(
                QPoint(logToggleButton_->width(), 0))).x();
        const int rightMargin = statusBarContents_->width() - logRight;
        if (leftMargin < 0 || rightMargin < 0) {
            return;
        }

        const QMargins alignedMargins(leftMargin, 0, rightMargin, 0);
        if (statusBarLayout_->contentsMargins() != alignedMargins) {
            statusBarLayout_->setContentsMargins(alignedMargins);
        }
    }

    void updateConnectionModeUi()
    {
        const bool mqtt = isMqttMode();
        const bool directHost = !mqtt && pcIsHost_;

        if (hostCaptionLabel_ != nullptr) {
            hostCaptionLabel_->setText(mqtt ? "HOST" : (directHost ? "LOCAL IP" : "HOST IP"));
        }
        if (roomCaptionLabel_ != nullptr) {
            roomCaptionLabel_->setVisible(mqtt);
        }
        if (roomEdit_ != nullptr) {
            roomEdit_->setVisible(mqtt);
        }
        if (hostEdit_ != nullptr) {
            if (mqtt) {
                hostEdit_->setReadOnly(false);
                hostEdit_->setPlaceholderText("MQTT broker");
                if (directShowingLocalHost_) {
                    hostEdit_->setText(mqttBrokerCache_.isEmpty() ?
                                           QStringLiteral("broker.hivemq.com") :
                                           mqttBrokerCache_);
                } else if (!hostEdit_->text().trimmed().isEmpty() &&
                           looksLikeMqttHost(hostEdit_->text().trimmed())) {
                    mqttBrokerCache_ = hostEdit_->text().trimmed();
                }
                directShowingLocalHost_ = false;
            } else if (directHost) {
                const QString current = hostEdit_->text().trimmed();
                if (!directShowingLocalHost_ && !current.isEmpty() &&
                    !looksLikeMqttHost(current)) {
                    directIpCache_ = current;
                }
                hostEdit_->setPlaceholderText("Local IP");
                hostEdit_->setText(localDirectIpAddress());
                hostEdit_->setReadOnly(true);
                directShowingLocalHost_ = true;
            } else {
                    hostEdit_->setPlaceholderText("Opponent IP");
                hostEdit_->setReadOnly(false);
                if (directShowingLocalHost_) {
                    hostEdit_->setText(directIpCache_);
                }
                directShowingLocalHost_ = false;
            }
        }
        if (directIpHistoryAction_ != nullptr) {
            directIpHistoryAction_->setVisible(true);
            directIpHistoryAction_->setEnabled(!mqtt && !pcIsHost_ && !directIpHistory_.isEmpty());
        }

    }

    static bool looksLikeMqttHost(const QString &host)
    {
        return host.contains("broker", Qt::CaseInsensitive) ||
               host.contains("mqtt", Qt::CaseInsensitive) ||
               host.contains("hivemq", Qt::CaseInsensitive) ||
               host.contains("mosquitto", Qt::CaseInsensitive);
    }

    void rememberDirectGuestIp(QSettings &settings, const QString &host)
    {
        const QStringList history = directIpHistoryWith(
            settings.value(kDirectIpHistorySettingsKey).toStringList(), host);
        settings.setValue(kDirectIpHistorySettingsKey, history);
        directIpHistory_ = history;
        directIpHistoryAction_->setEnabled(!history.isEmpty());
    }

    void rebuildDirectIpHistoryMenu()
    {
        directIpHistoryMenu_->clear();
        for (const QString &ip : directIpHistory_) {
            QAction *action = directIpHistoryMenu_->addAction(ip);
            connect(action, &QAction::triggered, this, [this, ip]() {
                hostEdit_->setText(ip);
                hostEdit_->setFocus();
            });
        }
    }

    static QString localDirectIpAddress()
    {
        const QList<QHostAddress> addresses = QNetworkInterface::allAddresses();
        for (const QHostAddress &address : addresses) {
            const QString text = address.toString();
            if (address.protocol() == QAbstractSocket::IPv4Protocol &&
                !address.isLoopback() &&
                !text.startsWith("169.254.")) {
                return text;
            }
        }
        return QHostAddress(QHostAddress::LocalHost).toString();
    }

    uint8_t pcRulesSide() const
    {
        return pcPlaysWhite_ ? MS_SIDE_A : MS_SIDE_B;
    }

    bool isPcPiece(char piece) const
    {
        return piece == (pcPlaysWhite_ ? MS_CELL_A : MS_CELL_B);
    }

    static void setLabelText(QLabel *label, const QString &text)
    {
        if (label != nullptr && label->text() != text) {
            label->setText(text);
        }
    }

    QString endpointText() const
    {
        QString host = hostEdit_ ? hostEdit_->text().trimmed() : QString();
        if (host.isEmpty()) {
            host = "-";
        }
        const int port = portSpin_ ? portSpin_->value() : 0;
        if (isMqttMode()) {
            const QString room = roomEdit_ ? roomEdit_->text().trimmed().toUpper() : QString("-");
            return QString("MQTT %1:%2 | room %3").arg(host).arg(port).arg(room);
        }
        if (pcIsHost_) {
            return QString("IP LISTEN:%1").arg(port);
        }
        return QString("IP %1:%2").arg(host).arg(port);
    }

    bool isConnected() const
    {
        if (socket_ == nullptr ||
            socket_->state() != QAbstractSocket::ConnectedState) {
            return false;
        }
        return isMqttMode() || directSessionReady_;
    }

    bool isDirectListening() const
    {
        return directServer_ != nullptr && directServer_->isListening() &&
               (socket_ == nullptr ||
                socket_->state() == QAbstractSocket::UnconnectedState);
    }

    bool isConnectionErrorStatus() const
    {
        return statusMessage_.startsWith("Connection refused") ||
               statusMessage_.startsWith(NETCHESSZX_UI_PHASE_CONNECTION_FAILED) ||
               statusMessage_ == NETCHESSZX_UI_ERROR_INVALID_IP ||
               statusMessage_ == kDirectHostBusyStatus ||
               statusMessage_ == NETCHESSZX_UI_ERROR_OPPONENT_APP_NOT_READY;
    }

    enum class StatusSeverity {
        Info,
        Waiting,
        Success,
        Error
    };

    static StatusSeverity statusBarTextSeverity(const QString &text)
    {
        if (text == NETCHESSZX_UI_PHASE_DISCONNECTED ||
            text == NETCHESSZX_UI_PHASE_CONNECTION_FAILED ||
            text == NETCHESSZX_UI_ERROR_CONNECTION_LOST ||
            text == NETCHESSZX_UI_ERROR_OPPONENT_DISCONNECTED ||
            text.startsWith("RESTART rejected") ||
            text.startsWith("Connection refused") ||
            text.startsWith(NETCHESSZX_UI_PHASE_CONNECTION_FAILED) ||
            text.contains("disconnected", Qt::CaseInsensitive) ||
            text.contains("rejected", Qt::CaseInsensitive) ||
            text.contains("failed", Qt::CaseInsensitive) ||
            text.contains("invalid", Qt::CaseInsensitive) ||
            text.contains("illegal", Qt::CaseInsensitive) ||
            text.contains("busy", Qt::CaseInsensitive) ||
            text.contains("not ready", Qt::CaseInsensitive)) {
            return StatusSeverity::Error;
        }
        if (text.contains("confirmed", Qt::CaseInsensitive)) {
            return StatusSeverity::Success;
        }
        if (text.contains("waiting", Qt::CaseInsensitive) ||
            text.contains("pending", Qt::CaseInsensitive) ||
            text.contains("requested", Qt::CaseInsensitive) ||
            text.contains("confirm", Qt::CaseInsensitive) ||
            text.contains("starting", Qt::CaseInsensitive)) {
            return StatusSeverity::Waiting;
        }
        if (text.contains("accepted", Qt::CaseInsensitive) ||
            text.contains("loaded", Qt::CaseInsensitive) ||
            text.contains("saved", Qt::CaseInsensitive) ||
            text.contains("started", Qt::CaseInsensitive) ||
            text.contains("ready", Qt::CaseInsensitive)) {
            return StatusSeverity::Success;
        }
        return StatusSeverity::Info;
    }

    static QString statusBarLabelStyle(StatusSeverity severity)
    {
#define MSH_STATUS_SHAPE " font-weight:700; font-size:11px;" \
                         " letter-spacing:0.5px; }"
        switch (severity) {
        case StatusSeverity::Error:
            return "QLabel { color:" MSH_ALERT_BRIGHT ";" MSH_STATUS_SHAPE;
        case StatusSeverity::Waiting:
            return "QLabel { color:" MSH_WAIT_BRIGHT ";" MSH_STATUS_SHAPE;
        case StatusSeverity::Success:
            return "QLabel { color:" MSH_SUCCESS ";" MSH_STATUS_SHAPE;
        case StatusSeverity::Info:
        default:
            return "QLabel { color:" MSH_ACCENT ";" MSH_STATUS_SHAPE;
        }
#undef MSH_STATUS_SHAPE
    }

    bool isConnecting() const
    {
        if (directConnectRetryActive_) {
            return true;
        }
        if (isDirectListening()) {
            return true;
        }
        if (socket_ == nullptr) {
            return false;
        }
        if (!isMqttMode() &&
            socket_->state() == QAbstractSocket::ConnectedState &&
            !directSessionReady_) {
            return true;
        }
        return socket_->state() == QAbstractSocket::HostLookupState ||
               socket_->state() == QAbstractSocket::ConnectingState;
    }

    QString statusStateText() const
    {
        if (isDirectListening()) {
            return NETCHESSZX_UI_PHASE_LISTENING;
        }
        if (isConnecting()) {
            return NETCHESSZX_UI_PHASE_CONNECTING;
        }
        if (!isConnected() && isConnectionErrorStatus()) {
            return NETCHESSZX_UI_PHASE_CONNECTION_FAILED;
        }
        if (!isConnected()) {
            return NETCHESSZX_UI_PHASE_DISCONNECTED;
        }
        if (gameOver_) {
            return statusMessage_;
        }
        if (!gameClockRunning_) {
            if (!directSessionReady_) {
                return pcIsHost_ ? NETCHESSZX_UI_PHASE_WAITING_OPPONENT_SHORT
                                 : NETCHESSZX_UI_PHASE_WAITING_HOST;
            }
            return statusMessage_.isEmpty() ? NETCHESSZX_UI_PHASE_OPPONENT_LINKED :
                                             statusMessage_;
        }
        if (directUiBusy_) {
            return NETCHESSZX_UI_NOTICE_WAITING_ACK;
        }
        if (!statusMessage_.isEmpty() && !isTurnStatusMessage(statusMessage_)) {
            return statusMessage_;
        }
        return QStringLiteral("LIVE");
    }

    static bool isTurnStatusMessage(const QString &text)
    {
        return text == NETCHESSZX_UI_PHASE_YOUR_TURN ||
               text == NETCHESSZX_UI_PHASE_OPPONENT_TURN ||
               text.contains(QStringLiteral("TO ALIGN"), Qt::CaseInsensitive) ||
               text.startsWith(QStringLiteral("Alignment ready"), Qt::CaseInsensitive);
    }

    QString statusContextText() const
    {
        if (!isConnected()) {
            const bool connectionStarting = isConnecting() || isDirectListening();
            if (!connectionStarting) {
                if (statusMessage_.isEmpty() ||
                    statusMessage_ == NETCHESSZX_UI_PHASE_DISCONNECTED ||
                    statusMessage_ == statusStateText()) {
                    return QString();
                }
                return statusMessage_;
            }
            if (statusMessage_.isEmpty() || statusMessage_ == statusStateText()) {
                if (!isMqttMode() && !pcIsHost_ &&
                    !InputHelpers::isDirectIpSyntaxOk(hostEdit_ ? hostEdit_->text().trimmed() :
                                                    QString())) {
                    return endpointText() + " | Invalid IP";
                }
                return endpointText();
            }
            return endpointText() + " | " + statusMessage_;
        }

        if (!gameClockRunning_) {
            if (gameOver_) {
                QStringList parts;
                if (directResignRestartPending_) {
                    parts << QString(NETCHESSZX_UI_NOTICE_RESTARTING_GAME);
                } else {
                    parts << QString(NETCHESSZX_UI_CONTEXT_PRESS_RESTART);
                }
                parts << endpointText();
                const QString peer = peerMachineStatusText();
                if (!peer.isEmpty()) {
                    parts << peer;
                }
                return parts.join(" | ");
            }
            QString action = pcIsHost_ ? QString(NETCHESSZX_UI_CONTEXT_PRESS_START) :
                                         QString(NETCHESSZX_UI_PHASE_WAITING_OPPONENT_START);
            if (!directSessionReady_) {
                action = pcIsHost_ ? QString(NETCHESSZX_UI_PHASE_WAITING_OPPONENT_SHORT)
                                   : QString(NETCHESSZX_UI_PHASE_WAITING_HOST);
            }

            QStringList parts;
            parts << action << endpointText();
            if (!statusMessage_.isEmpty() && statusMessage_ != statusStateText()) {
                parts << statusMessage_;
            }
            const QString peer = peerMachineStatusText();
            if (!peer.isEmpty()) {
                parts << peer;
            }
            return parts.join(" | ");
        }

        QStringList parts;
        parts << QString("Ply %1").arg(nextPly_);
        if (!lastMove_.isEmpty()) {
            parts << QString("Last %1").arg(lastMove_.toUpper());
        }
        if (!statusMessage_.isEmpty() && statusMessage_ != statusStateText() &&
            statusMessage_ != NETCHESSZX_UI_PHASE_WAITING_OPPONENT &&
            !isTurnStatusMessage(statusMessage_)) {
            parts << statusMessage_;
        }
        const QString peer = peerMachineStatusText();
        if (!peer.isEmpty()) {
            parts << peer;
        }
        return parts.join(" | ");
    }

    QString peerMachineStatusText() const
    {
        if (!directSessionReady_) {
            return QString();
        }
        const char *code = netchess_proto_mach_code(peerPlatform_);
        return QString("VS %1").arg(code == nullptr ? "?" : code);
    }

    void refreshStatusBar()
    {
        const QString stateText = statusStateText();

        setWidgetStyle(statusStateLabel_,
                       statusBarLabelStyle(statusBarTextSeverity(stateText)));
        setLabelText(statusStateLabel_, stateText.toUpper());
        if (statusContextLabel_ != nullptr) {
            const QString contextText = statusContextText();
            setWidgetStyle(statusContextLabel_,
                           statusBarLabelStyle(statusBarTextSeverity(contextText)));
            setLabelText(statusContextLabel_, contextText.toUpper());
        }
    }

    static QString sideStatusText(const QString &text)
    {
        if (text.isEmpty() || text == NETCHESSZX_UI_PHASE_DISCONNECTED ||
            text.startsWith(NETCHESSZX_UI_PHASE_DISCONNECTED)) {
            return NETCHESSZX_UI_SIDE_CONNECT_READY;
        }
        if (text == NETCHESSZX_UI_PHASE_OPPONENT_LINKED) {
            return NETCHESSZX_UI_SIDE_LINK_OK;
        }

        QString compact = text.toUpper();
        compact.replace(" - ", " | ");
        constexpr int kMaxSideStatusChars = 72;
        if (compact.size() > kMaxSideStatusChars) {
            compact = compact.left(kMaxSideStatusChars - 3) + "...";
        }
        return compact;
    }

    void setStatusText(const QString &text)
    {
        statusMessage_ = text;
        if (statusLabel_) {
            setLabelText(statusLabel_, sideStatusText(text));
        }
        refreshStatusBar();
        refreshTurnLabel();
    }

    void setStatusBarText(const QString &text)
    {
        statusMessage_ = text;
        refreshStatusBar();
    }

    void appendLog(const QString &text)
    {
        static const QLocale locale = QLocale::system();
        const QString now = locale.toString(QTime::currentTime(), QLocale::LongFormat);
        const QString line = QString("[%1] %2").arg(now, text);
        logLines_.append(line);
        trimLines(logLines_);
        if (!showingMoveHistory_ && logEdit_ != nullptr) {
            logEdit_->append(line);
            if (QScrollBar *bar = logEdit_->verticalScrollBar()) {
                bar->setValue(bar->maximum());
            }
        }
    }

    static void trimLines(QStringList &lines)
    {
        constexpr int kMaxLines = 400;
        while (lines.size() > kMaxLines) {
            lines.removeFirst();
        }
    }

    static void trimMoveRecords(QVector<MoveRecord> &records)
    {
        constexpr int kMaxRecords = 400;
        while (records.size() > kMaxRecords) {
            records.removeFirst();
        }
    }

    void appendMoveRecord(int ply, uint8_t side,
                          const QString &move, const QString &notation)
    {
        moveHistoryRecords_.append(MoveRecord{ply, side, move, notation});
        const bool trimmed = moveHistoryRecords_.size() > 400;
        trimMoveRecords(moveHistoryRecords_);
        if (showingMoveHistory_ && moveTable_ != nullptr) {
            if (trimmed) {
                renderMoveHistoryTable();
            } else {
                const int count = moveHistoryRecords_.size();
                appendMoveTableRecord(moveHistoryRecords_.last(), count > 1 ?
                    &moveHistoryRecords_.at(count - 2) : nullptr);
            }
        }
    }

    void clearMoveHistory()
    {
        moveHistoryRecords_.clear();
        if (showingMoveHistory_) {
            renderLogView();
        }
    }

    void toggleLogView()
    {
        showingMoveHistory_ = !showingMoveHistory_;
        renderLogView();
    }

    void renderLogView()
    {
        if (logEdit_ == nullptr || logStack_ == nullptr) {
            return;
        }

        if (showingMoveHistory_) {
            renderMoveHistoryTable();
            logStack_->setCurrentWidget(moveTable_);
        } else {
            logEdit_->setLineWrapMode(QTextEdit::NoWrap);
            logEdit_->setPlainText(logLines_.join("\n"));
            logStack_->setCurrentWidget(logEdit_);
        }
        if (logTitleLabel_ != nullptr) {
            logTitleLabel_->setText(showingMoveHistory_ ? "MOVES" : "LOG");
        }
        if (logToggleButton_ != nullptr) {
            logToggleButton_->setText(showingMoveHistory_ ? "Log" : "Moves");
        }
        if (!showingMoveHistory_) {
            if (QScrollBar *bar = logEdit_->verticalScrollBar()) {
                bar->setValue(bar->maximum());
            }
        }
    }

    static QString moveCellText(const MoveRecord &record)
    {
        if (record.move.isEmpty()) {
            return QString();
        }

        const QString alignment = record.move.toUpper();
        if (record.notation.isEmpty()) {
            return alignment;
        }
        return QStringLiteral("%1 (%2)").arg(alignment, record.notation);
    }

    void appendMoveTableRecord(const MoveRecord &record, const MoveRecord *previous)
    {
        const int number = (record.ply + 1) / 2;
        const bool pair = record.side == MS_SIDE_A && previous != nullptr &&
                          previous->side == MS_SIDE_B &&
                          (previous->ply + 1) / 2 == number;
        int row = moveTable_->rowCount() - 1;
        if (!pair || row < 0) {
            row = moveTable_->rowCount();
            moveTable_->insertRow(row);
            for (int col = 0; col < 3; ++col) {
                auto *item = new QTableWidgetItem();
                item->setFlags(Qt::ItemIsEnabled);
                item->setTextAlignment(col == 0 ? Qt::AlignCenter : Qt::AlignLeft | Qt::AlignVCenter);
                moveTable_->setItem(row, col, item);
            }
            moveTable_->item(row, 0)->setText(number == 0 ? QString() : QStringLiteral("%1.").arg(number));
        }
        moveTable_->item(row, record.side == MS_SIDE_B ? 1 : 2)->setText(moveCellText(record));
        moveTable_->scrollToBottom();
    }

    void renderMoveHistoryTable()
    {
        if (moveTable_ == nullptr) {
            return;
        }
        moveTable_->setRowCount(0);
        const MoveRecord *previous = nullptr;
        for (const MoveRecord &record : moveHistoryRecords_) {
            appendMoveTableRecord(record, previous);
            previous = &record;
        }
    }

    void appendChat(const QString &sender, const QString &text,
                    bool highlighted = false)
    {
        const QString now = QLocale::system().toString(QTime::currentTime(), QLocale::ShortFormat);
        QTextCharFormat format = chatLogEdit_->currentCharFormat();
        format.setFontWeight(sender == opponentChatName() ? QFont::Bold
                                                          : QFont::Normal);
        format.setForeground(highlighted ? QColor(0xff, 0x5a, 0x5a)
                                         : QColor(0xe8, 0xee, 0xf6));
        chatLogEdit_->setCurrentCharFormat(format);
        chatLogEdit_->appendPlainText(QString("%1 %2: %3")
                                          .arg(now, sender, text));
    }

    void appendControlEvent(bool local, const QString &event)
    {
        appendChat(local ? pcChatName() : opponentChatName(), event,
                   event == QStringLiteral("RESIGN"));
    }

    void clearChatLog()
    {
        if (chatLogEdit_ != nullptr) {
            chatLogEdit_->clear();
        }
    }

    QTcpSocket *socket_ = nullptr;
    QTcpServer *directServer_ = nullptr;
    DesktopSessionController sessionController_;
    DesktopTransportCodec transportCodec_;
    QHash<uint8_t, QPointer<QTcpSocket>> directSockets_;
    QHash<uint8_t, bool> directLinkUpSeen_;
    QPointer<QMessageBox> directDecisionBox_;
    QString directEndStatus_;
    QString mqttBrokerCache_;
    QString directIpCache_;
    int directPortCache_ = 5000;
    int mqttPortCache_ = 1883;
    bool directShowingLocalHost_ = false;
    QRadioButton *directRadio_ = nullptr;
    QRadioButton *mqttRadio_ = nullptr;
    QRadioButton *roleHostRadio_ = nullptr;
    QRadioButton *roleGuestRadio_ = nullptr;
    QLabel *hostColorLabel_ = nullptr;
    QRadioButton *hostEchoFirstRadio_ = nullptr;
    QRadioButton *hostSelfFirstRadio_ = nullptr;
    QLabel *hostCaptionLabel_ = nullptr;
    QLabel *roomCaptionLabel_ = nullptr;
    QLineEdit *hostEdit_ = nullptr;
    QAction *directIpHistoryAction_ = nullptr;
    QMenu *directIpHistoryMenu_ = nullptr;
    QStringList directIpHistory_;
    QLineEdit *roomEdit_ = nullptr;
    QCheckBox *showHintsCheck_ = nullptr;
    QSpinBox *portSpin_ = nullptr;
    QPushButton *connectButton_ = nullptr;
    QPushButton *startGameButton_ = nullptr;
    QPushButton *resetButton_ = nullptr;
    QPushButton *restoreButton_ = nullptr;
    QLineEdit *moveEdit_ = nullptr;
    QPushButton *flipBoardButton_ = nullptr;
    QLabel *chatCountLabel_ = nullptr;
    QPushButton *takebackButton_ = nullptr;
    QPushButton *resetIconButton_ = nullptr;
    QLineEdit *chatEdit_ = nullptr;
    QPushButton *chatButton_ = nullptr;
    QPlainTextEdit *chatLogEdit_ = nullptr;
    QWidget *turnCard_ = nullptr;
    QStackedWidget *turnStack_ = nullptr;
    RealityMeter *realityMeter_ = nullptr;
    QLabel *turnLabel_ = nullptr;
    QLabel *statusLabel_ = nullptr;
    QLabel *selectedLabel_ = nullptr;
    QLabel *statusStateLabel_ = nullptr;
    QLabel *statusContextLabel_ = nullptr;
    QLabel *gameClockLabel_ = nullptr;
    QLabel *moveClockLabel_ = nullptr;
    QWidget *statusBarContents_ = nullptr;
    QHBoxLayout *statusBarLayout_ = nullptr;
    QLabel *logTitleLabel_ = nullptr;
    QStackedWidget *logStack_ = nullptr;
    QTableWidget *moveTable_ = nullptr;
    QPushButton *logToggleButton_ = nullptr;
    QPushButton *saveGameButton_ = nullptr;
    QPushButton *loadGameButton_ = nullptr;
    QTextEdit *logEdit_ = nullptr;
    QTimer *clockTimer_ = nullptr;
    QTimer *directConnectRetryTimer_ = nullptr;
    QLabel *fileLabelsTop_[8] = {};
    QLabel *fileLabelsBottom_[8] = {};
    QLabel *rankLabelsLeft_[8] = {};
    QLabel *rankLabelsRight_[8] = {};
    QPushButton *squares_[8][8] = {};
    QString squareStyleCache_[8][8];
    quint16 squareVisualCache_[8][8] = {};
    char board_[8][8] = {};
    QString mqttRoom_;
    QString statusMessage_;
    QString lastSocketError_;
    QString lastMove_;
    QStringList chatInputHistory_;
    QStringList logLines_;
    QVector<MoveRecord> moveHistoryRecords_;
    TakebackSnapshot takebackSnapshot_;
    QStringList legalTargets_;
    QElapsedTimer gameTimer_;
    QElapsedTimer moveTimer_;
    QElapsedTimer linkWatch_;
    QElapsedTimer uiTickTimer_;
    qint64 gameTimerOffsetMs_ = 0;
    qint64 moveTimerOffsetMs_ = 0;
    int nextPly_ = 1;
    int selectedRow_ = -1;
    int selectedCol_ = -1;
    int chatInputHistoryIndex_ = 0;
    int targetRow_ = -1;
    int targetCol_ = -1;
    int feedbackRow_ = -1;
    int feedbackCol_ = -1;
    QTimer pieceRevealTimer_;
    QTimer pieceFlipTimer_;
    QTimer feedbackTimer_;
    int pieceRevealRing_ = 0;
    int pieceFlipStep_ = 0;
    int feedbackStep_ = 0;
    int abyssGeneration_ = 0;
    int chromeFadeGeneration_ = 0;
    int pieceRevealGeneration_ = 0;
    QVector<FlipAnim> pieceFlipCells_;
    int gameGeneration_ = 0;
    int directConnectRetryCount_ = 0;
    uint8_t directNextLinkId_ = 0u;
    uint8_t directPrimaryLinkId_ = SESSION_LINK_NONE;
    uint8_t directDecisionRequestId_ = 0u;
    uint8_t directDecisionControl_ = 0u;
    uint8_t peerPlatform_ = NETCHESS_PLAT_UNKNOWN;
    quint16 mqttSessionId_ = 0;
    quint64 mqttClientNonce_ = QRandomGenerator::global()->generate64();
    bool feedbackOn_ = false;
    bool mqttSubscribed_ = false;
    bool mqttSideReady_ = false;

    bool mqttSessionLinked_ = false;
    bool directSessionReady_ = false;
    bool localMachSent_ = false;
    uint8_t directUiBusy_ = 0u;
    bool directStartTransitionApplied_ = false;
    bool directLocalResignPending_ = false;
    bool directResignRestartPending_ = false;
    bool resetPromptOpen_ = false;
    bool pcTurn_ = false;
    bool pcIsHost_ = false;
    bool ignoreNextDisconnect_ = false;
    bool localDisconnectPending_ = false;
    bool directConnectRetryActive_ = false;
    bool hostPlaysWhite_ = false;
    bool pcPlaysWhite_ = false;
    bool coordinatesInitialized_ = false;
    bool lastCoordinateStandard_ = false;
    void applyBoardTexture() {
        const QString tex = boardCombo_ ? boardCombo_->currentData().toString() : QString();
        PieceRenderer::setBoardTexture(tex);
        QSettings s;
        s.setValue("appearance/board", tex);
        if (boardFrame_) {
            boardFrame_->setStyleSheet(boardFrameStyle());
        }
        refreshBoard();
    }

    QWidget *boardFrame_ = nullptr;
    BoardCollapseOverlay *collapseOverlay_ = nullptr;
    QComboBox *boardCombo_ = nullptr;
    bool boardStandardOrientation_ = true;
    bool boardOrientationManual_ = false;
    bool boardPiecesVisible_ = false;
    bool showingMoveHistory_ = true;
    bool gameOver_ = false;
    bool gameClockRunning_ = false;
    QHash<uint16_t, QString> mqttSubackPending_;
    QHash<uint16_t, QString> mqttUnsubackPending_;
    QSet<QString> mqttActiveSubscriptions_;
    QSet<QString> mqttTargetSubscriptions_;
    QSet<QString> mqttObsoleteSubscriptions_;
    QVector<MqttBufferedPublish> mqttBufferedPublishes_;
    bool mqttSideTransitionPending_ = false;
    uint16_t mqttNextPacketId_ = 1;
#ifdef NETCHESSZX_PC_MQTT_TX_FAILURE_TEST
    unsigned testBoardRefreshes_ = 0;
    unsigned testSquareRefreshes_ = 0;
    bool testMqttWriteFailure_ = false;
#endif
};

MainWindow::MainWindow()
    : impl_(std::make_unique<MainWindowImpl>())
{
}

MainWindow::~MainWindow() = default;

QIcon MainWindow::appIcon()
{
    return makeMirrorShiftIcon();
}

void MainWindow::setWindowIcon(const QIcon &icon)
{
    impl_->setWindowIcon(icon);
}

void MainWindow::showNormal()
{
    impl_->showNormal();
#ifdef Q_OS_MACOS
    applyMacWindowChrome(impl_.get(), QColor(QStringLiteral(MSH_INK)));
#endif
#ifdef Q_OS_WIN
    applyWinWindowChrome(impl_.get(), QColor(QStringLiteral(MSH_INK)));
#endif
}

#ifdef NETCHESSZX_PC_MQTT_TX_FAILURE_TEST
QTcpSocket *MainWindow::testSocket() const { return impl_->testSocket(); }
bool MainWindow::testPrepareMqttGuestSession()
{
    return impl_->testPrepareMqttGuestSession();
}
bool MainWindow::testPrepareMqttHostSession()
{
    return impl_->testPrepareMqttHostSession();
}
bool MainWindow::testBlackHostOpeningContract()
{
    return impl_->testBlackHostOpeningContract();
}
bool MainWindow::testEndAndRelinkMqttSession()
{
    return impl_->testEndAndRelinkMqttSession();
}
bool MainWindow::testPrepareMqttGuestBootstrap()
{
    return impl_->testPrepareMqttGuestBootstrap();
}
bool MainWindow::testBeginMqttRestore()
{
    return impl_->testBeginMqttRestore();
}
bool MainWindow::testRestoreUiIdle() const
{
    return impl_->testRestoreUiIdle();
}
bool MainWindow::testTakebackModalRechecksState()
{
    return impl_->testTakebackModalRechecksState();
}
void MainWindow::testFeedMqtt(const QByteArray &suffix,
                              const QByteArray &payload, bool retained)
{
    impl_->testFeedMqtt(suffix, payload, retained);
}
void MainWindow::testSetMqttWriteFailure(bool enabled)
{
    impl_->testSetMqttWriteFailure(enabled);
}
bool MainWindow::testSessionReady() const { return impl_->testSessionReady(); }
QString MainWindow::testStatusContextText() const
{
    return impl_->testStatusContextText();
}
bool MainWindow::testStatusBarAligned()
{
    return impl_->testStatusBarAligned();
}
bool MainWindow::testDisconnectButtonAvailable() const
{
    return impl_->testDisconnectButtonAvailable();
}
bool MainWindow::testSharedBoardOrientation()
{
    return impl_->testSharedBoardOrientation();
}
QByteArray MainWindow::testMqttClientId(bool host) const
{
    return impl_->testMqttClientId(host);
}
void MainWindow::testHandleMqttPacket(const QByteArray &packet)
{
    impl_->testHandleMqttPacket(packet);
}
QHash<uint16_t, QString> MainWindow::testMqttPendingSubacks() const
{
    return impl_->testMqttPendingSubacks();
}
QHash<uint16_t, QString> MainWindow::testMqttPendingUnsubacks() const
{
    return impl_->testMqttPendingUnsubacks();
}
QSet<QString> MainWindow::testMqttActiveSubscriptions() const
{
    return impl_->testMqttActiveSubscriptions();
}
bool MainWindow::testMqttOperational() const
{
    return impl_->testMqttOperational();
}
void MainWindow::testStartDirectGuestConnection(const QString &host, quint16 port)
{
    impl_->testStartDirectGuestConnection(host, port);
}
bool MainWindow::testDirectRetryPending() const
{
    return impl_->testDirectRetryPending();
}
bool MainWindow::testStartDirectHostListener(quint16 port)
{
    return impl_->testStartDirectHostListener(port);
}
bool MainWindow::testDirectListenerActive() const
{
    return impl_->testDirectListenerActive();
}
void MainWindow::testClickConnectButton()
{
    impl_->testClickConnectButton();
}
bool MainWindow::testReplaceDirectClientBeforeDisconnect()
{
    return impl_->testReplaceDirectClientBeforeDisconnect();
}
bool MainWindow::testResignRestartUiProjection()
{
    return impl_->testResignRestartUiProjection();
}
bool MainWindow::testRestoredMoveProjection()
{
    return impl_->testRestoredMoveProjection();
}
bool MainWindow::testUiRecoveryContracts()
{
    return impl_->testUiRecoveryContracts();
}
bool MainWindow::testTransportPortPersistence()
{
    return impl_->testTransportPortPersistence();
}
#endif

#ifdef NETCHESSZX_PC_MQTT_TX_FAILURE_TEST
bool MainWindow::testDesktopPresentation() { return impl_->testDesktopPresentation(); }
#endif
