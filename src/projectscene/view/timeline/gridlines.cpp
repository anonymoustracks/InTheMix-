/*
* Audacity: A Digital Audio Editor
*/

#include <QPainter>

#include "gridlines.h"
#include "gridplan.h"

#include <QQuickWindow>

#include <cmath>

#include "project/iaudacityproject.h"
#include "projectscene/iprojectviewstate.h"

using namespace au::projectscene;

GridLines::GridLines(QQuickItem* parent)
    : QQuickPaintedItem(parent), muse::Contextable(muse::iocCtxForQmlObject(this))
{
}

void GridLines::componentComplete()
{
    QQuickPaintedItem::componentComplete();

    // GlideMix: showing or hiding the grid has to repaint it, and nothing
    // else about the view changes. Subscribed here rather than in the
    // constructor (no ioc context yet) or in paint() (the render thread).
    if (const auto uiState = projectSceneUiState()) {
        uiState->gridLinesVisibleChanged().onNotify(this, [this]() {
            update();
        });
        // The canvas colour follows the waveform style unless it is set.
        uiState->laneBackgroundChanged().onNotify(this, [this]() {
            update();
        });
    }
    // InTheMix: a new grid size (or Snap on / off) redraws the lines.
    const auto watchSnap = [this]() {
        const auto project = globalContext() ? globalContext()->currentProject() : nullptr;
        if (const auto vs = project ? project->viewState() : nullptr) {
            vs->snap().ch.onReceive(this, [this](const Snap&) {
                update();
            });
        }
        update();
    };
    watchSnap();
    if (globalContext()) {
        globalContext()->currentProjectChanged().onNotify(this, watchSnap);
    }
}

void GridLines::paint(QPainter* painter)
{
    // set background
    QRectF rect = boundingRect();
    QColor canvasColor = uiconfiguration()->currentTheme().values.value(muse::ui::BACKGROUND_QUARTERNARY_COLOR).toString();
    // GlideMix: rekordbox's colour waveform is drawn on black, and the
    // theme's grey around it made the lanes look like windows onto a different
    // app. The whole canvas goes black with it.
    if (projectSceneUiState() && projectSceneUiState()->blackCanvas()) {
        canvasColor = QColor(0x08, 0x08, 0x0A);
    }
    // GlideMix (Chris, 2026-11-15): the workspace can also be Charcoal or
    // Grey, whatever the theme -- the panel colours of those two themes.
    if (projectSceneUiState()) {
        if (projectSceneUiState()->laneBackground() == 3) {
            canvasColor = QColor(0x1A, 0x1C, 0x1F);
        } else if (projectSceneUiState()->laneBackground() == 4) {
            canvasColor = QColor(0x2F, 0x35, 0x3B);
        }
        // GlideMix (2026-11-21): the reference DAW's lane colour, rows and empty space alike.
        if (projectSceneUiState()->studioCanvas()) {
            canvasColor = QColor(0x13, 0x14, 0x17);
        }
    }
    painter->fillRect(rect, canvasColor);

    // draw left border
    const qreal h = height();
    QPen pen = painter->pen();
    pen.setWidth(2);
    QColor beatStrokeColor = uiconfiguration()->currentTheme().values.value(muse::ui::WHITE_COLOR).toString();
    beatStrokeColor.setAlpha(static_cast<int>(0.1 * 256));
    pen.setColor(beatStrokeColor);
    painter->setPen(pen);
    painter->drawLine(QLineF(0, 0, 0, h));

    // GlideMix: a workspace can turn the grid off (the rekordbox look);
    // the canvas and its left border stay.
    if (!projectSceneUiState()->gridLinesVisible()) {
        return;
    }

    // GlideMix (2026-11-21): bars and beats get one grid rule for lanes and clips (gridplan.h);
    // the seconds ruler keeps its ticks.
    if (projectSceneUiState()->timelineRulerMode() == TimelineRulerMode::BEATS_AND_MEASURES) {
        drawMusicalGrid(painter);
        return;
    }
    drawGridLines(painter);
}

