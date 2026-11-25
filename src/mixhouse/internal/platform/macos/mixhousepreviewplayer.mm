/*
 * MixHouse DJ 2
 */
#include "../../mixhousepreviewplayer.h"

#import <AVFoundation/AVFoundation.h>
#include <AudioToolbox/AudioToolbox.h>
#include <CoreAudio/CoreAudio.h>

#include <algorithm>
#include <cmath>
#include <vector>

namespace {
// kAudioObjectPropertyElementMain; spelled as its value so this builds
// against the 11.0 deployment target without the Master/Main deprecation dance.
constexpr AudioObjectPropertyElement kElementMain = 0;

AudioDeviceID defaultOutputDevice()
{
    AudioObjectPropertyAddress addr{ kAudioHardwarePropertyDefaultOutputDevice,
                                     kAudioObjectPropertyScopeGlobal, kElementMain };
    AudioDeviceID device = kAudioObjectUnknown;
    UInt32 size = sizeof(device);
    AudioObjectGetPropertyData(kAudioObjectSystemObject, &addr, 0, nullptr, &size, &device);
    return device;
}

bool hasOutputStreams(AudioDeviceID device)
{
    AudioObjectPropertyAddress addr{ kAudioDevicePropertyStreams, kAudioObjectPropertyScopeOutput, kElementMain };
    UInt32 size = 0;
    return AudioObjectGetPropertyDataSize(device, &addr, 0, nullptr, &size) == noErr && size > 0;
}

/// Audacity stores its playback device as PortAudio's device name, which on
/// macOS is Core Audio's own device name -- so match on that.
AudioDeviceID findOutputDevice(const std::string& name)
{
    if (name.empty()) {
        return defaultOutputDevice();
    }
    AudioObjectPropertyAddress addr{ kAudioHardwarePropertyDevices, kAudioObjectPropertyScopeGlobal, kElementMain };
    UInt32 size = 0;
    if (AudioObjectGetPropertyDataSize(kAudioObjectSystemObject, &addr, 0, nullptr, &size) != noErr) {
        return defaultOutputDevice();
    }
    std::vector<AudioDeviceID> devices(size / sizeof(AudioDeviceID));
    if (devices.empty()
        || AudioObjectGetPropertyData(kAudioObjectSystemObject, &addr, 0, nullptr, &size, devices.data()) != noErr) {
        return defaultOutputDevice();
    }
    NSString* wanted = [NSString stringWithUTF8String:name.c_str()];
    for (AudioDeviceID device : devices) {
        if (!hasOutputStreams(device)) {
            continue;
        }
        AudioObjectPropertyAddress nameAddr{ kAudioObjectPropertyName, kAudioObjectPropertyScopeGlobal, kElementMain };
        CFStringRef deviceName = nullptr;
        UInt32 nameSize = sizeof(deviceName);
        if (AudioObjectGetPropertyData(device, &nameAddr, 0, nullptr, &nameSize, &deviceName) != noErr || !deviceName) {
            continue;
        }
        const bool match = [(__bridge NSString*)deviceName isEqualToString:wanted];
        CFRelease(deviceName);
        if (match) {
            return device;
        }
    }
    return defaultOutputDevice();
}
}

namespace au::mixhouse {
struct MixHousePreviewPlayer::Impl {
    AVAudioEngine* engine = nil;
    AVAudioPlayerNode* player = nil;
    AVAudioUnitTimePitch* timePitch = nil;   // GlideMix: speed for the synced preview
    AVAudioFile* file = nil;
    AVAudioFormat* connectedFormat = nil;
    AudioDeviceID device = kAudioObjectUnknown;
    AVAudioFramePosition startFrame = 0;
    double lastPosition = 0.0;
    bool playing = false;

    // GlideMix Beat window: a buffer the player node loops by itself (no gap),
    // and the next one, which takes over at the end of the current loop.
    bool looping = false;
    AVAudioFramePosition loopStart = 0;      // player sample time the current loop buffer began
    AVAudioFramePosition loopLength = 0;
    AVAudioFramePosition queuedAt = 0;       // player sample time the queued buffer takes over
    AVAudioFramePosition queuedLength = 0;   // 0 = nothing queued

