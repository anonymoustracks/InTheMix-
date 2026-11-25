/*
 * GlideMix: one grid rule for the lanes and the clips.
 *
 * Measured from a reference DAW on the same screen (docs/create-studio-grid-study.md):
 * three line levels with clear steps between them, no level closer than ~16 px, bar lines
 * thinned in powers of two when zoomed out (never closer than ~28 px), and the lane shaded
 * in alternating musical blocks (16 bars, 4 bars, 1 bar, 1 beat) at least ~80 px wide.
 * Every line and block edge sits on a whole device pixel.
 */
#pragma once

#include <algorithm>
#include <cmath>
#include <functional>

namespace au::projectscene {
struct GridPlan
{
    double origin = 0.0;     //!< bar 1, seconds
    double majorSec = 0.0;   //!< level 0: a bar, or 2 / 4 / 8 ... bars zoomed out
    double midSec = 0.0;     //!< level 1: the beat (or half bar); 0 = none
    double fineSec = 0.0;    //!< level 2: 8th ... 64th; 0 = none
    double shadeSec = 0.0;   //!< alternating shaded blocks

    bool valid() const { return majorSec > 0.0; }

    //! The finest step drawn.
    double stepSec() const { return fineSec > 0.0 ? fineSec : (midSec > 0.0 ? midSec : majorSec); }
};

constexpr double kGridMajorMinPx = 28.0;
constexpr double kGridMinPx = 16.0;
constexpr double kGridShadeMinPx = 80.0;

//! zoom: px per second. beatSec: one beat. beatsPerBar: the time signature's upper number.
inline GridPlan makeGridPlan(double zoom, double beatSec, int beatsPerBar, double barOneTime)
{
    GridPlan plan;
    if (zoom <= 0.0 || beatSec <= 0.0 || beatsPerBar <= 0) {
        return plan;
    }
    plan.origin = barOneTime;
    const double bar = beatSec * beatsPerBar;

    double major = bar;
    while (major * zoom < kGridMajorMinPx && major < bar * 1024.0) {
        major *= 2.0;
    }
    plan.majorSec = major;

    if (major == bar) {
        if (beatSec * zoom >= kGridMinPx) {
            plan.midSec = beatSec;
            double fine = beatSec / 2.0;
            double best = 0.0;
            while (fine * zoom >= kGridMinPx && fine >= beatSec / 16.0 - 1e-12) {
                best = fine;
                fine /= 2.0;
            }
            plan.fineSec = best;
        } else if (beatsPerBar % 2 == 0 && beatsPerBar >= 4 && (bar / 2.0) * zoom >= kGridMinPx) {
            plan.midSec = bar / 2.0;
        }
    }

    const double blocks[] = { beatSec, bar, bar * 4.0, bar * 16.0 };
    plan.shadeSec = bar * 16.0;
    for (double b : blocks) {
        if (b * zoom >= kGridShadeMinPx) {
            plan.shadeSec = b;
            break;
        }
    }
    return plan;
}

//! Calls `line(time, level)` for every grid line between t0 and t1 (seconds).
inline void forEachGridLine(const GridPlan& plan, double t0, double t1, const std::function<void(double, int)>& line)
{
    if (!plan.valid() || t1 <= t0) {
        return;
    }
    const double step = plan.stepSec();
    const long long first = static_cast<long long>(std::floor((t0 - plan.origin) / step));
    const long long last = static_cast<long long>(std::ceil((t1 - plan.origin) / step));
    if (last - first > 20000) {
        return;   // never more lines than pixels
    }
    const auto isMultiple = [&](long long n, double unit) {
        if (unit <= 0.0) {
            return false;
        }
        const double ratio = unit / step;
        const long long r = static_cast<long long>(std::llround(ratio));
        return r > 0 && n % r == 0;
    };
    for (long long n = first; n <= last; ++n) {
        const double t = plan.origin + n * step;
        if (t < t0 - 1e-9 || t > t1 + 1e-9) {
            continue;
        }
        int level = 2;
        if (isMultiple(n, plan.majorSec)) {
            level = 0;
        } else if (isMultiple(n, plan.midSec)) {
            level = 1;
        }
        line(t, level);
    }
}

//! Calls `block(start, end)` for every second (shaded) block between t0 and t1.
inline void forEachShadedBlock(const GridPlan& plan, double t0, double t1, const std::function<void(double, double)>& block)
{
    if (!plan.valid() || plan.shadeSec <= 0.0 || t1 <= t0) {
        return;
    }
    const long long first = static_cast<long long>(std::floor((t0 - plan.origin) / plan.shadeSec));
    const long long last = static_cast<long long>(std::ceil((t1 - plan.origin) / plan.shadeSec));
    for (long long n = first; n <= last; ++n) {
        if ((n % 2 + 2) % 2 == 0) {
            continue;   // the odd blocks are shaded
        }
        const double s = plan.origin + n * plan.shadeSec;
        block(std::max(s, t0), std::min(s + plan.shadeSec, t1));
    }
}

//! A position on a whole device pixel.
inline double snapToDevicePixel(double x, double dpr)
{
    return dpr > 0.0 ? std::round(x * dpr) / dpr : std::round(x);
}
}
