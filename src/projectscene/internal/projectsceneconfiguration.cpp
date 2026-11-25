/*
* Audacity: A Digital Audio Editor
*/
#include "settings.h"

#include "framework/global/stringutils.h"
#include "framework/global/serialization/json.h"
#include "framework/ui/iuicontextconfiguration.h"

#include "framework/global/log.h"

#include "types/projectscenetypes.h"
#include "uicomponents/types/numerictypes.h"

#include "projectsceneconfiguration.h"

#include <array>

using namespace au::projectscene;

static const std::string moduleName("projectscene");

static const muse::Settings::Key IS_MUSIC_LIBRARY_VISIBLE(moduleName, "projectscene/musicLibraryVisible");
static const muse::Settings::Key IS_VERTICAL_RULERS_VISIBLE(moduleName, "projectscene/verticalRulersVisible");
static const muse::Settings::Key IS_RMS_IN_WAVEFORM_VISIBLE(moduleName, "projectscene/rmsInWaveformVisible");
static const muse::Settings::Key IS_CLIPPING_IN_WAVEFORM_VISIBLE(moduleName, "projectscene/clippingInWaveformVisible");
static const muse::Settings::Key EFFECTS_PANEL_VISIBILITY(moduleName, "projectscene/effectsPanelVisible");

static const muse::Settings::Key MOUSE_ZOOM_PRECISION(moduleName, "projectscene/zoomPrecisionMouse");
static const muse::Settings::Key CLIP_STYLE(moduleName, "projectscene/clipStyle");
static const muse::Settings::Key STEREO_HEIGHTS_PREF(moduleName, "projectscene/asymmetricStereoHeights");
static const muse::Settings::Key ASYMMETRIC_STEREO_HEIGHTS_WORKSPACES(moduleName, "projectscene/asymmetricStereoHeightsWorkspaces");
static const muse::Settings::Key SELECTION_TIMECODE_FORMAT(moduleName, "projectscene/selectionTimecodeFormat");
static const muse::Settings::Key DURATION_TIMECODE_FORMAT(moduleName, "projectscene/durationTimecodeFormat");
static const muse::Settings::Key PLAYBACK_ON_RULER_CLICK_ENABLED(moduleName, "projectscene/playbackOnRulerClickEnabled");
static const muse::Settings::Key LABEL_EDITOR_COLUMN_FORMAT(moduleName, "projectscene/labelEditorColumnFormat");
static const muse::Settings::Key UPDATE_DISPLAY_WHILE_PLAYING_ENABLED(moduleName, "projectscene/updateDisplayWhilePlayingEnabled");
static const muse::Settings::Key PINNED_PLAY_HEAD_ENABLED(moduleName, "projectscene/pinnedPlayHeadEnabled");
static const muse::Settings::Key ZOOM_PRESET_1(moduleName, "projectscene/zoomPreset1");
static const muse::Settings::Key ZOOM_PRESET_2(moduleName, "projectscene/zoomPreset2");

static constexpr bool DEFAULT_VERTICAL_RULERS_VISIBILITY = false;
// Off by default: the library panel does not exist yet, so the toggle starts
// hidden and cannot change what the user sees until that panel is built.
static constexpr bool DEFAULT_MUSIC_LIBRARY_VISIBILITY = false;
static constexpr bool DEFAULT_RMS_IN_WAVEFORM_VISIBILITY = false;
static constexpr bool DEFAULT_CLIPPING_IN_WAVEFORM_VISIBILITY = false;
static const bool DEFAULT_PLAYBACK_ON_RULER_CLICK_ENABLED = false;
static const bool DEFAULT_UPDATE_DISPLAY_WHILE_PLAYING_ENABLED = true;
static const bool DEFAULT_PINNED_PLAY_HEAD_ENABLED = false;

