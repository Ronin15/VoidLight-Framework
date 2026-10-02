#include "ResourceTestAccess.hpp"

#include "managers/ResourceTemplateManager.hpp"

void ResourceTestAccess::resetFactory() {
    auto& rtm = ResourceTemplateManager::Instance();
    if (rtm.isInitialized()) {
        rtm.clean();
    } else {
        // Ensure any residual state from previous tests is cleared when not initialized
        rtm.clean();
    }
}
