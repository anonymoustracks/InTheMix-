/*
* Audacity: A Digital Audio Editor
*/
#pragma once

#include "au3-components/BezierAutomation.h"
#include <array>
#include <utility>
#include <vector>

#include "framework/global/modularity/ioc.h"
#include "context/iglobalcontext.h"
#include "trackedit/iprojecthistory.h"
#include "audio/iaudioengine.h"
#include "effects/effects_base/ieffectsprovider.h"

#include "au3wrap/au3types.h"

#include "automation/iclipgaininteraction.h"

namespace au::automation {
class Au3ClipGainInteraction : public IClipGainInteraction, public muse::Contextable
{
    muse::ContextInject<au::context::IGlobalContext> globalContext{ this };
    muse::ContextInject<au::trackedit::IProjectHistory> projectHistory{ this };
    muse::GlobalInject<au::audio::IAudioEngine> audioEngine;
    muse::GlobalInject<au::effects::IEffectsProvider> effectsProvider;

public:
    Au3ClipGainInteraction(const muse::modularity::ContextPtr& ctx);

    std::optional<AutomationInfo> clipGainInfo(const trackedit::ClipKey& key) const override;
    AutomationPoints clipGainPoints(const trackedit::ClipKey& key) const override;
    AutomationSegments clipGainSegments(const trackedit::ClipKey& key) const override;
    bool setClipGainSegments(const trackedit::ClipKey& key, const AutomationSegments& segments, bool completed) override;
    bool setClipGainPoint(const trackedit::ClipKey& key, double tAbs, double value, bool completed) override;
    bool removeClipGainPoint(const trackedit::ClipKey& key, int index, bool completed) override;
    bool setClipGainPointAtIndex(const trackedit::ClipKey& key, int index, double tAbs, double value, bool completed) override;
    bool beginClipGainPointDrag(const trackedit::ClipKey& clip, int pointIndex) override;
    bool updateClipGainPointDrag(const trackedit::ClipKey& clip, double tAbs, double value) override;
    bool endClipGainPointDrag(const trackedit::ClipKey& clip, bool commit) override;
    muse::async::Channel<trackedit::ClipKey, bool> clipGainChanged() const override;

    bool clipDisabled(const trackedit::ClipKey& key) const override;
    bool setClipDisabled(const trackedit::ClipKey& key, bool disabled) override;
    AutomationPoints clipBassPoints(const trackedit::ClipKey& key) const override;
    bool setClipBassPoints(const trackedit::ClipKey& key, const AutomationPoints& points) override;
    AutomationPoints clipBandPoints(const trackedit::ClipKey& key, int band) const override;
    AutomationPoints clipBandLinePoints(const trackedit::ClipKey& key, int band) const override;
    bool setClipBandPoints(const trackedit::ClipKey& key, int band, const AutomationPoints& points) override;
    void beginBandShift(const trackedit::ClipKey& key, int band) override;
    bool applyBandShift(const trackedit::ClipKey& key, int band, double delta) override;
    void endBandShift() override;
    AutomationPoints clipEffectPoints(const trackedit::ClipKey& key, const std::string& effectId,
                                      const std::string& parameterId) const override;
    bool setClipEffectPoints(const trackedit::ClipKey& key, const std::string& effectId, const std::string& parameterId,
                             const AutomationPoints& points, bool addIfMissing = true) override;
    std::optional<bool> pluginAutomationActive(trackedit::TrackId trackId, const std::string& instanceId,
                                               const std::string& parameterId) const override;
    bool setPluginAutomationActive(trackedit::TrackId trackId, const std::string& instanceId,
                                   const std::string& parameterId, bool active) override;
    std::string trackEffectInstanceId(trackedit::TrackId trackId, const std::string& effectId) const override;
    std::map<std::string, bool> pluginAutomationStates(trackedit::TrackId trackId,
                                                       const std::string& instanceId) const override;
    bool hasOverriddenPluginAutomation() const override;
    bool reenableAllPluginAutomation() override;
    bool deletePluginAutomation(trackedit::TrackId trackId, const std::string& instanceId,
                                const std::string& parameterId) override;
    double recordPluginAutomation(trackedit::TrackId trackId, const std::string& instanceId, const std::string& pluginId,
                                  const std::string& parameterId, double time, double previousTime, double value,
                                  bool stepped) override;

private:
    au3::Au3Project& projectRef() const;
    void reconfigureEffect(au3::Au3WaveTrack& waveTrack, const std::string& instanceId);

    muse::async::Channel<au::trackedit::ClipKey, bool> m_clipEnvelopeChanged;
    std::optional<AutomationDragSession> m_envDrag;

    //! Band knobs: each clip's line as it was when the knob was taken (no line: `had` false).
    //! GlideMix 1.3.10: the band's one line (cut + boost, bandlevels.h view values).
    struct BandShiftBase {
        trackedit::ClipKey key;
        int band = 0;
        bool had = false;
        clipautomation::Curve line;
    };
    std::vector<BandShiftBase> m_bandShift;
};
}
