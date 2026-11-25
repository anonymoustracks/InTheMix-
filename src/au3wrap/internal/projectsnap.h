/*
 * Audacity: A Digital Audio Editor
 */
#pragma once

#include "au3-registries/ClientData.h"
#include "au3-project/Project.h"

class AudacityProject;

namespace au::au3 {
class ProjectSnap : public ClientData::Base
{
public:
    static ProjectSnap& Get(AudacityProject& project);
    static const ProjectSnap& Get(const AudacityProject& project);

    ProjectSnap();
    ~ProjectSnap();

    void enableSnap(bool enable);
    bool isSnapEnabled() const;
    int snapType() const;
    void setSnapType(unsigned int type);
    bool isSnapTriplets() const;
    void setSnapTriplets(bool triplets);

private:
    // InTheMix: like MixMeister Fusion, a new project snaps to the beat, so a
    // song dragged on the timeline moves one beat at a time.
    // 18 = projectscene::SnapType::Beat (one beat of the project tempo).
    bool m_snapEnabled{ true };
    int m_snapType{ 18 };
    bool m_snapTriplets{ false };
};
}
