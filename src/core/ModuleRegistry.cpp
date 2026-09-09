#include "ModuleRegistry.h"

namespace core {

const std::vector<ModuleDescriptor> &moduleRegistry()
{
    static const std::vector<ModuleDescriptor> modules = {
        ModuleDescriptor{
            .id = std::string(kSquareColorModuleId),
            .displayName = "Square Color",
            .description = "Name a square - is it light or dark? 30 second sprint.",
            .iconName = "games-config-board",
            .qmlPage = "qrc:/qt/qml/ChessTrainer/qml/DrillPage.qml",
            .enabled = true,
        },
        ModuleDescriptor{
            .id = "coordinates",
            .displayName = "Coordinates",
            .description = "Find the named square on a blank board.",
            .iconName = "grid-rectangular",
            .qmlPage = {},
            .enabled = false,
        },
        ModuleDescriptor{
            .id = "knight-path",
            .displayName = "Knight Path",
            .description = "Shortest knight route between two squares.",
            .iconName = "path-mode-polyline",
            .qmlPage = {},
            .enabled = false,
        },
        ModuleDescriptor{
            .id = "board-vision",
            .displayName = "Board Vision",
            .description = "Recognise legal moves at a glance.",
            .iconName = "view-visible",
            .qmlPage = {},
            .enabled = false,
        },
    };

    return modules;
}

} // namespace core
