/*
 * GlideMix: logs when the main (UI) thread was blocked.
 */
#pragma once

#include <QElapsedTimer>
#include <QObject>
#include <QTimer>

namespace au::mixhouse {
//! A timer on the UI thread that ticks every 100 ms. When a tick arrives more
//! than 300 ms late, the thread was busy that long, and one log line says so.
//! The log lines just before it show what was going on (Marc, 2026-11-19:
//! "log when the GUI thread is busy > ~300 ms").
class MixHouseStallWatchdog : public QObject
{
public:
    explicit MixHouseStallWatchdog(QObject* parent = nullptr);
    void start();

private:
    void onTick();

    QTimer m_timer;
    QElapsedTimer m_clock;
    qint64 m_last = 0;
};
}