// Sampled at the centre of each swatch in the supplied Logic Pro reference.
// Keep this sequence ordered by row, left to right, so its indices match the
// visual grid in both the picker and Appearance preferences.
static const std::array<const char*, 96> LOGIC_PRO_COLOR_PALETTE {
    "#C93B10", "#CA5B0F", "#C97D11", "#CAA40F", "#CAC90F", "#A4C90F", "#7FCA11", "#5AC910", "#0DC92F", "#11CA5A", "#0ECA7F", "#10C9A4", "#0DC9CA", "#10A5C9", "#2596CE", "#3581D3", "#3E67D6", "#4644D8", "#5D41D6", "#7739D3", "#8F2ED1", "#A42DC9", "#C931CA", "#C92AA4",
    "#A33A1C", "#A3521A", "#A36A1C", "#A4871A", "#A3A31B", "#88A41A", "#6CA31C", "#51A31A", "#1AA432", "#1AA451", "#1AA46D", "#1BA388", "#19A3A4", "#1989A3", "#297CA7", "#376EAA", "#3C5BAE", "#4341B0", "#543FAE", "#6638AA", "#7A32AB", "#8823A3", "#A326A4", "#A32188",
    "#7C331E", "#7C431E", "#7D551F", "#7D691F", "#7C7C1D", "#697C1E", "#577D1F", "#447D1F", "#1D7C2D", "#1D7C43", "#1F7C58", "#1E7D6A", "#1E7C7C", "#1D697C", "#286280", "#315882", "#364B84", "#3A3884", "#483785", "#533482", "#612E81", "#691D7C", "#7D1E7D", "#7D1D6A",
    "#57291B", "#56331D", "#563D1B", "#574B1D", "#57571B", "#4B571D", "#3F561B", "#34571D", "#1D5725", "#1D5634", "#1B573F", "#1B574C", "#1B5656", "#1C4C57", "#224759", "#2A425C", "#2C385B", "#2E2D5B", "#352C5B", "#3E2A5C", "#452759", "#4C1C57", "#571B56", "#571D4C"
};

// GlideMix (Chris, 2026-11-15: "more vibrant colors"): the first row's hues
// at full brightness, colour indices 97..120. The 96 above keep their indices,
// so saved projects look the same.
static const std::array<const char*, 24> VIBRANT_COLOR_PALETTE {
    "#FF4F19", "#FF7719", "#FFA019", "#FFD019", "#FFFE19", "#D1FF19", "#A2FF19", "#75FF19", "#19FF43", "#19FF74", "#19FFA3", "#19FFD1", "#19FEFF", "#19D2FF", "#19B3FF", "#1988FF", "#1957FF", "#1D19FF", "#4519FF", "#7619FF", "#A219FF", "#C919FF", "#FE19FF", "#FF19CA"
};

// GlideMix (Chris, 2026-11-21): the "Creative set": the reference DAW's 43 colours (bright
// pastels and dusty tones, docs/create-studio-grid-study.md 3.2), indices
// 121..163, after the colours above so saved indices keep their colours.
static const std::array<const char*, 43> STUDIO_COLOR_PALETTE {
    "#FB6840", "#ECA846", "#EDD75E", "#F6F18C", "#EFE3B8", "#D9CDB8", "#CDB6AB", "#EA7930", "#E2AA3A", "#C2CD41", "#9CDF48", "#6FF364", "#B6F9A4", "#DDDDC7",
    "#BEC99D", "#34A93C", "#30C343", "#36D95D", "#4CED8C", "#7BFEBD", "#B4F7E1", "#CBEDED", "#C0D7CC", "#8DC4A4", "#299F94", "#2DB7AC", "#41CBC0", "#5EB7D8",
    "#7D93ED", "#9E88F7", "#C7A5F3", "#D3BCDE", "#ACB8CE", "#9F50F8", "#D16CF9", "#E99BF2", "#F0C2E3", "#D6B5D5", "#FC549F", "#FC8B81", "#FED39D", "#F1DACF",
    "#DEBDC7"
};

