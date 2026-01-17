#pragma once

#include "IPanel.hpp"

namespace NE::Graphics {
    class RenderGraph;
}

namespace Editor {

    class RenderGraphPanel : public IPanel {
    public:
        void SetRenderGraph(NE::Graphics::RenderGraph* graph) { m_RenderGraph = graph; }
        virtual void OnImGuiRender() override;

    private:
        void DrawPassList();
        void DrawResourceList();
        void DrawPassDetails(size_t passIndex);
        void DrawLifetimes();
        void DrawPoolStats();

        NE::Graphics::RenderGraph* m_RenderGraph = nullptr;
        int m_SelectedPass = -1;
    };

}
