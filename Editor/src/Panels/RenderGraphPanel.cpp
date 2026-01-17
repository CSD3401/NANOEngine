#include "RenderGraphPanel.hpp"

#include <imgui/imgui.h>
#include <Graphics/Core/RenderGraph.hpp>
#include <EditorInterface/RendererExports.hpp>
#include <Graphics/Interfaces/IFrameBuffer.hpp>

namespace Editor {

    void RenderGraphPanel::OnImGuiRender() {
        ImGui::Begin("Render Graph");

        // Auto-fetch render graph from GraphicsManager if not set
        if (!m_RenderGraph) {
            m_RenderGraph = NE::Renderer::Query::GetRenderGraph();
        }

        if (!m_RenderGraph) {
            ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "No render graph assigned");
            ImGui::TextWrapped("The render graph system is available but no graph has been created yet.");
            ImGui::End();
            return;
        }

        // Status bar
        ImGui::Text("Status: %s", m_RenderGraph->IsCompiled() ? "Compiled" : "Not Compiled");
        ImGui::SameLine();
        ImGui::Text("| Passes: %zu", m_RenderGraph->GetPassCount());
        ImGui::SameLine();
        ImGui::Text("| Resources: %zu", m_RenderGraph->GetResourceCount());
        ImGui::Separator();

        // Tabs for different views
        if (ImGui::BeginTabBar("RenderGraphTabs")) {
            if (ImGui::BeginTabItem("Passes")) {
                DrawPassList();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Resources")) {
                DrawResourceList();
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }

        ImGui::End();
    }

    void RenderGraphPanel::DrawPassList() {
        const auto& passes = m_RenderGraph->GetPasses();
        const auto& executionOrder = m_RenderGraph->GetExecutionOrder();

        if (passes.empty()) {
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "No passes registered");
            return;
        }

        // Two-column layout: pass list on left, details on right
        ImGui::Columns(2, "PassColumns", true);

        // Left column: Pass list in execution order
        ImGui::Text("Execution Order");
        ImGui::Separator();

