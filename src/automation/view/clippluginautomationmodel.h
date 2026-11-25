/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include <QObject>
#include <QVariantList>
#include <QVariantMap>
#include "modularity/ioc.h"
#include "async/asyncable.h"
#include "context/iglobalcontext.h"
#include "trackedit/iprojecthistory.h"
#include "trackedit/iclipsinteraction.h"
#include "automation/iclipgaininteraction.h"
#include "projectscene/types/projectscenetypes.h"
#include "projectscene/iprojectviewstate.h"
#include "projectscene/iprojectsceneuistate.h"
#include "effects/effects_base/ieffectchainpresets.h"

namespace au::automation {
class ClipPluginAutomationModel : public QObject, public muse::Contextable, public muse::async::Asyncable
{
    Q_OBJECT
    Q_PROPERTY(projectscene::ClipKey clipKey READ clipKey WRITE setClipKey NOTIFY changed)
    Q_PROPERTY(QVariantList segments READ segments NOTIFY changed)
    Q_PROPERTY(bool visible READ visible NOTIFY changed)
    Q_PROPERTY(double sequenceStart READ sequenceStart NOTIFY changed)
    //! InTheMix: one beat of the project's tempo, in seconds (the grid points snap to).
    Q_PROPERTY(double beatSeconds READ beatSeconds NOTIFY changed)
    Q_PROPERTY(QString label READ label NOTIFY changed)
    Q_PROPERTY(QString color READ color NOTIFY changed)
    //! False when the line was overridden by moving its control (Ableton): drawn grey, not played.
    Q_PROPERTY(bool active READ active NOTIFY changed)
    //! The parameter has a line on this clip (else the lane shows its value, dashed).
    Q_PROPERTY(bool automated READ automated NOTIFY changed)
    //! A switch or stepped choice: the lane draws hard steps.
    Q_PROPERTY(bool stepped READ stepped NOTIFY changed)
    //! The clip's other automated parameters of the same track, drawn faintly
    //! behind the edited one so every line can be seen at once:
    //! [{ segments, color, stepped, active }].
    Q_PROPERTY(QVariantList otherLines READ otherLines NOTIFY changed)
    //! GlideMix: a lane of its own under the track (Ableton's "Show
    //! Automation in New Lane"): this parameter, whatever the focus is.
    //! { instanceId, pluginId, parameterId, label, color, defaultValue, stepped }
    Q_PROPERTY(QVariantMap lane READ lane WRITE setLane NOTIFY changed)
    //! GlideMix: one of the track header's Low/Mid/High/Color lines: drawn
    //! with the transition pill's glow under it (colorLine: towards the centre).
    Q_PROPERTY(bool bandLine READ bandLine NOTIFY changed)
    Q_PROPERTY(bool colorLine READ isColor NOTIFY changed)
    //! GlideMix 1.3.9 band knobs: the track's "show the knob lines" switch is on -- its Low / Mid /
    //! High / Color lines, drawn faintly together: [{ segments, color }] (not the one being edited).
    Q_PROPERTY(bool bandLinesShown READ bandLinesShown NOTIFY changed)
    Q_PROPERTY(QVariantList bandLines READ bandLines NOTIFY changed)
    //! GlideMix 1.3.9: Preferences > Waveforms > Shade what the band lines take away.
    Q_PROPERTY(bool bandFill READ bandFill NOTIFY changed)
    //! GlideMix 1.3.10: the line put in front with V / T / M / B / C (0 Low, 1 Mid, 2 High,
    //! 3 Color, 4 Volume; -1 none), and which of those the edited line is (-1: another plug-in line).
    Q_PROPERTY(int frontLine READ frontLine NOTIFY changed)
    Q_PROPERTY(int editedBand READ editedBand NOTIFY changed)
    muse::ContextInject<context::IGlobalContext> globalContext{this};
    muse::ContextInject<trackedit::IProjectHistory> projectHistory{this};
    muse::GlobalInject<effects::IMixxxEffectIds> mixxxEffectIds;
    muse::ContextInject<trackedit::IClipsInteraction> clipsInteraction{this};
    muse::ContextInject<IClipGainInteraction> clipGainInteraction{this};
    muse::ContextInject<projectscene::IProjectSceneUiState> sceneUiState{this};
public:
    explicit ClipPluginAutomationModel(QObject* parent = nullptr);
    Q_INVOKABLE void init();
    Q_INVOKABLE bool commit(const QVariantList& segments);
    //! GlideMix 1.3.9: one of the header's knob lines (0 Low, 1 Mid, 2 High, 3 Color),
    //! edited straight on the song while the knobs' eye is on.
    Q_INVOKABLE bool commitBandLine(int band, const QVariantList& segments);
    projectscene::ClipKey clipKey() const { return m_key; }
    void setClipKey(const projectscene::ClipKey& key) { m_key = key; emit changed(); }
    QVariantList segments() const;
    bool visible() const;
    double sequenceStart() const;
    double beatSeconds() const;
    QString label() const;
    QString color() const;
    bool active() const;
    bool automated() const;
    bool stepped() const;
    QVariantList otherLines() const;
    QVariantMap lane() const { return m_lane; }
    bool bandLine() const;
    bool bandLinesShown() const;
    QVariantList bandLines() const;
    bool bandFill() const;
    int frontLine() const;
    int editedBand() const;
    void setLane(const QVariantMap& lane) { m_lane = lane; emit changed(); }
    //! Shows this track's band lines (a line key pressed over a song whose lines are hidden).
    Q_INVOKABLE void showBandLines();
    //! The edited line gets a lane of its own under its track.
    Q_INVOKABLE void showInOwnLane();
signals:
    void changed();
    void editRejected(const QString& reason);
    //! A line key over a song: put a point on line `line` (0 Low .. 3 Color) under the mouse.
    void pointAtMouseRequested(int line);
private:
    projectscene::IProjectViewState::PluginAutomationFocus spec() const;
    projectscene::ClipKey m_key;
    QVariantMap m_lane;
private:
    //! GlideMix: the track header's Color button -- the Filter DJ effect's
    //! low-pass and high-pass shown as one line (open in the middle), as the
    //! transition pill's Color lane draws it.
    bool isColor() const;
    std::string filterEffectId() const;
    QVariantList colorSegments() const;
    //! GlideMix 1.3.10: a Low / Mid / High line (cut + boost) in view values.
    QVariantList bandSegments(int band) const;
    bool commitColor(const QVariantList& segments);
    bool commitCurve(const projectscene::IProjectViewState::PluginAutomationFocus& f, const QVariantList& segments,
                     const std::string& historyName, const std::string& historyAction);
};
}