//! A colour of the palette (1..163) or the system colour picker, or invalid.
static QColor glideMixClipColor(au::trackedit::ClipColorIndex index)
{
    if (au::trackedit::isCustomClipColor(index)) {
        return QColor(QRgb(au::trackedit::customClipColorRgb(index)));
    }
    if (index > 0 && index <= static_cast<int>(LOGIC_PRO_COLOR_PALETTE.size())) {
        return QColor(QString::fromLatin1(LOGIC_PRO_COLOR_PALETTE[index - 1]));
    }
    const int vibrant = index - static_cast<int>(LOGIC_PRO_COLOR_PALETTE.size());
    if (vibrant > 0 && vibrant <= static_cast<int>(VIBRANT_COLOR_PALETTE.size())) {
        return QColor(QString::fromLatin1(VIBRANT_COLOR_PALETTE[vibrant - 1]));
    }
    const int studio = vibrant - static_cast<int>(VIBRANT_COLOR_PALETTE.size());
    if (studio > 0 && studio <= static_cast<int>(STUDIO_COLOR_PALETTE.size())) {
        return QColor(QString::fromLatin1(STUDIO_COLOR_PALETTE[studio - 1]));
    }
    return QColor();
}

void ProjectSceneConfiguration::init()
{
    muse::settings()->setDefaultValue(IS_MUSIC_LIBRARY_VISIBLE, muse::Val(DEFAULT_MUSIC_LIBRARY_VISIBILITY));
    muse::settings()->valueChanged(IS_MUSIC_LIBRARY_VISIBLE).onReceive(nullptr, [this](const muse::Val&) {
        m_isMusicLibraryVisibleChanged.send(isMusicLibraryVisible());
    });

    muse::settings()->setDefaultValue(IS_VERTICAL_RULERS_VISIBLE, muse::Val(DEFAULT_VERTICAL_RULERS_VISIBILITY));
    muse::settings()->valueChanged(IS_VERTICAL_RULERS_VISIBLE).onReceive(nullptr, [this](const muse::Val&) {
        m_isVerticalRulersVisibleChanged.send(isVerticalRulersVisible());
    });

    muse::settings()->setDefaultValue(IS_RMS_IN_WAVEFORM_VISIBLE, muse::Val(DEFAULT_RMS_IN_WAVEFORM_VISIBILITY));
    muse::settings()->valueChanged(IS_RMS_IN_WAVEFORM_VISIBLE).onReceive(nullptr, [this](const muse::Val&) {
        m_isRMSInWaveformVisibleChanged.send(isRMSInWaveformVisible());
    });

    muse::settings()->setDefaultValue(IS_CLIPPING_IN_WAVEFORM_VISIBLE, muse::Val(DEFAULT_CLIPPING_IN_WAVEFORM_VISIBILITY));
    muse::settings()->valueChanged(IS_CLIPPING_IN_WAVEFORM_VISIBLE).onReceive(nullptr, [this](const muse::Val&) {
        m_isClippingInWaveformVisibleChanged.send(isClippingInWaveformVisible());
    });

    muse::settings()->setDefaultValue(EFFECTS_PANEL_VISIBILITY, muse::Val(true));
    muse::settings()->valueChanged(EFFECTS_PANEL_VISIBILITY).onReceive(nullptr, [this](const muse::Val&) {
        m_effectsPanelVisible.notify();
    });

    muse::settings()->setDefaultValue(MOUSE_ZOOM_PRECISION, muse::Val(6));

    muse::settings()->setDefaultValue(CLIP_STYLE, muse::Val(ClipStyles::Style::COLORFUL));
    muse::settings()->valueChanged(CLIP_STYLE).onReceive(nullptr, [this](const muse::Val& val) {
        m_clipStyleChanged.send(val.toEnum<ClipStyles::Style>());
    });

    muse::settings()->setDefaultValue(STEREO_HEIGHTS_PREF,
                                      muse::Val(StereoHeightsPref::AsymmetricStereoHeights::NEVER));
    muse::settings()->valueChanged(STEREO_HEIGHTS_PREF).onReceive(nullptr, [this](const muse::Val&) {
        m_asymmetricStereoHeightsChanged.notify();
    });

    muse::settings()->setDefaultValue(ASYMMETRIC_STEREO_HEIGHTS_WORKSPACES,
                                      muse::Val("Modern"));
    muse::settings()->valueChanged(ASYMMETRIC_STEREO_HEIGHTS_WORKSPACES).onReceive(nullptr, [this](const muse::Val&) {
        m_asymmetricStereoHeightsWorkspacesChanged.notify();
    });

    muse::settings()->setDefaultValue(SELECTION_TIMECODE_FORMAT, muse::Val(au::uicomponents::TimecodeFormatType::HHMMSSHundredths));
    muse::settings()->valueChanged(SELECTION_TIMECODE_FORMAT).onReceive(nullptr, [this](const muse::Val& val) {
        UNUSED(val);
        m_selectionTimecodeFormatChanged.notify();
    });

    muse::settings()->setDefaultValue(DURATION_TIMECODE_FORMAT, muse::Val(au::uicomponents::TimecodeFormatType::HHMMSSHundredths));
    muse::settings()->valueChanged(DURATION_TIMECODE_FORMAT).onReceive(nullptr, [this](const muse::Val& val) {
        UNUSED(val);
        m_durationTimecodeFormatChanged.notify();
    });

    muse::settings()->setDefaultValue(PLAYBACK_ON_RULER_CLICK_ENABLED, muse::Val(DEFAULT_PLAYBACK_ON_RULER_CLICK_ENABLED));
    muse::settings()->valueChanged(PLAYBACK_ON_RULER_CLICK_ENABLED).onReceive(nullptr, [this](const muse::Val&) {
        m_playbackOnRulerClickEnabledChanged.notify();
    });

    muse::settings()->setDefaultValue(UPDATE_DISPLAY_WHILE_PLAYING_ENABLED, muse::Val(DEFAULT_UPDATE_DISPLAY_WHILE_PLAYING_ENABLED));
    muse::settings()->valueChanged(UPDATE_DISPLAY_WHILE_PLAYING_ENABLED).onReceive(nullptr, [this](const muse::Val&) {
        m_updateDisplayWhilePlayingEnabledChanged.notify();
    });

    muse::settings()->setDefaultValue(PINNED_PLAY_HEAD_ENABLED, muse::Val(DEFAULT_PINNED_PLAY_HEAD_ENABLED));
    muse::settings()->valueChanged(PINNED_PLAY_HEAD_ENABLED).onReceive(nullptr, [this](const muse::Val&) {
        m_pinnedPlayHeadEnabledChanged.notify();
    });

    muse::settings()->setDefaultValue(ZOOM_PRESET_1, muse::Val(static_cast<int>(ZoomPresets::ZoomDefault)));
    muse::settings()->setDefaultValue(ZOOM_PRESET_2, muse::Val(static_cast<int>(ZoomPresets::Zoom4To1)));
}