void GridLines::drawMusicalGrid(QPainter* painter)
{
    TimelineContext* ctx = m_timelineRuler ? m_timelineRuler->timelineContext() : nullptr;
    if (!ctx || ctx->zoom() <= 0.0) {
        return;
    }
    double tempo = ctx->BPM();
    int upper = ctx->timeSigUpper();
    int lower = ctx->timeSigLower();
    if (const auto prj = globalContext() ? globalContext()->currentTrackeditProject() : nullptr) {
        const auto ts = prj->timeSignature();
        if (ts.tempo > 0.0 && ts.upper > 0 && ts.lower > 0) {
            tempo = ts.tempo;   // the exact tempo (the context rounds it to a whole BPM)
            upper = ts.upper;
            lower = ts.lower;
        }
    }
    if (tempo <= 0.0 || upper <= 0 || lower <= 0) {
        return;
    }
    const double beatSec = 60.0 / tempo * (4.0 / lower);
    GridPlan plan = makeGridPlan(ctx->zoom(), beatSec, upper, ctx->barOneTime());
    // InTheMix (23/11, "the grid does not change"): the lines follow the Grid
    // menu's size while Snap is on, as Ableton's do -- 1 Bar shows bars only,
    // 1/2 half bars, 1/4 beats, 1/8 ... finer (when they are 6 px apart).
    if (const auto project = globalContext() ? globalContext()->currentProject() : nullptr) {
        if (const auto vs = project->viewState()) {
            const Snap snap = vs->getSnap();
            const double quarter = 60.0 / tempo;
            const double bar = beatSec * upper;
            double unit = 0.0;
            switch (snap.type) {
            case SnapType::Bar: unit = bar; break;
            case SnapType::Half: unit = quarter * 2.0; break;
            case SnapType::Quarter: unit = quarter; break;
            case SnapType::Eighth: unit = quarter / 2.0; break;
            case SnapType::Sixteenth: unit = quarter / 4.0; break;
            case SnapType::ThirtySecond: unit = quarter / 8.0; break;
            case SnapType::Beat: unit = beatSec; break;
            default: break;
            }
            if (snap.enabled && !snap.isSnapTriplets && unit > 0.0 && std::abs(plan.majorSec - bar) < 1e-9) {
                if (unit >= bar - 1e-9) {
                    plan.midSec = 0.0;
                    plan.fineSec = 0.0;
                } else if (unit >= beatSec - 1e-9) {
                    plan.midSec = unit * ctx->zoom() >= 6.0 ? unit : 0.0;
                    plan.fineSec = 0.0;
                } else {
                    plan.midSec = beatSec;
                    plan.fineSec = unit * ctx->zoom() >= 6.0 ? unit : 0.0;
                }
            }
        }
    }
    const double zoom = ctx->zoom();
    const double t0 = ctx->frameStartTime();
    const double t1 = t0 + width() / zoom;
    const qreal h = height();
    const double dpr = window() ? window()->devicePixelRatio() : 1.0;

    // Shaded blocks: a light lift of the lane, every other musical block.
    QColor shade(255, 255, 255);
    shade.setAlphaF(0.012);
    forEachShadedBlock(plan, t0, t1, [&](double s, double e) {
        const double x0 = snapToDevicePixel((s - t0) * zoom, dpr);
        const double x1 = snapToDevicePixel((e - t0) * zoom, dpr);
        if (x1 > x0) {
            painter->fillRect(QRectF(x0, 0, x1 - x0, h), shade);
        }
    });

    // Lines: bar (or bar group), beat (or half bar), finer -- one point wide, on whole device pixels.
    QColor ink[3] = { QColor(255, 255, 255), QColor(255, 255, 255), QColor(255, 255, 255) };
    // InTheMix: brighter, so the grid chosen can be seen (was 0.06 / 0.022 / 0.013).
    ink[0].setAlphaF(0.13);
    ink[1].setAlphaF(0.065);
    ink[2].setAlphaF(0.035);
    forEachGridLine(plan, t0, t1, [&](double t, int level) {
        const double x = snapToDevicePixel((t - t0) * zoom, dpr);
        painter->fillRect(QRectF(x, 0, 1.0, h), ink[level]);
    });
}

