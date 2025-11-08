#pragma once
#include "IPanel.hpp"

namespace Editor {

    class AnimatorRuntimePanel : public IPanel {
    public:
        void OnImGuiRender() override;
    };

} // namespace Editor
