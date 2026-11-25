/*
 * GlideMix
 */
#include "mixhousekeypersist.h"

#include "au3-registries/ClientData.h"
#include "au3-project/Project.h"
#include "au3-xml/XMLAttributeValueView.h"
#include "au3-xml/XMLWriter.h"

namespace {
struct KeyState : public ClientData::Base
{
    std::function<std::string()> writer;
    bool loaded = false;
    std::string value;

    static KeyState& Get(AudacityProject& project);
};

const AttachedProjectObjects::RegisteredFactory key {
    [](AudacityProject&) { return std::make_shared<KeyState>(); }
};

KeyState& KeyState::Get(AudacityProject& project)
{
    return project.AttachedObjects::Get<KeyState&>(key);
}

ProjectFileIORegistry::AttributeWriterEntry entry {
    [](const AudacityProject& project, XMLWriter& xmlFile) {
        auto& state = KeyState::Get(const_cast<AudacityProject&>(project));
        // Match keys off and nothing auto-matched writes nothing: a project
        // without the attribute opens with Match keys off, as before 1.3.8.
        // Not loaded yet: keep what was read.
        const std::string value = state.writer ? state.writer() : (state.loaded ? state.value : std::string());
        if (!value.empty()) {
            xmlFile.WriteAttr(wxT("glidemix_keys"), wxString::FromUTF8(value));
        }
    }
};

ProjectFileIORegistry::AttributeReaderEntries entries {
    (KeyState & (*)(AudacityProject&)) & KeyState::Get, {
        { "glidemix_keys", [](auto& state, auto value) {
                state.loaded = true;
                state.value = value.ToWString().ToStdString(wxConvUTF8);
            } }
    }
};
}

void au::mixhouse::setKeyStateWriter(void* au3Project, std::function<std::string()> writer)
{
    if (au3Project) {
        KeyState::Get(*reinterpret_cast<AudacityProject*>(au3Project)).writer = std::move(writer);
    }
}

bool au::mixhouse::loadedKeyState(void* au3Project, std::string* value)
{
    if (!au3Project) {
        return false;
    }
    const KeyState& state = KeyState::Get(*reinterpret_cast<AudacityProject*>(au3Project));
    if (value) {
        *value = state.value;
    }
    return state.loaded;
}
