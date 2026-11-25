#pragma once
#include <algorithm>
#include <set>
#include <vector>
#include <map>

#include <functional>
#include <string>

#include <memory>

#include "global/async/notification.h"
#include "global/async/channel.h"
#include "global/types/retval.h"

#include "trackedit/trackedittypes.h"
#include "types/projectscenetypes.h"
#include "types/secs.h"

namespace au::projectscene {
class IProjectViewState
{
public:
    //! Each plugin parameter's own lane colour, the same every time.
    static std::string pluginParameterColor(const std::string& instanceId, const std::string& parameterId)
    {
        static const char* const palette[] = {
            "#FFD43B", "#4DD0FF", "#FF6B9D", "#7CFF6B", "#FF9F43",
            "#B388FF", "#00E5C3", "#FF5252", "#E6FF4D", "#82B1FF",
        };
        return palette[std::hash<std::string>{}(instanceId + "/" + parameterId) % 10];
    }
    struct PluginAutomationFocus {
        trackedit::TrackId trackId{-1};
        std::string instanceId, pluginId, parameterId, label, color;
        double defaultValue{};
        bool visible{};
        bool stepped{};   // a switch or stepped choice: drawn and played as steps
    };
    //! GlideMix: one of the track header's band lines opened on its own (0 Low, 1 Mid, 2 High,
    //! 3 Color) -- what clicking its name under the knob shows.
    static PluginAutomationFocus bandLineFocus(trackedit::TrackId trackId, int band)
    {
        static const char* const parameter[4] = { "low", "mid", "high", "color" };
        static const char* const label[4] = { "Low", "Mid", "High", "Color" };
        static const char* const color[4] = { "#F0506E", "#F5A04A", "#4A9BFF", "#A07CF5" };
        PluginAutomationFocus focus;
        focus.trackId = trackId;
        // The built-in band EQ; Color = the Filter effect's two lines shown as one.
        focus.instanceId = band == 3 ? "glidemix-color" : "b7c3e0a2-4d1f-4c8e-9a55-6f1d2e7a4b10";
        focus.pluginId = band == 3 ? "" : "mixtape.builtin";
        focus.parameterId = parameter[band];
        focus.label = label[band];
        focus.color = color[band];
        focus.defaultValue = band == 3 ? 0.5 : 1.0;   // Color open / the band at full
        focus.visible = true;
        return focus;
    }
    //! Which band line is open on its own (0..3), or -1.
    int openBandLine() const
    {
        const auto& f = m_pluginFocus;
        if (!f.visible) {
            return -1;
        }
        for (int band = 0; band < 4; ++band) {
            const auto b = bandLineFocus(f.trackId, band);
            if (f.instanceId == b.instanceId && f.parameterId == b.parameterId) {
                return band;
            }
        }
        return -1;
    }
    // Transient, project-local editing focus. No curve points live on a track.
    const PluginAutomationFocus& pluginAutomationFocus() const { return m_pluginFocus; }
    void setPluginAutomationFocus(PluginAutomationFocus focus)
    { m_pluginFocus = std::move(focus); m_pluginFocusChanged.notify(); }
    muse::async::Notification pluginAutomationFocusChanged() const { return m_pluginFocusChanged; }
    //! GlideMix (2026-11-21): the track row under the mouse, its header or its lane
    //! (-1 = none), so both light up together like the reference DAW's rows.
    trackedit::TrackId hoveredRow() const { return m_hoveredRow; }
    void setHoveredRow(trackedit::TrackId id)
    {
        if (m_hoveredRow != id) {
            m_hoveredRow = id;
            m_hoveredRowChanged.notify();
        }
    }
    muse::async::Notification hoveredRowChanged() const { return m_hoveredRowChanged; }
    //! GlideMix: Ableton's "Show Automation in New Lane": parameters shown in
    //! lanes of their own under a track, each editable directly.
    static constexpr int kAutomationLaneHeight = 56;
    using AutomationLane = PluginAutomationFocus;
    const std::vector<AutomationLane>& automationLanes(trackedit::TrackId trackId) const
    {
        static const std::vector<AutomationLane> none;
        const auto it = m_automationLanes.find(trackId);
        return it == m_automationLanes.end() ? none : it->second;
    }
    //! False when the parameter already has a lane on that track.
    bool addAutomationLane(trackedit::TrackId trackId, AutomationLane lane)
    {
        auto& lanes = m_automationLanes[trackId];
        for (const auto& l : lanes) {
            if (l.instanceId == lane.instanceId && l.parameterId == lane.parameterId) {
                return false;
            }
        }
        lane.trackId = trackId;
        lane.visible = true;
        lanes.push_back(std::move(lane));
        saveAutomationLanes(trackId);
        m_automationLanesChanged.notify();
        return true;
    }
    bool removeAutomationLane(trackedit::TrackId trackId, const std::string& instanceId, const std::string& parameterId)
    {
        auto it = m_automationLanes.find(trackId);
        if (it == m_automationLanes.end()) {
            return false;
        }
        auto& lanes = it->second;
        const auto before = lanes.size();
        lanes.erase(std::remove_if(lanes.begin(), lanes.end(), [&](const AutomationLane& l) {
            return l.instanceId == instanceId && l.parameterId == parameterId;
        }), lanes.end());
        if (lanes.size() == before) {
            return false;
        }
        saveAutomationLanes(trackId);
        m_automationLanesChanged.notify();
        return true;
    }
    muse::async::Notification automationLanesChanged() const { return m_automationLanesChanged; }

