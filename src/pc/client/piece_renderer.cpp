#include "piece_renderer.h"
#include "ui_theme.h"

#include <QByteArray>
#include <QColor>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QFont>
#include <QHash>
#include <QImage>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QPolygonF>
#include <QRadialGradient>
#include <QString>
#include <QStringList>
#include <QTransform>
#include <QtGlobal>
#include <QtMath>

extern "C" {
#include "common/reversi/reversi.h"
}

namespace PieceRenderer {

struct SquareIconKey {
    quint16 squareSize = 0;
    quint16 pieceIconSize = 0;
    qint8 row = 0;
    qint8 col = 0;
    char piece = 0;
    quint8 flags = 0;

    bool operator==(const SquareIconKey &other) const
    {
        return squareSize == other.squareSize &&
               pieceIconSize == other.pieceIconSize &&
               row == other.row &&
               col == other.col &&
               piece == other.piece &&
               flags == other.flags;
    }
};

static_assert(sizeof(SquareIconKey) == 8,
              "SquareIconKey must stay packed for qHashBits");

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
inline size_t qHash(const SquareIconKey &key, size_t seed = 0) noexcept
{
    return qHashBits(&key, sizeof(key), seed);
}
#else
inline uint qHash(const SquareIconKey &key, uint seed = 0)
{
    return qHashBits(&key, sizeof(key), seed);
}
#endif

// Procedural glass lattices.
enum GlassMotif : int {
    MotifIce = 0,     // hexagonal crystal facets
    MotifAurora = 1,  // soft wave arcs
    MotifViolet = 2,  // diamond facet star
    MotifEmber = 3    // radial ember sparks
};

struct GlassTheme {
    const char *id;
    const char *title;
    GlassMotif motif;
    // Board tiles (mid-tone darks so ECHO discs stay readable).
    QRgb light0, light1, light2;
    QRgb dark0, dark1, dark2;
    QRgb plate;
    QRgb grid;
    QRgb frame;
    // Piece glass tints: SELF (bright) / ECHO (deep) — multi-stop body.
    QRgb selfHi, selfMid, selfLo, selfRim;
    QRgb echoHi, echoMid, echoLo, echoRim;
};

static const GlassTheme kGlassThemes[] = {
    {
        // Dark cells ~ mid-light slate so ECHO discs read; light cells stay bright.
        "glass-lattice", "Glass Ice", MotifIce,
        qRgb(0xf6, 0xfa, 0xff), qRgb(0xe0, 0xee, 0xf8), qRgb(0xcc, 0xde, 0xf0),
        qRgb(0xa0, 0xb4, 0xc8), qRgb(0x90, 0xa4, 0xb8), qRgb(0x82, 0x96, 0xaa),
        qRgb(0x0c, 0x18, 0x24), qRgb(0x40, 0xe8, 0xff), qRgb(0x60, 0xf0, 0xff),
        qRgb(0xff, 0xff, 0xff), qRgb(0xf4, 0xf8, 0xfc), qRgb(0xc0, 0xd0, 0xe0), qRgb(0x20, 0x38, 0x50),
        qRgb(0x6a, 0x7e, 0x96), qRgb(0x22, 0x2e, 0x42), qRgb(0x0a, 0x10, 0x1a), qRgb(0x10, 0x18, 0x28),
    },
    {
        "glass-aurora", "Glass Aurora", MotifAurora,
        qRgb(0xf2, 0xfc, 0xf8), qRgb(0xd8, 0xf4, 0xea), qRgb(0xc0, 0xe8, 0xdc),
        qRgb(0x88, 0xb0, 0xac), qRgb(0x78, 0xa0, 0x9c), qRgb(0x6a, 0x90, 0x8c),
        qRgb(0x06, 0x18, 0x1a), qRgb(0x30, 0xf8, 0xd8), qRgb(0x50, 0xff, 0xe8),
        qRgb(0xfc, 0xff, 0xfe), qRgb(0xe8, 0xf8, 0xf4), qRgb(0xb0, 0xd8, 0xcc), qRgb(0x18, 0x48, 0x44),
        qRgb(0x58, 0x88, 0x80), qRgb(0x18, 0x38, 0x36), qRgb(0x08, 0x14, 0x14), qRgb(0x0c, 0x20, 0x1e),
    },
    {
        "glass-violet", "Glass Violet", MotifViolet,
        qRgb(0xf8, 0xf4, 0xfc), qRgb(0xea, 0xe0, 0xf6), qRgb(0xd8, 0xcc, 0xec),
        qRgb(0xa8, 0x9c, 0xc0), qRgb(0x98, 0x8c, 0xb0), qRgb(0x8a, 0x7e, 0xa0),
        qRgb(0x12, 0x0c, 0x1e), qRgb(0xd0, 0x98, 0xff), qRgb(0xe0, 0xb0, 0xff),
        qRgb(0xff, 0xfc, 0xff), qRgb(0xf2, 0xec, 0xf8), qRgb(0xc8, 0xb8, 0xd8), qRgb(0x38, 0x28, 0x50),
        qRgb(0x70, 0x60, 0x90), qRgb(0x28, 0x1c, 0x40), qRgb(0x0c, 0x08, 0x16), qRgb(0x14, 0x0c, 0x22),
    },
    {
        "glass-ember", "Glass Ember", MotifEmber,
        qRgb(0xfc, 0xf6, 0xf0), qRgb(0xf4, 0xe6, 0xd8), qRgb(0xe8, 0xd4, 0xc4),
        qRgb(0xb0, 0x9c, 0x94), qRgb(0xa0, 0x8c, 0x84), qRgb(0x92, 0x7e, 0x78),
        qRgb(0x16, 0x10, 0x12), qRgb(0xff, 0xa8, 0x70), qRgb(0xff, 0xc0, 0x90),
        qRgb(0xff, 0xfc, 0xf8), qRgb(0xf8, 0xee, 0xe4), qRgb(0xd8, 0xc0, 0xaa), qRgb(0x50, 0x30, 0x24),
        qRgb(0x80, 0x60, 0x54), qRgb(0x30, 0x1c, 0x18), qRgb(0x10, 0x08, 0x08), qRgb(0x1a, 0x0c, 0x0a),
    },
};

static const GlassTheme *findGlassTheme(const QString &name)
{
    for (const GlassTheme &theme : kGlassThemes) {
        if (name == QLatin1String(theme.id)) {
            return &theme;
        }
    }
    return nullptr;
}

static QString g_boardTexture;

static const GlassTheme &activeGlassTheme()
{
    if (const GlassTheme *t = findGlassTheme(g_boardTexture)) {
        return *t;
    }
    return kGlassThemes[0];
}

QString assetPath(const QString &relativePath)
{
    const QString appDir = QCoreApplication::applicationDirPath();
    const QStringList bases = {
        QDir::cleanPath(appDir + "/" + relativePath),
        QDir::cleanPath(appDir + "/../Resources/" + relativePath),
        QDir::cleanPath(appDir + "/../../" + relativePath),
        QDir::cleanPath(QDir::currentPath() + "/" + relativePath),
    };
    for (const QString &path : bases) {
        if (QFileInfo::exists(path)) return path;
    }
    return QString();
}

static QPolygonF hexagonAt(const QPointF &c, qreal radius)
{
    QPolygonF hex;
    hex.reserve(6);
    for (int i = 0; i < 6; ++i) {
        const qreal a = -M_PI / 2.0 + i * M_PI / 3.0;
        hex << QPointF(c.x() + qCos(a) * radius, c.y() + qSin(a) * radius);
    }
    return hex;
}

// Cut-glass structure inside the disc. Filled facets, not ink logos: the
// silhouette stays a Reversi puck; the lattice is the crystal.
static void drawCrystalMotif(QPainter &p, const QPointF &c, qreal radius,
                             bool realityA, const GlassTheme &theme)
{
    const QColor hi = QColor::fromRgb(realityA ? theme.selfHi : theme.echoHi);
    const QColor mid = QColor::fromRgb(realityA ? theme.selfMid : theme.echoMid);
    const QColor lo = QColor::fromRgb(realityA ? theme.selfLo : theme.echoLo);
    const QColor accent = QColor::fromRgb(theme.grid);
    QColor edge = accent.lighter(realityA ? 160 : 110);
    edge.setAlpha(realityA ? 160 : 95);

    switch (theme.motif) {
    case MotifIce: {
        const QPolygonF hex = hexagonAt(c, radius * 0.48);
        for (int i = 0; i < 6; ++i) {
            QPolygonF tri;
            tri << c << hex.at(i) << hex.at((i + 1) % 6);
            QColor fill = realityA
                ? hi.darker(100 + (i % 2) * 12)
                : lo.lighter(110 + (i % 2) * 18);
            fill.setAlpha(realityA ? (95 + (i % 2) * 22) : (110 + (i % 2) * 20));
            p.setPen(Qt::NoPen);
            p.setBrush(fill);
            p.drawPolygon(tri);
        }
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(edge, 1.05, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.drawPolygon(hex);
        p.setPen(QPen(edge, 0.8));
        for (int i = 0; i < 6; ++i)
            p.drawLine(c, hex.at(i));
        // Crystal edge catch on the upper-left facets, not a plastic dome.
        p.setPen(QPen(QColor(255, 255, 255, realityA ? 200 : 80), 1.2,
                      Qt::SolidLine, Qt::RoundCap));
        p.drawLine(hex.at(5), hex.at(0));
        p.drawLine(hex.at(0), c);
        break;
    }
    case MotifAurora: {
        auto wave = [&](qreal y0, qreal y1, qreal y2, qreal y3, int alpha) {
            QPainterPath w;
            w.moveTo(c.x() - radius * 0.55, c.y() + y0);
            w.cubicTo(c.x() - radius * 0.15, c.y() + y1,
                      c.x() + radius * 0.15, c.y() + y2,
                      c.x() + radius * 0.55, c.y() + y3);
            QColor band = accent;
            band.setAlpha(alpha);
            p.setPen(QPen(band, 1.6, Qt::SolidLine, Qt::RoundCap));
            p.setBrush(Qt::NoBrush);
            p.drawPath(w);
        };
        wave(-radius * 0.08, -radius * 0.32, radius * 0.18, -radius * 0.04,
             realityA ? 120 : 80);
        wave(radius * 0.16, -radius * 0.04, radius * 0.34, radius * 0.08,
             realityA ? 90 : 60);
        QColor core = hi;
        core.setAlpha(realityA ? 70 : 50);
        p.setPen(QPen(QColor(255, 255, 255, realityA ? 90 : 50), 1.0));
        p.setBrush(core);
        p.drawEllipse(c, radius * 0.18, radius * 0.18);
        break;
    }
    case MotifViolet: {
        QPolygonF dia;
        dia << QPointF(c.x(), c.y() - radius * 0.50)
            << QPointF(c.x() + radius * 0.34, c.y())
            << QPointF(c.x(), c.y() + radius * 0.50)
            << QPointF(c.x() - radius * 0.34, c.y());
        for (int i = 0; i < 4; ++i) {
            QPolygonF tri;
            tri << c << dia.at(i) << dia.at((i + 1) % 4);
            QColor fill = (i % 2) ? hi : mid;
            if (!realityA)
                fill = (i % 2) ? mid : lo;
            fill.setAlpha(realityA ? 100 : 115);
            p.setPen(Qt::NoPen);
            p.setBrush(fill);
            p.drawPolygon(tri);
        }
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(edge, 1.05, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.drawPolygon(dia);
        p.drawLine(dia.at(0), dia.at(2));
        p.drawLine(dia.at(1), dia.at(3));
        p.setPen(QPen(QColor(255, 255, 255, realityA ? 180 : 70), 1.15,
                      Qt::SolidLine, Qt::RoundCap));
        p.drawLine(dia.at(3), dia.at(0));
        break;
    }
    case MotifEmber: {
        for (int i = 0; i < 8; ++i) {
            const qreal a0 = i * M_PI / 4.0 + M_PI / 8.0;
            const qreal a1 = a0 + M_PI / 4.0;
            QPolygonF wedge;
            wedge << QPointF(c.x() + qCos(a0) * radius * 0.18,
                             c.y() + qSin(a0) * radius * 0.18)
                  << QPointF(c.x() + qCos(a0) * radius * 0.50,
                             c.y() + qSin(a0) * radius * 0.50)
                  << QPointF(c.x() + qCos(a1) * radius * 0.50,
                             c.y() + qSin(a1) * radius * 0.50)
                  << QPointF(c.x() + qCos(a1) * radius * 0.18,
                             c.y() + qSin(a1) * radius * 0.18);
            QColor fill = (i % 2) ? accent : (realityA ? hi : lo);
            fill.setAlpha(realityA ? (90 + (i % 2) * 25) : (100 + (i % 2) * 20));
            p.setPen(Qt::NoPen);
            p.setBrush(fill);
            p.drawPolygon(wedge);
        }
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(edge, 1.05));
        p.drawEllipse(c, radius * 0.18, radius * 0.18);
        break;
    }
    }
}

// Reversi puck of cut glass: circular disc, visible side wall, concave
// well, transmissive rim. Motif is the crystal lattice inside the glass.
static void drawFragmentFace(QPainter &p, const QPointF &c, qreal radius, bool realityA,
                             bool drawShadow, bool drawPuck)
{
    const GlassTheme &theme = activeGlassTheme();
    const QColor hi = QColor::fromRgb(realityA ? theme.selfHi : theme.echoHi);
    const QColor mid = QColor::fromRgb(realityA ? theme.selfMid : theme.echoMid);
    const QColor lo = QColor::fromRgb(realityA ? theme.selfLo : theme.echoLo);
    const QColor accent = QColor::fromRgb(theme.grid);
    const qreal thick = radius * 0.14;

    if (drawShadow) {
        QRadialGradient shadow(c.x() + 0.8, c.y() + radius * 0.52, radius * 1.15);
        shadow.setColorAt(0.0, QColor(0, 6, 14, 90));
        shadow.setColorAt(0.55, QColor(0, 6, 14, 32));
        shadow.setColorAt(1.0, QColor(0, 0, 0, 0));
        p.setPen(Qt::NoPen);
        p.setBrush(shadow);
        p.drawEllipse(QPointF(c.x() + 0.5, c.y() + radius * 0.42),
                      radius * 0.92, radius * 0.32);
    }

    if (drawPuck) {
        QColor wall = lo;
        if (!realityA)
            wall = wall.darker(130);
        wall.setAlpha(realityA ? 230 : 245);
        p.setPen(Qt::NoPen);
        p.setBrush(wall);
        p.drawEllipse(QPointF(c.x(), c.y() + thick), radius, radius);
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(QColor(255, 255, 255, realityA ? 80 : 35), 1.0,
                      Qt::SolidLine, Qt::RoundCap));
        p.drawArc(QRectF(c.x() - radius, c.y() - radius + thick,
                         radius * 2.0, radius * 2.0),
                  220 * 16, 100 * 16);
    }

    {
        QRadialGradient bowl(c.x() + radius * 0.06, c.y() + radius * 0.08, radius);
        if (realityA) {
            QColor well = mid.darker(108);
            well.setAlpha(185);
            QColor body = mid;
            body.setAlpha(175);
            QColor rim = hi;
            rim.setAlpha(155);
            QColor glass = hi;
            glass.setAlpha(110);
            QColor edge = lo;
            edge.setAlpha(70);
            bowl.setColorAt(0.00, well);
            bowl.setColorAt(0.38, body);
            bowl.setColorAt(0.70, rim);
            bowl.setColorAt(0.90, glass);
            bowl.setColorAt(1.00, edge);
        } else {
            QColor well = lo.darker(125);
            well.setAlpha(230);
            QColor body = lo;
            body.setAlpha(215);
            QColor lift = mid;
            lift.setAlpha(190);
            QColor glass = mid.lighter(120);
            glass.setAlpha(140);
            QColor edge = lo;
            edge.setAlpha(100);
            bowl.setColorAt(0.00, well);
            bowl.setColorAt(0.40, body);
            bowl.setColorAt(0.72, lift);
            bowl.setColorAt(0.90, glass);
            bowl.setColorAt(1.00, edge);
        }
        p.setPen(Qt::NoPen);
        p.setBrush(bowl);
        p.drawEllipse(c, radius, radius);
    }

    {
        QRadialGradient well(c.x() - radius * 0.04, c.y() - radius * 0.06, radius * 0.70);
        well.setColorAt(0.0, QColor(0, 8, 16, realityA ? 40 : 80));
        well.setColorAt(0.65, QColor(0, 8, 16, realityA ? 14 : 28));
        well.setColorAt(1.0, QColor(0, 0, 0, 0));
        p.setBrush(well);
        p.drawEllipse(c, radius * 0.78, radius * 0.78);
    }

    {
        QPainterPath clip;
        clip.addEllipse(c, radius, radius);
        p.save();
        p.setClipPath(clip);
        QRadialGradient caustic(c.x() + radius * 0.28, c.y() + radius * 0.22, radius * 0.42);
        QColor glow = accent;
        glow.setAlpha(realityA ? 55 : 30);
        caustic.setColorAt(0.0, glow);
        caustic.setColorAt(1.0, QColor(accent.red(), accent.green(), accent.blue(), 0));
        p.setBrush(caustic);
        p.drawEllipse(QPointF(c.x() + radius * 0.22, c.y() + radius * 0.18),
                      radius * 0.42, radius * 0.32);
        p.restore();
    }

    {
        QPainterPath clip;
        clip.addEllipse(c, radius * 0.86, radius * 0.86);
        p.save();
        p.setClipPath(clip);
        drawCrystalMotif(p, c, radius, realityA, theme);
        p.restore();
    }

    {
        QPainterPath outer;
        outer.addEllipse(c, radius * 0.99, radius * 0.99);
        QPainterPath inner;
        inner.addEllipse(c, radius * 0.78, radius * 0.78);
        QPainterPath ring = outer.subtracted(inner);
        QRadialGradient rimGrad(c, radius);
        if (realityA) {
            rimGrad.setColorAt(0.78, QColor(255, 255, 255, 0));
            rimGrad.setColorAt(0.90, QColor(255, 255, 255, 170));
            rimGrad.setColorAt(0.97, QColor(255, 255, 255, 80));
            rimGrad.setColorAt(1.00, QColor(200, 220, 240, 40));
        } else {
            rimGrad.setColorAt(0.78, QColor(255, 255, 255, 0));
            rimGrad.setColorAt(0.90, QColor(hi.red(), hi.green(), hi.blue(), 110));
            rimGrad.setColorAt(0.97, QColor(mid.red(), mid.green(), mid.blue(), 55));
            rimGrad.setColorAt(1.00, QColor(0, 0, 0, 50));
        }
        p.setPen(Qt::NoPen);
        p.setBrush(rimGrad);
        p.drawPath(ring);
    }

    {
        QPainterPath clip;
        clip.addEllipse(c, radius * 0.97, radius * 0.97);
        p.save();
        p.setClipPath(clip);
        QRadialGradient lip(
            c.x() - radius * 0.22,
            c.y() - radius * 0.62,
            radius * 0.42);
        lip.setColorAt(0.0, QColor(255, 255, 255, realityA ? 160 : 70));
        lip.setColorAt(0.40, QColor(255, 255, 255, realityA ? 35 : 14));
        lip.setColorAt(1.0, QColor(255, 255, 255, 0));
        p.setBrush(lip);
        p.drawEllipse(QPointF(c.x() - radius * 0.18, c.y() - radius * 0.58),
                      radius * 0.40, radius * 0.20);
        p.restore();
    }

    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(QColor(255, 255, 255, realityA ? 150 : 55), 1.15,
                  Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.drawArc(QRectF(c.x() - radius, c.y() - radius, radius * 2.0, radius * 2.0),
              50 * 16, 130 * 16);
    p.setPen(QPen(QColor(0, 6, 14, realityA ? 85 : 175), 1.2,
                  Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.drawArc(QRectF(c.x() - radius, c.y() - radius, radius * 2.0, radius * 2.0),
              220 * 16, 130 * 16);

    if (realityA) {
        p.setPen(QPen(QColor(16, 28, 44, 145), 1.0));
        p.drawEllipse(c, radius + 0.45, radius + 0.45);
    } else {
        p.setPen(QPen(QColor(0, 0, 0, 210), 0.95));
        p.drawEllipse(c, radius + 0.30, radius + 0.30);
    }
}

static void drawFragment(QPainter &p, bool realityA)
{
    drawFragmentFace(p, QPointF(26.0, 26.0), 20.5, realityA, true, true);
}

static bool isEmptyCell(char cell)
{
    return cell == '.' || cell == MS_CELL_EMPTY;
}

// Coin flip: thick crystal disc. Start face at 0, end face at PI.
// An empty start is a placement: nothing at 0, arriving disc after the edge.
static void drawTokenFlip3D(QPainter &p, char startFace, char endFace, qreal angle)
{
    const GlassTheme &theme = activeGlassTheme();
    const QPointF c(26.0, 26.0);
    const qreal R = 20.5;
    const qreal height = 8.0;
    const qreal cosA = qCos(angle);
    const qreal sinA = qAbs(qSin(angle));
    const qreal absC = qAbs(cosA);
    const qreal faceRx = R * qMax(0.05, absC);
    const qreal rimExtra = height * sinA;
    const qreal dir = (cosA >= 0.0) ? 1.0 : -1.0;
    const bool startEmpty = isEmptyCell(startFace);
    const bool startA = startFace == MS_CELL_A;
    const bool endA = endFace == MS_CELL_A;
    const bool showStart = (cosA >= 0.0);

    if (angle <= 0.0) {
        if (!startEmpty) {
            drawFragment(p, startA);
        }
        return;
    }
    if (angle >= M_PI) {
        drawFragment(p, endA);
        return;
    }

    {
        const qreal shadowAmt = (startEmpty && showStart)
            ? sinA
            : (0.35 + 0.65 * absC);
        QRadialGradient shadow(c.x() + 0.8, c.y() + 2.4, R + 3.0);
        shadow.setColorAt(0.0, QColor(0, 6, 14, int(95 * shadowAmt)));
        shadow.setColorAt(1.0, QColor(0, 0, 0, 0));
        p.setPen(Qt::NoPen);
        p.setBrush(shadow);
        p.drawEllipse(QPointF(c.x() + 0.6, c.y() + 1.8),
                      faceRx + rimExtra * 0.45 + 2.0, R + 1.2);
    }

    if (rimExtra > 0.4) {
        const qreal wallCx = c.x() + dir * (faceRx * 0.12);
        QRectF wall(wallCx - faceRx - rimExtra * 0.55,
                    c.y() - R,
                    faceRx * 2.0 + rimExtra * 1.15,
                    R * 2.0);
        QLinearGradient glassEdge(wall.left(), c.y(), wall.right(), c.y());
        const bool lightLeft = startEmpty
            ? ((dir > 0.0) ? endA : !endA)
            : ((dir > 0.0) ? startA : !startA);
        if (startEmpty) {
            if (endA) {
                glassEdge.setColorAt(0.0, QColor::fromRgb(theme.selfHi));
                glassEdge.setColorAt(0.45, QColor::fromRgb(theme.selfMid));
                glassEdge.setColorAt(1.0, QColor::fromRgb(theme.selfLo));
            } else {
                glassEdge.setColorAt(0.0, QColor::fromRgb(theme.echoLo));
                glassEdge.setColorAt(0.45, QColor::fromRgb(theme.echoMid));
                glassEdge.setColorAt(1.0, QColor::fromRgb(theme.echoHi));
            }
        } else if (lightLeft) {
            glassEdge.setColorAt(0.0, QColor::fromRgb(theme.selfHi));
            glassEdge.setColorAt(0.35, QColor::fromRgb(theme.selfMid));
            glassEdge.setColorAt(0.55, QColor::fromRgb(theme.echoMid));
            glassEdge.setColorAt(1.0, QColor::fromRgb(theme.echoLo));
        } else {
            glassEdge.setColorAt(0.0, QColor::fromRgb(theme.echoLo));
            glassEdge.setColorAt(0.45, QColor::fromRgb(theme.echoMid));
            glassEdge.setColorAt(0.65, QColor::fromRgb(theme.selfMid));
            glassEdge.setColorAt(1.0, QColor::fromRgb(theme.selfHi));
        }
        p.setPen(QPen(QColor(0, 40, 70, 90), 0.9));
        p.setBrush(glassEdge);
        p.drawEllipse(wall);

        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(QColor(255, 255, 255, int(70 + 90 * sinA)), 1.2));
        p.drawArc(wall.adjusted(1.0, 1.0, -1.0, -1.0),
                  (dir > 0 ? 70 : 250) * 16, 40 * 16);
    }

    if (!(showStart && startEmpty)) {
        const bool faceA = showStart ? startA : endA;
        p.save();
        p.translate(c);
        p.scale(qMax(0.05, absC), 1.0);
        p.translate(-c);
        p.translate(-dir * (rimExtra * 0.08) / qMax(0.05, absC), 0.0);
        drawFragmentFace(p, c, R, faceA, false, false);
        p.restore();
    }

    if (rimExtra > 0.8) {
        const qreal hx = c.x() + dir * faceRx * 0.85;
        QLinearGradient hi(hx - 1.5, c.y() - R, hx + 1.5, c.y() + R);
        hi.setColorAt(0.0, QColor(255, 255, 255, 0));
        hi.setColorAt(0.5, QColor(220, 245, 255, int(45 + 80 * sinA)));
        hi.setColorAt(1.0, QColor(255, 255, 255, 0));
        p.setPen(Qt::NoPen);
        p.setBrush(hi);
        p.drawEllipse(QPointF(hx, c.y()), qMax(1.2, rimExtra * 0.35), R * 0.92);
    }
}

static QHash<QString, QIcon> s_iconCache;
static QHash<SquareIconKey, QIcon> s_squareIconCache;
static QString s_boardCacheName;
static int s_boardCacheSquareSize = 0;
static QImage s_boardCache;
static QColor s_wellColor;
static QString s_wellTexture;
static int s_wellSquareSize = -1;
static bool s_prewarmed = false;

void setBoardTexture(const QString &name)
{
    g_boardTexture = name;
    s_iconCache.clear();
    s_squareIconCache.clear();
    s_boardCache = QImage();
    s_boardCacheName.clear();
    s_boardCacheSquareSize = 0;
    s_wellColor = QColor();
    s_wellTexture.clear();
    s_wellSquareSize = -1;
    s_prewarmed = false;
}
QString boardTexture() { return g_boardTexture; }

QStringList glassBoardThemeIds()
{
    QStringList ids;
    for (const GlassTheme &theme : kGlassThemes) {
        ids.append(QLatin1String(theme.id));
    }
    return ids;
}

QString glassBoardThemeTitle(const QString &id)
{
    if (const GlassTheme *theme = findGlassTheme(id)) {
        return QLatin1String(theme->title);
    }
    return id;
}

bool isGlassBoardTheme(const QString &id)
{
    return findGlassTheme(id) != nullptr;
}

QColor glassBoardFrameColor(const QString &id)
{
    if (const GlassTheme *theme = findGlassTheme(id)) {
        return QColor::fromRgb(theme->frame);
    }
    return QColor(QStringLiteral(MSH_BOARD_EDGE));
}

QColor boardWellColor(int squareSize)
{
    if (s_wellTexture == g_boardTexture && s_wellSquareSize == squareSize &&
        s_wellColor.isValid()) {
        return s_wellColor;
    }
    if (const GlassTheme *theme = findGlassTheme(g_boardTexture)) {
        s_wellColor = QColor::fromRgb(theme->plate);
    } else {
        const QImage board = boardTextureImage(squareSize);
        if (board.isNull() || squareSize <= 0) {
            s_wellColor = QColor(QStringLiteral(MSH_BOARD_WELL));
        } else {
            const QImage sample = board.copy(4 * squareSize, 3 * squareSize,
                                             squareSize, squareSize)
                                       .scaled(1, 1, Qt::IgnoreAspectRatio,
                                               Qt::SmoothTransformation);
            s_wellColor = sample.pixelColor(0, 0).darker(190);
        }
    }
    s_wellTexture = g_boardTexture;
    s_wellSquareSize = squareSize;
    return s_wellColor;
}


QString boardSquareStyle(int row, int col, int squareSize,
                         const QString &background, const QString &foreground,
                         const QString &border)
{
    const bool light = ((row + col) % 2) == 0;
    const QString defaultBg = light ? QStringLiteral(MSH_SQ_LIGHT)
                                    : QStringLiteral(MSH_SQ_DARK);
    const bool textured = background == defaultBg &&
                          !boardTextureImage(squareSize).isNull();
    const QString bg = textured ? QStringLiteral("transparent") : background;
    return QString("QPushButton { background:%1; color:%2; border:%3;"
                   " font:700 24px Consolas; padding:0; margin:0; }")
        .arg(bg, foreground, border);
}

// --- Icon rendering ---

QIcon pieceIcon(char piece)
{
    if (piece == MS_CELL_EMPTY) return QIcon();

    if (piece != MS_CELL_A && piece != MS_CELL_B) return QIcon();
    const QString cacheKey = g_boardTexture + QChar(piece);
    const auto cached = s_iconCache.constFind(cacheKey);
    if (cached != s_iconCache.constEnd())
        return cached.value();

    // Procedural glass tokens keep SELF/ECHO consistent with the lattice.
    QPixmap pixmap(52, 52);
    pixmap.fill(Qt::transparent);
    QPainter p(&pixmap);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);
    p.translate(0.5, 0.5);
    drawFragment(p, piece == MS_CELL_A);
    p.end();
    QIcon icon(pixmap);
    s_iconCache.insert(cacheKey, icon);
    return icon;
}

void prewarmPieceIcons()
{
    if (s_prewarmed) return;
    s_prewarmed = true;
    for (const char piece : QByteArray("AB"))
        (void)pieceIcon(piece);
}

// --- Board texture ---

static void paintGlassSquare(QPainter &p, const QRectF &rect, bool light,
                             const GlassTheme &theme)
{
    const qreal radius = qMax(2.8, rect.width() * 0.07);
    const QRectF cell = rect.adjusted(1.0, 1.0, -1.0, -1.0);

    // Deep glass well under the tile (depth).
    p.setPen(Qt::NoPen);
    p.setBrush(QColor::fromRgb(theme.plate));
    p.drawRoundedRect(rect.adjusted(0.3, 0.3, -0.3, -0.3), radius + 0.5, radius + 0.5);

    // Crystal tile face with stronger refraction gradient.
    QLinearGradient body(cell.topLeft(), cell.bottomRight());
    if (light) {
        body.setColorAt(0.00, QColor::fromRgb(theme.light0));
        body.setColorAt(0.35, QColor::fromRgb(theme.light1));
        body.setColorAt(0.75, QColor::fromRgb(theme.light2));
        body.setColorAt(1.00, QColor::fromRgb(theme.light2).darker(108));
    } else {
        body.setColorAt(0.00, QColor::fromRgb(theme.dark0));
        body.setColorAt(0.40, QColor::fromRgb(theme.dark1));
        body.setColorAt(0.80, QColor::fromRgb(theme.dark2));
        body.setColorAt(1.00, QColor::fromRgb(theme.dark2).darker(110));
    }
    p.setBrush(body);
    QColor edge = QColor::fromRgb(theme.grid);
    edge.setAlpha(light ? 100 : 75);
    p.setPen(QPen(edge, 1.1));
    p.drawRoundedRect(cell, radius, radius);

    // Wet glass specular blotch.
    {
        QPainterPath clip;
        clip.addRoundedRect(cell, radius, radius);
        p.save();
        p.setClipPath(clip);
        QRadialGradient gloss(
            cell.left() + cell.width() * 0.30,
            cell.top() + cell.height() * 0.28,
            cell.width() * 0.70);
        gloss.setColorAt(0.0, QColor(255, 255, 255, light ? 70 : 35));
        gloss.setColorAt(0.45, QColor(255, 255, 255, light ? 18 : 10));
        gloss.setColorAt(1.0, QColor(255, 255, 255, 0));
        p.setPen(Qt::NoPen);
        p.setBrush(gloss);
        p.drawRect(cell);

        // Secondary caustic strip.
        QLinearGradient strip(cell.topLeft(), cell.bottomRight());
        strip.setColorAt(0.0, QColor(255, 255, 255, 0));
        strip.setColorAt(0.40, QColor(255, 255, 255, light ? 40 : 18));
        strip.setColorAt(0.55, QColor(255, 255, 255, 0));
        p.setBrush(strip);
        p.drawRect(cell);
        p.restore();
    }

    // Inner glass lip.
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(QColor(255, 255, 255, light ? 70 : 30), 1.0));
    p.drawRoundedRect(cell.adjusted(1.8, 1.8, -1.8, -1.8), radius * 0.75, radius * 0.75);
}

static QImage buildGlassLatticeBoard(int squareSize, const GlassTheme &theme)
{
    const int total = squareSize * 8;
    QImage img(total, total, QImage::Format_ARGB32_Premultiplied);
    img.fill(QColor::fromRgb(theme.plate));

    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing, true);

