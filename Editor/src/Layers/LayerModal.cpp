#include "pch.h"
#include "LayerModal.hpp"
#include <string>
#include <array>
#include <Core/Layers.hpp>
#include <imgui/imgui.h>

namespace Editor::Layers {
	namespace {
        std::string_view TrimSV(std::string_view s) {
            auto is_ws = [](char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r'; };
            while (!s.empty() && is_ws(s.front())) s.remove_prefix(1);
            while (!s.empty() && is_ws(s.back()))  s.remove_suffix(1);
            return s;
        }

        std::string NormalizeForCompare(std::string_view sv) {
            sv = TrimSV(sv);
            std::string out;
            out.reserve(sv.size());
            for (char c : sv) out.push_back((char)std::tolower((unsigned char)c));
            return out;
        }

        bool IsReservedLayer(NE::Core::LayerID id) {
            return id == 0;
        }
	}

    LayerModalResult DrawLayerModal(LayerDatabase& db, const char* popupId) {
        LayerModalResult res{};
        constexpr int kBufSize = 64;

        struct State {
            bool initialized = false;
            std::array<std::array<char, kBufSize>, NE::Core::MAX_LAYERS> buf{};
            std::array<bool, NE::Core::MAX_LAYERS> dup{};
            bool showError = false;
            char error[160]{};
        };
        static State s;

        if (ImGui::BeginPopupModal(popupId, nullptr,
            ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse)) {
            res.open = true;

            if (!s.initialized) {
                for (NE::Core::LayerID i = 0; i < NE::Core::MAX_LAYERS; ++i) {
                    std::memset(s.buf[i].data(), 0, s.buf[i].size());

                    std::string_view name = db.GetName(i);
                    if (!name.empty()) {
                        strncpy_s(s.buf[i].data(), s.buf[i].size(), name.data(), _TRUNCATE);
                    }
                }
                s.dup.fill(false);
                s.showError = false;
                s.error[0] = '\0';
                s.initialized = true;
            }

            // Duplicate detection
            {
                s.dup.fill(false);
                std::unordered_map<std::string, int> seen;
                seen.reserve(NE::Core::MAX_LAYERS);

                for (int i = 0; i < (int)NE::Core::MAX_LAYERS; ++i) {
                    std::string_view raw = TrimSV(s.buf[i].data());
                    if (raw.empty()) continue;

                    std::string key = NormalizeForCompare(raw);
                    auto [it, inserted] = seen.emplace(key, i);
                    if (!inserted) {
                        s.dup[i] = true;
                        s.dup[it->second] = true;
                    }
                }
            }

            ImGui::TextUnformatted("Edit Layers");
            ImGui::SameLine();
            ImGui::TextDisabled("(Apply saves; Cancel discards)");
            ImGui::Separator();

            ImGui::BeginChild("##LayerScroll", ImVec2(0, 450),
                ImGuiChildFlags_Border);

            if (ImGui::BeginTable("##LayerEditTable", 2,
                ImGuiTableFlags_RowBg |
                ImGuiTableFlags_BordersInnerV |
                ImGuiTableFlags_SizingStretchSame)) {
                ImGui::TableSetupColumn("ID", ImGuiTableColumnFlags_WidthFixed, 40.0f);
                ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);

                for (int i = 0; i < (int)NE::Core::MAX_LAYERS; ++i) {
                    NE::Core::LayerID id = (NE::Core::LayerID)i;

                    ImGui::TableNextRow();

                    ImGui::TableSetColumnIndex(0);
                    ImGui::Text("%d", i);

                    ImGui::TableSetColumnIndex(1);

                    ImGui::PushID(i);

                    const bool reserved = IsReservedLayer(id);
                    if (reserved) ImGui::BeginDisabled();

                    if (s.dup[i]) {
                        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.35f, 0.15f, 0.15f, 1.0f));
                    }

                    ImGui::SetNextItemWidth(-FLT_MIN);
                    ImGui::InputText("##layer_name", s.buf[i].data(), s.buf[i].size());

                    if (s.dup[i]) {
                        ImGui::PopStyleColor();
                    }

                    if (reserved) {
                        ImGui::EndDisabled();
                    }

                    ImGui::PopID();
                }

                ImGui::EndTable();
            }

            ImGui::EndChild();

            ImGui::Separator();

            bool hasDup = false;
            for (bool d : s.dup) { if (d) { hasDup = true; break; } }

            if (hasDup) {
                ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1), "Fix duplicate names before applying.");
            } else if (s.showError && s.error[0]) {
                ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1), "%s", s.error);
            } else {
                ImGui::TextDisabled("Note: empty name deactivates a layer.");
            }

            if (hasDup) ImGui::BeginDisabled();
            if (ImGui::Button("Apply")) {
                s.showError = false;
                s.error[0] = '\0';

                bool ok = true;
                for (int i = 0; i < (int)NE::Core::MAX_LAYERS; ++i) {
                    NE::Core::LayerID id = (NE::Core::LayerID)i;
                    if (IsReservedLayer(id))
                        continue;

                    std::string_view newName = TrimSV(s.buf[i].data());

                    if (!db.RenameLayer(id, newName)) {
                        ok = false;
                        s.showError = true;
                        std::snprintf(s.error, sizeof(s.error),
                            "Failed to apply layer %d rename. Check LayerDatabase::RenameLayer rules.", i);
                        break;
                    }
                }

                if (ok) {
                    res.applied = true;
                    ImGui::CloseCurrentPopup();
                    s.initialized = false;
                }
            }
            if (hasDup) ImGui::EndDisabled();

            ImGui::SameLine();

            if (ImGui::Button("Cancel")) {
                ImGui::CloseCurrentPopup();
                s.initialized = false;
            }

            ImGui::EndPopup();
        } else {
            s.initialized = false;
        }

        return res;
    }

}
