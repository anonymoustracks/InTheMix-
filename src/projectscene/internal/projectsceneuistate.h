/*
* Audacity: A Digital Audio Editor
*/
#pragma once

#include "../iprojectsceneuistate.h"

#include "framework/global/async/asyncable.h"
#include "framework/global/modularity/ioc.h"
#include "framework/ui/iuistate.h"
#include "framework/ui/iuiconfiguration.h"

namespace au::projectscene {
class ProjectSceneUiState : public IProjectSceneUiState, public muse::Contextable, public muse::async::Asyncable
{
    muse::ContextInject<muse::ui::IUiState> uiState { this };
    muse::GlobalInject<muse::ui::IUiConfiguration> uiConfiguration;

public:
    ProjectSceneUiState(const muse::modularity::ContextPtr& ctx)
        : muse::Contextable(ctx) {}

    void init();

    TimelineRulerMode timelineRulerMode() const override;
    void setTimelineRulerMode(TimelineRulerMode mode) override;
    muse::async::Notification timelineRulerModeChanged() const override;

    bool gridLinesVisible() const override;
    void setGridLinesVisible(bool visible) override;
    muse::async::Notification gridLinesVisibleChanged() const override;

    bool clipWaveformRgb() const override;
    void setClipWaveformRgb(bool rgb) override;
    muse::async::Notification clipWaveformRgbChanged() const override;
    bool clipWaveformStereo() const override;
    void setClipWaveformStereo(bool stereo) override;

    int laneBackground() const override;
    void setLaneBackground(int mode) override;
    muse::async::Notification laneBackgroundChanged() const override;
    bool blackCanvas() const override;
    bool studioCanvas() const override;
    int allTracksView() const override;
    void setAllTracksView(int view) override;
    muse::async::Notification allTracksViewChanged() const override;
    bool clipsSeeThrough() const override;
    void setClipsSeeThrough(bool on) override;
    muse::async::Notification clipsSeeThroughChanged() const override;
    bool bandLineFill() const override;
    void setBandLineFill(bool on) override;
    muse::async::Notification bandLineFillChanged() const override;
    double waveformScale() const override;
    void setWaveformScale(double scale) override;
    muse::async::Notification waveformScaleChanged() const override;

private:
    muse::async::Notification m_clipsSeeThroughChanged;
    muse::async::Notification m_bandLineFillChanged;
    muse::async::Notification m_waveformScaleChanged;
    muse::async::Notification m_timelineRulerModeChanged;
    muse::async::Notification m_gridLinesVisibleChanged;
    muse::async::Notification m_clipWaveformRgbChanged;
    muse::async::Notification m_laneBackgroundChanged;
    muse::async::Notification m_allTracksViewChanged;
};
}
