/*
* Audacity: A Digital Audio Editor
*/
#include "projectsceneuistate.h"

#include <algorithm>
#include <cmath>

#include "global/settings.h"

using namespace au::projectscene;

static const QString TIMELINE_RULER_MODE_KEY("projectscene/timelineRulerMode");
static const QString GRID_LINES_VISIBLE_KEY("projectscene/gridLinesVisible");
static const QString CLIP_WAVEFORM_RGB_KEY("projectscene/clipWaveformRgb");
static const QString CLIP_WAVEFORM_STEREO_KEY("projectscene/clipWaveformStereo");
// GlideMix: the colour waveform and its stereo option are app settings, not
// part of a workspace: kept per workspace, switching workspace or pressing
// View > Color waveform in one of them quietly turned it off for good.
static const muse::Settings::Key WAVEFORM_RGB_SETTING("projectscene", "projectscene/colorWaveform");
static const muse::Settings::Key WAVEFORM_STEREO_SETTING("projectscene", "projectscene/colorWaveformStereo");
static const QString LANE_BACKGROUND_KEY("projectscene/laneBackground");
static const muse::Settings::Key ALL_TRACKS_VIEW_SETTING("projectscene", "projectscene/allTracksView");
static const muse::Settings::Key CLIPS_SEE_THROUGH_SETTING("projectscene", "projectscene/clipsSeeThrough");
static const muse::Settings::Key BAND_LINE_FILL_SETTING("projectscene", "projectscene/bandLineFill");
static const muse::Settings::Key WAVEFORM_SCALE_SETTING("projectscene", "projectscene/waveformScale");
static constexpr double kWaveformScaleMin = 0.2;
static constexpr double kWaveformScaleMax = 1.5;

void ProjectSceneUiState::init()
{
    // The plain waveform first, as Audacity's Modern workspace draws it; the
    // colour waveform is the option (View > Color waveform, Preferences > Waveforms).
    muse::settings()->setDefaultValue(WAVEFORM_RGB_SETTING, muse::Val(false));
    muse::settings()->setDefaultValue(WAVEFORM_STEREO_SETTING, muse::Val(false));
    muse::settings()->setDefaultValue(ALL_TRACKS_VIEW_SETTING, muse::Val(1));
    muse::settings()->setDefaultValue(CLIPS_SEE_THROUGH_SETTING, muse::Val(false));
    muse::settings()->valueChanged(CLIPS_SEE_THROUGH_SETTING).onReceive(this, [this](const muse::Val&) {
        m_clipsSeeThroughChanged.notify();
    });
    // On by default: the band lines have always been drawn with their fill.
    muse::settings()->setDefaultValue(BAND_LINE_FILL_SETTING, muse::Val(true));
    muse::settings()->valueChanged(BAND_LINE_FILL_SETTING).onReceive(this, [this](const muse::Val&) {
        m_bandLineFillChanged.notify();
    });
    muse::settings()->setDefaultValue(WAVEFORM_SCALE_SETTING, muse::Val(1.0));
    muse::settings()->valueChanged(WAVEFORM_SCALE_SETTING).onReceive(this, [this](const muse::Val&) {
        m_waveformScaleChanged.notify();
    });
    muse::settings()->valueChanged(ALL_TRACKS_VIEW_SETTING).onReceive(this, [this](const muse::Val&) {
        m_allTracksViewChanged.notify();
    });
    muse::settings()->valueChanged(WAVEFORM_RGB_SETTING).onReceive(this, [this](const muse::Val&) {
        m_clipWaveformRgbChanged.notify();
        m_laneBackgroundChanged.notify();
    });
    muse::settings()->valueChanged(WAVEFORM_STEREO_SETTING).onReceive(this, [this](const muse::Val&) {
        m_clipWaveformRgbChanged.notify();
    });
    uiState()->uiItemStateChanged(TIMELINE_RULER_MODE_KEY).onNotify(this, [this]() {
        m_timelineRulerModeChanged.notify();
    });
    uiState()->uiItemStateChanged(GRID_LINES_VISIBLE_KEY).onNotify(this, [this]() {
        m_gridLinesVisibleChanged.notify();
    });
    uiState()->uiItemStateChanged(CLIP_WAVEFORM_RGB_KEY).onNotify(this, [this]() {
        m_clipWaveformRgbChanged.notify();
        // The canvas follows the waveform style unless it has been set.
        m_laneBackgroundChanged.notify();
    });
    // Stereo on or off repaints the clips the same way the colour waveform does.
    uiState()->uiItemStateChanged(CLIP_WAVEFORM_STEREO_KEY).onNotify(this, [this]() {
        m_clipWaveformRgbChanged.notify();
    });
    uiState()->uiItemStateChanged(LANE_BACKGROUND_KEY).onNotify(this, [this]() {
        m_laneBackgroundChanged.notify();
    });
    // The Charcoal canvas follows the theme.
    if (uiConfiguration()) {
        uiConfiguration()->currentThemeChanged().onNotify(this, [this]() {
            m_laneBackgroundChanged.notify();
        });
    }
}

