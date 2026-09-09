#include "pc/client/desktop_session_controller.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>

#include <cstdio>

static int failures;

static void check(bool ok, const char *label)
{
    if (!ok) {
        std::printf("FAIL: %s\n", label);
        ++failures;
    }
}

static bool runTimerRearmAndReinitCase()
{
    DesktopSessionController controller;
    DesktopSessionController::Callbacks callbacks;
    int sends = 0;

    callbacks.send = [&](DesktopSessionController::Mode mode,
                         const SessionAction &, const QByteArray &) {
        if (mode == DesktopSessionController::Mode::Direct) {
            ++sends;
        }
        return true;
    };
    controller.setCallbacks(callbacks);
    if (!controller.initializeDirect(SESSION_ROLE_HOST, SESSION_COLOR_WHITE) ||
        !controller.linkUp(1u) || sends != 1) {
        return false;
    }

    QElapsedTimer wait;
    wait.start();
    while (sends < 2 && wait.elapsed() < 4000) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
    }
    if (sends != 2 ||
        !controller.initializeMqtt(SESSION_ROLE_GUEST,
                                   SESSION_COLOR_UNKNOWN, 0u)) {
        return false;
    }

    wait.restart();
    while (wait.elapsed() < 3200) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
    }
    if (sends != 2) {
        return false;
    }
    return controller.initializeDirect(SESSION_ROLE_HOST, SESSION_COLOR_WHITE) &&
           controller.linkUp(1u) && sends == 3;
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    DesktopSessionController controller;
    DesktopSessionController::Callbacks callbacks;
    int sends = 0;
    int sideChanges = 0;
    int sessionChanges = 0;
    uint8_t lastSessionStatus = SESSION_CHANGED_READY;
    uint8_t lastSessionReason = SESSION_END_REASON_NONE;
    bool mqttTransportReady = true;
    DesktopSessionController::Mode lastMode =
        DesktopSessionController::Mode::Mqtt;

    callbacks.send = [&](DesktopSessionController::Mode mode,
                         const SessionAction &, const QByteArray &payload) {
        lastMode = mode;
        ++sends;
        return !payload.isEmpty();
    };
    callbacks.mqttTransportReady = [&]() {
        return mqttTransportReady;
    };
    callbacks.sideChanged = [&](DesktopSessionController::Mode mode,
                                uint8_t, uint16_t) {
        if (mode == DesktopSessionController::Mode::Mqtt) {
            ++sideChanges;
            mqttTransportReady = false;
        }
    };
    callbacks.error = [&](const QString &) {
        ++failures;
    };
    callbacks.sessionChanged = [&](uint8_t status, uint8_t reason) {
        ++sessionChanges;
        lastSessionStatus = status;
        lastSessionReason = reason;
    };
    controller.setCallbacks(callbacks);

    check(controller.initializeDirect(SESSION_ROLE_HOST, SESSION_COLOR_WHITE),
          "initialize direct");
    check(controller.linkUp(1u), "direct link produces actions");
    check(sends > 0 && lastMode == DesktopSessionController::Mode::Direct,
          "direct send callback");
    check(controller.linkDown(1u), "direct transport loss produces actions");
    check(sessionChanges == 1 &&
              lastSessionStatus == SESSION_CHANGED_ENDED &&
              lastSessionReason == SESSION_END_REASON_TRANSPORT_LOST,
          "direct transport loss forwards typed end reason");

    sends = 0;
    check(controller.initializeMqtt(SESSION_ROLE_GUEST,
                                    SESSION_COLOR_UNKNOWN, 0u),
          "initialize MQTT");
    check(!controller.linkUp(1u), "MQTT guest link-up has no send");
    check(controller.receiveMqtt(
              1u, QByteArray("netchesszx/v1/ROOM/meta"), false,
              QByteArray("H W 77")),
          "MQTT live host accepted");
    check(sideChanges == 1 && sends == 0,
          "side-dependent send deferred until transport ready");
    mqttTransportReady = true;
    check(controller.pump() && sends == 2,
          "deferred MQTT send resumes with original result flow");
    check(!controller.receiveMqtt(1u, QByteArray("invalid/topic"), false,
                                  QByteArray("payload")),
          "reject unknown MQTT route");

    DesktopSessionController interrupted;
    DesktopSessionController::Callbacks interruptedCallbacks;
    bool interruptedReady = true;
    bool dropOnSend = false;
    int interruptedSends = 0;
    interruptedCallbacks.mqttTransportReady = [&]() {
        return interruptedReady;
    };
    interruptedCallbacks.sideChanged =
        [&](DesktopSessionController::Mode, uint8_t, uint16_t) {
            interruptedReady = false;
        };
    interruptedCallbacks.send =
        [&](DesktopSessionController::Mode, const SessionAction &,
            const QByteArray &) {
            ++interruptedSends;
            if (dropOnSend) {
                interrupted.linkDown(1u);
            }
            return true;
        };
    interrupted.setCallbacks(interruptedCallbacks);
    check(interrupted.initializeMqtt(SESSION_ROLE_GUEST,
                                     SESSION_COLOR_UNKNOWN, 0u) &&
              !interrupted.linkUp(1u) &&
              interrupted.receiveMqtt(
                  1u, QByteArray("netchesszx/v1/ROOM/meta"), false,
                  QByteArray("H W 77")),
          "initialize interrupted MQTT batch");
    interruptedReady = true;
    dropOnSend = true;
    (void)interrupted.pump();
    const int sendsAfterDrop = interruptedSends;
    check(sendsAfterDrop == 1,
          "link-down interrupts the resumed MQTT send once");
    dropOnSend = false;
    check(!interrupted.pump() && interruptedSends == sendsAfterDrop,
          "invalidated MQTT batch and followups are not resumed");
    check(runTimerRearmAndReinitCase(),
          "timers rearm once and stop across session reinitialization");

    if (failures != 0) {
        return 1;
    }
    std::printf("desktop session controller tests ok\n");
    return 0;
}
