#include <QtCore/QString>
#include <QtCore/QVector>
#include <QtGui/QColor>
#include <QtGui/QPainter>

#include "timelineruler.h"
#include "gridplan.h"

#include <QtQuick/QQuickWindow>
#include <cmath>
#include <algorithm>
#include <vector>

namespace {
constexpr int MINORMINOR_TICK_HEIGHT_RATIO = 8;
constexpr int MINOR_TICK_HEIGHT_RATIO = 4;
constexpr int FONT_SIZE = 12;
constexpr double TICK_ALPHA_MAJOR = 1.0;
constexpr double TICK_ALPHA_MINOR = 0.5;
constexpr double LABEL_ALPHA_MAJOR = 1.0;
constexpr double LABEL_ALPHA_MINOR = 0.75;
constexpr int LABEL_OFFSET = 2;
}

using namespace au::projectscene;

TimelineRuler::TimelineRuler(QQuickItem* parent)
    : QQuickPaintedItem(parent), muse::Contextable(muse::iocCtxForQmlObject(this))
{
    uiconfiguration()->currentThemeChanged().onNotify(this, [this]() { update(); });
}

void TimelineRuler::componentComplete()
{
    QQuickPaintedItem::componentComplete();

    setFormatter(projectSceneUiState()->timelineRulerMode());

    projectSceneUiState()->timelineRulerModeChanged().onNotify(this, [this](){
        setFormatter(projectSceneUiState()->timelineRulerMode());
        update();
    });
}

bool TimelineRuler::isMajorSection(int y) const
{
    return y < (height() / 2);
}

IntervalInfo TimelineRuler::intervalInfo()
{
    return m_formatter->intervalInfo(m_context);
}

void TimelineRuler::setFormatter(const TimelineRulerMode mode)
{
    if (mode == TimelineRulerMode::MINUTES_AND_SECONDS) {
        m_formatter = std::make_unique<TimeFormat>();
    } else {
        m_formatter = std::make_unique<BeatsMeasuresFormat>();
    }
}

void TimelineRuler::paint(QPainter* painter)
{
    const qreal w = width();
    const qreal h = height();

    // determine current time interval and prepare ticks
    IntervalInfo interval = m_formatter->intervalInfo(m_context);
    Ticks ticks = prepareTickData(interval, w, h);

    // begin painting
    QPen pen = painter->pen();
    const QColor rulerStrokeColor = uiconfiguration()->currentTheme().extra.value("timeline_ruler_stroke_color").toString();

    pen.setWidth(2);
    pen.setColor(rulerStrokeColor);
    painter->setPen(pen);

    // vertical line (ruler border)
    painter->drawLine(QLineF(0, 0, 0, h));

    pen.setWidth(1);
    painter->setPen(pen);
    // horizontal line in the middle
    painter->drawLine(QLineF(0, h / 2, w, h / 2));

    // GlideMix (2026-11-21): bars and beats follow the grid plan -- numbers every 1 / 2 / 4 / 8 ...
    // bars so they stay apart, ticks on the grid's own pixels. The seconds ruler is unchanged.
    if (projectSceneUiState() && projectSceneUiState()->timelineRulerMode() == TimelineRulerMode::BEATS_AND_MEASURES
        && m_context && m_context->zoom() > 0.0) {
        drawMusicalRuler(painter, w, h);
    } else {
        drawTicks(painter, ticks);
        drawLabels(painter, ticks, w, h);
    }

    emit ticksChanged(ticks);
}

