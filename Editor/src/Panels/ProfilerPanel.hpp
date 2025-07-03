#pragma once

#include "IPanel.hpp"

namespace Editor {
    class ProfilerPanel : public IPanel {
    public:
        virtual void OnImGuiRender() override;
    };
}
