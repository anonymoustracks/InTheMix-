/*
 * GlideMix
 */
#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

#include "modularity/ioc.h"

#include "mixhouse/imixhousetransitionconfiguration.h"
#include "mixhouse/imixhouselibraryconfiguration.h"

namespace au::preferences {
/// Backs TransitionsPreferencesPage.qml: the list of transitions, the builder that edits one, and
/// what Mix Now does. Values cross to QML as plain maps: a transition is
///   { name, builtIn, lengthBars, volume: lane, bass: lane, filter, noise, effectOut }
/// and a lane is { curve, strength, crossing, leaveOutBeats } with curve 0..4 (None, Crossfade,
/// Fade in, Fade out, Swap) and strength and crossing in percent. filter is 0..4 (None, Out
/// high-pass, Out low-pass, In high-pass, In low-pass), noise 0..2 (None, Riser, Downsweep),
/// effectOut 0..2 (None, Echo, Reverb): the MixHouseFilterSweep / NoiseSweep / EffectOut values.
class TransitionsPreferencesModel : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    muse::GlobalInject<au::mixhouse::IMixHouseTransitionConfiguration> configuration;
    muse::GlobalInject<au::mixhouse::IMixHouseLibraryConfiguration> libraryConfiguration;

    /// [{ name, builtIn, summary }], the built-in ones first.
    Q_PROPERTY(QVariantList transitions READ transitions NOTIFY changed)
    /// [{ text, value }] for Mix Now's dropdown: "none", Mixtape's and the user's transitions, then MixMeister's.
    Q_PROPERTY(QVariantList mixNowChoices READ mixNowChoices NOTIFY changed)
    Q_PROPERTY(QString mixNowTransition READ mixNowTransition WRITE setMixNowTransition NOTIFY changed)
    /// 0 = the next bar, 1 = the next phrase.
    Q_PROPERTY(int mixNowLanding READ mixNowLanding WRITE setMixNowLanding NOTIFY changed)
    /// 0 = the transition window, 1 = the two-channel mixer.
    Q_PROPERTY(int panelView READ panelView WRITE setPanelView NOTIFY changed)
    /// The transition labels on the timeline (the toolbar's LABELS button too).
    Q_PROPERTY(bool showLabels READ showLabels WRITE setShowLabels NOTIFY changed)
    //! Match keys moves a song at most this many semitones (1 or 2).
    Q_PROPERTY(int matchKeysMaxSemitones READ matchKeysMaxSemitones WRITE setMatchKeysMaxSemitones NOTIFY changed)
    //! Auto-Keylock: percent off its own tempo above which a song keeps its pitch (0 = off).
    Q_PROPERTY(int autoKeylockPercent READ autoKeylockPercent WRITE setAutoKeylockPercent NOTIFY changed)

public:
    explicit TransitionsPreferencesModel(QObject* parent = nullptr);

    QVariantList transitions() const;
    QVariantList mixNowChoices() const;
    QString mixNowTransition() const;
    void setMixNowTransition(const QString& name);
    int mixNowLanding() const;
    void setMixNowLanding(int landing);
    int panelView() const;
    void setPanelView(int view);
    bool showLabels() const;
    int matchKeysMaxSemitones() const;
    void setMatchKeysMaxSemitones(int semitones);
    int autoKeylockPercent() const;
    void setAutoKeylockPercent(int percent);
    void setShowLabels(bool show);

    /// A fresh spec to start the builder from.
    Q_INVOKABLE QVariantMap newTransition() const;
    Q_INVOKABLE QVariantMap transition(const QString& name) const;
    /// Saves `spec` as the user's own transition. `originalName` is what it was called when it was
    /// opened (empty for a new one), so a rename replaces rather than duplicates. Returns an empty
    /// string on success, else what is wrong.
    Q_INVOKABLE QString save(const QVariantMap& spec, const QString& originalName);
    Q_INVOKABLE bool remove(const QString& name);
    /// The lines the builder draws, in beats from the mix point: { lengthBeats, volumeOut,
    /// volumeIn, bassOut, bassIn (each a list of [beat, 0..1]), error }.
    Q_INVOKABLE QVariantMap preview(const QVariantMap& spec) const;

signals:
    void changed();
};
}
