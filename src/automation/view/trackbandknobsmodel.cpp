/*
 * GlideMix
 */
#include "trackbandknobsmodel.h"

#include <algorithm>
#include <cmath>

#include "trackedit/itrackeditproject.h"
#include "project/iaudacityproject.h"
#include "projectscene/iprojectviewstate.h"
#include "automation/bandlevels.h"

using namespace au::automation;

namespace {
const char* kKnobBandName[4] = { "Low", "Mid", "High", "Color" };

double knobLineAt(const AutomationPoints& points, double t, double fallback)
{
    if (points.empty()) {
        return fallback;
    }
    if (t <= points.front().xValue) {
        return points.front().yValue;
    }
    for (size_t i = 1; i < points.size(); ++i) {
        if (t <= points[i].xValue) {
            const double span = points[i].xValue - points[i - 1].xValue;
            const double u = span > 0.0 ? (t - points[i - 1].xValue) / span : 1.0;
            return points[i - 1].yValue + u * (points[i].yValue - points[i - 1].yValue);
        }
    }
    return points.back().yValue;
}

// Color as one value from the Filter effect's two lines (clippluginautomationmodel.cpp):
// 0..0.5 low-pass closing, 0.5 open, 0.5..1 high-pass opening.
double knobCombineColor(double lpf, double hpf)
{
    return lpf < 0.999 ? 0.5 * std::clamp(lpf, 0.0, 1.0) : std::clamp(0.5 + 0.5 * hpf, 0.0, 1.0);
}
}

TrackBandKnobsModel::TrackBandKnobsModel(QObject* parent)
    : QObject(parent), muse::Contextable(muse::iocCtxForQmlObject(this))
{
    // The knobs follow the playhead a few times a second, not every frame.
    m_refresh.setSingleShot(true);
    m_refresh.setInterval(150);
    connect(&m_refresh, &QTimer::timeout, this, [this]() { emit valuesChanged(); });
}

void TrackBandKnobsModel::setTrackId(const QVariant& id)
{
    const trackedit::TrackId value = id.toLongLong();
    if (value == m_trackId) {
        return;
    }
    m_trackId = value;
    emit trackIdChanged();
    emit valuesChanged();
    emit linesShownChanged();
}

void TrackBandKnobsModel::init()
{
    const auto changed = [this]() {
        if (!m_refresh.isActive()) {
            m_refresh.start();
        }
    };
    if (globalContext()) {
        if (const auto state = globalContext()->playbackState()) {
            state->playbackPositionChanged().onReceive(this, [changed](muse::secs_t) { changed(); });
        }
        if (const auto project = globalContext()->currentProject()) {
            project->viewState()->bandLinesShownChanged().onNotify(this, [this]() { emit linesShownChanged(); });
        }
    }
    if (projectHistory()) {
        projectHistory()->historyChanged().onReceive(this, [changed](trackedit::HistoryEvent) { changed(); });
    }
    if (clipGainInteraction()) {
        clipGainInteraction()->clipGainChanged().onReceive(this, [this, changed](const trackedit::ClipKey& key, bool) {
            if (key.trackId == m_trackId) {
                changed();
            }
        });
    }
}

bool TrackBandKnobsModel::linesShown() const
{
    const auto project = globalContext() ? globalContext()->currentProject() : nullptr;
    return project && project->viewState()->bandLinesShown(m_trackId);
}

void TrackBandKnobsModel::setLinesShown(bool shown)
{
    const auto project = globalContext() ? globalContext()->currentProject() : nullptr;
    if (project) {
        project->viewState()->setBandLinesShown(m_trackId, shown);
        // GlideMix 1.3.10 (Chris): the EQ automation button brings every line back as it was.
        project->viewState()->setFrontLine(-1);
    }
}

std::vector<au::trackedit::ClipKey> TrackBandKnobsModel::clips() const
{
    std::vector<trackedit::ClipKey> out;
    const auto project = globalContext() ? globalContext()->currentTrackeditProject() : nullptr;
    if (!project || m_trackId < 0) {
        return out;
    }
    for (const trackedit::Clip& clip : project->clipList(m_trackId)) {
        out.push_back(clip.key);
    }
    return out;
}

std::string TrackBandKnobsModel::filterEffectId() const
{
    return mixxxEffectIds() ? mixxxEffectIds()->effectIdForMixxxId("org.mixxx.effects.filter").toStdString() : std::string();
}

double TrackBandKnobsModel::valueAt(int band, double time) const
{
    const auto project = globalContext() ? globalContext()->currentTrackeditProject() : nullptr;
    if (!project || !clipGainInteraction()) {
        return band == 3 ? 0.5 : bandlevels::kNormalAt;
    }
    // The song under the playhead, else the track's first one at its start.
    trackedit::ClipKey key;
    double at = time;
    for (const trackedit::Clip& clip : project->clipList(m_trackId)) {
        if (!key.isValid()) {
            key = clip.key;
            at = clip.startTime;
        }
        if (time >= clip.startTime && time <= clip.endTime) {
            key = clip.key;
            at = time;
            break;
        }
    }
    if (!key.isValid()) {
        return band == 3 ? 0.5 : bandlevels::kNormalAt;
    }
    if (band < 3) {
        // GlideMix 1.3.10: the cut and boost as one line (bandlevels.h view values).
        return knobLineAt(clipGainInteraction()->clipBandLinePoints(key, band), at, bandlevels::kNormalAt);
    }
    const std::string filter = filterEffectId();
    if (filter.empty()) {
        return 0.5;
    }
    return knobCombineColor(knobLineAt(clipGainInteraction()->clipEffectPoints(key, filter, "lpf"), at, 1.0),
                        knobLineAt(clipGainInteraction()->clipEffectPoints(key, filter, "hpf"), at, 0.0));
}