    // Luminous under-glass plate.
    QRadialGradient plate(total * 0.5, total * 0.45, total * 0.8);
    QColor p0 = QColor::fromRgb(theme.plate).lighter(140);
    QColor p1 = QColor::fromRgb(theme.plate);
    plate.setColorAt(0.0, p0);
    plate.setColorAt(0.55, p1);
    plate.setColorAt(1.0, p1.darker(120));
    p.fillRect(0, 0, total, total, plate);

    for (int row = 0; row < 8; ++row) {
        for (int col = 0; col < 8; ++col) {
            paintGlassSquare(
                p,
                QRectF(col * squareSize, row * squareSize, squareSize, squareSize),
                ((row + col) % 2) == 0,
                theme);
        }
    }

    // Crystal grid veins.
    QColor grid = QColor::fromRgb(theme.grid);
    grid.setAlpha(130);
    p.setPen(QPen(grid, 1.35));
    for (int i = 0; i <= 8; ++i) {
        const qreal x = i * squareSize + 0.5;
        p.drawLine(QPointF(x, 0), QPointF(x, total));
        p.drawLine(QPointF(0, x), QPointF(total, x));
    }
    grid.setAlpha(40);
    p.setPen(QPen(grid, 3.0));
    for (int i = 0; i <= 8; ++i) {
        const qreal x = i * squareSize + 0.5;
        p.drawLine(QPointF(x, 0), QPointF(x, total));
        p.drawLine(QPointF(0, x), QPointF(total, x));
    }

