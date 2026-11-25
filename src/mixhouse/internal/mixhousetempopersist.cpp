/*
 * GlideMix
 */
#include "mixhousetempopersist.h"

#include "au3-registries/ClientData.h"
#include "au3-project/Project.h"
#include "au3-xml/XMLAttributeValueView.h"
#include "au3-xml/XMLWriter.h"

namespace {
struct TempoLine : public ClientData::Base
{
    std::function<std::string()> writer;
    std::string loaded;

    static TempoLine& Get(AudacityProject& project);
};

const AttachedProjectObjects::RegisteredFactory key {
    [](AudacityProject&) { return std::make_shared<TempoLine>(); }
};

TempoLine& TempoLine::Get(AudacityProject& project)
{
    return project.AttachedObjects::Get<TempoLine&>(key);
}

ProjectFileIORegistry::AttributeWriterEntry entry {
    [](const AudacityProject& project, XMLWriter& xmlFile) {
        auto& line = TempoLine::Get(const_cast<AudacityProject&>(project));
        const std::string value = line.writer ? line.writer() : line.loaded;
        if (!value.empty()) {
            xmlFile.WriteAttr(wxT("glidemix_tempo"), wxString::FromUTF8(value));
        }
    }
};

ProjectFileIORegistry::AttributeReaderEntries entries {
    (TempoLine & (*)(AudacityProject&)) & TempoLine::Get, {
        { "glidemix_tempo", [](auto& line, auto value) {
                line.loaded = value.ToWString().ToStdString(wxConvUTF8);
            } }
    }
};
}

void au::mixhouse::setTempoLineWriter(void* au3Project, std::function<std::string()> writer)
{
    if (au3Project) {
        TempoLine::Get(*reinterpret_cast<AudacityProject*>(au3Project)).writer = std::move(writer);
    }
}

std::string au::mixhouse::loadedTempoLine(void* au3Project)
{
    return au3Project ? TempoLine::Get(*reinterpret_cast<AudacityProject*>(au3Project)).loaded : std::string();
}
