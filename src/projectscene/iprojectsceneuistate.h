/*
* Audacity: A Digital Audio Editor
*/
#pragma once

#include "global/async/notification.h"
#include "modularity/imoduleinterface.h"

#include "types/projectscenetypes.h"

namespace au::projectscene {
class IProjectSceneUiState : MODULE_CONTEXT_INTERFACE
{
    INTERFACE_ID(IProjectSceneUiState)

public:
    virtual ~IProjectSceneUiState() = default;

    virtual TimelineRulerMode timelineRulerMode() const = 0;
    virtual void setTimelineRulerMode(TimelineRulerMode mode) = 0;
    virtual muse::async::Notification timelineRulerModeChanged() const = 0;

    //! GlideMix, per workspace: the bar and beat lines behind the tracks
    //! (and their zebra shading). Off is the rekordbox look -- the clips carry
    //! their own red bar markers and white beat ticks, so the lines are only a
    //! second grid drawn over them.
    virtual bool gridLinesVisible() const = 0;
    virtual void setGridLinesVisible(bool visible) = 0;
    virtual muse::async::Notification gridLinesVisibleChanged() const = 0;

    //! GlideMix, per workspace: draw clips as rekordbox's colour
    //! waveform (blue bass, orange mids, white highs on black) instead of
    //! Audacity's single-colour one.
    virtual bool clipWaveformRgb() const = 0;
    virtual void setClipWaveformRgb(bool rgb) = 0;
    virtual muse::async::Notification clipWaveformRgbChanged() const = 0;
    //! GlideMix: the colour waveform as two lanes, left and right, each channel analysed apart.
    virtual bool clipWaveformStereo() const = 0;
    virtual void setClipWaveformStereo(bool stereo) = 0;

    //! GlideMix, per workspace: what the lanes are painted with.
    //! 0 follows the waveform style (black under the colour waveform, the
    //! theme's grey otherwise), 1 is always black, 2 always the theme.
    virtual int laneBackground() const = 0;
    virtual void setLaneBackground(int mode) = 0;
    virtual muse::async::Notification laneBackgroundChanged() const = 0;

    //! GlideMix: how every song's track is drawn (View > Track view,
    //! Preferences > Waveforms): 0 Waveform, 1 Single wave, 2 Half-wave,
    //! 3 Spectrogram, 4 Multi-view; -1 = never chosen (each track keeps its own).
    //! An app setting; songs added later get it too.
    virtual int allTracksView() const = 0;
    virtual void setAllTracksView(int view) = 0;
    virtual muse::async::Notification allTracksViewChanged() const = 0;

    //! What laneBackground() comes out as right now.
    virtual bool blackCanvas() const = 0;
    //! GlideMix (Chris, 2026-11-21): the Charcoal canvas measured from the reference
    //! DAW: rows and the empty space one colour (#131417), no tint per row. True for
    //! the Charcoal theme (lanes following it) and the Charcoal lane background.
    virtual bool studioCanvas() const = 0;

    //! GlideMix (View > Transparent clip background): every clip drawn with
    //! no body, as Color intensity 0 does for one track -- the lane and the
    //! ruler's grid show under the waveform. An app setting.
    virtual bool clipsSeeThrough() const = 0;
    virtual void setClipsSeeThrough(bool on) = 0;
    virtual muse::async::Notification clipsSeeThroughChanged() const = 0;

    //! GlideMix 1.3.9: the colour fill under the Low / Mid / High / Color lines.
    virtual bool bandLineFill() const = 0;
    virtual void setBandLineFill(bool on) = 0;
    virtual muse::async::Notification bandLineFillChanged() const = 0;

    //! GlideMix (Chris, 2026-11-21): how big the waveform is drawn in every clip,
    //! 0.2 .. 1.5 (1 = as recorded). The toolbar's wave button sets it.
    virtual double waveformScale() const = 0;
    virtual void setWaveformScale(double scale) = 0;
    virtual muse::async::Notification waveformScaleChanged() const = 0;
};
}