double TrackBandKnobsModel::knobFromValue(int band, double value)
{
    // GlideMix 1.3.10 (MixMeister): a band knob is -50 dB .. 0 .. +10 dB, 0 dB straight up.
    return band == 3 ? (value - 0.5) * 200.0 : bandlevels::knobFromView(value);
}

double TrackBandKnobsModel::valueFromKnob(int band, double knob)
{
    return band == 3 ? std::clamp(0.5 + knob / 200.0, 0.0, 1.0) : bandlevels::viewFromKnob(knob);
}

QVariantList TrackBandKnobsModel::values() const
{
    const auto state = globalContext() ? globalContext()->playbackState() : nullptr;
    const double t = state ? state->playbackPosition().to_double() : 0.0;
    QVariantList out;
    for (int band = 0; band < 4; ++band) {
        out << std::round(knobFromValue(band, valueAt(band, t)));
    }
    return out;
}

QString TrackBandKnobsModel::knobText(int band, double knob) const
{
    if (band == 3) {
        return QString::number(std::lround(knob)) + QStringLiteral(" %");
    }
    const double db = bandlevels::dbFromView(bandlevels::viewFromKnob(knob));
    if (knob <= -100.0 + 1e-6 || !std::isfinite(db)) {
        return QStringLiteral("-50 dB");
    }
    return (db > 0.05 ? QStringLiteral("+") : QString()) + QString::number(db, 'f', 1) + QStringLiteral(" dB");
}

void TrackBandKnobsModel::begin(int band)
{
    if (band < 0 || band > 3 || !clipGainInteraction()) {
        return;
    }
    m_band = band;
    m_moved = false;
    m_colorBase.clear();
    clipGainInteraction()->endBandShift();
    const auto state = globalContext() ? globalContext()->playbackState() : nullptr;
    m_startKnob = knobFromValue(band, valueAt(band, state ? state->playbackPosition().to_double() : 0.0));
    const std::string filter = filterEffectId();
    for (const trackedit::ClipKey& key : clips()) {
        if (band < 3) {
            clipGainInteraction()->beginBandShift(key, band);
        } else if (!filter.empty()) {
            m_colorBase.push_back({ key, clipGainInteraction()->clipEffectPoints(key, filter, "lpf"),
                                    clipGainInteraction()->clipEffectPoints(key, filter, "hpf") });
        }
    }
}

void TrackBandKnobsModel::turn(int band, double knob)
{
    if (band != m_band || !clipGainInteraction()) {
        return;
    }
    const double delta = valueFromKnob(band, knob) - valueFromKnob(band, m_startKnob);
    m_moved = true;
    if (band < 3) {
        for (const trackedit::ClipKey& key : clips()) {
            clipGainInteraction()->applyBandShift(key, band, delta);
        }
    } else {
        const std::string filter = filterEffectId();
        const auto project = globalContext() ? globalContext()->currentTrackeditProject() : nullptr;
        if (filter.empty() || !project) {
            return;
        }
        for (const ColorBase& base : m_colorBase) {
            const trackedit::Clip clip = project->clip(base.key);
            // Every place either line has a point, as one Color line, moved, then split again.
            std::vector<double> times;
            for (const auto* line : { &base.lpf, &base.hpf }) {
                for (const auto& p : *line) {
                    times.push_back(p.xValue);
                }
            }
            if (times.empty()) {
                times = { clip.startTime, clip.endTime };
            }
            std::sort(times.begin(), times.end());
            times.erase(std::unique(times.begin(), times.end(), [](double a, double b) { return std::abs(a - b) < 1e-6; }),
                        times.end());
            AutomationPoints lpf, hpf;
            bool open = true;
            for (const double t : times) {
                const double c = std::clamp(knobCombineColor(knobLineAt(base.lpf, t, 1.0), knobLineAt(base.hpf, t, 0.0)) + delta, 0.0, 1.0);
                lpf.push_back({ t, std::min(1.0, 2.0 * c) });
                hpf.push_back({ t, std::max(0.0, 2.0 * c - 1.0) });
                open = open && std::abs(c - 0.5) < 1e-3;
            }
            // As the Color line editor does: the Filter effect joins the track once the line leaves "open".
            clipGainInteraction()->setClipEffectPoints(base.key, filter, "lpf", lpf, !open);
            clipGainInteraction()->setClipEffectPoints(base.key, filter, "hpf", hpf, !open);
        }
    }
    emit valuesChanged();
}

void TrackBandKnobsModel::end(int band)
{
    if (band != m_band) {
        return;
    }
    if (clipGainInteraction()) {
        clipGainInteraction()->endBandShift();
    }
    if (m_moved && projectHistory() && band >= 0 && band <= 3) {
        projectHistory()->pushHistoryState(std::string("Moved the ") + kKnobBandName[band] + " line",
                                           std::string(kKnobBandName[band]) + " line");
    }
    m_band = -1;
    m_moved = false;
    m_colorBase.clear();
    emit valuesChanged();
}