TimelineRulerMode ProjectSceneUiState::timelineRulerMode() const
{
    // GlideMix: bars and beats unless seconds were chosen ("0"). The ruler and
    // the grid lines then follow the project tempo, which the first song sets
    // and every later song is stretched to -- the beat lines sit on the music,
    // as in any DAW (Chris, 2026-11-13; Logic shows bars and beats for a
    // musical project and its grid follows the tempo map).
    return uiState()->uiItemState(TIMELINE_RULER_MODE_KEY) == "0"
           ? TimelineRulerMode::MINUTES_AND_SECONDS
           : TimelineRulerMode::BEATS_AND_MEASURES;
}

void ProjectSceneUiState::setTimelineRulerMode(TimelineRulerMode mode)
{
    const QString value = mode == TimelineRulerMode::BEATS_AND_MEASURES ? "1" : "0";
    if (uiState()->uiItemState(TIMELINE_RULER_MODE_KEY) == value) {
        return;
    }
    uiState()->setUiItemState(TIMELINE_RULER_MODE_KEY, value);
}

muse::async::Notification ProjectSceneUiState::timelineRulerModeChanged() const
{
    return m_timelineRulerModeChanged;
}

// A workspace that says nothing about them gets Audacity's own look: grid on,
// waveform in the clip's colour.
bool ProjectSceneUiState::gridLinesVisible() const
{
    return uiState()->uiItemState(GRID_LINES_VISIBLE_KEY) != "0";
}

void ProjectSceneUiState::setGridLinesVisible(bool visible)
{
    const QString value = visible ? "1" : "0";
    if (uiState()->uiItemState(GRID_LINES_VISIBLE_KEY) == value) {
        return;
    }
    uiState()->setUiItemState(GRID_LINES_VISIBLE_KEY, value);
}

muse::async::Notification ProjectSceneUiState::gridLinesVisibleChanged() const
{
    return m_gridLinesVisibleChanged;
}

bool ProjectSceneUiState::clipWaveformRgb() const
{
    return muse::settings()->value(WAVEFORM_RGB_SETTING).toBool();
}

void ProjectSceneUiState::setClipWaveformRgb(bool rgb)
{
    if (clipWaveformRgb() != rgb) {
        muse::settings()->setSharedValue(WAVEFORM_RGB_SETTING, muse::Val(rgb));
    }
}

bool ProjectSceneUiState::clipWaveformStereo() const
{
    return muse::settings()->value(WAVEFORM_STEREO_SETTING).toBool();
}

void ProjectSceneUiState::setClipWaveformStereo(bool stereo)
{
    if (clipWaveformStereo() != stereo) {
        muse::settings()->setSharedValue(WAVEFORM_STEREO_SETTING, muse::Val(stereo));
    }
}

