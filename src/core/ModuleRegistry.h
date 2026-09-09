#pragma once

#include <string_view>
#include <vector>

#include "ModuleDescriptor.h"

namespace core {

inline constexpr std::string_view kSquareColorModuleId = "square-color";

/// The modules this build knows about, in display order.
const std::vector<ModuleDescriptor> &moduleRegistry();

} // namespace core
