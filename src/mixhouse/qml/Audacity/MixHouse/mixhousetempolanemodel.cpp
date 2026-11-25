/*
 * GlideMix
 */
#include "mixhousetempolanemodel.h"

using namespace au::mixhouse;

MixHouseTempoLaneModel::MixHouseTempoLaneModel(QObject* parent)
    : QObject(parent), muse::Contextable(muse::iocCtxForQmlObject(this))
{
    m_followHintTimer.setSingleShot(true);
    m_followHintTimer.setInterval(8000);
    QObject::connect(&m_followHintTimer, &QTimer::timeout, this, [this]() { setFollowHint(false); });
}

void MixHouseTempoLaneModel::componentComplete()
{
    if (configuration()) {
        configuration()->settingsChanged().onNotify(this, [this]() { emit laneChanged(); });
    }
    emit laneChanged();
    if (tempoMap()) {
        tempoMap()->tempoMapChanged().onNotify(this, [this]() {
            emit tempoChanged();
            if (followsTracks()) {
                setFollowHint(false);   // chosen: no more pointing at it
            }
        });
        tempoMap()->doubleTimeMatched().onNotify(this, [this]() { setFollowHint(true); });
    }
    if (keyMatch()) {
        keyMatch()->matchKeysChanged().onNotify(this, [this]() { emit matchKeysChanged(); });
    }
}

bool MixHouseTempoLaneModel::matchKeys() const
{
    return keyMatch() && keyMatch()->matchKeysOn();
}

void MixHouseTempoLaneModel::setMatchKeys(bool on)
{
    if (keyMatch()) {
        keyMatch()->setMatchKeysOn(on);
    }
}

bool MixHouseTempoLaneModel::keyClashHint() const
{
    return keyMatch() && keyMatch()->clashHint();
}

int MixHouseTempoLaneModel::matchKeysMaxSemitones() const
{
    return configuration() ? configuration()->matchKeysMaxSemitones() : 1;
}

bool MixHouseTempoLaneModel::followsTracks() const
{
    return tempoMap() && tempoMap()->followsTracks();
}

void MixHouseTempoLaneModel::setFollowsTracks(bool follow)
{
    if (tempoMap()) {
        tempoMap()->setFollowsTracks(follow);
    }
}

void MixHouseTempoLaneModel::nudgeMasterBpm(double deltaBpm)
{
    if (!tempoMap()) {
        return;
    }
    const auto playback = globalContext() ? globalContext()->playbackState() : nullptr;
    const double at = playback ? playback->playbackPosition().to_double() : 0.0;
    tempoMap()->nudgeMasterBpm(deltaBpm, at);
}

bool MixHouseTempoLaneModel::laneVisible() const
{
    return configuration() && configuration()->tempoLaneVisible();
}

bool MixHouseTempoLaneModel::energyLaneVisible() const
{
    return configuration() && configuration()->energyLaneVisible();
}

bool MixHouseTempoLaneModel::transitionLabelsVisible() const
{
    return !configuration() || configuration()->transitionLabelsVisible();
}

void MixHouseTempoLaneModel::setTransitionLabelsVisible(bool visible)
{
    if (configuration() && visible != configuration()->transitionLabelsVisible()) {
        configuration()->setTransitionLabelsVisible(visible);
    }
}

int MixHouseTempoLaneModel::laneHeight() const
{
    return configuration() ? configuration()->tempoLaneHeight() : defaultLaneHeight();
}

void MixHouseTempoLaneModel::setLaneHeight(int height)
{
    if (configuration() && height != laneHeight()) {
        configuration()->setTempoLaneHeight(height);
    }
}

void MixHouseTempoLaneModel::reset()
{
    if (tempoMap()) {
        tempoMap()->reset();
    }
}

void MixHouseTempoLaneModel::setFollowHint(bool on)
{
    if (on) {
        m_followHintTimer.start();
    }
    if (on == m_followHint) {
        return;
    }
    m_followHint = on;
    emit followHintChanged();
}