    QColor frame = QColor::fromRgb(theme.frame);
    frame.setAlpha(140);
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(frame, 1.0));
    p.drawRect(QRectF(1.0, 1.0, total - 2.0, total - 2.0));

    p.end();
    return img;
}

QImage boardTextureImage(int squareSize)
{
    if (g_boardTexture.isEmpty()) return QImage();
    if (s_boardCacheName == g_boardTexture &&
        s_boardCacheSquareSize == squareSize &&
        !s_boardCache.isNull()) {
        return s_boardCache;
    }

    QImage img;
    const GlassTheme *theme = findGlassTheme(g_boardTexture);
    if (theme == nullptr) return QImage();
    img = buildGlassLatticeBoard(squareSize, *theme);

    s_boardCacheName = g_boardTexture;
    s_boardCacheSquareSize = squareSize;
    s_boardCache = img;
    return s_boardCache;
}

QIcon boardSquareIcon(char piece, int row, int col, int squareSize,
                      int pieceIconSize, bool pieceVisible, bool legalHint,
                      bool targetHighlight)
{
    SquareIconKey cacheKey;
    cacheKey.squareSize = static_cast<quint16>(qBound(0, squareSize, 0xffff));
    cacheKey.pieceIconSize = static_cast<quint16>(qBound(0, pieceIconSize, 0xffff));
    cacheKey.row = static_cast<qint8>(row);
    cacheKey.col = static_cast<qint8>(col);
    cacheKey.piece = piece;
    cacheKey.flags = static_cast<quint8>(
        (pieceVisible ? 1u : 0u) |
        (legalHint ? 2u : 0u) |
        (targetHighlight ? 4u : 0u));
    const auto cached = s_squareIconCache.constFind(cacheKey);
    if (cached != s_squareIconCache.constEnd()) {
        return cached.value();
    }

    const QImage board = boardTextureImage(squareSize);
    if (board.isNull()) {
        return pieceVisible ? pieceIcon(piece) : QIcon();
    }

    QPixmap pixmap = QPixmap::fromImage(
        board.copy(col * squareSize, row * squareSize, squareSize, squareSize));
    if ((pieceVisible && piece != '.') || legalHint || targetHighlight) {
        QPainter painter(&pixmap);
        painter.setRenderHint(QPainter::Antialiasing, true);
        if (targetHighlight) {
            painter.setPen(QPen(QColor(0, 215, 255, 200), 2.5));
            painter.setBrush(QColor(0, 180, 255, 55));
            painter.drawRoundedRect(QRectF(2.5, 2.5, squareSize - 5.0, squareSize - 5.0),
                                    5.0, 5.0);
        }
        if (pieceVisible && piece != '.') {
            const QPixmap piecePixmap = pieceIcon(piece).pixmap(pieceIconSize, pieceIconSize);
            const int offset = (squareSize - pieceIconSize) / 2;
            painter.drawPixmap(offset, offset, piecePixmap);
        } else if (legalHint) {
            const qreal r = qMax(4.0, squareSize / 9.0);
            QRadialGradient hint(squareSize / 2.0, squareSize / 2.0, r + 2.0);
            hint.setColorAt(0.0, QColor(0, 220, 255, 200));
            hint.setColorAt(0.65, QColor(0, 180, 255, 120));
            hint.setColorAt(1.0, QColor(0, 160, 255, 0));
            painter.setPen(Qt::NoPen);
            painter.setBrush(hint);
            painter.drawEllipse(QPointF(squareSize / 2.0, squareSize / 2.0), r + 1.0, r + 1.0);
        }
    }
    QIcon icon(pixmap);
    s_squareIconCache.insert(cacheKey, icon);
    return icon;
}