bool ProjectSceneConfiguration::isMusicLibraryVisible() const
{
    return muse::settings()->value(IS_MUSIC_LIBRARY_VISIBLE).toBool();
}

void ProjectSceneConfiguration::setMusicLibraryVisible(bool visible)
{
    muse::settings()->setSharedValue(IS_MUSIC_LIBRARY_VISIBLE, muse::Val(visible));
}

muse::async::Channel<bool> ProjectSceneConfiguration::isMusicLibraryVisibleChanged() const
{
    return m_isMusicLibraryVisibleChanged;
}

bool ProjectSceneConfiguration::isVerticalRulersVisible() const
{
    return muse::settings()->value(IS_VERTICAL_RULERS_VISIBLE).toBool();
}

void ProjectSceneConfiguration::setVerticalRulersVisible(bool visible)
{
    muse::settings()->setSharedValue(IS_VERTICAL_RULERS_VISIBLE, muse::Val(visible));
}

muse::async::Channel<bool> ProjectSceneConfiguration::isVerticalRulersVisibleChanged() const
{
    return m_isVerticalRulersVisibleChanged;
}

bool ProjectSceneConfiguration::isRMSInWaveformVisible() const
{
    return muse::settings()->value(IS_RMS_IN_WAVEFORM_VISIBLE).toBool();
}