    //! GlideMix 1.3.9 band knobs: the track shows its Low / Mid / High / Color and volume lines
    //! together, faintly (the knobs' own show/hide button). Not saved with the mix.
    bool bandLinesShown(trackedit::TrackId trackId) const { return m_bandLinesShown.count(trackId) > 0; }
    void setBandLinesShown(trackedit::TrackId trackId, bool shown)
    {
        if (shown == bandLinesShown(trackId)) {
            return;
        }
        if (shown) {
            m_bandLinesShown.insert(trackId);
        } else {
            m_bandLinesShown.erase(trackId);
        }
        m_bandLinesShownChanged.notify();
    }
    muse::async::Notification bandLinesShownChanged() const { return m_bandLinesShownChanged; }

    //! GlideMix 1.3.10 (MixMeister's V / T / M / B keys): the line put in front of the others on
    //! every song -- 0 Low, 1 Mid, 2 High, 3 Color, 4 Volume; -1 = none (a press takes the
    //! nearest line). Only that line takes clicks while set. Not saved with the mix.
    int frontLine() const { return m_frontLine; }
    void setFrontLine(int line)
    {
        if (line == m_frontLine) {
            return;
        }
        m_frontLine = line;
        m_bandLinesShownChanged.notify();
    }
    //! GlideMix 1.3.10 (MixMeister's Add Bass/Midrange/Treble Marker): a line key asks the song
    //! under the mouse, if any, to put a point on that line there (0 Low .. 3 Color).
    void requestPointAtMouse(int line) { m_pointAtMouseRequested.send(line); }
    muse::async::Channel<int> pointAtMouseRequested() const { return m_pointAtMouseRequested; }

    //! Ableton's Automation Arm: while playing, moving a plugin control writes its line.
    bool automationArmed() const { return m_automationArmed; }
    void setAutomationArmed(bool armed) { m_automationArmed = armed; m_pluginFocusChanged.notify(); }
    virtual ~IProjectViewState() = default;

    //State of elements
    virtual muse::ValCh<int> totalTrackHeight() const = 0;
    virtual muse::ValCh<int> trackHeight(const trackedit::TrackId& trackId) const = 0;
    virtual muse::ValCh<bool> isTrackCollapsed(const trackedit::TrackId& trackId) const = 0;
    virtual muse::ValCh<double> channelHeightRatio(const trackedit::TrackId& trackId) const = 0;

    virtual int trackVerticalPosition(const trackedit::TrackId& trackId) const = 0;
    virtual void changeTrackHeight(const trackedit::TrackId& trackId, int deltaY) = 0;
    virtual void setTrackHeight(const trackedit::TrackId& trackId, int height) = 0;
    //! GlideMix: one height for several tracks, laid out once (smooth track zoom).
    virtual void setTracksHeight(const trackedit::TrackIdList& trackIds, int height) = 0;
    //! GlideMix: changes to many lanes count as one: the same lanes stay in
    //! view, and one undo state.
    virtual void beginTracksChange() = 0;
    virtual void endTracksChange() = 0;
    virtual void setChannelHeightRatio(const trackedit::TrackId& trackId, double ratio) = 0;
    virtual trackedit::TrackId trackAtPosition(double y) const = 0;
    virtual trackedit::TrackIdList tracksInRange(double y1, double y2) const = 0;

    virtual bool isSnapEnabled() const = 0;
    virtual void setIsSnapEnabled(bool enabled) = 0;

    virtual SnapType snapType() const = 0;
    virtual void setSnapType(SnapType type) = 0;

    virtual bool isSnapTripletsEnabled() const = 0;
    virtual void setIsSnapTripletsEnabled(bool enabled) = 0;

    virtual void setSnap(const Snap& s) = 0;
    virtual Snap getSnap() const = 0;
    virtual muse::ValCh<Snap> snap() const = 0;

    virtual void setClipGainAutomationEnabled(bool enabled) = 0;
    virtual muse::ValCh<bool> clipGainAutomationEnabled() const = 0;

    virtual void setSplitToolEnabled(bool enabled) = 0;
    virtual muse::ValCh<bool> splitToolEnabled() const = 0;

    virtual muse::ValCh<std::pair<float, float> > verticalDisplayBounds(const trackedit::TrackId& trackId) const = 0;
    virtual void zoomInVertically(const trackedit::TrackId& trackId) = 0;
    virtual void zoomOutVertically(const trackedit::TrackId& trackId) = 0;
    virtual void resetVerticalZoom(const trackedit::TrackId& trackId) = 0;
    virtual bool isDefaultVerticalZoom(const trackedit::TrackId& trackId) const = 0;
    virtual bool isMaxVerticalZoom(const trackedit::TrackId& trackId) const = 0;
    virtual bool isMinVerticalZoom(const trackedit::TrackId& trackId) const = 0;

