#pragma once

#include <QtGui/QColor>
#include <QtGui/QFont>
#include <QtQuick/QQuickPaintedItem>

#include "modularity/ioc.h"
#include "ui/iuiconfiguration.h"
#include "async/asyncable.h"

#include "iprojectsceneuistate.h"

#include "timeformat.h"
#include "beatsmeasuresformat.h"
#include "timelinecontext.h"
#include "context/iglobalcontext.h"

class QPainter;

namespace au::projectscene {
struct TickInfo {
    double x = 0.0;
    QString tickLabel;
    TickType tickType = TickType::MINORMINOR;
    QLineF line;
    double timeValue;
};

using Ticks = QVector<TickInfo>;

class TimelineRuler : public QQuickPaintedItem, public muse::async::Asyncable, public muse::Contextable
{
    Q_OBJECT

    Q_PROPERTY(TimelineContext * context READ timelineContext WRITE setTimelineContext NOTIFY timelineContextChanged FINAL)

    muse::GlobalInject<muse::ui::IUiConfiguration> uiconfiguration;
    muse::ContextInject<IProjectSceneUiState> projectSceneUiState { this };
    muse::ContextInject<context::IGlobalContext> globalContext { this };

signals:
    void offsetChanged();
    void zoomChanged();

public:
    explicit TimelineRuler(QQuickItem* parent = nullptr);
    ~TimelineRuler() = default;

    Q_INVOKABLE bool isMajorSection(int y) const;

    IntervalInfo intervalInfo();

    void setFormatter(const TimelineRulerMode mode);

    void componentComplete() override;
    void paint(QPainter* painter) override;

    TimelineContext* timelineContext() const;
    void setTimelineContext(TimelineContext* newContext);

signals:
    void timelineContextChanged();
    void ticksChanged(Ticks ticks);
    void formatterChanged();

private:
    Ticks prepareTickData(const IntervalInfo& timeInterval, double w, double h);
    TickType determineTickType(double value, const IntervalInfo& timeInterval);
    void drawLabels(QPainter* painter, const Ticks& ticks, double w, double h);
    void drawTicks(QPainter* painter, const Ticks& ticks);
    //! GlideMix: the bars-and-beats ruler on the grid plan (gridplan.h).
    void drawMusicalRuler(QPainter* painter, double w, double h);
    //! InTheMix: beats counted from each song's start, in the ruler's lower half.
    void drawSongBeatRuler(QPainter* painter, double w, double h, double beatSec,
                           double t0, double t1, double zoom, double dpr);

    TimelineContext* m_context = nullptr;
    std::unique_ptr<IRulerFormat> m_formatter;
};
}