// startFace at angle 0, endFace at PI. Empty start is a placement from a
// vacant square. angle is 0 .. PI.
QIcon boardSquareIconFlip(char startFace, char endFace, int row, int col,
                          int squareSize, int pieceIconSize, qreal angle)
{
    const QImage board = boardTextureImage(squareSize);
    QPixmap pixmap;
    if (!board.isNull()) {
        pixmap = QPixmap::fromImage(
            board.copy(col * squareSize, row * squareSize, squareSize, squareSize));
    } else {
        pixmap = QPixmap(squareSize, squareSize);
        pixmap.fill(Qt::transparent);
    }

    if (isEmptyCell(startFace) && isEmptyCell(endFace)) {
        return QIcon(pixmap);
    }

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    QPixmap token(52, 52);
    token.fill(Qt::transparent);
    {
        QPainter tp(&token);
        tp.setRenderHint(QPainter::Antialiasing, true);
        tp.translate(0.5, 0.5);
        drawTokenFlip3D(tp, startFace, endFace, angle);
    }

    const QPixmap scaled = token.scaled(
        pieceIconSize, pieceIconSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    const int ox = (squareSize - scaled.width()) / 2;
    const int oy = (squareSize - scaled.height()) / 2;
    painter.drawPixmap(ox, oy, scaled);
    return QIcon(pixmap);
}

QIcon makeMirrorShiftIcon()
{
    const QString path = assetPath(QStringLiteral("assets/pc-client/mirrorshift-icon.png"));
    if (!path.isEmpty()) {
        QIcon icon(path);
        if (!icon.isNull()) {
            return icon;
        }
    }

    QPixmap pixmap(256, 256);
    pixmap.fill(Qt::transparent);
    QPainter p(&pixmap);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);

    p.setPen(Qt::NoPen);
    p.setBrush(QColor(QStringLiteral(MSH_INK)));
    p.drawRoundedRect(QRectF(8, 8, 240, 240), 52, 52);

    p.translate(28, 52);
    p.scale(3.4, 3.4);
    drawFragment(p, true);
    p.resetTransform();
    p.translate(116, 52);
    p.scale(3.4, 3.4);
    drawFragment(p, false);

    p.resetTransform();
    QLinearGradient blade(QPointF(128, 72), QPointF(128, 184));
    blade.setColorAt(0.0, QColor(255, 255, 255, 0));
    blade.setColorAt(0.5, QColor(200, 250, 255, 230));
    blade.setColorAt(1.0, QColor(255, 255, 255, 0));
    p.setPen(QPen(QBrush(blade), 4.0, Qt::SolidLine, Qt::RoundCap));
    p.drawLine(QPointF(128, 78), QPointF(128, 178));
    p.end();
    return QIcon(pixmap);
}

} // namespace PieceRenderer
