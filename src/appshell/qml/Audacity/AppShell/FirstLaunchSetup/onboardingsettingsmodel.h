/*
 * GlideMix: the choices of the first-start wizard's own pages (onboarding,
 * docs/onboarding-plan.md). Each one is an existing setting, written at once
 * like the other wizard pages do; Skip keeps whatever is set.
 */
#pragma once

#include <QObject>
#include <QtQml/qqmlregistration.h>

#include "async/asyncable.h"
#include "modularity/ioc.h"
#include "actions/iactionsdispatcher.h"

namespace au::appshell {
class OnboardingSettingsModel : public QObject, public muse::async::Asyncable, public muse::Contextable
{
    Q_OBJECT
    QML_ELEMENT

    muse::ContextInject<muse::actions::IActionsDispatcher> dispatcher { this };

    //! 0 transparent single wave, 1 single wave on colour, 2 dual (stereo), 3 colour waveform.
    Q_PROPERTY(int waveformLook READ waveformLook WRITE setWaveformLook NOTIFY changed)
    //! Colour waveform: left and right apart.
    Q_PROPERTY(bool colourStereo READ colourStereo WRITE setColourStereo NOTIFY changed)
    //! The whole window, 50-200 % (after a restart).
    Q_PROPERTY(int interfaceSize READ interfaceSize WRITE setInterfaceSize NOTIFY changed)
    //! Analysis speed: 0 Performance, 1 Normal, 2 Power saving.
    Q_PROPERTY(int analysisMode READ analysisMode WRITE setAnalysisMode NOTIFY changed)
    Q_PROPERTY(int keyNotation READ keyNotation WRITE setKeyNotation NOTIFY changed)
    Q_PROPERTY(bool keylock READ keylock WRITE setKeylock NOTIFY changed)
    //! DJ clock: every readout (Complete) or the standard set.
    Q_PROPERTY(bool clockComplete READ clockComplete WRITE setClockComplete NOTIFY changed)
    Q_PROPERTY(bool clockFlash READ clockFlash WRITE setClockFlash NOTIFY changed)
    //! The interface size GlideMix runs at now (the setting may differ until a restart).
    Q_PROPERTY(int runningInterfaceSize READ runningInterfaceSize CONSTANT)

public:
    explicit OnboardingSettingsModel(QObject* parent = nullptr);

    int waveformLook() const;
    void setWaveformLook(int look);
    bool colourStereo() const;
    void setColourStereo(bool on);
    int interfaceSize() const;
    void setInterfaceSize(int percent);
    int analysisMode() const;
    void setAnalysisMode(int mode);
    int keyNotation() const;
    void setKeyNotation(int notation);
    bool keylock() const;
    void setKeylock(bool on);
    bool clockComplete() const;
    void setClockComplete(bool on);
    bool clockFlash() const;
    void setClockFlash(bool on);
    int runningInterfaceSize() const;

    //! On a true first start (wizard never finished), select the recommended
    //! waveform look once per session; reopening the wizard later shows the
    //! user's own settings instead.
    Q_INVOKABLE void preselectRecommended();
    //! Restart GlideMix so a new Interface size applies; the wizard opens again
    //! on `resumePage` (0 = not from the wizard). The usual save question first.
    Q_INVOKABLE void restartToApplySize(int resumePage);

signals:
    void changed();
};
}