TimelineRuler* GridLines::timelineRuler() const
{
    return m_timelineRuler;
}

void GridLines::setTimelineRuler(TimelineRuler* newTimelineRuler)
{
    if (m_timelineRuler == newTimelineRuler) {
        return;
    }

    if (m_timelineRuler) {
        disconnect(m_timelineRuler, nullptr, this, nullptr);
    }

    m_timelineRuler = newTimelineRuler;

    if (m_timelineRuler) {
        auto updateView = [this] (Ticks ticks) {
            m_ticks = ticks;
            update();
        };

        connect(m_timelineRuler, &TimelineRuler::ticksChanged, this, updateView);
    }
}

void GridLines::drawGridLines(QPainter* painter)
{
    const qreal h = height();
    QColor beatStrokeColor = uiconfiguration()->currentTheme().values.value(muse::ui::WHITE_COLOR).toString();
    beatStrokeColor.setAlpha(static_cast<int>(0.06 * 256));
    QColor majorBeatStrokeColor = uiconfiguration()->currentTheme().values.value(muse::ui::WHITE_COLOR).toString();
    majorBeatStrokeColor.setAlpha(static_cast<int>(0.1 * 256));

    QPen pen = painter->pen();
    pen.setWidth(1);

    for (const auto& tick : m_ticks) {
        double x = tick.line.p1().x();

        if (tick.tickType == TickType::MAJOR) {
            pen.setColor(majorBeatStrokeColor);
        } else {
            pen.setColor(beatStrokeColor);
        }
        painter->setPen(pen);
        painter->drawLine(QLineF(x, 0, x, h));
    }
}

void GridLines::drawZebraHighlighting(QPainter* painter)
{
    if (m_ticks.isEmpty()) {
        return;
    }

    const qreal h = height();
    const qreal w = width();
    QColor highlightColor = uiconfiguration()->currentTheme().values.value(muse::ui::WHITE_COLOR).toString();
    highlightColor.setAlpha(static_cast<int>(0.02 * 256));

    int BPM = m_timelineRuler->timelineContext()->BPM();
    double SPB = static_cast<double>(60) / BPM;
    IntervalInfo intervalInfo = m_timelineRuler->intervalInfo();

    // highlight only when displaying at zoom between 1/4 and 1/32 ticks
    if (intervalInfo.minorMinor >= 2 * SPB || intervalInfo.minorMinor <= (SPB / 16)) {
        return;
    }

    int highlightStart = 0;
    int highlightEnd = 0;
    bool pendingDraw = false;
    const double tolerance = 0.01; // Tolerance for floating-point comparison

    for (auto i = 0; i < m_ticks.size(); ++i) {
        auto tick = m_ticks.at(i);
        double spbRemainder = std::abs(std::remainder(tick.timeValue, SPB));
        double doubledSpbRemainder = std::abs(std::remainder(tick.timeValue, SPB * 2));
        if (spbRemainder < tolerance && doubledSpbRemainder < tolerance) {
            highlightStart = tick.line.p1().x();
            pendingDraw = true;
        } else if (spbRemainder < tolerance && doubledSpbRemainder >= tolerance) {
            highlightEnd = tick.line.p1().x();
            QRectF rect = QRectF(QPointF(highlightStart, 0), QPointF(highlightEnd, h));
            painter->fillRect(rect, highlightColor);
            pendingDraw = false;
        }
    }

    if (pendingDraw) {
        QRectF rect = QRectF(QPointF(highlightStart, 0), QPointF(w, h));
        painter->fillRect(rect, highlightColor);
    }
}
