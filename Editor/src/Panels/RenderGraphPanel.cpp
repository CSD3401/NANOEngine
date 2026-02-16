#include "RenderGraphPanel.hpp"

#include <imgui/imgui.h>
#include <Graphics/Core/RenderGraph.hpp>
#include <Graphics/Core/PostProcessingSettings.hpp>
#include <EditorInterface/RendererExports.hpp>
#include <Graphics/Interfaces/IFrameBuffer.hpp>
#include <Graphics/Core/RenderGraph/TexturePool.hpp>

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

        // Runtime controls
        auto& ppSettings = NE::Renderer::Command::GetPostProcessingSettings();
        bool postEnabled = ppSettings.enabled;
        if (ImGui::Checkbox("Post Processing Enabled", &postEnabled)) {
            ppSettings.enabled = postEnabled;
        }

        bool bloomEnabled = ppSettings.bloomSettings.enabled;
        if (ImGui::Checkbox("Bloom Enabled", &bloomEnabled)) {
            ppSettings.bloomSettings.enabled = bloomEnabled;
        }
        ImGui::SameLine();
        bool ssaoEnabled = ppSettings.ssaoSettings.enabled;
        if (ImGui::Checkbox("SSAO Enabled", &ssaoEnabled)) {
            ppSettings.ssaoSettings.enabled = ssaoEnabled;
        }
        ImGui::SameLine();
        bool taaEnabled = ppSettings.taaSettings.enabled;
        if (ImGui::Checkbox("TAA Enabled", &taaEnabled)) {
            ppSettings.taaSettings.enabled = taaEnabled;
        }

        bool poolingEnabled = m_RenderGraph->IsPoolingEnabled();
        if (ImGui::Checkbox("Texture Pooling Enabled", &poolingEnabled)) {
            m_RenderGraph->SetPoolingEnabled(poolingEnabled);
        }
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
            if (ImGui::BeginTabItem("Lifetimes")) {
                DrawLifetimes();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Pool Stats")) {
                DrawPoolStats();
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

    void RenderGraphPanel::DrawLifetimes() {
        if (!m_RenderGraph->IsCompiled()) {
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "Graph not compiled - no lifetime data");
            return;
        }

        const auto& passes = m_RenderGraph->GetPasses();
        const auto& resources = m_RenderGraph->GetResources();
        const auto& executionOrder = m_RenderGraph->GetExecutionOrder();
        const auto& lifetimes = m_RenderGraph->GetResourceLifetimes();

        if (executionOrder.empty() || lifetimes.empty()) {
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "No lifetime data available");
            return;
        }

        const int numPasses = static_cast<int>(executionOrder.size());

        // Draw header with pass names
        ImGui::Text("Resource Lifetimes (horizontal bars show when each resource is alive)");
        ImGui::Separator();

        // Calculate layout
        const float nameColumnWidth = 150.0f;
        const float barAreaWidth = ImGui::GetContentRegionAvail().x - nameColumnWidth - 20.0f;
        const float passWidth = barAreaWidth / static_cast<float>(numPasses);
        const float barHeight = 18.0f;
        const float rowSpacing = 4.0f;

        // Draw pass labels at top
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + nameColumnWidth);
        for (size_t i = 0; i < executionOrder.size(); ++i) {
            size_t passIdx = executionOrder[i];
            const auto& pass = passes[passIdx];

            // Truncate name if too long
            char shortName[16];
            snprintf(shortName, sizeof(shortName), "%zu", i + 1);

            float xPos = nameColumnWidth + i * passWidth;
            ImGui::SetCursorPosX(xPos);
            ImGui::Text("%s", shortName);

            if (ImGui::IsItemHovered()) {
                ImGui::BeginTooltip();
                ImGui::Text("%zu. %s", i + 1, pass.name.c_str());
                ImGui::EndTooltip();
            }

            if (i < executionOrder.size() - 1) {
                ImGui::SameLine();
            }
        }

        ImGui::Separator();

        // Draw each resource's lifetime bar
        ImDrawList* drawList = ImGui::GetWindowDrawList();

        for (size_t resIdx = 0; resIdx < lifetimes.size(); ++resIdx) {
            const auto& lifetime = lifetimes[resIdx];
            const auto& resource = resources[resIdx];

            // Resource name (fixed width column)
     ImVec2 textStartPos = ImGui::GetCursorScreenPos();
     ImGui::Text("%s", resource.name.c_str());
    
            // Move to next line for the bar
      ImVec2 barLinePos = ImGui::GetCursorScreenPos();

       // Calculate bar position (aligned with pass columns)
            float barStartX = textStartPos.x + nameColumnWidth;
            float barY = textStartPos.y;

   // Choose color based on resource type
         ImU32 barColor;
            if (lifetime.isImported) {
        barColor = IM_COL32(100, 150, 255, 200); // Blue for imported
         } else {
barColor = IM_COL32(255, 180, 100, 200); // Orange for transient
            }
            ImU32 barColorDark = IM_COL32(60, 60, 60, 150);

    // Draw background (full width, darker)
ImVec2 bgStart(barStartX, barY);
            ImVec2 bgEnd(barStartX + barAreaWidth, barY + barHeight - 2.0f);
 drawList->AddRectFilled(bgStart, bgEnd, barColorDark, 2.0f);

   // Draw lifetime bar
          if (lifetime.isImported) {
      // Imported resources span entire lifetime
   ImVec2 barStart(bgStart.x, barY);
          ImVec2 barEnd(bgEnd.x, barY + barHeight - 2.0f);
       drawList->AddRectFilled(barStart, barEnd, barColor, 2.0f);
 } else if (lifetime.firstPassIndex >= 0 && lifetime.lastPassIndex >= 0) {
                // Transient resources show actual lifetime
                float startX = barStartX + lifetime.firstPassIndex * passWidth;
    float endX = barStartX + (lifetime.lastPassIndex + 1) * passWidth;

    ImVec2 barStart(startX, barY);
  ImVec2 barEnd(endX, barY + barHeight - 2.0f);
  drawList->AddRectFilled(barStart, barEnd, barColor, 2.0f);
            }

      // Draw pass dividers
            for (int i = 1; i < numPasses; ++i) {
  float divX = barStartX + i * passWidth;
    drawList->AddLine(
          ImVec2(divX, barY),
            ImVec2(divX, barY + barHeight - 2.0f),
              IM_COL32(80, 80, 80, 100)
          );
            }

         ImGui::Dummy(ImVec2(0, rowSpacing));
 }

        // Legend
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Text("Legend:");
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.4f, 0.6f, 1.0f, 1.0f), "[Imported: always alive]");
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.4f, 1.0f), "[Transient: limited lifetime]");
    }

    void RenderGraphPanel::DrawPoolStats() {
        auto* pool = NE::Renderer::Query::GetTexturePool();

        if (!pool) {
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "No texture pool available");
            return;
        }

        const auto& stats = pool->GetStats();
        bool poolEnabled = m_RenderGraph && m_RenderGraph->IsPoolingEnabled();
        size_t transientCount = 0;
        if (m_RenderGraph) {
            for (const auto& resource : m_RenderGraph->GetResources()) {
                if (resource.type == NE::Graphics::ResourceType::TransientTexture) {
                    ++transientCount;
                }
            }
        }

        // Pooling status
        if (poolEnabled) {
            ImGui::TextColored(ImVec4(0.3f, 0.8f, 0.3f, 1.0f), "Texture Pooling: ENABLED");
        } else {
            ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.2f, 1.0f), "Texture Pooling: DISABLED");
        }
        if (transientCount == 0) {
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f),
                "Pool linked, but no transient resources in current graph.");
        } else {
            ImGui::Text("Transient Resources: %zu", transientCount);
        }
        ImGui::Separator();

        // Statistics
        ImGui::Text("Pool Statistics:");
        ImGui::Indent();

        ImGui::Text("Pool Size:       %zu textures", stats.poolSize);
        ImGui::Text("Currently In Use: %zu", stats.inUseCount);
        ImGui::Text("Available:       %zu", stats.poolSize - stats.inUseCount);
        ImGui::Spacing();

        // Hit/miss stats
        size_t totalRequests = stats.hits + stats.misses;
        float hitRate = totalRequests > 0 ? (static_cast<float>(stats.hits) / totalRequests * 100.0f) : 0.0f;

        ImGui::Text("Cache Hits:      %zu", stats.hits);
        ImGui::Text("Cache Misses:    %zu", stats.misses);
        if (totalRequests > 0) {
            ImVec4 hitColor = hitRate > 80.0f ? ImVec4(0.3f, 0.8f, 0.3f, 1.0f) :
                             hitRate > 50.0f ? ImVec4(1.0f, 0.8f, 0.2f, 1.0f) :
                                               ImVec4(1.0f, 0.4f, 0.2f, 1.0f);
            ImGui::TextColored(hitColor, "Hit Rate:        %.1f%%", hitRate);
        }
        ImGui::Spacing();
        ImGui::Text("Total Allocated: %zu", stats.totalAllocated);

        ImGui::Unindent();
        ImGui::Separator();

        // Pooled texture list
        const auto& textures = pool->GetTextures();
        if (!textures.empty()) {
            ImGui::Text("Pooled Textures (%zu):", textures.size());

            if (ImGui::BeginTable("PooledTextureTable", 5,
                ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable)) {

                ImGui::TableSetupColumn("ID", ImGuiTableColumnFlags_WidthFixed, 40.0f);
                ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthFixed, 100.0f);
                ImGui::TableSetupColumn("Format", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("GL ID", ImGuiTableColumnFlags_WidthFixed, 60.0f);
                ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthFixed, 80.0f);
                ImGui::TableHeadersRow();

                for (size_t i = 0; i < textures.size(); ++i) {
                    const auto& tex = textures[i];

                    ImGui::TableNextRow();

                    // ID
                    ImGui::TableNextColumn();
                    ImGui::Text("%zu", i);

                    // Size
                    ImGui::TableNextColumn();
                    ImGui::Text("%ux%u", tex->width, tex->height);

                    // Format
                    ImGui::TableNextColumn();
                    ImGui::Text("%s", NE::Graphics::RenderGraph::GetTextureFormatString(tex->format));

                    // GL ID
                    ImGui::TableNextColumn();
                    ImGui::Text("%u", tex->textureId);

                    // Status
                    ImGui::TableNextColumn();
                    if (tex->inUse) {
                        ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.2f, 1.0f), "In Use");
                    } else {
                        ImGui::TextColored(ImVec4(0.3f, 0.8f, 0.3f, 1.0f), "Available");
                    }
                }

                ImGui::EndTable();
            }
        } else {
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Pool is empty");
        }

        // Memory estimation
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Text("Memory Savings Estimate:");
        ImGui::Indent();

        if (stats.hits > 0) {
            // Rough estimate: average texture size * hits avoided
            // Assume average 1920x1080 RGBA16F texture = ~16MB
            size_t avgTextureSizeMB = 16;
            size_t estimatedSavingsMB = stats.hits * avgTextureSizeMB;
            ImGui::TextColored(ImVec4(0.3f, 0.8f, 0.3f, 1.0f),
                "Avoided ~%zu texture allocations", stats.hits);
            ImGui::TextColored(ImVec4(0.3f, 0.8f, 0.3f, 1.0f),
                "Estimated savings: ~%zu MB (assuming avg 16MB/texture)", estimatedSavingsMB);
        } else {
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "No savings yet (no cache hits)");
        }

        ImGui::Unindent();
    }

}