    virtual muse::ValCh<int> verticalRulerWidth() const = 0;

    virtual muse::ValCh<bool> isHalfWave(const trackedit::TrackId& trackId) const = 0;
    virtual void toggleHalfWave(const trackedit::TrackId& trackId) = 0;
    //! GlideMix: a stereo track drawn as one waveform in half the height.
    virtual muse::ValCh<bool> isSingleWave(const trackedit::TrackId& trackId) const = 0;
    virtual void toggleSingleWave(const trackedit::TrackId& trackId) = 0;
    //! GlideMix Samples (B2): the track is a song's sample line (saved with the project).
    virtual bool isSampleLine(const trackedit::TrackId& trackId) const { (void)trackId; return false; }
    virtual void setSampleLine(const trackedit::TrackId& trackId, bool sampleLine) { (void)trackId; (void)sampleLine; }

    virtual muse::ValCh<trackedit::TrackViewType> trackViewType(const trackedit::TrackId& trackId) const = 0;
    virtual void setTrackViewType(const trackedit::TrackId& trackId, trackedit::TrackViewType viewType) = 0;
    virtual void toggleGlobalSpectrogramView() = 0;
    virtual bool globalSpectrogramToggleIsOn() const = 0;
    virtual muse::async::Notification globalSpectrogramToggleIsOnChanged() const = 0;

    virtual muse::ValCh<int> trackRulerType(const trackedit::TrackId& trackId) const = 0;
    virtual void setTrackRulerType(const trackedit::TrackId& trackId, int rulerType) = 0;

    //State of user interaction
    virtual double mousePositionY() const = 0;
    virtual void setMousePositionY(double y) = 0;

    virtual muse::ValCh<int> tracksVerticalOffset() const = 0;
    virtual void changeTracksVerticalOffset(int deltaY) = 0;
    virtual muse::ValCh<bool> tracksVerticalScrollLocked() const = 0;
    virtual void setTracksVerticalScrollLocked(bool lock) = 0;

    virtual void setItemEditStartTimeOffset(double val) = 0;
    virtual double itemEditStartTimeOffset() const = 0;

    virtual void setItemEditEndTimeOffset(double val) = 0;
    virtual double itemEditEndTimeOffset() const = 0;

    virtual void setMoveInitiated(bool val) = 0;
    virtual bool moveInitiated() const = 0;

    virtual void setLastEditedClip(const trackedit::ClipKey& clipKey) = 0;
    virtual trackedit::ClipKey lastEditedClip() const = 0;

    //! Key of the item currently being moved/trimmed/stretched. While set, it is always
    //! omitted from itemsBoundaries so a drag never snaps the item to its own edges, even
    //! when the boundary set is rebuilt mid-drag (e.g. from trackChanged notifications).
    virtual void setEditedItem(const trackedit::TrackItemKey& key) = 0;

    virtual void setItemsBoundaries(const std::set<muse::secs_t>& boundaries) = 0;
    virtual std::set<muse::secs_t> itemsBoundaries() const = 0;
    virtual void updateItemsBoundaries(bool excludeCurrentSelection,
                                       const trackedit::TrackItemKey& itemKeyToOmit = trackedit::TrackItemKey {}) = 0;

    virtual void setZoomState(const ZoomState& state) = 0;
    virtual ZoomState zoomState() const = 0;
    //! Fires whenever the timeline saves a new zoom or scroll position
    virtual muse::async::Notification zoomStateChanged() const = 0;

    virtual muse::async::Notification rolledBack() const = 0;

    virtual muse::ValCh<bool> altPressed() const = 0;
    virtual muse::ValCh<bool> ctrlPressed() const = 0;

    virtual muse::ValCh<bool> keyboardMoveActive() const = 0;
    virtual void setKeyboardMoveActive(bool active) = 0;

    virtual muse::async::Notification modifiersReleased() const = 0;

    virtual int trackDefaultHeight() const = 0;
protected:
    //! Writes a track's lanes into the project (saved with it).
    virtual void saveAutomationLanes(trackedit::TrackId) {}
    void notifyAutomationLanesChanged() const { m_automationLanesChanged.notify(); }
    mutable std::map<trackedit::TrackId, std::vector<AutomationLane> > m_automationLanes;
private:
    PluginAutomationFocus m_pluginFocus;
    trackedit::TrackId m_hoveredRow = -1;
    muse::async::Notification m_hoveredRowChanged;
    muse::async::Notification m_pluginFocusChanged;
    bool m_automationArmed = false;
    mutable muse::async::Notification m_automationLanesChanged;
    std::set<trackedit::TrackId> m_bandLinesShown;
    int m_frontLine = -1;
    muse::async::Channel<int> m_pointAtMouseRequested;
    muse::async::Notification m_bandLinesShownChanged;
};

using IProjectViewStatePtr = std::shared_ptr<IProjectViewState>;
}
