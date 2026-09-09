#include "app_banner.h"
#include "piece_renderer.h"
#include "ui_theme.h"
#include <QColor>
#include <QFont>
#include <QFontMetricsF>
#include <QKeyEvent>
#include <QLinearGradient>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QRandomGenerator>
#include <QTimerEvent>

AppBanner::AppBanner(QWidget *parent)
    : QWidget(parent)
    , mosaicImage_(620, 70, QImage::Format_ARGB32_Premultiplied)
    , logoImage_(PieceRenderer::assetPath(
          QStringLiteral("assets/pc-client/mirrorshift-wordmark.png")))
{
    setFixedHeight(76);
    setAccessibleName(QStringLiteral("About Mirror Shift"));
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::StrongFocus);
    setToolTip(QStringLiteral("About Mirror Shift"));
    renderBridgeMosaic();
    scheduleNextShine();
}

void AppBanner::keyPressEvent(QKeyEvent *event)
{
    if ((event->key() == Qt::Key_Return ||
         event->key() == Qt::Key_Enter ||
         event->key() == Qt::Key_Space) &&
        clicked) {
        clicked();
        event->accept();
        return;
    }
    QWidget::keyPressEvent(event);
}

void AppBanner::mousePressEvent(QMouseEvent *event)
{ pressed_ = event->button() == Qt::LeftButton; }

void AppBanner::mouseReleaseEvent(QMouseEvent *event)
{
    const bool activate = pressed_ && event->button() == Qt::LeftButton && rect().contains(event->pos());
    pressed_ = false;
    if (activate && clicked) clicked();
}

void AppBanner::scheduleNextShine()
{
    shineStep_ = -1;
    const int delayMs = 18000 +
        static_cast<int>(QRandomGenerator::global()->bounded(18001u));
    shineTimer_.start(delayMs, Qt::VeryCoarseTimer, this);
}

void AppBanner::timerEvent(QTimerEvent *event)
{
    if (event->timerId() != shineTimer_.timerId()) {
        QWidget::timerEvent(event);
        return;
    }
    if (shineStep_ < 0) {
        shineStep_ = 0;
        shineTimer_.start(32, Qt::PreciseTimer, this);
    } else if (++shineStep_ >= kShineSteps) {
        update();
        scheduleNextShine();
        return;
    }
    update();
}