void TimelineRuler::drawMusicalRuler(QPainter* painter, double w, double h)
{
    double tempo = m_context->BPM();
    int upper = m_context->timeSigUpper();
    int lower = m_context->timeSigLower();
    if (const auto prj = globalContext() ? globalContext()->currentTrackeditProject() : nullptr) {
        const auto ts = prj->timeSignature();
        if (ts.tempo > 0.0 && ts.upper > 0 && ts.lower > 0) {
            tempo = ts.tempo;
            upper = ts.upper;
            lower = ts.lower;
        }
    }
    if (tempo <= 0.0 || upper <= 0 || lower <= 0) {
        return;
    }
    const double zoom = m_context->zoom();
    const double beatSec = 60.0 / tempo * (4.0 / lower);
    const double barSec = beatSec * upper;
    const double barOne = m_context->barOneTime();
    const GridPlan plan = makeGridPlan(zoom, beatSec, upper, barOne);
    const double t0 = m_context->frameStartTime();
    const double t1 = t0 + w / zoom;
    const double dpr = window() ? window()->devicePixelRatio() : 1.0;

    // Ticks: full height on the main lines, short stubs below for beats and finer.
    QColor majorTick = QColor(uiconfiguration()->currentTheme().extra.value("timeline_ruler_stroke_color").toString());
    QColor minorTick = QColor(uiconfiguration()->currentTheme().values.value(muse::ui::FONT_PRIMARY_COLOR).toString());
    minorTick.setAlphaF(0.35);
    forEachGridLine(plan, t0, t1, [&](double t, int level) {
        const double x = snapToDevicePixel((t - t0) * zoom, dpr);
        if (level == 0) {
            painter->fillRect(QRectF(x, 0, 1.0, h), majorTick);
        } else {
            const double len = level == 1 ? h * 0.3 : h * 0.18;
            painter->fillRect(QRectF(x, h - len, 1.0, len), minorTick);
        }
    });

    // Numbers: every 1 / 2 / 4 / 8 ... bars, never closer than ~58 pt; beats ("12.3") when there is room.
    constexpr double kLabelMinPx = 58.0;
    double labelSec = barSec;
    while (labelSec * zoom < kLabelMinPx && labelSec < barSec * 4096.0) {
        labelSec *= 2.0;
    }
    const bool beatLabels = beatSec * zoom >= kLabelMinPx;
    const double step = beatLabels ? beatSec : labelSec;

    QFont font;
    font.setPixelSize(FONT_SIZE);
    painter->setFont(font);
    QColor labelColor = QColor(uiconfiguration()->currentTheme().values.value(muse::ui::FONT_PRIMARY_COLOR).toString());
    const long long first = static_cast<long long>(std::floor((t0 - barOne) / step)) - 1;
    const long long last = static_cast<long long>(std::ceil((t1 - barOne) / step));
    for (long long n = first; n <= last; ++n) {
        const double t = barOne + n * step;
        const double barsFromOne = (t - barOne) / barSec;
        const long long barIndex = static_cast<long long>(std::floor(barsFromOne + 1e-6));
        const int beatInBar = static_cast<int>(std::llround((barsFromOne - barIndex) * upper)) + 1;
        // Bars before bar 1 read 0, -1, ... (as before).
        const long long barNumber = barIndex >= 0 ? barIndex + 1 : barIndex;
        const bool onBar = beatInBar == 1 || beatInBar > upper;
        QString text;
        if (onBar) {
            text = QString::number(barNumber);
            labelColor.setAlphaF(1.0);
        } else {
            text = QStringLiteral("%1.%2").arg(barNumber).arg(beatInBar);
            labelColor.setAlphaF(0.6);
        }
        painter->setPen(labelColor);
        const double x = snapToDevicePixel((t - t0) * zoom, dpr) + LABEL_OFFSET + 1;
        painter->drawText(QRectF(x, 0, 200, h / 2), Qt::AlignLeft | Qt::AlignVCenter, text);
    }

    drawSongBeatRuler(painter, w, h, beatSec, t0, t1, zoom, dpr);
}