        if (m_RenderGraph->IsCompiled() && !executionOrder.empty()) {
            for (size_t i = 0; i < executionOrder.size(); ++i) {
                size_t passIdx = executionOrder[i];
                const auto& pass = passes[passIdx];

                ImGui::PushID(static_cast<int>(i));

                // Color-coded pass display
                ImVec4 passColor(0.2f, 0.7f, 0.3f, 1.0f); // Green for normal passes

                char label[128];
                snprintf(label, sizeof(label), "%zu. %s", i + 1, pass.name.c_str());

                if (ImGui::Selectable(label, m_SelectedPass == static_cast<int>(passIdx))) {
                    m_SelectedPass = static_cast<int>(passIdx);
                }

                // Tooltip with quick info
                if (ImGui::IsItemHovered()) {
                    ImGui::BeginTooltip();
                    ImGui::Text("Reads: %zu resources", pass.reads.size());
                    ImGui::Text("Writes: %zu resources", pass.writes.size());
                    ImGui::EndTooltip();
                }

                ImGui::PopID();
            }
        } else {
            // Show passes in registration order if not compiled
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "(Not compiled - showing registration order)");
            for (size_t i = 0; i < passes.size(); ++i) {
                const auto& pass = passes[i];

                char label[128];
                snprintf(label, sizeof(label), "%zu. %s", i + 1, pass.name.c_str());

                if (ImGui::Selectable(label, m_SelectedPass == static_cast<int>(i))) {
                    m_SelectedPass = static_cast<int>(i);
                }
            }
        }

        // Right column: Pass details
        ImGui::NextColumn();

        if (m_SelectedPass >= 0 && m_SelectedPass < static_cast<int>(passes.size())) {
            DrawPassDetails(static_cast<size_t>(m_SelectedPass));
        } else {
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Select a pass to view details");
        }

        ImGui::Columns(1);
    }

    void RenderGraphPanel::DrawResourceList() {
        const auto& resources = m_RenderGraph->GetResources();

        if (resources.empty()) {
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "No resources registered");
            return;
        }

        // Resource table
        if (ImGui::BeginTable("ResourceTable", 4,
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable)) {

            ImGui::TableSetupColumn("ID", ImGuiTableColumnFlags_WidthFixed, 40.0f);
            ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 140.0f);
            ImGui::TableSetupColumn("Details", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableHeadersRow();

            for (size_t i = 0; i < resources.size(); ++i) {
                const auto& resource = resources[i];

                ImGui::TableNextRow();

                // ID
                ImGui::TableNextColumn();
                ImGui::Text("%zu", i);

                // Name
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(resource.name.c_str());

                // Type with color coding
                ImGui::TableNextColumn();
                ImVec4 typeColor;
                switch (resource.type) {
                    case NE::Graphics::ResourceType::ImportedTexture:
                        typeColor = ImVec4(0.4f, 0.7f, 1.0f, 1.0f); // Blue
                        break;
                    case NE::Graphics::ResourceType::ImportedFramebuffer:
                        typeColor = ImVec4(0.7f, 0.4f, 1.0f, 1.0f); // Purple
                        break;
                    case NE::Graphics::ResourceType::TransientTexture:
                        typeColor = ImVec4(1.0f, 0.7f, 0.3f, 1.0f); // Orange
                        break;
                    default:
                        typeColor = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
                        break;
                }
                ImGui::TextColored(typeColor, "%s",
                    NE::Graphics::RenderGraph::GetResourceTypeString(resource.type));

                // Details
                ImGui::TableNextColumn();
                if (resource.type == NE::Graphics::ResourceType::TransientTexture) {
                    ImGui::Text("%ux%u %s",
                        resource.desc.width,
                        resource.desc.height,
                        NE::Graphics::RenderGraph::GetTextureFormatString(resource.desc.format));
                    if (resource.allocated) {
                        ImGui::SameLine();
                        ImGui::TextColored(ImVec4(0.3f, 0.8f, 0.3f, 1.0f), "[Allocated]");
                    }
                } else if (resource.type == NE::Graphics::ResourceType::ImportedTexture) {
                    ImGui::Text("GL ID: %u", resource.textureId);
                } else if (resource.type == NE::Graphics::ResourceType::ImportedFramebuffer) {
                    if (resource.framebuffer) {
                        ImGui::Text("FBO %ux%u",
                            resource.framebuffer->GetWidth(),
                            resource.framebuffer->GetHeight());
                    } else {
                        ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "NULL");
                    }
                }
            }

            ImGui::EndTable();
        }
    }

    void RenderGraphPanel::DrawPassDetails(size_t passIndex) {
        const auto& passes = m_RenderGraph->GetPasses();
        const auto& pass = passes[passIndex];

        ImGui::Text("Pass: %s", pass.name.c_str());
        ImGui::Separator();

        // Find execution order position
        const auto& executionOrder = m_RenderGraph->GetExecutionOrder();
        int execPos = -1;
        for (size_t i = 0; i < executionOrder.size(); ++i) {
            if (executionOrder[i] == passIndex) {
                execPos = static_cast<int>(i) + 1;
                break;
            }
        }
        if (execPos > 0) {
            ImGui::Text("Execution Order: #%d of %zu", execPos, executionOrder.size());
        }

        ImGui::Spacing();

        // Inputs section
        ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Inputs (%zu):", pass.reads.size());
        if (pass.reads.empty()) {
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "  (none)");
        } else {
            for (const auto& input : pass.reads) {
                const auto& name = m_RenderGraph->GetResourceName(input);
                ImGui::BulletText("%s (id: %u)", name.c_str(), input.id);
            }
        }

        ImGui::Spacing();

        // Outputs section
        ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.2f, 1.0f), "Outputs (%zu):", pass.writes.size());
        if (pass.writes.empty()) {
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "  (none)");
        } else {
            for (const auto& output : pass.writes) {
                const auto& name = m_RenderGraph->GetResourceName(output);
                ImGui::BulletText("%s (id: %u)", name.c_str(), output.id);
            }
        }

        ImGui::Spacing();

        // Callback status
        if (pass.executeCallback) {
            ImGui::TextColored(ImVec4(0.3f, 0.8f, 0.3f, 1.0f), "Execute callback: Set");
        } else {
            ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Execute callback: Not set!");
        }
    }

}
