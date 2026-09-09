#ifndef APP_BANNER_H
#define APP_BANNER_H

#include <QBasicTimer>
#include <QImage>
#include <QWidget>
#include <functional>

class QKeyEvent;
class QTimerEvent;

class AppBanner : public QWidget {
    Q_DISABLE_COPY_MOVE(AppBanner)
public:
    explicit AppBanner(QWidget *parent = nullptr);

    std::function<void()> clicked;

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void timerEvent(QTimerEvent *event) override;

private:
    void paintEvent(QPaintEvent *) override;
    void renderBridgeMosaic();
    void scheduleNextShine();

    struct MosaicPixel {
        int x, y, size, r, g, b, a;
    };

    QImage mosaicImage_;
    QImage logoImage_;
    QBasicTimer shineTimer_;
    int shineStep_ = -1;
    bool pressed_ = false;

    static constexpr int kShineSteps = 18;
};

#endif // APP_BANNER_H
