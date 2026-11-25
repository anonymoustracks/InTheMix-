/*
 * GlideMix
 */
#include "mixhousewarppersist.h"

#include "au3-registries/ClientData.h"
#include "au3-project/Project.h"
#include "au3-xml/XMLAttributeValueView.h"
#include "au3-xml/XMLWriter.h"

namespace {
struct WarpState : public ClientData::Base
{
    std::function<std::string()> writer;
    bool loaded = false;
    std::string value;

    static WarpState& Get(AudacityProject& project);
};

const AttachedProjectObjects::RegisteredFactory key {
    [](AudacityProject&) { return std::make_shared<WarpState>(); }
};

WarpState& WarpState::Get(AudacityProject& project)
{
    return project.AttachedObjects::Get<WarpState&>(key);
}

ProjectFileIORegistry::AttributeWriterEntry entry {
    [](const AudacityProject& project, XMLWriter& xmlFile) {
        auto& state = WarpState::Get(const_cast<AudacityProject&>(project));
        // Written by every project this version saves, even with nothing in
        // it: its presence is what says "saved by 1.3.5 or later".
        // Not decided yet (no history state since opening): keep what was read;
        // a file without it stays without it, so an old project stays old.
        const std::string value = state.writer ? state.writer() : (state.loaded ? state.value : std::string());
        if (!value.empty()) {
            xmlFile.WriteAttr(wxT("glidemix_warp"), wxString::FromUTF8(value));
        }
    }
};

ProjectFileIORegistry::AttributeReaderEntries entries {
    (WarpState & (*)(AudacityProject&)) & WarpState::Get, {
        { "glidemix_warp", [](auto& state, auto value) {
                state.loaded = true;
                state.value = value.ToWString().ToStdString(wxConvUTF8);
            } }
    }
};
}

void au::mixhouse::setWarpStateWriter(void* au3Project, std::function<std::string()> writer)
{
    if (au3Project) {
        WarpState::Get(*reinterpret_cast<AudacityProject*>(au3Project)).writer = std::move(writer);
    }
}

bool au::mixhouse::loadedWarpState(void* au3Project, std::string* value)
{
    if (!au3Project) {
        return false;
    }
    const WarpState& state = WarpState::Get(*reinterpret_cast<AudacityProject*>(au3Project));
    if (value) {
        *value = state.value;
    }
    return state.loaded;
}
