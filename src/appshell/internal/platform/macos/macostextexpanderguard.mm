/*
 * GlideMix
 */
#include "macostextexpanderguard.h"

#import <AppKit/AppKit.h>
#include <ApplicationServices/ApplicationServices.h>
#include <unistd.h>

#include <algorithm>

#include <QString>

#include "log.h"

namespace {
// macOS virtual key codes: Backspace ("Delete" on Mac keyboards) and forward Delete.
constexpr unsigned short kBackspace = 51;
constexpr unsigned short kForwardDelete = 117;
// A text expander erases within a few tens of ms of the last typed key; people take longer.
constexpr NSTimeInterval kEraseWindow = 0.35;
// ... then types (or pastes) its replacement: that app's keys are ignored this long.
constexpr NSTimeInterval kReplacementWindow = 1.0;

int64_t sourcePid(NSEvent* event)
{
    const CGEventRef cg = [event CGEvent];
    return cg ? CGEventGetIntegerValueField(cg, kCGEventSourceUnixProcessID) : 0;
}

NSString* appName(int64_t pid)
{
    NSRunningApplication* app = [NSRunningApplication runningApplicationWithProcessIdentifier:static_cast<pid_t>(pid)];
    return app.localizedName ?: [NSString stringWithFormat:@"pid %lld", pid];
}

id g_monitor = nil;
}

void au::appshell::installTextExpanderGuard()
{
    if (g_monitor) {
        return;
    }
    __block NSTimeInterval lastKeyTime = -1.0;
    __block int64_t lastKeyPid = 0;
    __block int64_t blockedPid = 0;
    __block NSTimeInterval blockedUntil = -1.0;
    const int64_t ownPid = getpid();

    g_monitor = [NSEvent addLocalMonitorForEventsMatchingMask:NSEventMaskKeyDown handler:^NSEvent*(NSEvent* event) {
        const NSTimeInterval now = event.timestamp;
        const int64_t pid = sourcePid(event);

        if (blockedPid != 0 && pid == blockedPid && now < blockedUntil) {
            return nil;   // the expander's replacement text
        }

        const bool erase = event.keyCode == kBackspace || event.keyCode == kForwardDelete;
        if (erase) {
            const bool fromOtherApp = pid != lastKeyPid && pid != ownPid && pid > 0;
            if (fromOtherApp && lastKeyTime >= 0.0 && now - lastKeyTime < kEraseWindow) {
                LOGI() << "GlideMix: ignored a Backspace/Delete sent by "
                       << QString::fromNSString(appName(pid)).toStdString()
                       << " right after a typed key (a text expander) -- nothing deleted";
                blockedPid = pid;
                blockedUntil = now + kReplacementWindow;
                return nil;
            }
            return event;
        }

        lastKeyTime = now;
        lastKeyPid = pid;
        return event;
    }];
}

std::vector<std::string> au::appshell::runningTextExpanders()
{
    // Apps that type into other apps: bundle id prefixes, and names (vendors change ids --
    // Typinator is com.macility.typinator2 today).
    // Not static: this file is built without ARC, so an array kept in a static would be
    // freed after the first call and the next call (a second window) would crash.
    NSArray<NSString*>* const kBundles = @[
        @"com.macility.typinator", @"com.ergonis.typinator", @"com.smileonmymac.textexpander",
        @"com.trankynam.atext", @"com.federicoterzi.espanso", @"com.stairways.keyboardmaestro", @"com.typeitapp",
    ];
    NSArray<NSString*>* const kNames = @[ @"Typinator", @"TextExpander", @"aText", @"espanso", @"Keyboard Maestro",
                                          @"TypeIt4Me" ];
    std::vector<std::string> names;
    for (NSRunningApplication* app in [NSWorkspace sharedWorkspace].runningApplications) {
        NSString* bundle = app.bundleIdentifier.lowercaseString ?: @"";
        NSString* name = app.localizedName ?: @"";
        bool known = false;
        for (NSString* prefix in kBundles) {
            known = known || [bundle hasPrefix:prefix.lowercaseString];
        }
        for (NSString* known_name in kNames) {
            known = known || [name rangeOfString:known_name options:NSCaseInsensitiveSearch].location != NSNotFound;
        }
        if (!known) {
            continue;
        }
        // "Keyboard Maestro Engine" and "Keyboard Maestro" are one app to the user.
        std::string shown = QString::fromNSString(name.length ? name : bundle).toStdString();
        if (shown.rfind("Keyboard Maestro", 0) == 0) {
            shown = "Keyboard Maestro";
        }
        if (std::find(names.begin(), names.end(), shown) == names.end()) {
            names.push_back(shown);
        }
    }
    return names;
}
