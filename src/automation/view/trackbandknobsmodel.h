/*
 * GlideMix
 */
#pragma once

#include <QObject>
#include <QTimer>
#include <QVariantList>

#include "async/asyncable.h"
#include "modularity/ioc.h"
#include "context/iglobalcontext.h"
#include "trackedit/iprojecthistory.h"
#include "automation/iclipgaininteraction.h"
#include "effects/effects_base/ieffectchainpresets.h"

namespace au::automation {
/// GlideMix 1.3.9 (Chris, 2026-11-16): LOW / MID / HIGH / COLOR knobs in a
/// song's track header. A knob shows its band's level at the playhead; turning
/// it moves that band's WHOLE line on the track up or down, keeping its shape.
/// Middle = no change (a band at full, Color open). Bands can only be cut (the
/// engine has no boost), so LOW / MID / HIGH stop at the middle on the right;
/// COLOR's left is the low-pass, its right the high-pass. One undo step per turn.
class TrackBandKnobsModel : public QObject, public muse::Contextable, public muse::async::Asyncable
{
    Q_OBJECT
    Q_PROPERTY(QVariant trackId READ trackId WRITE setTrackId NOTIFY trackIdChanged)
    //! -100..100 per knob (LOW, MID, HIGH, COLOR): the band at the playhead.
    Q_PROPERTY(QVariantList values READ values NOTIFY valuesChanged)
    //! The track's knob lines are shown together (the knobs' own show button).
    Q_PROPERTY(bool linesShown READ linesShown WRITE setLinesShown NOTIFY linesShownChanged)

    muse::ContextInject<context::IGlobalContext> globalContext { this };
    muse::ContextInject<trackedit::IProjectHistory> projectHistory { this };
    muse::ContextInject<IClipGainInteraction> clipGainInteraction { this };
    muse::GlobalInject<effects::IMixxxEffectIds> mixxxEffectIds;

public:
    explicit TrackBandKnobsModel(QObject* parent = nullptr);

    QVariant trackId() const { return QVariant::fromValue(m_trackId); }
    void setTrackId(const QVariant& id);
    QVariantList values() const;
    bool linesShown() const;
    void setLinesShown(bool shown);

    Q_INVOKABLE void init();
    //! The knob is taken: keeps every clip's line as it is now.
    Q_INVOKABLE void begin(int band);
    //! The knob is at `knob` (-100..100): the line moved by how far it turned since begin().
    Q_INVOKABLE void turn(int band, double knob);
    //! Let go: one undo step ("Moved the Low line").
    Q_INVOKABLE void end(int band);
    //! GlideMix 1.3.10: the knob's value as MixMeister shows it ("+4.0 dB"); Color in %.
    Q_INVOKABLE QString knobText(int band, double knob) const;

signals:
    void trackIdChanged();
    void valuesChanged();
    void linesShownChanged();

private:
    std::vector<trackedit::ClipKey> clips() const;
    double valueAt(int band, double time) const;   // 0..1 at project time
    std::string filterEffectId() const;
    static double knobFromValue(int band, double value);
    static double valueFromKnob(int band, double knob);

    trackedit::TrackId m_trackId = -1;
    int m_band = -1;
    double m_startKnob = 0.0;
    bool m_moved = false;
    // COLOR: each clip's low-pass and high-pass lines when the knob was taken.
    struct ColorBase {
        trackedit::ClipKey key;
        AutomationPoints lpf, hpf;
    };
    std::vector<ColorBase> m_colorBase;
    QTimer m_refresh;
};
}