int ProjectSceneUiState::allTracksView() const
{
    return std::clamp(muse::settings()->value(ALL_TRACKS_VIEW_SETTING).toInt(), -1, 4);
}

void ProjectSceneUiState::setAllTracksView(int view)
{
    view = std::clamp(view, -1, 4);
    if (allTracksView() != view) {
        muse::settings()->setSharedValue(ALL_TRACKS_VIEW_SETTING, muse::Val(view));
    }
}

muse::async::Notification ProjectSceneUiState::allTracksViewChanged() const
{
    return m_allTracksViewChanged;
}

muse::async::Notification ProjectSceneUiState::clipWaveformRgbChanged() const
{
    return m_clipWaveformRgbChanged;
}

int ProjectSceneUiState::laneBackground() const
{
    const QString value = uiState()->uiItemState(LANE_BACKGROUND_KEY);
    // 0 follow the waveform style, 1 black, 2 theme, 3 charcoal, 4 grey.
    const int mode = value.toInt();
    return mode >= 1 && mode <= 4 ? mode : 0;
}

void ProjectSceneUiState::setLaneBackground(int mode)
{
    const QString value = QString::number(std::clamp(mode, 0, 4));
    if (uiState()->uiItemState(LANE_BACKGROUND_KEY) == value) {
        return;
    }
    uiState()->setUiItemState(LANE_BACKGROUND_KEY, value);
}

muse::async::Notification ProjectSceneUiState::laneBackgroundChanged() const
{
    return m_laneBackgroundChanged;
}

bool ProjectSceneUiState::blackCanvas() const
{
    switch (laneBackground()) {
    case 1: return true;
    case 2:
    case 3:
    case 4: return false;
    default: return clipWaveformRgb();
    }
}

bool ProjectSceneUiState::studioCanvas() const
{
    switch (laneBackground()) {
    case 3: return true;
    case 1:
    case 4: return false;
    default:
        return !blackCanvas() && uiConfiguration()
               && uiConfiguration()->currentTheme().codeKey == muse::ui::CHARCOAL_THEME_CODE;
    }
}

bool ProjectSceneUiState::clipsSeeThrough() const
{
    return muse::settings()->value(CLIPS_SEE_THROUGH_SETTING).toBool();
}

void ProjectSceneUiState::setClipsSeeThrough(bool on)
{
    if (clipsSeeThrough() != on) {
        muse::settings()->setSharedValue(CLIPS_SEE_THROUGH_SETTING, muse::Val(on));
    }
}

muse::async::Notification ProjectSceneUiState::clipsSeeThroughChanged() const
{
    return m_clipsSeeThroughChanged;
}

bool ProjectSceneUiState::bandLineFill() const
{
    return muse::settings()->value(BAND_LINE_FILL_SETTING).toBool();
}

void ProjectSceneUiState::setBandLineFill(bool on)
{
    if (bandLineFill() != on) {
        muse::settings()->setSharedValue(BAND_LINE_FILL_SETTING, muse::Val(on));
    }
}

muse::async::Notification ProjectSceneUiState::bandLineFillChanged() const
{
    return m_bandLineFillChanged;
}

double ProjectSceneUiState::waveformScale() const
{
    const double scale = muse::settings()->value(WAVEFORM_SCALE_SETTING).toDouble();
    return scale > 0.0 ? std::clamp(scale, kWaveformScaleMin, kWaveformScaleMax) : 1.0;
}

void ProjectSceneUiState::setWaveformScale(double scale)
{
    scale = std::clamp(scale, kWaveformScaleMin, kWaveformScaleMax);
    if (std::abs(waveformScale() - scale) > 1e-4) {
        muse::settings()->setSharedValue(WAVEFORM_SCALE_SETTING, muse::Val(scale));
    }
}

muse::async::Notification ProjectSceneUiState::waveformScaleChanged() const
{
    return m_waveformScaleChanged;
}
