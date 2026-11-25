/*
 * GlideMix
 */
#pragma once

#include <QObject>
#include <QTimer>
#include <QQmlParserStatus>
#include <QtQmlIntegration>

#include "async/asyncable.h"
#include "modularity/ioc.h"

#include "mixhouse/imixhousekeymatch.h"
#include "mixhouse/imixhouselibraryconfiguration.h"
#include "mixhouse/imixhousetempomap.h"
#include "context/iglobalcontext.h"

namespace au::mixhouse {
/// State shared by the TEMPO lane (under the ruler) and its header row in the
/// tracks panel: whether it is shown, its height, and the Reset button.
class MixHouseTempoLaneModel : public QObject, public QQmlParserStatus, public muse::Contextable,
    public muse::async::Asyncable
{
    Q_OBJECT
    Q_INTERFACES(QQmlParserStatus)
    QML_ELEMENT

    Q_PROPERTY(bool laneVisible READ laneVisible NOTIFY laneChanged)
    Q_PROPERTY(bool energyLaneVisible READ energyLaneVisible NOTIFY laneChanged)
    //! The transition labels on the timeline (toolbar LABELS, Preferences → Transitions).
    Q_PROPERTY(bool transitionLabelsVisible READ transitionLabelsVisible WRITE setTransitionLabelsVisible NOTIFY laneChanged)
    Q_PROPERTY(int laneHeight READ laneHeight WRITE setLaneHeight NOTIFY laneChanged)
    Q_PROPERTY(int defaultLaneHeight READ defaultLaneHeight CONSTANT)
    Q_PROPERTY(bool followsTracks READ followsTracks WRITE setFollowsTracks NOTIFY tempoChanged)
    //! A song was just matched double-time: Follow tracks blinks for a few seconds.
    Q_PROPERTY(bool followHint READ followHint NOTIFY followHintChanged)
    //! Match keys (the key twin of Follow tracks), and its flash on a new clash.
    Q_PROPERTY(bool matchKeys READ matchKeys WRITE setMatchKeys NOTIFY matchKeysChanged)
    Q_PROPERTY(bool keyClashHint READ keyClashHint NOTIFY matchKeysChanged)
    Q_PROPERTY(int matchKeysMaxSemitones READ matchKeysMaxSemitones NOTIFY laneChanged)

    muse::GlobalInject<IMixHouseLibraryConfiguration> configuration;
    muse::ContextInject<IMixHouseTempoMap> tempoMap { this };
    muse::ContextInject<context::IGlobalContext> globalContext { this };
    muse::ContextInject<IMixHouseKeyMatch> keyMatch { this };

public:
    explicit MixHouseTempoLaneModel(QObject* parent = nullptr);

    bool laneVisible() const;
    bool energyLaneVisible() const;
    bool transitionLabelsVisible() const;
    void setTransitionLabelsVisible(bool visible);
    int laneHeight() const;
    void setLaneHeight(int height);
    int defaultLaneHeight() const { return 96; }

    Q_INVOKABLE void reset();
    Q_INVOKABLE void nudgeMasterBpm(double deltaBpm);

    bool followsTracks() const;
    void setFollowsTracks(bool follow);
    bool followHint() const { return m_followHint; }
    bool matchKeys() const;
    void setMatchKeys(bool on);
    bool keyClashHint() const;
    int matchKeysMaxSemitones() const;

    void classBegin() override {}
    void componentComplete() override;

signals:
    void laneChanged();
    void tempoChanged();
    void followHintChanged();
    void matchKeysChanged();

private:
    void setFollowHint(bool on);
    bool m_followHint = false;
    QTimer m_followHintTimer;
};
}
