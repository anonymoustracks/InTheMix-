/*
 * GlideMix
 */
#include "mixhouseanchorpersist.h"

#include "au3-registries/ClientData.h"
#include "au3-project/Project.h"
#include "au3-xml/XMLAttributeValueView.h"
#include "au3-xml/XMLWriter.h"

namespace {
struct AnchorState : public ClientData::Base
{
    std::function<std::string()> writer;
    bool loaded = false;
    std::string value;

    static AnchorState& Get(AudacityProject& project);
};

const AttachedProjectObjects::RegisteredFactory key {
    [](AudacityProject&) { return std::make_shared<AnchorState>(); }
};

AnchorState& AnchorState::Get(AudacityProject& project)
{
    return project.AttachedObjects::Get<AnchorState&>(key);
}

ProjectFileIORegistry::AttributeWriterEntry entry {
    [](const AudacityProject& project, XMLWriter& xmlFile) {
        auto& state = AnchorState::Get(const_cast<AudacityProject&>(project));
        // Only the songs whose anchors were changed in this project; empty
        // (nothing changed here) writes nothing, and the songs read the
        // library's anchors as before. Not loaded yet: keep what was read.
        const std::string value = state.writer ? state.writer() : (state.loaded ? state.value : std::string());
        if (!value.empty()) {
            xmlFile.WriteAttr(wxT("glidemix_anchors"), wxString::FromUTF8(value));
        }
    }
};

ProjectFileIORegistry::AttributeReaderEntries entries {
    (AnchorState & (*)(AudacityProject&)) & AnchorState::Get, {
        { "glidemix_anchors", [](auto& state, auto value) {
                state.loaded = true;
                state.value = value.ToWString().ToStdString(wxConvUTF8);
            } }
    }
};
}

void au::mixhouse::setAnchorStateWriter(void* au3Project, std::function<std::string()> writer)
{
    if (au3Project) {
        AnchorState::Get(*reinterpret_cast<AudacityProject*>(au3Project)).writer = std::move(writer);
    }
}

bool au::mixhouse::loadedAnchorState(void* au3Project, std::string* value)
{
    if (!au3Project) {
        return false;
    }
    const AnchorState& state = AnchorState::Get(*reinterpret_cast<AudacityProject*>(au3Project));
    if (value) {
        *value = state.value;
    }
    return state.loaded;
}