void ProjectSceneConfiguration::setRMSInWaveformVisible(bool visible)
{
    muse::settings()->setSharedValue(IS_RMS_IN_WAVEFORM_VISIBLE, muse::Val(visible));
}

muse::async::Channel<bool> ProjectSceneConfiguration::isRMSInWaveformVisibleChanged() const
{
    return m_isRMSInWaveformVisibleChanged;
}

bool ProjectSceneConfiguration::isClippingInWaveformVisible() const
{
    return muse::settings()->value(IS_CLIPPING_IN_WAVEFORM_VISIBLE).toBool();
}

void ProjectSceneConfiguration::setClippingInWaveformVisible(bool visible)
{
    muse::settings()->setSharedValue(IS_CLIPPING_IN_WAVEFORM_VISIBLE, muse::Val(visible));
}

muse::async::Channel<bool> ProjectSceneConfiguration::isClippingInWaveformVisibleChanged() const
{
    return m_isClippingInWaveformVisibleChanged;
}

double ProjectSceneConfiguration::zoom(const muse::modularity::ContextPtr& ctx) const
{
    auto uiCtxConfig = muse::modularity::ioc(ctx)->resolve<muse::ui::IUiContextConfiguration>("projectscene");
    return uiCtxConfig ? uiCtxConfig->physicalDpi() / 4 : 96.0 / 4;
}

int ProjectSceneConfiguration::mouseZoomPrecision() const
{
    return muse::settings()->value(MOUSE_ZOOM_PRECISION).toInt();
}

void ProjectSceneConfiguration::setMouseZoomPrecision(int precision)
{
    muse::settings()->setSharedValue(MOUSE_ZOOM_PRECISION, muse::Val(precision));
}

bool ProjectSceneConfiguration::isEffectsPanelVisible() const
{
    return muse::settings()->value(EFFECTS_PANEL_VISIBILITY).toBool();
}

void ProjectSceneConfiguration::setIsEffectsPanelVisible(bool visible)
{
    muse::settings()->setSharedValue(EFFECTS_PANEL_VISIBILITY, muse::Val(visible));
}

muse::async::Notification ProjectSceneConfiguration::isEffectsPanelVisibleChanged() const
{
    return m_effectsPanelVisible;
}

const std::vector<ClipColorInfo>& ProjectSceneConfiguration::clipColorInfos() const
{
    static const std::vector<ClipColorInfo> infos = {
        { muse::TranslatableString("projectscene", "Blue"), 1 },
        { muse::TranslatableString("projectscene", "Violet"), 2 },
        { muse::TranslatableString("projectscene", "Magenta"), 3 },
        { muse::TranslatableString("projectscene", "Red"), 4 },
        { muse::TranslatableString("projectscene", "Orange"), 5 },
        { muse::TranslatableString("projectscene", "Yellow"), 6 },
        { muse::TranslatableString("projectscene", "Green"), 7 },
        { muse::TranslatableString("projectscene", "Turquoise"), 8 },
        { muse::TranslatableString("projectscene", "Cyan"), 9 }
    };

    return infos;
}

muse::Color ProjectSceneConfiguration::clipColor(trackedit::ClipColorIndex index) const
{
    if (const QColor glideMix = glideMixClipColor(index); glideMix.isValid()) {
        return muse::Color::fromQColor(glideMix);
    }
    QString key = QString("clip_color_%1").arg(index);
    QColor color = uiConfiguration()->currentTheme().extra[key].value<QColor>();
    if (!color.isValid()) {
        color = uiConfiguration()->currentTheme().extra["clip_color_1"].value<QColor>();
    }
    return muse::Color::fromQColor(color);
}

muse::Color ProjectSceneConfiguration::clipSelectedColor(trackedit::ClipColorIndex index) const
{
    if (const QColor glideMix = glideMixClipColor(index); glideMix.isValid()) {
        return muse::Color::fromQColor(glideMix);
    }
    QString key = QString("clip_selected_color_%1").arg(index);
    QColor color = uiConfiguration()->currentTheme().extra[key].value<QColor>();
    if (!color.isValid()) {
        color = uiConfiguration()->currentTheme().extra["clip_selected_color_1"].value<QColor>();
    }
    return muse::Color::fromQColor(color);
}

