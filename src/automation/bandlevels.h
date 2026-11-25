/*
 * GlideMix
 */
#pragma once

// GlideMix 1.3.10 (MixMeister's bass / mid / treble knobs, -50..+10 dB): a track
// band's line on the song and its knob show the band's cut ("low", 0..1 gain,
// 1 = untouched -- what transitions write) and its boost ("lowBoost", 0..1 =
// 0..+10 dB) as ONE line:
//   view 0 .. 0.8 = the cut, linear in gain (as before 1.3.10),
//   view 0.8      = untouched (0 dB), where the Color line's "open" also sits,
//   view 0.8 .. 1 = the boost, linear in dB, up to +10 dB.

#include <algorithm>
#include <cmath>
#include <utility>

#include "au3-wave-track/ClipParameterAutomation.h"

namespace au::automation::bandlevels {
inline constexpr double kNormalAt = 0.8;
inline constexpr double kMaxDb = clipautomation::kMixtapeBandBoostMaxDb;
inline constexpr double kMinDb = -50.0;   // the knob's left end, as MixMeister's

inline const char* cutParameter(int band)
{
    return band == 1 ? clipautomation::kMixtapeMidBandParameter
                     : band == 2 ? clipautomation::kMixtapeHighBandParameter : clipautomation::kMixtapeLowBandParameter;
}

inline const char* boostParameter(int band)
{
    return band == 1 ? clipautomation::kMixtapeMidBoostParameter
                     : band == 2 ? clipautomation::kMixtapeHighBoostParameter : clipautomation::kMixtapeLowBoostParameter;
}

inline double viewFrom(double cut, double boost)
{
    cut = std::clamp(cut, 0.0, 1.0);
    boost = std::clamp(boost, 0.0, 1.0);
    return boost > 1e-9 && cut >= 1.0 - 1e-9 ? kNormalAt + (1.0 - kNormalAt) * boost : kNormalAt * cut;
}

//! cut, boost
inline std::pair<double, double> split(double view)
{
    view = std::clamp(view, 0.0, 1.0);
    if (view <= kNormalAt) {
        return { view / kNormalAt, 0.0 };
    }
    return { 1.0, (view - kNormalAt) / (1.0 - kNormalAt) };
}

//! The line's height in dB (-inf for a band killed).
inline double dbFromView(double view)
{
    if (view >= kNormalAt) {
        return (std::min(view, 1.0) - kNormalAt) / (1.0 - kNormalAt) * kMaxDb;
    }
    return view <= 0.0 ? -INFINITY : 20.0 * std::log10(view / kNormalAt);
}

inline double viewFromDb(double db)
{
    if (db >= 0.0) {
        return kNormalAt + (1.0 - kNormalAt) * std::min(db, kMaxDb) / kMaxDb;
    }
    return std::isfinite(db) ? kNormalAt * std::pow(10.0, db / 20.0) : 0.0;
}

//! The knob: -100..0 = -50..0 dB, 0..100 = 0..+10 dB (0 dB straight up).
inline double knobFromView(double view)
{
    const double db = dbFromView(view);
    if (db >= 0.0) {
        return db / kMaxDb * 100.0;
    }
    return std::isfinite(db) ? std::max(-100.0, db / -kMinDb * 100.0) : -100.0;
}

inline double viewFromKnob(double knob)
{
    knob = std::clamp(knob, -100.0, 100.0);
    return viewFromDb(knob >= 0.0 ? knob / 100.0 * kMaxDb : knob / 100.0 * -kMinDb);
}

//! split() puts a point where a straight stretch crosses 0 dB; reading the line back, such a
//! point (at 0 dB, straight on both sides, exactly on the line between its neighbours) goes again.
inline clipautomation::Curve withoutCrossingPoints(clipautomation::Curve line)
{
    for (size_t i = 1; i < line.segments.size();) {
        const auto& a = line.segments[i - 1];
        const auto& b = line.segments[i];
        const double t0 = a.startPoint.timeOffset, v0 = a.startPoint.value;
        const double t1 = a.endPoint.timeOffset, v1 = a.endPoint.value;
        const double t2 = b.endPoint.timeOffset, v2 = b.endPoint.value;
        const bool atZero = std::abs(v1 - kNormalAt) < 1e-6;
        const bool crosses = (v0 - kNormalAt) * (v2 - kNormalAt) < 0.0;
        const bool straight = a.curvature == 0 && b.curvature == 0 && !a.eased && !b.eased;
        const bool onLine = t2 > t0 && std::abs(v0 + (v2 - v0) * (t1 - t0) / (t2 - t0) - v1) < 1e-4;
        if (atZero && crosses && straight && onLine) {
            line.segments[i - 1].endPoint = b.endPoint;
            line.segments.erase(line.segments.begin() + static_cast<long>(i));
            continue;
        }
        ++i;
    }
    return line;
}

//! The one line from the stored cut and (optional) boost lines.
inline clipautomation::Curve join(const clipautomation::Curve& cut, const clipautomation::Curve* boost)
{
    clipautomation::Curve out;
    const bool zipped = boost && boost->segments.size() == cut.segments.size()
                        && std::equal(cut.segments.begin(), cut.segments.end(), boost->segments.begin(),
                                      [](const auto& a, const auto& b) {
        return std::abs(a.startPoint.timeOffset - b.startPoint.timeOffset) < 1e-9
               && std::abs(a.endPoint.timeOffset - b.endPoint.timeOffset) < 1e-9;
    });
    if (!boost || boost->segments.empty() || zipped) {
        for (size_t i = 0; i < cut.segments.size(); ++i) {
            auto s = cut.segments[i];
            const double b0 = zipped ? boost->segments[i].startPoint.value : 0.0;
            const double b1 = zipped ? boost->segments[i].endPoint.value : 0.0;
            s.startPoint.value = viewFrom(s.startPoint.value, b0);
            s.endPoint.value = viewFrom(s.endPoint.value, b1);
            out.segments.push_back(s);
        }
        return out;
    }
    // Different points (an older edit, or a transition rewrote the cut): straight
    // segments through every point of either line.
    std::vector<double> times;
    for (const auto* c : { &cut, boost }) {
        for (const auto& s : c->segments) {
            times.push_back(s.startPoint.timeOffset);
            times.push_back(s.endPoint.timeOffset);
        }
    }
    std::sort(times.begin(), times.end());
    times.erase(std::unique(times.begin(), times.end(), [](double a, double b) { return std::abs(a - b) < 1e-9; }),
                times.end());
    const auto at = [](const clipautomation::Curve& c, double t) {
        for (const auto& s : c.segments) {
            if (t <= s.endPoint.timeOffset) {
                return s.Evaluate(std::max(t, s.startPoint.timeOffset));
            }
        }
        return c.segments.empty() ? 0.0 : c.segments.back().endPoint.value;
    };
    for (size_t i = 1; i < times.size(); ++i) {
        out.segments.push_back({ { times[i - 1], viewFrom(at(cut, times[i - 1]), at(*boost, times[i - 1])) },
                                 { times[i], viewFrom(at(cut, times[i]), at(*boost, times[i])) }, 0, false });
    }
    return out;
}

//! Back to the stored cut and boost lines (same points on both). A segment that
//! crosses 0 dB is split there, each half straight.
inline std::pair<clipautomation::Curve, clipautomation::Curve> split(const clipautomation::Curve& view)
{
    std::pair<clipautomation::Curve, clipautomation::Curve> out;
    const auto push = [&](const clipautomation::Segment& s) {
        auto c = s;
        auto b = s;
        std::tie(c.startPoint.value, b.startPoint.value) = split(s.startPoint.value);
        std::tie(c.endPoint.value, b.endPoint.value) = split(s.endPoint.value);
        out.first.segments.push_back(c);
        out.second.segments.push_back(b);
    };
    for (const auto& s : view.segments) {
        const double v0 = s.startPoint.value;
        const double v1 = s.endPoint.value;
        const bool crosses = (v0 < kNormalAt - 1e-9 && v1 > kNormalAt + 1e-9) || (v0 > kNormalAt + 1e-9 && v1 < kNormalAt - 1e-9);
        if (!crosses || s.endPoint.timeOffset - s.startPoint.timeOffset < 1e-6) {
            push(s);
            continue;
        }
        double lo = s.startPoint.timeOffset;
        double hi = s.endPoint.timeOffset;
        for (int i = 0; i < 40; ++i) {
            const double mid = 0.5 * (lo + hi);
            const bool sameSideAsStart = (s.Evaluate(mid) < kNormalAt) == (v0 < kNormalAt);
            (sameSideAsStart ? lo : hi) = mid;
        }
        const clipautomation::Point cross{ 0.5 * (lo + hi), kNormalAt };
        push({ s.startPoint, cross, 0, false });
        push({ cross, s.endPoint, 0, false });
    }
    return out;
}

//! Writes a band's line (in view values) into the clip's automation: the cut, and the
//! boost while any part of the line is above 0 dB (else the boost line is switched off).
//! Throws as Curve::SetBezier does for a malformed line.
inline bool write(ClipParameterAutomation& data, int band, const clipautomation::Curve& view)
{
    auto [cut, boost] = split(view);
    const bool boosted = std::any_of(boost.segments.begin(), boost.segments.end(), [](const auto& s) {
        return s.startPoint.value > 1e-9 || s.endPoint.value > 1e-9;
    });
    auto* cutCurve = data.FindCurve(clipautomation::kMixtapeBuiltinInstanceId, cutParameter(band));
    if (!cutCurve) {
        cutCurve = data.AddCurve(clipautomation::kMixtapeBuiltinInstanceId, clipautomation::kMixtapeBuiltinPluginId,
                                 cutParameter(band), 1.0);
    }
    if (!cutCurve) {
        return false;
    }
    auto* boostCurve = data.FindCurve(clipautomation::kMixtapeBuiltinInstanceId, boostParameter(band));
    if (!boostCurve && boosted) {
        boostCurve = data.AddCurve(clipautomation::kMixtapeBuiltinInstanceId, clipautomation::kMixtapeBuiltinPluginId,
                                   boostParameter(band), 0.0);
        if (!boostCurve) {
            return false;
        }
        cutCurve = data.FindCurve(clipautomation::kMixtapeBuiltinInstanceId, cutParameter(band));   // AddCurve may move it
    }
    cutCurve->enabled = true;
    cutCurve->SetBezier(std::move(cut));
    if (boostCurve) {
        boostCurve->enabled = boosted;
        if (boosted) {
            boostCurve->SetBezier(std::move(boost));
        } else {
            boostCurve->bezier = {};
            boostCurve->envelope.Flatten(0.0);
        }
    }
    return true;
}

//! The band's one line as stored in `data` (empty when the clip has no line for it).
inline clipautomation::Curve read(const ClipParameterAutomation& data, int band)
{
    const ClipParameterAutomation::Curve* cut = nullptr;
    const ClipParameterAutomation::Curve* boost = nullptr;
    for (const auto& c : data.Curves()) {
        if (!c.enabled || c.effectInstanceId != clipautomation::kMixtapeBuiltinInstanceId) {
            continue;
        }
        if (c.parameterId == cutParameter(band)) {
            cut = &c;
        } else if (c.parameterId == boostParameter(band)) {
            boost = &c;
        }
    }
    if (!cut && !boost) {
        return {};
    }
    const clipautomation::Curve boostCurve = boost ? boost->Bezier() : clipautomation::Curve{};
    if (cut) {
        return withoutCrossingPoints(join(cut->Bezier(), boost ? &boostCurve : nullptr));
    }
    // Only a boost: the cut is untouched.
    clipautomation::Curve flat;
    for (const auto& s : boostCurve.segments) {
        flat.segments.push_back({ { s.startPoint.timeOffset, 1.0 }, { s.endPoint.timeOffset, 1.0 }, 0, false });
    }
    return withoutCrossingPoints(join(flat, &boostCurve));
}
}
