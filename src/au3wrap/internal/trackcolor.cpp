#include "trackcolor.h"

#include "trackedit/trackedittypes.h"

using namespace au::au3;
using namespace au::trackedit;

static const AttachedTrackObjects::RegisteredFactory keyTrackColor{
    [](Track& track) -> std::shared_ptr<TrackColor> { return std::make_shared<TrackColor>(track); }
};

static constexpr auto ColorIndexAttr = "colorindex";

static size_t lastColorIndex = 0;

// GlideMix (Chris, 2026-11-21): new tracks walk the reference DAW's order round the colour
// wheel, ~25-30 degrees a track, in the Creative set (colours 121..163): lime, mint, teal,
// sky, periwinkle, violet, magenta, pink, coral, orange, gold, yellow-olive. Neighbours never
// match and no track is louder than another.
// InTheMix: every song a colour of its own, in the InTheMix order -- Anonymous
// Tracks red, orange, teal, blue, violet, yellow, then pink, green, sky, amber.
static constexpr ClipColorIndex NEW_TRACK_COLORS[] = {
    CLIP_COLOR_CUSTOM_FLAG | 0xE3121B,   // Anonymous Tracks red
    CLIP_COLOR_CUSTOM_FLAG | 0xF07A1A,   // orange
    CLIP_COLOR_CUSTOM_FLAG | 0x1FB5A5,   // teal
    CLIP_COLOR_CUSTOM_FLAG | 0x3D7BE0,   // blue
    CLIP_COLOR_CUSTOM_FLAG | 0x9B5DE0,   // violet
    CLIP_COLOR_CUSTOM_FLAG | 0xE5C21F,   // yellow
    CLIP_COLOR_CUSTOM_FLAG | 0xE0508F,   // pink
    CLIP_COLOR_CUSTOM_FLAG | 0x4CC25A,   // green
    CLIP_COLOR_CUSTOM_FLAG | 0x3FB8E8,   // sky
    CLIP_COLOR_CUSTOM_FLAG | 0xD9963A    // amber
};
static constexpr size_t NEW_TRACK_COLOR_COUNT = sizeof(NEW_TRACK_COLORS) / sizeof(NEW_TRACK_COLORS[0]);

TrackColor& TrackColor::Get(Track* track)
{
    return track->AttachedTrackObjects::Get<TrackColor>(keyTrackColor);
}

TrackColor& TrackColor::Get(const Track* track)
{
    return Get(const_cast<Track*>(track));
}

void TrackColor::Init(size_t index)
{
    lastColorIndex = index;
}

void TrackColor::CopyTo(Track& track) const
{
    auto& color = Get(&track);
    color.SetColorIndex(mColorIndex);
    // The next new track continues the order after this one's colour.
    for (size_t i = 0; i < NEW_TRACK_COLOR_COUNT; ++i) {
        if (NEW_TRACK_COLORS[i] == mColorIndex) {
            lastColorIndex = i + 1;
            break;
        }
    }
}

TrackColor::TrackColor(Track& track)
    : mTrack{track.shared_from_this()}
{
    auto ntrack = mTrack.lock();
    if (!ntrack) {
        return;
    }

    if (ntrack->GetId() == -1) {
        //Only assign color to newly created tracks
        assignColor();
    }
}

void TrackColor::Reparent(const std::shared_ptr<Track>& parent)
{
    mTrack = parent;
}

void TrackColor::WriteXMLAttributes(XMLWriter& writer) const
{
    writer.WriteAttr(ColorIndexAttr, mColorIndex);
}

bool TrackColor::HandleXMLAttribute(const std::string_view& attr, const XMLAttributeValueView& valueView)
{
    if (attr == ColorIndexAttr) {
        long nValue;
        if (valueView.TryGet(nValue)) {
            mColorIndex = static_cast<ClipColorIndex>(nValue);
        }
        return true;
    }

    return false;
}

ClipColorIndex TrackColor::GetColorIndex() const
{
    return mColorIndex;
}

void TrackColor::SetColorIndex(ClipColorIndex colorIndex)
{
    mColorIndex = colorIndex;
}

void TrackColor::assignColor()
{
    mColorIndex = NEW_TRACK_COLORS[lastColorIndex++ % NEW_TRACK_COLOR_COUNT];
}
