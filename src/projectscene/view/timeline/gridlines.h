/*
* Audacity: A Digital Audio Editor
*/

#include "qquickpainteditem.h"
#include "view/timeline/timelineruler.h"

#include "global/async/asyncable.h"
#include "context/iglobalcontext.h"

class QPainter;

namespace au::projectscene {
class GridLines : public QQuickPaintedItem, public muse::Contextable, public muse::async::Asyncable
{
    Q_OBJECT

    Q_PROPERTY(TimelineRuler * timelineRuler READ timelineRuler WRITE setTimelineRuler NOTIFY timelineRulerChanged FINAL)

    muse::GlobalInject<muse::ui::IUiConfiguration> uiconfiguration;
    muse::ContextInject<IProjectSceneUiState> projectSceneUiState { this };
    muse::ContextInject<context::IGlobalContext> globalContext { this };

public:
    explicit GridLines(QQuickItem* parent = nullptr);
    ~GridLines() = default;

    void paint(QPainter* painter) override;
    void componentComplete() override;

    TimelineRuler* timelineRuler() const;
    void setTimelineRuler(TimelineRuler* newTimelineRuler);

signals:
    void timelineRulerChanged();

private:
    void drawGridLines(QPainter* painter);
    //! GlideMix: the bars-and-beats grid (gridplan.h).
    void drawMusicalGrid(QPainter* painter);
    void drawZebraHighlighting(QPainter* painter);
    void highlightHalfSection(QPainter* painter, qreal h, qreal w, const QColor& color, bool firstHalf, double nextTick,
                              const IntervalInfo& intervalInfo);

    TimelineRuler* m_timelineRuler = nullptr;
    Ticks m_ticks = {};
};
}