void AppBanner::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.drawImage(rect(), mosaicImage_);
    const QColor borderColor(QStringLiteral(MSH_BORDER_SOFT));
    const bool compact = width() < 520;
    const QString slogan = QStringLiteral("Two sides. One reality");
    QFont sloganFont(UiTheme::uiFamily(), compact ? 8 : 9, QFont::Bold);
    sloganFont.setLetterSpacing(QFont::AbsoluteSpacing, 0.3);
    const QFontMetricsF sloganMetrics(sloganFont);
    const QRectF sloganGlyphs = sloganMetrics.tightBoundingRect(slogan);
    const qreal sloganHeight = qMax<qreal>(1.0, sloganGlyphs.height());
    const qreal aboveCaps = sloganMetrics.ascent() + sloganGlyphs.top();
    const qreal sloganGap = 0.0;
    // Transparent padding in assets/pc-client/mirrorshift-wordmark.png.
    constexpr qreal kWordmarkPadY = 18.0 / 252.0;
    constexpr qreal kWordmarkPadX = 18.0 / 1734.0;

    painter.setPen(borderColor);
    painter.drawLine(0, height() - 1, width(), height() - 1);
    const qreal logoLeft = compact ? 14.0 : 18.0;
    const qreal maxLogoWidth = qMax<qreal>(110.0, width() - logoLeft - 14.0);
    const qreal maxLogoHeight = compact ? 36.0 : 54.0;
    QRectF logoRect;
    qreal padX = 0.0;
    qreal padY = 0.0;
    if (!logoImage_.isNull()) {
        QSizeF logoSize = logoImage_.size();
        logoSize.scale(maxLogoWidth, maxLogoHeight, Qt::KeepAspectRatio);
        padX = logoSize.width() * kWordmarkPadX;
        padY = logoSize.height() * kWordmarkPadY;
        const qreal stackHeight =
            logoSize.height() - 2.0 * padY + sloganGap + sloganHeight;
        const qreal visualTop = (height() - stackHeight) / 2.0;
        logoRect = QRectF(logoLeft, visualTop - padY,
                          logoSize.width(), logoSize.height());
        painter.drawImage(logoRect, logoImage_);
    } else {
        QFont fallbackFont(UiTheme::uiFamily(), compact ? 19 : 26, QFont::Bold);
        fallbackFont.setLetterSpacing(QFont::AbsoluteSpacing, 1.6);
        const QFontMetricsF fallbackMetrics(fallbackFont);
        const QString fallbackTitle = QStringLiteral("MIRRORSHIFT");
        const QSizeF fallbackSize(
            qMin(maxLogoWidth, fallbackMetrics.horizontalAdvance(fallbackTitle)),
            qMin(maxLogoHeight, fallbackMetrics.height()));
        const qreal stackHeight = fallbackSize.height() + sloganGap + sloganHeight;
        logoRect = QRectF(logoLeft,
                          (height() - stackHeight) / 2.0,
                          fallbackSize.width(), fallbackSize.height());
        painter.setFont(fallbackFont);
        painter.setPen(Qt::white);
        painter.drawText(logoRect, Qt::AlignLeft | Qt::AlignVCenter,
                         fallbackTitle);
    }

    if (shineStep_ >= 0 && !logoImage_.isNull()) {
        const qreal bandWidth = compact ? 16.0 : 22.0;
        const qreal progress = static_cast<qreal>(shineStep_) /
            static_cast<qreal>(kShineSteps - 1);
        const qreal centre = logoRect.left() - bandWidth +
            (logoRect.width() + bandWidth * 2.0) * progress;
        QPainterPath clip;
        clip.moveTo(centre - bandWidth * 0.15, logoRect.top());
        clip.lineTo(centre + bandWidth * 0.85, logoRect.top());
        clip.lineTo(centre + bandWidth * 0.15, logoRect.bottom());
        clip.lineTo(centre - bandWidth * 0.85, logoRect.bottom());
        clip.closeSubpath();
        painter.save();
        painter.setClipPath(clip);
        painter.setCompositionMode(QPainter::CompositionMode_Screen);
        painter.setOpacity(0.72);
        painter.drawImage(logoRect, logoImage_);
        painter.restore();
    }

    const QRectF sloganRect(logoRect.left() + padX,
                            logoRect.bottom() - padY + sloganGap - aboveCaps,
                            qMax<qreal>(1.0, logoRect.width() - 2.0 * padX),
                            sloganHeight + 2.0);
    painter.setFont(sloganFont);
    painter.setPen(QColor(QStringLiteral(MSH_ACCENT)));
    painter.drawText(sloganRect, Qt::AlignRight | Qt::AlignTop, slogan);
}