    // The player's own sample clock since play(), or -1 when not known yet.
    AVAudioFramePosition playerSample()
    {
        AVAudioTime* nodeTime = player.lastRenderTime;
        AVAudioTime* playerTime = nodeTime ? [player playerTimeForNodeTime:nodeTime] : nil;
        return (playerTime && playerTime.sampleTimeValid) ? std::max<AVAudioFramePosition>(0, playerTime.sampleTime) : -1;
    }
};

namespace {
AVAudioPCMBuffer* readWhole(AVAudioFile* file)
{
    AVAudioPCMBuffer* buffer = [[AVAudioPCMBuffer alloc] initWithPCMFormat:file.processingFormat
                                                             frameCapacity:static_cast<AVAudioFrameCount>(file.length)];
    NSError* error = nil;
    if (!buffer || ![file readIntoBuffer:buffer error:&error] || buffer.frameLength == 0) {
        NSLog(@"GlideMix beat preview: cannot read the loop: %@", error);
        return nil;
    }
    return buffer;
}
}

MixHousePreviewPlayer::MixHousePreviewPlayer()
    : m_impl(std::make_unique<Impl>())
{
}

MixHousePreviewPlayer::~MixHousePreviewPlayer()
{
    stop();
}

bool MixHousePreviewPlayer::load(const std::string& utf8Path, const std::string& outputDeviceName)
{
    Impl& d = *m_impl;
    stop();

    NSError* error = nil;
    NSURL* url = [NSURL fileURLWithPath:[NSString stringWithUTF8String:utf8Path.c_str()]];
    AVAudioFile* file = [[AVAudioFile alloc] initForReading:url error:&error];
    if (!file || file.length <= 0) {
        NSLog(@"GlideMix preview: cannot open %@: %@", url.path, error);
        return false;
    }

    if (!d.engine) {
        d.engine = [[AVAudioEngine alloc] init];
        d.player = [[AVAudioPlayerNode alloc] init];
        d.timePitch = [[AVAudioUnitTimePitch alloc] init];
        [d.engine attachNode:d.player];
        [d.engine attachNode:d.timePitch];
    }

    // The output unit's device can only change while the engine is stopped
    // (stop() above guarantees that).
    const AudioDeviceID device = findOutputDevice(outputDeviceName);
    if (device != kAudioObjectUnknown && device != d.device) {
        AudioUnit outputUnit = d.engine.outputNode.audioUnit;
        AudioDeviceID value = device;
        if (outputUnit && AudioUnitSetProperty(outputUnit, kAudioOutputUnitProperty_CurrentDevice,
                                               kAudioUnitScope_Global, 0, &value, sizeof(value)) == noErr) {
            d.device = device;
        }
    }

    // Player -> main mixer in the file's own format; the mixer converts to the
    // output device's rate, so 44.1k files play correctly on a 48k interface.
    AVAudioFormat* format = file.processingFormat;
    if (!d.connectedFormat || ![d.connectedFormat isEqual:format]) {
        [d.engine disconnectNodeOutput:d.player];
        [d.engine disconnectNodeOutput:d.timePitch];
        [d.engine connect:d.player to:d.timePitch format:format];
        [d.engine connect:d.timePitch to:d.engine.mainMixerNode format:format];
        d.connectedFormat = format;
    }

    d.file = file;
    d.startFrame = 0;
    d.lastPosition = 0.0;
    return true;
}

void MixHousePreviewPlayer::playFrom(double fraction)
{
    Impl& d = *m_impl;
    if (!d.file) {
        return;
    }
    const AVAudioFramePosition length = d.file.length;
    const AVAudioFramePosition frame = std::clamp<AVAudioFramePosition>(
        static_cast<AVAudioFramePosition>(std::clamp(fraction, 0.0, 1.0) * static_cast<double>(length)),
        0, length - 1);

    [d.player stop];
    d.timePitch.bypass = NO;   // as always for the library's previews (the Beat loop bypasses it)
    [d.player scheduleSegment:d.file
                startingFrame:frame
                   frameCount:static_cast<AVAudioFrameCount>(length - frame)
                       atTime:nil
            completionHandler:nil];
    if (!d.engine.isRunning) {
        NSError* error = nil;
        [d.engine prepare];
        if (![d.engine startAndReturnError:&error]) {
            NSLog(@"GlideMix preview: engine start failed: %@", error);
            d.playing = false;
            return;
        }
    }
    [d.player play];
    d.startFrame = frame;
    d.lastPosition = static_cast<double>(frame) / static_cast<double>(length);
    d.playing = true;
}

void MixHousePreviewPlayer::stop()
{
    Impl& d = *m_impl;
    if (d.playing) {
        d.lastPosition = position();
    }
    d.playing = false;
    d.looping = false;
    d.queuedLength = 0;
    if (d.player) {
        [d.player stop];
    }
    // Stopping the engine (not just the node) releases the output device
    // while nothing is previewing.
    if (d.engine && d.engine.isRunning) {
        [d.engine stop];
    }
}

bool MixHousePreviewPlayer::isPlaying()
{
    if (m_impl->playing) {
        position();   // detects the end of the file
    }
    return m_impl->playing;
}

bool MixHousePreviewPlayer::playLoop(const std::string& utf8Path, const std::string& outputDeviceName)
{
    Impl& d = *m_impl;
    if (!load(utf8Path, outputDeviceName)) {
        return false;
    }
    AVAudioPCMBuffer* buffer = readWhole(d.file);
    if (!buffer) {
        return false;
    }
    [d.player stop];
    // At normal speed the time-pitch unit only colours sharp drum hits: out of the way.
    d.timePitch.bypass = std::abs(d.timePitch.rate - 1.0f) < 1e-4f;
    [d.player scheduleBuffer:buffer atTime:nil options:AVAudioPlayerNodeBufferLoops completionHandler:nil];
    if (!d.engine.isRunning) {
        NSError* error = nil;
        [d.engine prepare];
        if (![d.engine startAndReturnError:&error]) {
            NSLog(@"GlideMix beat preview: engine start failed: %@", error);
            d.playing = false;
            return false;
        }
    }
    [d.player play];
    d.looping = true;
    d.loopStart = 0;
    d.loopLength = buffer.frameLength;
    d.queuedLength = 0;
    d.startFrame = 0;
    d.lastPosition = 0.0;
    d.playing = true;
    return true;
}

bool MixHousePreviewPlayer::queueLoop(const std::string& utf8Path)
{
    Impl& d = *m_impl;
    if (!d.looping || !d.playing || d.queuedLength > 0) {
        return false;
    }
    NSError* error = nil;
    NSURL* url = [NSURL fileURLWithPath:[NSString stringWithUTF8String:utf8Path.c_str()]];
    AVAudioFile* file = [[AVAudioFile alloc] initForReading:url error:&error];
    // Another sample format would need a new connection: the caller restarts instead.
    if (!file || file.length <= 0 || !d.connectedFormat || ![file.processingFormat isEqual:d.connectedFormat]) {
        return false;
    }
    AVAudioPCMBuffer* buffer = readWhole(file);
    if (!buffer) {
        return false;
    }
    const AVAudioFramePosition now = d.playerSample();
    if (now < 0 || d.loopLength <= 0) {
        return false;
    }
    // AVAudioPlayerNodeBufferInterruptsAtLoop: the playing loop finishes its
    // round, then the new one starts -- in time, without a gap.
    const AVAudioFramePosition rounds = (now - d.loopStart) / d.loopLength + 1;
    d.queuedAt = d.loopStart + rounds * d.loopLength;
    d.queuedLength = buffer.frameLength;
    [d.player scheduleBuffer:buffer atTime:nil
                     options:AVAudioPlayerNodeBufferLoops | AVAudioPlayerNodeBufferInterruptsAtLoop
           completionHandler:nil];
    d.file = file;
    return true;
}

bool MixHousePreviewPlayer::loopQueued()
{
    position();   // moves to the queued loop once it has started
    return m_impl->looping && m_impl->queuedLength > 0;
}

double MixHousePreviewPlayer::position()
{
    Impl& d = *m_impl;
    if (!d.file || !d.playing) {
        return d.lastPosition;
    }
    if (d.looping) {
        const AVAudioFramePosition now = d.playerSample();
        if (now < 0 || d.loopLength <= 0) {
            return d.lastPosition;
        }
        if (d.queuedLength > 0 && now >= d.queuedAt) {
            d.loopStart = d.queuedAt;
            d.loopLength = d.queuedLength;
            d.queuedLength = 0;
        }
        const AVAudioFramePosition into = std::max<AVAudioFramePosition>(0, now - d.loopStart) % d.loopLength;
        d.lastPosition = static_cast<double>(into) / static_cast<double>(d.loopLength);
        return d.lastPosition;
    }
    const double length = static_cast<double>(d.file.length);
    AVAudioTime* nodeTime = d.player.lastRenderTime;
    AVAudioTime* playerTime = nodeTime ? [d.player playerTimeForNodeTime:nodeTime] : nil;
    if (!playerTime || !playerTime.sampleTimeValid) {
        return d.lastPosition;
    }
    // playerTime counts from this play() call, in the connection's (the
    // file's) sample rate -- the same frame units as startFrame.
    const double frames = static_cast<double>(d.startFrame) + static_cast<double>(std::max<AVAudioFramePosition>(0, playerTime.sampleTime));
    if (frames >= length) {
        d.lastPosition = 1.0;
        d.playing = false;
        [d.player stop];
        [d.engine stop];
        return 1.0;
    }
    d.lastPosition = frames / length;
    return d.lastPosition;
}
void MixHousePreviewPlayer::setRate(double rate)
{
    if (m_impl->timePitch) {
        m_impl->timePitch.rate = static_cast<float>(std::clamp(rate, 0.5, 2.0));
    }
}
void MixHousePreviewPlayer::setVolume(double volume)
{
    if (m_impl->player) {
        m_impl->player.volume = static_cast<float>(std::clamp(volume, 0.0, 1.0));
    }
}
}