//! InTheMix: MixMeister Fusion's ruler -- in the lower half, the beats counted
//! from each song's own start (0, 16, 32 ...), so every song reads from 0.
void TimelineRuler::drawSongBeatRuler(QPainter* painter, double w, double h, double beatSec,
                                      double t0, double t1, double zoom, double dpr)
{
    const auto prj = globalContext() ? globalContext()->currentTrackeditProject() : nullptr;
    if (!prj || beatSec <= 0.0 || zoom <= 0.0) {
        return;
    }
    std::vector<double> starts;
    for (const au::trackedit::Track& track : prj->trackList()) {
        for (const au::trackedit::Clip& clip : prj->clipList(track.id)) {
            if (clip.endTime > clip.startTime) {
                starts.push_back(clip.startTime);
            }
        }
    }
    if (starts.empty()) {
        return;
    }
    std::sort(starts.begin(), starts.end());
    std::vector<double> songs;
    for (double s : starts) {
        if (songs.empty() || s - songs.back() > 0.05) {
            songs.push_back(s);
        }
    }

    // Every 16 beats, doubled until the numbers are ~44 pt apart.
    double stepBeats = 16.0;
    while (stepBeats * beatSec * zoom < 44.0 && stepBeats < 4096.0) {
        stepBeats *= 2.0;
    }

    QFont font;
    font.setPixelSize(FONT_SIZE);
    painter->setFont(font);
    QColor dim = QColor(uiconfiguration()->currentTheme().values.value(muse::ui::FONT_PRIMARY_COLOR).toString());
    dim.setAlphaF(0.55);
    const QColor accent(0xE3, 0x12, 0x1B);   // Anonymous Tracks red: where a song starts

    for (size_t i = 0; i < songs.size(); ++i) {
        const double start = songs[i];
        const double end = i + 1 < songs.size() ? songs[i + 1] : t1;
        if (end < t0 || start > t1) {
            continue;
        }
        const long long firstK = std::max(0LL, static_cast<long long>(std::floor((t0 - start) / (stepBeats * beatSec))));
        for (long long k = firstK;; ++k) {
            const double t = start + k * stepBeats * beatSec;
            // Stop a little before the next song's 0 so the numbers don't collide.
            if (t > t1 || (i + 1 < songs.size() && t > end - 0.5 * stepBeats * beatSec && k > 0)) {
                break;
            }
            const double x = snapToDevicePixel((t - t0) * zoom, dpr);
            if (x < -60 || x > w + 10) {
                continue;
            }
            if (k == 0) {
                painter->fillRect(QRectF(x, h / 2, 2.0, h / 2), accent);
                QFont bold = font;
                bold.setBold(true);
                painter->setFont(bold);
                painter->setPen(accent);
            } else {
                painter->setFont(font);
                painter->setPen(dim);
            }
            const QString label = QString::number(static_cast<long long>(k * stepBeats));
            painter->drawText(QRectF(x + LABEL_OFFSET + 1, h / 2, 80, h / 2), Qt::AlignLeft | Qt::AlignVCenter, label);
        }
    }
}

Ticks TimelineRuler::prepareTickData(const IntervalInfo& timeInterval, double w, double h)
{
    Ticks ticks;

    // start calculating 100px over left edge of the ruler
    double leftBuffer = 100;
    double value = m_context->frameStartTime() - leftBuffer / m_context->zoom();
    double x = -leftBuffer;

    // find value and position of the first tick
    double remainder = std::remainder(value, timeInterval.minorMinor);
    if (remainder != 0) {
        x += (timeInterval.minorMinor - remainder) * m_context->zoom();
        value += (timeInterval.minorMinor - remainder);
    }

    auto tickHeight = [&](TickType tickType) {
        switch (tickType) {
        case TickType::MAJOR: return h;
        case TickType::MINOR: return h / MINOR_TICK_HEIGHT_RATIO;
        case TickType::MINORMINOR: return h / MINORMINOR_TICK_HEIGHT_RATIO;
        }
        return 0.0;
    };

    while (x < w)
    {
        // determine tick type
        TickType tickType = determineTickType(value, timeInterval);
        QString tickLabel = m_formatter->label(value, timeInterval, tickType, m_context);
        QLineF tick = QLineF(x, h - 2, x, h - 1 - tickHeight(tickType));
        double labelPos = x + LABEL_OFFSET;

        if (tickType == TickType::MAJOR || tickType == TickType::MINOR) {
            // add tick with label
            ticks.append(TickInfo { labelPos, tickLabel, tickType, tick, value });
        } else {
            // add tick without label
            ticks.append(TickInfo { -1.0, QString(), tickType, tick, value });
        }

        x += m_context->zoom() * timeInterval.minorMinor;
        value += timeInterval.minorMinor;
    }

    return ticks;
}