ClipStyles::Style ProjectSceneConfiguration::clipStyle() const
{
    return muse::settings()->value(CLIP_STYLE).toEnum<ClipStyles::Style>();
}

void ProjectSceneConfiguration::setClipStyle(ClipStyles::Style style)
{
    muse::settings()->setSharedValue(CLIP_STYLE, muse::Val(style));
}

muse::async::Channel<ClipStyles::Style> ProjectSceneConfiguration::clipStyleChanged() const
{
    return m_clipStyleChanged;
}

StereoHeightsPref::AsymmetricStereoHeights
ProjectSceneConfiguration::stereoHeightsPref() const
{
    return muse::settings()->value(STEREO_HEIGHTS_PREF).toEnum<StereoHeightsPref::AsymmetricStereoHeights>();
}

void ProjectSceneConfiguration::setStereoHeightsPref(
    StereoHeightsPref::AsymmetricStereoHeights pref)
{
    if (stereoHeightsPref() == pref) {
        return;
    }
    muse::settings()->setSharedValue(STEREO_HEIGHTS_PREF, muse::Val(pref));
}

muse::async::Notification ProjectSceneConfiguration::stereoHeightsPrefChanged() const
{
    return m_asymmetricStereoHeightsChanged;
}

std::vector<std::string> ProjectSceneConfiguration::asymmetricStereoHeightsWorkspaces() const
{
    // workspaces that have asymmetricStereoHeights enabled are stored as a string in muse::settings
    // "Classic|Music|Modern"
    std::vector<std::string> result;
    std::string combinedString = muse::settings()->value(ASYMMETRIC_STEREO_HEIGHTS_WORKSPACES).toString();

    muse::strings::split(combinedString, result, "|");

    return result;
}

void ProjectSceneConfiguration::setAsymmetricStereoHeightsWorkspaces(std::vector<std::string>& asymmetricWorkspaces)
{
    if (asymmetricStereoHeightsWorkspaces() == asymmetricWorkspaces) {
        return;
    }

    // convert vector to single string saveable in muse::settings
    std::string result = muse::strings::join(asymmetricWorkspaces, "|");

    muse::settings()->setSharedValue(ASYMMETRIC_STEREO_HEIGHTS_WORKSPACES, muse::Val(result));
}

muse::async::Notification ProjectSceneConfiguration::asymmetricStereoHeightsWorkspacesChanged() const
{
    return m_asymmetricStereoHeightsWorkspacesChanged;
}

int ProjectSceneConfiguration::selectionTimecodeFormat() const
{
    return muse::settings()->value(SELECTION_TIMECODE_FORMAT).toInt();
}

void ProjectSceneConfiguration::setSelectionTimecodeFormat(int format)
{
    muse::settings()->setSharedValue(SELECTION_TIMECODE_FORMAT, muse::Val(format));
}

muse::async::Notification ProjectSceneConfiguration::selectionTimecodeFormatChanged() const
{
    return m_selectionTimecodeFormatChanged;
}

int ProjectSceneConfiguration::durationTimecodeFormat() const
{
    return muse::settings()->value(DURATION_TIMECODE_FORMAT).toInt();
}

void ProjectSceneConfiguration::setDurationTimecodeFormat(int format)
{
    muse::settings()->setSharedValue(DURATION_TIMECODE_FORMAT, muse::Val(format));
}

muse::async::Notification ProjectSceneConfiguration::durationTimecodeFormatChanged() const
{
    return m_durationTimecodeFormatChanged;
}

bool ProjectSceneConfiguration::playbackOnRulerClickEnabled() const
{
    return muse::settings()->value(PLAYBACK_ON_RULER_CLICK_ENABLED).toBool();
}

