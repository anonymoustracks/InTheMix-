/*
 * MixHouse DJ 2
 */
#include "../../mixhousepreviewplayer.h"

// STUB -- used on Windows (and Linux) until a real preview player exists.
// It compiles and does nothing: load() fails, so clicking a track's overview
// in the library plays nothing. Qt Multimedia is not part of this build, so a
// Windows version needs either Media Foundation (IMFMediaEngine / Source
// Reader + WASAPI) or a small player on GlideMix's own decoder
// (MixHouseAudioSource) and PortAudio. See WINDOWS_PORT.md.

namespace au::mixhouse {
struct MixHousePreviewPlayer::Impl {};

MixHousePreviewPlayer::MixHousePreviewPlayer() = default;
MixHousePreviewPlayer::~MixHousePreviewPlayer() = default;

bool MixHousePreviewPlayer::load(const std::string&, const std::string&) { return false; }
void MixHousePreviewPlayer::playFrom(double) {}
void MixHousePreviewPlayer::stop() {}
bool MixHousePreviewPlayer::isPlaying() { return false; }
double MixHousePreviewPlayer::position() { return 0.0; }
void MixHousePreviewPlayer::setRate(double)
{
}
void MixHousePreviewPlayer::setVolume(double)
{
}

// GlideMix Beat window: no seamless loop here; the window restarts the file itself, as before.
bool MixHousePreviewPlayer::playLoop(const std::string&, const std::string&) { return false; }
bool MixHousePreviewPlayer::queueLoop(const std::string&) { return false; }
bool MixHousePreviewPlayer::loopQueued() { return false; }
}