TickType TimelineRuler::determineTickType(double value, const IntervalInfo& timeInterval)
{
    if (muse::is_zero(std::abs(std::remainder(value, timeInterval.major)))) {
        return TickType::MAJOR;
    } else if (muse::is_zero(std::abs(std::remainder(value, timeInterval.minor)))) {
        return TickType::MINOR;
    } else {
        return TickType::MINORMINOR;
    }
}

void TimelineRuler::drawLabels(QPainter* painter, const Ticks& ticks, double w, double h)
{
    QFont textFont;
    textFont.setPixelSize(FONT_SIZE);
    QPen pen = painter->pen();
    QColor labelColor = uiconfiguration()->currentTheme().values.value(muse::ui::FONT_PRIMARY_COLOR).toString();
    pen.setColor(labelColor);
    painter->setPen(pen);

    for (qsizetype i = 0; i < ticks.count(); i++) {
        if (ticks[i].x == -1.0) {
            continue;
        }
        labelColor.setAlphaF(ticks[i].tickType == TickType::MAJOR ? LABEL_ALPHA_MAJOR : LABEL_ALPHA_MINOR);
        pen.setColor(labelColor);
        painter->setPen(pen);
        textFont.setBold(ticks[i].tickType == TickType::MAJOR);
        painter->setFont(textFont);
        painter->drawText(QRectF(ticks[i].x, 0, w, h / 2), Qt::AlignLeft | Qt::AlignVCenter, ticks[i].tickLabel);
    }
}

void TimelineRuler::drawTicks(QPainter* painter, const Ticks& ticks)
{
    QPen pen = painter->pen();
    QColor majorTickColor = QColor(uiconfiguration()->currentTheme().extra.value("timeline_ruler_stroke_color").toString());
    QColor minorTickColor = QColor(uiconfiguration()->currentTheme().values.value(muse::ui::FONT_PRIMARY_COLOR).toString());

    for (auto i = 0; i < ticks.count(); ++i) {
        QColor tickColor = ticks[i].tickType == TickType::MAJOR ? majorTickColor : minorTickColor;
        tickColor.setAlphaF(ticks[i].tickType == TickType::MAJOR ? TICK_ALPHA_MAJOR : TICK_ALPHA_MINOR);
        pen.setColor(tickColor);
        painter->setPen(pen);
        painter->drawLine(ticks[i].line);
    }
}

TimelineContext* TimelineRuler::timelineContext() const
{
    return m_context;
}

void TimelineRuler::setTimelineContext(TimelineContext* newContext)
{
    if (m_context == newContext) {
        return;
    }

    if (m_context) {
        disconnect(m_context, nullptr, this, nullptr);
    }

    m_context = newContext;

    if (m_context) {
        auto updateView = [this] () { update(); };

        connect(m_context, &TimelineContext::frameTimeChanged, this, updateView);

        connect(m_context, &TimelineContext::BPMChanged, this, updateView);
        connect(m_context, &TimelineContext::timeSigUpperChanged, this, updateView);
        connect(m_context, &TimelineContext::timeSigLowerChanged, this, updateView);
        connect(m_context, &TimelineContext::barOneTimeChanged, this, updateView);
    }

    emit timelineContextChanged();
}
