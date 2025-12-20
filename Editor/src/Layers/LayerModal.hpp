#pragma once

#include "LayerDatabase.hpp"

namespace Editor::Layers {
    struct LayerModalResult {
        bool applied = false; // true when Apply succeeded
        bool open = false; // true while popup is open this frame
    };

    LayerModalResult DrawLayerModal(LayerDatabase& db, const char* popupId = "Layer Settings");
}
