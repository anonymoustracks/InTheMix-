/*
 * GlideMix: logs when the main (UI) thread was blocked.
 */
#include "mixhousestallwatchdog.h"

#include "log.h"

using namespace au::mixhouse;

namespace {
constexpr int kTickMs = 100;
constexpr qint64 kReportMs = 300;
// Longer gaps are the Mac asleep or the app suspended, not a stall.
constexpr qint64 kIgnoreMs = 60000;
}

MixHouseStallWatchdog::MixHouseStallWatchdog(QObject* parent)
    : QObject(parent)
{
    m_timer.setTimerType(Qt::PreciseTimer);
    m_timer.setInterval(kTickMs);
    QObject::connect(&m_timer, &QTimer::timeout, this, [this]() { onTick(); });
}

void MixHouseStallWatchdog::start()
{
    m_clock.start();
    m_last = 0;
    m_timer.start();
}

void MixHouseStallWatchdog::onTick()
{
    const qint64 now = m_clock.elapsed();
    const qint64 late = now - m_last - kTickMs;
    m_last = now;
    if (late >= kReportMs && late < kIgnoreMs) {
        LOGW() << "GlideMix stall: the UI thread was busy for " << late << " ms (see the lines above for what ran)";
    }
}