void ProjectSceneConfiguration::setPlaybackOnRulerClickEnabled(bool enabled)
{
    muse::settings()->setSharedValue(PLAYBACK_ON_RULER_CLICK_ENABLED, muse::Val(enabled));
}

muse::ByteArray ProjectSceneConfiguration::labelEditorColumnFormatJson() const
{
    return muse::ByteArray(muse::String::fromStdString(muse::settings()->value(
                                                           LABEL_EDITOR_COLUMN_FORMAT).toString()).toUtf8());
}

int ProjectSceneConfiguration::labelEditorColumnFormat(const std::string& columnName) const
{
    int result = -1;

    muse::ByteArray json = labelEditorColumnFormatJson();
    if (json.empty()) {
        return result;
    }

    std::string err;
    muse::JsonDocument jsodDoc = muse::JsonDocument::fromJson(json, &err);
    if (!err.empty()) {
        LOGE() << err;
        return result;
    }

    if (!jsodDoc.isObject()) {
        return result;
    }

    muse::JsonObject obj = jsodDoc.rootObject();
    if (obj.contains(columnName)) {
        result = obj.value(columnName).toInt();
    }

    return result;
}

void ProjectSceneConfiguration::setLabelEditorColumnFormat(const std::string& columnName, int format) const
{
    muse::ByteArray json = labelEditorColumnFormatJson();
    muse::JsonObject obj;
    if (json.empty()) {
        obj[columnName] = format;
    } else {
        obj = muse::JsonDocument::fromJson(json).rootObject();
        obj[columnName] = format;
    }

    muse::ByteArray newJson = muse::JsonDocument(obj).toJson();
    muse::settings()->setSharedValue(LABEL_EDITOR_COLUMN_FORMAT, muse::Val(muse::String::fromUtf8(newJson).toStdString()));
}

muse::async::Notification ProjectSceneConfiguration::playbackOnRulerClickEnabledChanged() const
{
    return m_playbackOnRulerClickEnabledChanged;
}

bool ProjectSceneConfiguration::updateDisplayWhilePlayingEnabled() const
{
    return muse::settings()->value(UPDATE_DISPLAY_WHILE_PLAYING_ENABLED).toBool();
}

void ProjectSceneConfiguration::setUpdateDisplayWhilePlayingEnabled(bool enabled)
{
    muse::settings()->setSharedValue(UPDATE_DISPLAY_WHILE_PLAYING_ENABLED, muse::Val(enabled));
}

muse::async::Notification ProjectSceneConfiguration::updateDisplayWhilePlayingEnabledChanged() const
{
    return m_updateDisplayWhilePlayingEnabledChanged;
}

bool ProjectSceneConfiguration::pinnedPlayHeadEnabled() const
{
    return muse::settings()->value(PINNED_PLAY_HEAD_ENABLED).toBool();
}

void ProjectSceneConfiguration::setPinnedPlayHeadEnabled(bool enabled)
{
    muse::settings()->setSharedValue(PINNED_PLAY_HEAD_ENABLED, muse::Val(enabled));
}

muse::async::Notification ProjectSceneConfiguration::pinnedPlayHeadEnabledChanged() const
{
    return m_pinnedPlayHeadEnabledChanged;
}

ZoomPresets::Preset ProjectSceneConfiguration::zoomPreset1() const
{
    return muse::settings()->value(ZOOM_PRESET_1).toEnum<ZoomPresets::Preset>();
}

void ProjectSceneConfiguration::setZoomPreset1(ZoomPresets::Preset preset)
{
    muse::settings()->setSharedValue(ZOOM_PRESET_1, muse::Val(static_cast<int>(preset)));
}

ZoomPresets::Preset ProjectSceneConfiguration::zoomPreset2() const
{
    return muse::settings()->value(ZOOM_PRESET_2).toEnum<ZoomPresets::Preset>();
}

void ProjectSceneConfiguration::setZoomPreset2(ZoomPresets::Preset preset)
{
    muse::settings()->setSharedValue(ZOOM_PRESET_2, muse::Val(static_cast<int>(preset)));
}