void AppBanner::renderBridgeMosaic()
{
    static const MosaicPixel bridgeMosaic[] = {
        {547,36,9,255,216,0,225},{481,19,6,216,0,0,139},{574,41,1,0,200,0,250},{574,38,8,0,180,220,250},
        {537,36,1,0,200,0,212},{576,-8,18,255,216,0,247},{564,45,7,255,216,0,247},{456,52,4,255,216,0,107},
        {410,-3,2,216,0,0,47},{569,29,4,0,180,220,254},{526,-6,16,255,216,0,198},{514,-5,2,0,180,220,182},
        {602,42,3,0,180,220,213},{562,4,6,0,200,0,245},{626,57,9,0,200,0,182},{401,71,6,0,200,0,45},
        {517,8,1,216,0,0,186},{610,76,3,216,0,0,203},{543,2,7,255,216,0,220},{492,73,1,216,0,0,154},
        {367,70,15,255,216,0,45},{492,44,18,255,216,0,154},{431,37,7,0,200,0,74},{580,47,7,0,180,220,242},
        {563,73,7,0,180,220,246},{520,35,3,255,216,0,190},{488,59,3,216,0,0,148},{594,32,7,0,200,0,224},
        {629,22,4,0,180,220,178},{507,44,14,255,216,0,173},{594,18,19,216,0,0,224},{597,27,13,255,216,0,220},
        {535,36,4,0,200,0,210},{523,44,8,216,0,0,194},{582,17,4,0,200,0,239},{523,1,2,216,0,0,194},
        {522,48,6,255,216,0,193},{458,8,11,0,200,0,109},{453,48,9,255,216,0,103},{492,61,6,0,180,220,154},
        {465,77,14,255,216,0,118},{456,6,4,0,180,220,107},{468,18,10,216,0,0,122},{602,72,7,0,180,220,213},
        {520,47,9,216,0,0,190},{605,40,4,255,216,0,210},{578,63,8,0,180,220,245},{489,59,13,0,200,0,150},
        {596,22,18,0,180,220,221},{612,61,3,0,180,220,200},{584,8,18,0,180,220,237},{502,70,3,255,216,0,167},
        {585,25,14,0,180,220,236},{496,48,1,0,200,0,159},{444,40,9,0,180,220,91},{474,45,1,0,180,220,130},
        {608,67,4,255,216,0,206},{579,67,16,0,180,220,243},{565,68,14,0,200,0,248},{514,31,6,255,216,0,182},
        {435,32,2,0,180,220,80},{435,62,5,255,216,0,80},{553,43,16,0,180,220,233},{492,33,2,255,216,0,154},
        {528,15,4,216,0,0,200},{548,23,2,216,0,0,226},{588,-5,12,0,180,220,232},{588,-5,19,216,0,0,232},
        {524,45,2,0,200,0,195},{595,40,6,216,0,0,222},{614,17,3,0,180,220,198},{573,68,15,255,216,0,251},
        {547,34,3,255,216,0,225},{542,-7,6,255,216,0,219},{476,75,6,0,200,0,133},{582,-3,2,255,216,0,239},
        {537,19,10,216,0,0,212},{574,-8,9,0,200,0,250},{549,42,13,216,0,0,228},{483,73,9,216,0,0,142},
        {515,38,17,255,216,0,184},{517,6,16,216,0,0,186},{507,73,6,216,0,0,173},{601,60,19,0,200,0,215},
        {574,12,4,0,180,220,250},{556,70,18,216,0,0,237},{576,2,10,216,0,0,247},{459,1,4,255,216,0,111},
        {546,77,12,0,180,220,224},{340,44,5,255,216,0,45},{591,37,17,216,0,0,228},{543,56,15,0,180,220,220},
        {533,10,8,0,200,0,207},{529,45,3,255,216,0,202},{457,65,12,216,0,0,108},{576,3,11,216,0,0,247},
        {578,5,17,255,216,0,245},{531,30,4,255,216,0,204},{511,16,5,216,0,0,178},{585,35,7,255,216,0,236},
        {435,-1,3,0,180,220,80},{436,4,1,255,216,0,81},{578,29,11,0,180,220,245},{570,48,2,255,216,0,255},
        {560,71,10,255,216,0,242},{455,65,3,216,0,0,106},{567,34,8,0,180,220,251},{600,12,7,0,200,0,216},
        {568,4,6,216,0,0,252},{451,-2,17,216,0,0,100},{492,40,2,0,200,0,154},{560,13,4,0,200,0,242},
        {600,12,9,216,0,0,216},{523,48,7,0,180,220,194},{371,68,2,255,216,0,45},{595,-6,4,0,200,0,222},
        {487,35,4,255,216,0,147},{469,29,16,255,216,0,124},{500,5,18,255,216,0,164},{488,58,12,0,180,220,148},
        {616,61,8,216,0,0,195},{582,57,6,0,180,220,239},{496,-8,12,0,180,220,159},{564,43,7,255,216,0,247},
        {620,77,4,255,216,0,190},{525,71,7,216,0,0,196},{592,-6,15,0,200,0,226},{455,31,9,0,200,0,106},
        {595,67,4,0,200,0,222},{602,5,8,0,200,0,213},{551,45,2,216,0,0,230},{523,56,15,255,216,0,194},
        {568,12,13,0,200,0,252},{397,45,9,255,216,0,45},{508,18,10,0,200,0,174},{627,50,17,0,180,220,181},
        {505,27,7,0,200,0,170},{511,1,1,255,216,0,178},{426,36,2,216,0,0,68},{608,-4,9,0,180,220,206},
        {558,61,13,255,216,0,239},{617,0,2,216,0,0,194},{594,30,13,216,0,0,224},{584,66,10,216,0,0,237},
        {555,13,9,0,180,220,236},{561,19,4,0,180,220,243},{542,34,7,216,0,0,219},{438,24,3,216,0,0,83},
        {626,36,4,0,200,0,182},{369,32,2,0,180,220,45},{607,54,6,0,180,220,207},{489,25,6,255,216,0,150},
        {621,27,1,0,180,220,189},{560,53,8,0,200,0,242},{428,3,4,0,180,220,70},{557,74,2,0,180,220,238},
        {570,55,5,216,0,0,255},{498,-8,4,255,216,0,161},{629,25,15,0,200,0,178},{477,69,4,216,0,0,134},
        {617,36,3,0,180,220,194},{582,4,4,0,180,220,239},{538,57,8,216,0,0,213},{367,18,1,255,216,0,45},
        {621,43,8,216,0,0,189},{448,16,12,0,180,220,96},{495,5,8,216,0,0,158},{377,40,13,0,180,220,45},
        {533,2,19,0,200,0,207},{563,60,7,0,180,220,246},{504,2,2,216,0,0,169},{546,34,6,255,216,0,224},
        {595,-7,11,255,216,0,222},{488,74,12,216,0,0,148},{575,41,17,255,216,0,248},{565,47,2,0,180,220,248},
        {470,29,7,0,200,0,125},{518,44,9,0,200,0,187},{586,4,3,0,180,220,234},{358,23,14,255,216,0,45},
        {414,40,16,216,0,0,52},{554,0,15,255,216,0,234},{516,3,5,216,0,0,185},{512,10,9,255,216,0,180},
        {490,27,7,255,216,0,151},{621,-2,12,216,0,0,189},{363,-8,7,216,0,0,45},{599,49,5,216,0,0,217},
        {557,10,6,0,200,0,238},{624,41,8,216,0,0,185},{626,-6,1,216,0,0,182},{555,46,16,0,180,220,236},
        {563,10,17,216,0,0,246},{503,49,5,0,200,0,168},{505,60,6,255,216,0,170},{546,14,4,0,180,220,224},
        {476,10,4,0,200,0,133},{500,22,4,216,0,0,164},{564,6,11,0,180,220,247},{466,52,16,216,0,0,120},
        {587,60,15,0,200,0,233},{584,4,17,0,180,220,237},{515,-5,3,216,0,0,184},{476,43,3,0,200,0,133},
        {546,69,1,255,216,0,224},{549,74,7,255,216,0,228},{420,63,12,0,200,0,60},{576,60,8,255,216,0,247},
        {354,60,13,216,0,0,45},{533,39,10,0,180,220,207},
    };
    QPainter painter(&mosaicImage_);
    painter.setRenderHint(QPainter::Antialiasing, true);
    QLinearGradient bg(QPointF(0, 0), QPointF(620, 0));
    bg.setColorAt(0.0, QColor(22, 22, 30));
    bg.setColorAt(1.0, QColor(32, 32, 42));
    painter.fillRect(0, 0, 620, 70, bg);
    static const QColor mirrorPalette[] = {
        QColor(0, 0, 0, 244),
        QColor(1, 5, 18, 242),
        QColor(2, 14, 42, 240),
        QColor(4, 31, 78, 236),
        QColor(5, 50, 116, 232),
        QColor(0, 77, 153, 228),
        QColor(5, 111, 184, 224),
        QColor(10, 148, 211, 220),
        QColor(21, 188, 231, 214),
        QColor(91, 211, 244, 206),
        QColor(18, 53, 103, 232),
        QColor(26, 93, 151, 224),
    };
    constexpr int mirrorPaletteSize =
        sizeof(mirrorPalette) / sizeof(mirrorPalette[0]);

    const auto drawSquare = [&](const MosaicPixel &pixel) {
        constexpr int mosaicLeft = 390;
        if (pixel.x < mosaicLeft || pixel.x >= 620 ||
            pixel.y + pixel.size < 0 || pixel.y >= 70 || pixel.a == 0)
            return;
        const unsigned seed = static_cast<unsigned>(
            pixel.x * 17 + pixel.y * 13 + pixel.size * 7 +
            pixel.r * 3 + pixel.g * 5 + pixel.b * 11);
        const int distance = qBound(0, pixel.x - mosaicLeft,
                                    620 - mosaicLeft);
        const unsigned density = static_cast<unsigned>(
            (distance * 255) / (620 - mosaicLeft));
        if (((seed >> 8) & 0xffu) > density)
            return;
        const int paletteIndex = static_cast<int>(seed % mirrorPaletteSize);
        QColor fragment = mirrorPalette[paletteIndex];
        fragment.setAlpha(
            (fragment.alpha() * qBound(0, pixel.a, 255) + 127) / 255);

        painter.fillRect(pixel.x, pixel.y, pixel.size, pixel.size, fragment);
        if (pixel.size >= 4 && fragment.alpha() >= 90) {
            painter.setBrush(Qt::NoBrush);
            painter.setPen(QPen(QColor(91, 183, 218,
                                       qMin(82, fragment.alpha() / 3)), 0.5));
            painter.drawRect(pixel.x, pixel.y, pixel.size, pixel.size);
        }
        if (pixel.size >= 8 && fragment.alpha() >= 130) {
            painter.setPen(QColor(173, 231, 247,
                                  qMin(58, fragment.alpha() / 4)));
            painter.drawLine(pixel.x + 1, pixel.y + 1,
                             pixel.x + pixel.size - 2,
                             pixel.y + pixel.size - 2);
        }
    };

    for (const MosaicPixel &pixel : bridgeMosaic)
        drawSquare(pixel);

    static const MosaicPixel extraMirrorMosaic[] = {
        {500, -2, 4, 12, 31, 74, 175}, {518, 1, 6, 37, 82, 126, 212},
        {537, -1, 3, 64, 121, 166, 194}, {555, 3, 7, 8, 42, 91, 226},
        {575, -3, 5, 23, 68, 118, 188}, {594, 1, 8, 74, 142, 184, 235},
        {611, -2, 4, 17, 49, 94, 204}, {507, 8, 7, 48, 108, 153, 226},
        {526, 10, 3, 6, 28, 61, 183}, {545, 9, 5, 86, 154, 196, 238},
        {564, 12, 8, 15, 57, 104, 222}, {584, 8, 4, 38, 92, 141, 201},
        {603, 11, 6, 5, 24, 55, 190}, {518, 19, 5, 70, 136, 181, 220},
        {536, 20, 8, 12, 47, 89, 230}, {556, 18, 3, 51, 111, 156, 177},
        {575, 21, 6, 4, 20, 46, 209}, {594, 19, 5, 95, 167, 204, 234},
        {613, 22, 7, 19, 62, 111, 218}, {502, 29, 6, 43, 102, 149, 198},
        {523, 31, 3, 7, 35, 74, 184}, {542, 28, 7, 63, 126, 170, 229},
        {562, 32, 4, 21, 70, 120, 193}, {580, 29, 8, 5, 25, 59, 222},
        {601, 31, 4, 80, 149, 191, 203}, {616, 34, 5, 29, 79, 128, 217},
        {509, 41, 8, 9, 38, 82, 225}, {530, 40, 4, 60, 119, 162, 190},
        {548, 43, 6, 16, 54, 101, 232}, {568, 40, 3, 87, 157, 198, 205},
        {586, 44, 7, 3, 18, 43, 218}, {606, 42, 5, 46, 106, 151, 228},
        {520, 53, 4, 75, 143, 185, 207}, {539, 51, 7, 11, 45, 88, 231},
        {558, 55, 5, 34, 85, 132, 187}, {578, 52, 8, 6, 31, 68, 223},
        {597, 55, 3, 89, 160, 201, 215}, {613, 52, 6, 20, 65, 113, 202},
        {504, 64, 5, 51, 113, 160, 192}, {526, 62, 7, 14, 50, 96, 221},
        {546, 65, 4, 68, 132, 177, 236}, {564, 61, 6, 2, 21, 52, 211},
        {584, 65, 5, 43, 96, 143, 225}, {603, 62, 8, 9, 42, 83, 198},
    };
    for (const MosaicPixel &pixel : extraMirrorMosaic)
        drawSquare(pixel);

    // A deterministic column gradient fills the far right while tapering to
    // almost nothing near the first fragment.  The wordmark and slogan sit on
    // the left over the dark ground, not over this field.
    for (int column = 0; column < 16; ++column) {
        const int x = 392 + column * 15 + ((column * 7) % 5);
        for (int row = 0; row < 8; ++row) {
            const int y = row * 10 + ((column * 5 + row * 3) % 7) - 3;
            const int size = 2 + ((column * 3 + row * 5) % 6);
            const int tint = (column * 37 + row * 53) & 0xff;
            const int alpha = 168 + ((column * 13 + row * 17) % 88);
            const MosaicPixel square = {
                x, y, size, tint, (tint * 3) & 0xff, (tint * 5) & 0xff, alpha};
            drawSquare(square);
        }
    }

    painter.setBrush(Qt::NoBrush);
    painter.setPen(Qt::NoPen);
}
