#include "onboardingsettingsmodel.h"

#include <algorithm>
#include <cmath>

#include <QCoreApplication>
#include <QStringList>
#include <QTimer>

#include "global/settings.h"
#include "appshell/interfacesize.h"

using namespace au::appshell;
using muse::Settings;
using muse::Val;

namespace {
// The settings these choices are (their owners: projectscene, mixhouse, appshell).
const Settings::Key TRACK_VIEW("projectscene", "projectscene/allTracksView");     // 0 Waveform, 1 Single wave
const Settings::Key COLOUR_WAVEFORM("projectscene", "projectscene/colorWaveform");
const Settings::Key COLOUR_STEREO("projectscene", "projectscene/colorWaveformStereo");
const Settings::Key SEE_THROUGH("projectscene", "projectscene/clipsSeeThrough");
const Settings::Key ANALYSIS_MODE("mixhouse", "mixhouse/library/analysisMode");
const Settings::Key KEY_NOTATION("mixhouse", "mixhouse/library/keyNotation");
const Settings::Key KEYLOCK("mixhouse", "mixhouse/deck/keylock");
const Settings::Key CLOCK_SHOWN("mixhouse", "mixhouse/clock/shown");
const Settings::Key CLOCK_FLASH("mixhouse", "mixhouse/clock/flash");
const Settings::Key INTERFACE_SIZE("appshell", kInterfaceSizeKey);

// Same lists as MixHouseClockDisplays (mixhouseclockdisplays.cpp).
constexpr const char* kClockStandard = "songtime,barbeat,nextmix,key,song";
constexpr const char* kClockComplete = "mixtotal,songtime,barbeat,phrase,nextmix,tempo,key,energy,song,cpu,disk,memory";

Val value(const Settings::Key& key)
{
    return muse::settings()->value(key);
}

void set(const Settings::Key& key, const Val& v)
{
    muse::settings()->setSharedValue(key, v);
}
}

OnboardingSettingsModel::OnboardingSettingsModel(QObject* parent)
    : QObject(parent), muse::Contextable(muse::iocCtxForQmlObject(this))
{
}

void OnboardingSettingsModel::restartToApplySize(int resumePage)
{
    set(Settings::Key("appshell", "appshell/onboardingResumePage"), Val(std::max(0, resumePage)));
    // Not from inside the button's click: the restart closes the wizard (or
    // Preferences) and deleting a window while its own click handler still
    // runs aborts Qt ("destroyed while one of its QML signal handlers is in
    // progress" -- Chris's crash, 2026-11-13). Run it once the click is done;
    // the dispatcher outlives this model, so the queued call needs no `this`.
    auto actions = dispatcher();
    QTimer::singleShot(0, qApp, [actions]() {
        if (actions) {
            actions->dispatch("restart");
        }
    });
}

int OnboardingSettingsModel::waveformLook() const
{
    if (value(COLOUR_WAVEFORM).toBool()) {
        return 3;
    }
    if (value(TRACK_VIEW).toInt() == 0) {
        return 2;
    }
    return value(SEE_THROUGH).toBool() ? 0 : 1;
}

void OnboardingSettingsModel::setWaveformLook(int look)
{
    switch (look) {
    case 0: // transparent single wave
        set(COLOUR_WAVEFORM, Val(false));
        set(TRACK_VIEW, Val(1));
        set(SEE_THROUGH, Val(true));
        break;
    case 1: // single wave on colour
        set(COLOUR_WAVEFORM, Val(false));
        set(TRACK_VIEW, Val(1));
        set(SEE_THROUGH, Val(false));
        break;
    case 2: // dual (stereo): both channels
        set(COLOUR_WAVEFORM, Val(false));
        set(TRACK_VIEW, Val(0));
        set(SEE_THROUGH, Val(false));
        break;
    case 3: // colour waveform on black
        set(TRACK_VIEW, Val(1));
        set(SEE_THROUGH, Val(false));
        set(COLOUR_WAVEFORM, Val(true));
        break;
    default:
        return;
    }
    emit changed();
}

void OnboardingSettingsModel::preselectRecommended()
{
    static bool done = false;
    if (done) {
        return;
    }
    done = true;
    if (value(Settings::Key("appshell", "application/hasCompletedFirstLaunchSetup")).toBool()) {
        return;
    }
    setWaveformLook(0);   // transparent single wave (Chris's pick)
}

bool OnboardingSettingsModel::colourStereo() const { return value(COLOUR_STEREO).toBool(); }
void OnboardingSettingsModel::setColourStereo(bool on) { set(COLOUR_STEREO, Val(on)); emit changed(); }

int OnboardingSettingsModel::interfaceSize() const
{
    const int v = value(INTERFACE_SIZE).toInt();
    return v >= 50 && v <= 200 ? v : 100;
}

void OnboardingSettingsModel::setInterfaceSize(int percent)
{
    set(INTERFACE_SIZE, Val(std::clamp(percent, 50, 200)));
    emit changed();
}

int OnboardingSettingsModel::runningInterfaceSize() const
{
    const QByteArray env = qgetenv("QT_SCALE_FACTOR");
    return env.isEmpty() ? 100 : int(std::lround(env.toDouble() * 100.0));
}

int OnboardingSettingsModel::analysisMode() const { return std::clamp(value(ANALYSIS_MODE).toInt(), 0, 2); }
void OnboardingSettingsModel::setAnalysisMode(int mode) { set(ANALYSIS_MODE, Val(std::clamp(mode, 0, 2))); emit changed(); }
int OnboardingSettingsModel::keyNotation() const { return value(KEY_NOTATION).toInt(); }
void OnboardingSettingsModel::setKeyNotation(int notation) { set(KEY_NOTATION, Val(notation)); emit changed(); }
bool OnboardingSettingsModel::keylock() const { return value(KEYLOCK).toBool(); }
void OnboardingSettingsModel::setKeylock(bool on) { set(KEYLOCK, Val(on)); emit changed(); }

bool OnboardingSettingsModel::clockComplete() const
{
    return QString::fromStdString(value(CLOCK_SHOWN).toString()).split(',', Qt::SkipEmptyParts).size()
           >= QString::fromLatin1(kClockComplete).split(',').size();
}

void OnboardingSettingsModel::setClockComplete(bool on)
{
    set(CLOCK_SHOWN, Val(std::string(on ? kClockComplete : kClockStandard)));
    emit changed();
}

bool OnboardingSettingsModel::clockFlash() const
{
    const Val v = value(CLOCK_FLASH);
    return v.isNull() ? true : v.toBool();
}

void OnboardingSettingsModel::setClockFlash(bool on) { set(CLOCK_FLASH, Val(on)); emit changed(); }
