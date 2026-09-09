#pragma once

#include <string>

namespace core {

/// Everything the home screen needs to render one training module, and
/// everything the shell needs to open it.
///
/// There is deliberately no abstract TrainingModule base class: module two
/// needs exactly a row on the home list and a page of its own, and an
/// interface designed against a single implementation would guess wrong about
/// what modules actually share.
struct ModuleDescriptor {
    std::string id;
    std::string displayName;
    std::string description;
    std::string iconName; ///< freedesktop icon name
    std::string qmlPage;  ///< empty for modules that are not enabled yet
    bool enabled = false;
};

} // namespace core
