#include "pch.h"
#include "EditorUI.hpp"

#include <Math/Vec2.hpp>
#include <Math/Vec3.hpp>
#include "AssetManagement/AssetManager.hpp"
#include <imgui/widgets/imsearch/imsearch.h>
#include <Events/EventBus.hpp>
#include "EditorEvents.hpp"
#include "ThumbnailManager.hpp"

namespace Editor {
    namespace {
        ImU32 ColorScale(ImU32 col, float scale) {
            ImVec4 c = ImGui::ColorConvertU32ToFloat4(col);
            c.x = std::clamp(c.x * scale, 0.0f, 1.0f);
            c.y = std::clamp(c.y * scale, 0.0f, 1.0f);
            c.z = std::clamp(c.z * scale, 0.0f, 1.0f);
            return ImGui::ColorConvertFloat4ToU32(c);
        }

        bool DrawAxisPill(const char* axis_id,
            const char* axis_text,
            float* value,
            ImU32 axis_color,
            float width,
            float drag_speed_per_pixel,
            float reset_value,
            int precision)
        {
            ImGui::PushID(axis_id);

            const float old_value = *value;

            const float height = ImGui::GetFrameHeight() * 0.7f;
            const float rounding = height * 0.5f;
            const float seg_w = std::clamp(height * 1.25f, 28.0f, 48.0f);

            const ImVec2 p0 = ImGui::GetCursorScreenPos() + ImVec2(0.f, ImGui::GetFrameHeight() * 0.25f);
            const ImVec2 sz(width, height);

            ImGui::InvisibleButton("##pill", sz, ImGuiButtonFlags_AllowOverlap);

            // Save layout cursor after the pill item, so we can restore it after overlay drawing.
            const ImVec2 cursor_after = ImGui::GetCursorPos();

            ImGuiIO& io = ImGui::GetIO();
            ImDrawList* dl = ImGui::GetWindowDrawList();

            const bool hovered = ImGui::IsItemHovered();
            const bool held = ImGui::IsItemActive();
            const ImGuiID item_id = ImGui::GetItemID();

            const float local_x = io.MousePos.x - p0.x;
            const bool over_axis = hovered && (local_x >= 0.0f) && (local_x < seg_w);
            const bool over_value = hovered && (local_x >= seg_w) && (local_x < width);

            // ---- Persistent state ----
            ImGuiStorage* st = ImGui::GetStateStorage();
            const ImGuiID kDragging = ImGui::GetID("##dragging");   // 0/1
            const ImGuiID kStartX = ImGui::GetID("##startx");
            const ImGuiID kStartVal = ImGui::GetID("##startval");

            const ImGuiID kEditing = ImGui::GetID("##editing");    // 0/1
            const ImGuiID kEditStart = ImGui::GetID("##editstart");  // float for Esc revert

            bool editing = (st->GetInt(kEditing, 0) != 0);
            bool start_edit = false;

            // Cursor hint
            if (hovered)
                ImGui::SetMouseCursor(editing ? ImGuiMouseCursor_TextInput
                    : (over_value ? ImGuiMouseCursor_ResizeEW : ImGuiMouseCursor_Hand));

            // Enter edit mode on double click in value area
            if (!editing && over_value && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                st->SetInt(kEditing, 1);
                st->SetInt(kDragging, 0);
                st->SetFloat(kEditStart, *value);
                editing = true;
                start_edit = true;
            }

            // ---- Interaction (non-edit) ----
            if (!editing) {
                // Axis click resets
                if (over_axis && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                    *value = reset_value;
                }
                // Click in value area starts dragging
                else if (over_value && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                    st->SetInt(kDragging, 1);
                    st->SetFloat(kStartX, io.MousePos.x);
                    st->SetFloat(kStartVal, *value);
                }

                // Dragging continues even if mouse leaves the rect
                if (st->GetInt(kDragging, 0) != 0) {
                    if (io.MouseDown[ImGuiMouseButton_Left]) {
                        float speed = drag_speed_per_pixel;
                        if (io.KeyShift) speed *= 0.1f;
                        if (io.KeyCtrl)  speed *= 10.0f;

                        const float start_x = st->GetFloat(kStartX, io.MousePos.x);
                        const float start_val = st->GetFloat(kStartVal, *value);
                        *value = start_val + (io.MousePos.x - start_x) * speed;
                    } else {
                        st->SetInt(kDragging, 0);
                    }
                }
            }

            // ---- Determine changed ----
            bool changed = (*value != old_value);
            if (changed)
                ImGui::MarkItemEdited(item_id);

            // ---- Colors ----
            ImU32 bg = ImGui::GetColorU32(ImGuiCol_FrameBg);
            ImU32 border = ImGui::GetColorU32(ImGuiCol_Border);
            ImU32 text = ImGui::GetColorU32(ImGuiCol_Text);
            if ((border >> 24) == 0) border = IM_COL32(0, 0, 0, 255);

            ImU32 axis_fill = axis_color;
            if (over_axis && !editing) axis_fill = ColorScale(axis_fill, 1.08f);
            if (over_value && !editing) bg = ColorScale(bg, 1.03f);

            // ---- Draw pill ----
            dl->AddRectFilled(p0, p0 + sz, bg, rounding);
            dl->AddRect(p0, p0 + sz, border, rounding, 0, 1.25f);

            dl->AddRectFilled(p0, p0 + ImVec2(seg_w, height), axis_fill, rounding, ImDrawFlags_RoundCornersLeft);
            dl->AddLine(p0 + ImVec2(seg_w, 1.0f), p0 + ImVec2(seg_w, height - 1.0f), border, 1.0f);

            {
                const ImVec2 ts = ImGui::CalcTextSize(axis_text);
                const ImVec2 tp = p0 + ImVec2((seg_w - ts.x) * 0.5f, (height - ts.y) * 0.5f);
                dl->AddText(tp, IM_COL32(255, 255, 255, 255), axis_text);
            }

            if (editing) {
                ImGui::SetItemAllowOverlap();

                if (over_axis && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                    *value = reset_value;
                    st->SetInt(kEditing, 0);
                } else {
                    ImGui::SetCursorScreenPos(p0);
                    ImGui::PushItemWidth(width);

                    const float pad_x = ImGui::GetStyle().FramePadding.x;

                    ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(0, 0, 0, 0));
                    ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(0, 0, 0, 0));
                    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
                    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
                    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(seg_w + pad_x, ImGui::GetStyle().FramePadding.y));

                    char fmt[16];
                    std::snprintf(fmt, sizeof(fmt), "%%.%df", precision);

                    if (start_edit)
                        ImGui::SetKeyboardFocusHere();

                    ImGuiInputTextFlags flags = ImGuiInputTextFlags_AutoSelectAll;

                    bool input_changed = ImGui::InputFloat("##edit", value, 0.0f, 0.0f, fmt, flags);
                    if (input_changed) {
                        changed = true;
                    }

                    if (ImGui::IsItemActive() && ImGui::IsKeyPressed(ImGuiKey_Escape)) {
                        *value = st->GetFloat(kEditStart, *value);
                        st->SetInt(kEditing, 0);
                    }

                    if (ImGui::IsItemDeactivatedAfterEdit() || ImGui::IsItemDeactivated())
                        st->SetInt(kEditing, 0);

                    ImGui::PopStyleVar(3);
                    ImGui::PopStyleColor(2);
                    ImGui::PopItemWidth();

                    ImGui::SetCursorPos(cursor_after);
                }
            } else {
                char fmt[16];
                std::snprintf(fmt, sizeof(fmt), "%%.%df", precision);

                char buf[64];
                std::snprintf(buf, sizeof(buf), fmt, *value);

                const float pad_x = ImGui::GetStyle().FramePadding.x;
                const ImVec2 ts = ImGui::CalcTextSize(buf);
                const ImVec2 tp = p0 + ImVec2(seg_w + pad_x, (height - ts.y) * 0.5f);
                dl->AddText(tp, text, buf);
            }

            ImGui::PopID();
            return changed;
        }
    }

    bool BeginPillCombo(const char* id, const char* preview) {
        ImGuiStyle& s = ImGui::GetStyle();

        // Make it pill-shaped: rounding based on height
        const float h = ImGui::GetFrameHeight();
        const float rounding = h * 0.5f;

        // Slightly tighter and cleaner
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, rounding);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(s.FramePadding.x + 2.0f, s.FramePadding.y));
        ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, s.PopupRounding > 0.0f ? s.PopupRounding : rounding);

        // Subtle colors (uses existing theme but softens)
        ImVec4 frame = ImGui::GetStyleColorVec4(ImGuiCol_FrameBg);
        ImVec4 hov = ImGui::GetStyleColorVec4(ImGuiCol_FrameBgHovered);
        ImVec4 act = ImGui::GetStyleColorVec4(ImGuiCol_FrameBgActive);

        frame.w = ImClamp(frame.w * 1.10f, 0.0f, 1.0f);
        hov.w = ImClamp(hov.w * 1.05f, 0.0f, 1.0f);
        act.w = ImClamp(act.w * 1.05f, 0.0f, 1.0f);

        ImGui::PushStyleColor(ImGuiCol_FrameBg, frame);
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, hov);
        ImGui::PushStyleColor(ImGuiCol_FrameBgActive, act);

        // Optional: reduce border harshness
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(1, 1, 1, 0.10f));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);

        bool opened = ImGui::BeginCombo(id, preview, ImGuiComboFlags_HeightLargest);

        // If not opened, still need to pop the style we pushed
        if (!opened) {
            ImGui::PopStyleVar(1);            // FrameBorderSize
            ImGui::PopStyleColor(1);          // Border

            ImGui::PopStyleColor(3);          // frame/hover/active
            ImGui::PopStyleVar(3);            // rounding/padding/popup rounding
        }

        return opened;
    }

    void EndPillCombo() {
        ImGui::EndCombo();

        ImGui::PopStyleVar(1);    // FrameBorderSize
        ImGui::PopStyleColor(1);  // Border

        ImGui::PopStyleColor(3);
        ImGui::PopStyleVar(3);
    }

    bool DrawHDRColorPicker(const char* id, HDRColor& hdr) {
        bool changed = false;

        ImGui::PushID(id);

        ImGui::TextUnformatted("HDR Color");
        ImGui::Separator();

        ImGuiColorEditFlags picker_flags =
            ImGuiColorEditFlags_Float |
            ImGuiColorEditFlags_HDR |
            ImGuiColorEditFlags_DisplayRGB |
            ImGuiColorEditFlags_InputRGB |
            ImGuiColorEditFlags_PickerHueWheel |
            ImGuiColorEditFlags_NoSidePreview |
            ImGuiColorEditFlags_NoSmallPreview;

        ImVec2 picker_size(200.0f, 0.0f);
        ImGui::BeginGroup();
        ImGui::SetNextItemWidth(picker_size.x);
        if (ImGui::ColorPicker4("##picker", (float*)&hdr.color, picker_flags))
            changed = true;
        ImGui::EndGroup();

        ImGui::Spacing();
        ImGui::TextUnformatted("Result");
        ImVec4 preview = hdr.color;
        preview.x *= hdr.intensity;
        preview.y *= hdr.intensity;
        preview.z *= hdr.intensity;
        ImGui::ColorButton("##preview", preview,
            ImGuiColorEditFlags_HDR | ImGuiColorEditFlags_NoTooltip,
            ImVec2(60, 20));

        ImGui::Separator();

        ImGui::TextUnformatted("RGB 0-255");
        int rgb[3] = {
            (int)ImClamp(hdr.color.x * 255.0f, 0.0f, 255.0f),
            (int)ImClamp(hdr.color.y * 255.0f, 0.0f, 255.0f),
            (int)ImClamp(hdr.color.z * 255.0f, 0.0f, 255.0f)
        };

        ImGui::SetNextItemWidth(210.0f);
        if (ImGui::DragInt3("##rgb", rgb, 1.0f, 0, 255)) {
            hdr.color.x = rgb[0] / 255.0f;
            hdr.color.y = rgb[1] / 255.0f;
            hdr.color.z = rgb[2] / 255.0f;
            changed = true;
        }

        ImGui::Spacing();
        ImGui::TextUnformatted("Intensity");

        ImGui::SetNextItemWidth(150.0f);
        if (ImGui::SliderFloat("##intensity_slider", &hdr.intensity, 0.0f, 5.0f))
            changed = true;

        ImGui::SameLine();
        ImGui::SetNextItemWidth(70.0f);
        if (ImGui::DragFloat("##intensity_value", &hdr.intensity, 0.01f, 0.0f, 20.0f, "%.5f"))
            changed = true;

        ImGui::Spacing();
        static const float ev_values[] = { -2.0f, -1.0f, 1.0f, 2.0f };
        const char* ev_labels[] = { "-2", "-1", "+1", "+2" };

        for (int i = 0; i < IM_ARRAYSIZE(ev_values); ++i) {
            if (i > 0) ImGui::SameLine();
            if (ImGui::Button(ev_labels[i], ImVec2(40.0f, 30.0f))) {
                hdr.intensity = powf(2.0f, ev_values[i]);
                changed = true;
            }
        }

        ImGui::Separator();

        ImGui::TextUnformatted("Defaults");

        static bool   swatches_initialized = false;
        static ImVec4 swatches[8 * 8];

        if (!swatches_initialized) {
            swatches_initialized = true;
            int idx = 0;
            for (int y = 0; y < 8; ++y) {
                for (int x = 0; x < 8; ++x) {
                    float fx = (float)x / 7.0f;
                    float fy = (float)y / 7.0f;
                    swatches[idx++] = ImVec4(fx, fy, 1.0f - fx, 1.0f);
                }
            }
        }

        float cell = ImGui::GetFrameHeight(); // roughly square
        int   cols = 8;

        for (int i = 0; i < 64; ++i) {
            if (i % cols != 0)
                ImGui::SameLine();

            ImGui::PushID(i);
            if (ImGui::ColorButton("##swatch", swatches[i],
                ImGuiColorEditFlags_NoTooltip |
                ImGuiColorEditFlags_NoDragDrop,
                ImVec2(cell, cell))) {
                hdr.color = swatches[i];
                changed = true;
            }

            // Right-click to save current color into this slot
            if (ImGui::BeginPopupContextItem("swatch_ctx")) {
                if (ImGui::MenuItem("Save current color here")) {
                    swatches[i] = hdr.color;
                }
                ImGui::EndPopup();
            }

            ImGui::PopID();
        }

        ImGui::Spacing();
        ImGui::TextDisabled("Click a swatch to apply.\nRight-click to overwrite.");

        ImGui::PopID();
        return changed;
    }

    // Generic float
    bool DrawFloatControl(const std::string& label, float& value, float step)
    {
        //bool changed = false;
        //ImGui::Text("%s", label.c_str());
        //ImGui::SameLine();
        //changed = ImGui::DragFloat(("##" + label).c_str(), &value, step);
        //return changed;
        bool changed = false;
        ImGui::Text("%s", label.c_str());
        ImGui::SameLine();
        changed = ImGui::InputFloat(("##" + label).c_str(), &value, step);
        return changed;
    }

    bool DrawEnumPillCombo(const char* label, int& currentIndex, const char* const* items, int itemsCount, float rightWidth) {
        if (!items || itemsCount <= 0) return false;
        currentIndex = ImClamp(currentIndex, 0, itemsCount - 1);

        ImGui::PushID(label);

        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(label);

        // Put combo on the same line
        ImGui::SameLine();

        // Width we want for the combo
        const float avail = ImGui::GetContentRegionAvail().x;
        float w = rightWidth <= 0.0f ? avail : ImMin(rightWidth, avail);

        // Align to the RIGHT edge of the window's content region
        const float contentRightX = ImGui::GetWindowContentRegionMax().x; // local (window) space
        float targetX = contentRightX - w;

        // Don't go left of the current cursor (prevents overlap with label)
        targetX = ImMax(targetX, ImGui::GetCursorPosX());

        ImGui::SetCursorPosX(targetX);
        ImGui::SetNextItemWidth(w);

        bool changed = false;
        const char* preview = items[currentIndex];

        if (BeginPillCombo("##enum_combo", preview)) {
            for (int i = 0; i < itemsCount; ++i) {
                bool selected = (i == currentIndex);
                if (ImGui::Selectable(items[i], selected)) {
                    currentIndex = i;
                    changed = true;
                }
                if (selected) ImGui::SetItemDefaultFocus();
            }
            EndPillCombo();
        }

        ImGui::PopID();
        return changed;
    }

    void DrawAssetField(
        const char* label,
        const std::string& assetPath,
        bool rightAligned,
        bool* openPopup,
        ImVec2 size,
        float plusWidth,
        const char* dndPayloadType,
        AssetDropFn onDrop
    ) {
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        if (window->SkipItems) return;

        ImGuiContext& g = *ImGui::GetCurrentContext();
        const ImGuiStyle& style = g.Style;

        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(label);
        ImGui::SameLine();

        const float fieldStartX = ImGui::GetCursorPosX();
        const float avail = ImGui::GetContentRegionAvail().x;

        const float frame_h = ImGui::GetFrameHeight();
        if (size.y <= 0.0f) size.y = frame_h;

        const float kMinDrawWidth = style.FramePadding.x * 2.0f + 6.0f;
        if (avail <= kMinDrawWidth || size.y <= 0.0f)
            return;

        // Default: fill remaining width
        if (size.x <= 0.0f) size.x = avail;
        size.x = ImClamp(size.x, kMinDrawWidth, avail);

        if (rightAligned) {
            float startX = fieldStartX + (avail - size.x);
            ImGui::SetCursorPosX(startX);
        }

        const ImVec2 pos = window->DC.CursorPos;
        const ImRect bb(pos, pos + size);

        ImGui::ItemSize(bb, style.FramePadding.y);

        ImGuiID id = window->GetID((std::string("##") + label).c_str());
        if (!ImGui::ItemAdd(bb, id)) return;

        ImDrawList* dl = ImGui::GetWindowDrawList();
        const float radius = size.y * 0.5f;

        const ImU32 pillCol = IM_COL32(45, 45, 45, 255);
        const ImU32 plusCol = IM_COL32(65, 65, 65, 255);
        const ImU32 plusHover = IM_COL32(90, 90, 90, 255);
        const ImU32 plusLine = IM_COL32(255, 255, 255, 255);

        dl->AddRectFilled(bb.Min, bb.Max, pillCol, radius);

        const float kMinLeftWidth = 12.0f;
        const float kMinPlusWidth = ImMax(size.y, 18.0f);

        const float maxPlus = ImMax(0.0f, size.x - kMinLeftWidth);
        plusWidth = ImClamp(plusWidth, kMinPlusWidth, maxPlus);

        if (plusWidth <= 0.0f || maxPlus <= 0.0f) {
            ImRect pbb(bb.Min, bb.Max);
            if (pbb.GetWidth() <= 0.0f || pbb.GetHeight() <= 0.0f) return;

            ImVec2 cursor_backup = ImGui::GetCursorScreenPos();
            ImGui::SetCursorScreenPos(pbb.Min);
            ImGui::PushID(label);

            bool pressed = ImGui::InvisibleButton("##plus_min", pbb.GetSize());
            bool hovered = ImGui::IsItemHovered();

            ImGui::PopID();
            ImGui::SetCursorScreenPos(cursor_backup);

            dl->AddRectFilled(pbb.Min, pbb.Max, hovered ? plusHover : plusCol, radius);

            ImVec2 center = pbb.GetCenter();
            float s = pbb.GetHeight() * 0.25f;
            dl->AddLine(center + ImVec2(-s, 0), center + ImVec2(s, 0), plusLine, 2.0f);
            dl->AddLine(center + ImVec2(0, -s), center + ImVec2(0, s), plusLine, 2.0f);

            if (pressed && openPopup) *openPopup = true;
            return;
        }

        ImRect plusBB(ImVec2(bb.Max.x - plusWidth, bb.Min.y), bb.Max);
        ImRect leftBB = bb; leftBB.Max.x = plusBB.Min.x;

        const bool canLeft = leftBB.GetWidth() > 0.0f && leftBB.GetHeight() > 0.0f;
        const bool canPlus = plusBB.GetWidth() > 0.0f && plusBB.GetHeight() > 0.0f;

        ImVec2 cursor_backup = ImGui::GetCursorScreenPos();

        bool left_double_clicked = false;
        if (canLeft) {
            ImGui::SetCursorScreenPos(leftBB.Min);
            ImGui::PushID(label);

            ImGui::InvisibleButton("##left", leftBB.GetSize(), ImGuiButtonFlags_AllowOverlap);
            const bool left_hovered = ImGui::IsItemHovered();
            left_double_clicked = left_hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);

            if (dndPayloadType && ImGui::BeginDragDropTarget()) {
                if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload(dndPayloadType)) {
                    if (onDrop) onDrop(p);
                }
                ImGui::EndDragDropTarget();
            }

            ImGui::PopID();
            ImGui::SetCursorScreenPos(cursor_backup);
        }

        bool pressed = false;
        bool hovered = false;
        if (canPlus) {
            ImGui::SetCursorScreenPos(plusBB.Min);
            ImGui::PushID(label);

            pressed = ImGui::InvisibleButton("##plus", plusBB.GetSize(), ImGuiButtonFlags_AllowOverlap);
            hovered = ImGui::IsItemHovered();

            ImGui::PopID();
            ImGui::SetCursorScreenPos(cursor_backup);
        }

        dl->AddRectFilled(plusBB.Min, plusBB.Max, hovered ? plusHover : plusCol,
            radius, ImDrawFlags_RoundCornersRight);

        if (canLeft) {
            ImRect textBB = leftBB;
            textBB.Min.x += style.FramePadding.x;
            textBB.Max.x -= style.FramePadding.x;

            ImGui::RenderTextClipped(
                textBB.Min, textBB.Max,
                assetPath.c_str(), nullptr,
                nullptr, ImVec2(0.0f, 0.5f),
                &textBB
            );
        }

        ImVec2 center = plusBB.GetCenter();
        float s = size.y * 0.25f;
        dl->AddLine(center + ImVec2(-s, 0), center + ImVec2(s, 0), plusLine, 2.0f);
        dl->AddLine(center + ImVec2(0, -s), center + ImVec2(0, s), plusLine, 2.0f);

        if (pressed && openPopup) {
            *openPopup = true;
        } else if (!assetPath.empty() && left_double_clicked) {
            NANOEngine::Events::EventBus::Get().Dispatch(
                NANOEngine::Events::EventDomain::Editor,
                Events::GotoAssetPathEvent{ assetPath }
            );
        }
    }

    void DrawAssetField(const char* label, const std::string& assetPath, bool* openPopup, bool rightAligned, ImVec2 size, float plusWidth) {
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        if (window->SkipItems) return;

        ImGuiContext& g = *ImGui::GetCurrentContext();
        const ImGuiStyle& style = g.Style;

        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(label);
        ImGui::SameLine();

        if (rightAligned) {
            float fullWidth = ImGui::GetContentRegionAvail().x;
            float startX = ImGui::GetCursorPosX() + (fullWidth - size.x);
            ImGui::SetCursorPosX(startX);
        }

        const float frame_h = ImGui::GetFrameHeight();
        if (size.y <= 0.0f) size.y = frame_h;

        if (size.x <= 0.0f) size.x = ImGui::CalcItemWidth();

        const ImVec2 pos = window->DC.CursorPos;
        const ImRect bb(pos, pos + size);

        ImGui::ItemSize(bb, style.FramePadding.y);

        ImGuiID id = window->GetID((std::string("##") + label).c_str());
        if (!ImGui::ItemAdd(bb, id)) return;

        ImDrawList* dl = ImGui::GetWindowDrawList();
        const float radius = size.y * 0.5f;

        // Colors (swap to your theme system later)
        const ImU32 pillCol = IM_COL32(45, 45, 45, 255);
        const ImU32 plusCol = IM_COL32(65, 65, 65, 255);
        const ImU32 plusHover = IM_COL32(90, 90, 90, 255);
        //const ImU32 textCol = ImGui::GetColorU32(ImGuiCol_Text);

        dl->AddRectFilled(bb.Min, bb.Max, pillCol, radius);

        plusWidth = ImMin(plusWidth, size.x);
        ImRect plusBB(ImVec2(bb.Max.x - plusWidth, bb.Min.y), bb.Max);
        ImRect leftBB = bb;
        leftBB.Max.x = plusBB.Min.x;

        ImVec2 cursor_backup = ImGui::GetCursorScreenPos();

        ImGui::SetCursorScreenPos(leftBB.Min);
        ImGui::PushID(label);
        ImGui::InvisibleButton("##left", leftBB.GetSize());
        bool left_hovered = ImGui::IsItemHovered();
        ImGui::PopID();
        ImGui::SetCursorScreenPos(cursor_backup);

        bool left_double_clicked = left_hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);

        ImGui::SetCursorScreenPos(plusBB.Min);
        ImGui::PushID(label);
        bool pressed = ImGui::InvisibleButton("##plus", plusBB.GetSize());
        bool hovered = ImGui::IsItemHovered();
        //bool held = ImGui::IsItemActive();
        ImGui::PopID();

        ImGui::SetCursorScreenPos(cursor_backup);

        dl->AddRectFilled(plusBB.Min, plusBB.Max,
            hovered ? plusHover : plusCol,
            radius, ImDrawFlags_RoundCornersRight);

        ImRect textBB = bb;
        textBB.Max.x = plusBB.Min.x;

        textBB.Min.x += style.FramePadding.x;
        textBB.Max.x -= style.FramePadding.x;

        ImGui::RenderTextClipped(
            textBB.Min, textBB.Max,
            assetPath.c_str(), nullptr,
            nullptr,
            ImVec2(0.0f, 0.5f),
            &textBB
        );

        const ImVec2 center = plusBB.GetCenter();
        const float s = size.y * 0.25f;
        const ImU32 plusLine = IM_COL32(255, 255, 255, 255);

        dl->AddLine(center + ImVec2(-s, 0), center + ImVec2(s, 0), plusLine, 2.0f);
        dl->AddLine(center + ImVec2(0, -s), center + ImVec2(0, s), plusLine, 2.0f);

        if (pressed && openPopup) 
            *openPopup = true;
        else if (!assetPath.empty() && left_double_clicked)
            NANOEngine::Events::EventBus::Get().Dispatch(
                NANOEngine::Events::EventDomain::Editor,
                Events::GotoAssetPathEvent{ assetPath }
            );
    }

    // New Styling
    void ToolTip(const char* text) {
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
            ImGui::BeginTooltip();
            ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
            ImGui::TextUnformatted(text);
            ImGui::PopTextWrapPos();
            ImGui::EndTooltip();
        }
    }

    bool DrawFloatSliderWithField(const char* label, float& value, float min, float max, float step, bool rightAligned) {
        bool changed = false;
        ImGui::PushID(label);

        // Label
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(label);
        ImGui::SameLine();

        ImGuiStyle& style = ImGui::GetStyle();

        float fullWidth = ImGui::GetContentRegionAvail().x;
        float sliderWidth = fullWidth * 0.70f;
        float inputWidth = 60.0f;
        float spacing = style.ItemInnerSpacing.x;
        float totalWidth = sliderWidth + spacing + inputWidth;

        if (rightAligned) {
            float startX = ImGui::GetCursorPosX() + (fullWidth - totalWidth);
            if (startX < ImGui::GetCursorPosX())
                startX = ImGui::GetCursorPosX();
            ImGui::SetCursorPosX(startX);
        }

        ImGui::SetNextItemWidth(sliderWidth);
        changed |= ImGui::SliderFloat("##slider", &value, min, max, " ");

        ImGui::SameLine();

        ImGui::SetNextItemWidth(inputWidth);
        changed |= ImGui::DragFloat("##input", &value, step, min, max, "%.2f");

        ImGui::PopID();
        return changed;
    }

    bool DrawFloatField(const char* label, float& value, float step, bool rightAligned) {
        bool changed = false;
        ImGui::PushID(label);

        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(label);
        ImGui::SameLine();

        //ImGuiStyle& style = ImGui::GetStyle();
        float fullWidth = ImGui::GetContentRegionAvail().x;
        float fieldWidth = 150.f;

        if (rightAligned) {
            float startX = ImGui::GetCursorPosX() + (fullWidth - fieldWidth);
            ImGui::SetCursorPosX(startX);
        }

        ImGui::SetNextItemWidth(fieldWidth);
        changed |= ImGui::DragFloat("##input", &value, step, -FLT_MAX, FLT_MAX, "%.3f");

        ImGui::PopID();
        return changed;
    }

    bool DrawIntField(const char* label, int& value, bool rightAligned) {
        bool changed = false;
        ImGui::PushID(label);

        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(label);
        ImGui::SameLine();

        //ImGuiStyle& style = ImGui::GetStyle();
        float fullWidth = ImGui::GetContentRegionAvail().x;
        float fieldWidth = 150.f;

        if (rightAligned) {
            float startX = ImGui::GetCursorPosX() + (fullWidth - fieldWidth);
            ImGui::SetCursorPosX(startX);
        }

        ImGui::SetNextItemWidth(fieldWidth);
        changed |= ImGui::DragInt("##input", &value, 1);

        ImGui::PopID();
        return changed;
    }

    // Generic int
    bool DrawIntControl(const std::string& label, int& value)
    {
        bool changed = false;
        ImGui::Text("%s", label.c_str());
        ImGui::SameLine();
        changed = ImGui::DragInt(("##" + label).c_str(), &value);
        return changed;
    }

    // Checkbox (bool)
    bool DrawCheckbox(const std::string& label, bool& value)
    {
        return ImGui::Checkbox(label.c_str(), &value);
    }

    // String (with buffer size)
    bool DrawStringControl(const std::string& label, std::string& value, size_t bufferSize)
    {
        char buffer[256];
        strncpy_s(buffer, value.c_str(), bufferSize);
        buffer[bufferSize - 1] = 0;
        bool changed = ImGui::InputText(label.c_str(), buffer, bufferSize);
        if (changed) value = buffer;
        return changed;
    }

    bool DrawAssetField(const char* label, const std::string& assetPath, const char* buttonLabel, float width, bool* openPopup)
    {
        bool changed = false;

        ImGui::Text("%s", label); ImGui::SameLine();

        ImGui::PushID(label);

        if (width == 0) width = ImGui::GetContentRegionAvail().x;
        ImGui::BeginChild("AssetBox", ImVec2(width, 28), true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(assetPath.empty() ? "<None>" : assetPath.c_str());
        ImGui::SameLine();

        std::string popupName = std::string("AssetPicker_") + label;

        float btnWidth = 28.0f;
        ImGui::SetCursorPosX(ImGui::GetWindowWidth() - btnWidth - ImGui::GetStyle().FramePadding.x);
        if (ImGui::Button(buttonLabel, ImVec2(btnWidth, 0))) {
            // TBD open asset picker
            // set a flag or call a callback
			//std::string assetPickerPopupName = std::string("AssetPicker_") + label;
            //ImGui::OpenPopup(("AssetPicker_" + std::string(label)).c_str());
            //ImGui::OpenPopup("AssetPicker_Model");
            //changed = true; // or open a popup, etc.
            *openPopup = true;
        }

        ImGui::EndChild();

        ImGui::PopID();
        return changed;
    }

    bool DrawTextureField(
        const char* label,
        const std::shared_ptr<NE::Graphics::OpenGL::GLTexture>& slotTex,
        float previewSize,
        std::function<void(const std::string&)> assignById)
    {
        bool changed = false;

        ImGui::PushID(label);
        ImGui::TextUnformatted(label);

        if (slotTex) {
            //if (auto* gltex = dynamic_cast<NE::Graphics::OpenGL::GLTexture*>(slotTex.get()))
            //    img = (ImTextureID)(uintptr_t)gltex->GLName();

            unsigned int img = Assets::ThumbnailManager::GetInstance().GetThumbnailByUUID(slotTex->uuid);

            ImGui::Image((ImTextureID)(intptr_t)img, ImVec2(previewSize, previewSize));
            ImGui::SameLine();
            if (ImGui::Button("x")) {
                assignById("");
                changed = true;
            }
        } else {
            ImVec2 cursor = ImGui::GetCursorScreenPos();
            ImDrawList* dl = ImGui::GetWindowDrawList();
            ImVec2 rectMax = ImVec2(cursor.x + previewSize, cursor.y + previewSize);

            dl->AddRectFilled(cursor, rectMax, IM_COL32(50, 50, 50, 255)); // background
            dl->AddRect(cursor, rectMax, IM_COL32(100, 100, 100, 255));    // border

            // Add '+' sign
            ImVec2 center = ImVec2(cursor.x + previewSize * 0.5f, cursor.y + previewSize * 0.5f);
            float half = previewSize * 0.25f;
            dl->AddLine(ImVec2(center.x - half, center.y), ImVec2(center.x + half, center.y), IM_COL32(180, 180, 180, 255), 2.0f);
            dl->AddLine(ImVec2(center.x, center.y - half), ImVec2(center.x, center.y + half), IM_COL32(180, 180, 180, 255), 2.0f);

            ImGui::Dummy(ImVec2(previewSize, previewSize));

            ImGui::SetCursorScreenPos(cursor); // reset cursor to top-left of the quad
            if (ImGui::InvisibleButton("##texture_slot_btn", ImVec2(previewSize, previewSize))) {
                ImGui::OpenPopup("AssetPicker_Texture");
            }
        }

        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload("TEXTURE_ASSET_PATH")) {
                std::string dropped((const char*)p->Data, p->DataSize - 1);
                auto uuid = Assets::AssetManager::GetInstance().RetrieveUUID(dropped);
                assignById(uuid);
                changed = true;
            }
            ImGui::EndDragDropTarget();
        }

        if (ImGui::BeginPopup("AssetPicker_Texture")) {
            ImGui::Text("Select a texture");
            ImGui::Separator();
            auto& textureList = Assets::AssetManager::GetInstance().GetInstance().GetAssetsOfType(Assets::AssetType::Texture);

            if (ImSearch::BeginSearch()) {
                ImSearch::SearchBar();
                for (const auto& [textureName, uuid] : textureList) {
                    ImSearch::SearchableItem(textureName.c_str(), [&, textureName](const char*) {
                        if (ImGui::Selectable(textureName.c_str())) {
                            assignById(uuid);
                            ImGui::CloseCurrentPopup();
                        }
                        });
                }
                ImSearch::EndSearch();
            }
            ImGui::EndPopup();
        }

        ImGui::PopID();
        return changed;
    }

    bool DrawHDRColorField(const char* label, HDRColor& hdr) {
        bool changed = false;

        ImGui::PushID(label);

        // Label
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(label);
        ImGui::SameLine();

        ImVec4 preview = hdr.color;
        preview.x *= hdr.intensity;
        preview.y *= hdr.intensity;
        preview.z *= hdr.intensity;

        float fullWidth = ImGui::GetContentRegionAvail().x;
        ImVec2 barSize(fullWidth, ImGui::GetFrameHeight());

        ImVec2 popupPos(0, 0);
        if (ImGui::ColorButton("##hdr_bar", preview,
            ImGuiColorEditFlags_HDR |
            ImGuiColorEditFlags_NoTooltip,
            barSize)) {
            ImGui::OpenPopup("HDRPickerPopup");
        }

        popupPos = ImVec2(ImGui::GetItemRectMin().x,
            ImGui::GetItemRectMax().y + 4.0f);
        ImGui::SetNextWindowPos(popupPos, ImGuiCond_Appearing);

        if (ImGui::BeginPopup("HDRPickerPopup", ImGuiWindowFlags_AlwaysAutoResize)) {
            if (DrawHDRColorPicker("##HDRPickerContent", hdr))
                changed = true;

            ImGui::EndPopup();
        }

        ImGui::PopID();
        return changed;
    }

    // NEW STYLING
    bool DrawVec3Control(const char* label,
        NE::Math::Vec3& v,
        float label_width,
        float drag_speed_per_pixel,
        float reset_value,
        int precision)
    {
        ImGui::PushID(label);

        const float line_start_x = ImGui::GetCursorPosX();
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(label);

        ImGui::SameLine();
        const float target_x = line_start_x + label_width;
        ImGui::SetCursorPosX(std::max(ImGui::GetCursorPosX(), target_x));

        const float avail = ImGui::GetContentRegionAvail().x;
        const float spacing = ImGui::GetStyle().ItemSpacing.x;
        const float w = std::max(1.0f, (avail - spacing * 2.0f) / 3.0f);

        bool changed = false;

        changed |= DrawAxisPill("X", "X", &v.x, IM_COL32(220, 60, 60, 255), w, drag_speed_per_pixel, reset_value, precision);
        ImGui::SameLine(0.0f, spacing);
        changed |= DrawAxisPill("Y", "Y", &v.y, IM_COL32(60, 180, 75, 255), w, drag_speed_per_pixel, reset_value, precision);
        ImGui::SameLine(0.0f, spacing);
        changed |= DrawAxisPill("Z", "Z", &v.z, IM_COL32(70, 120, 230, 255), w, drag_speed_per_pixel, reset_value, precision);

        ImGui::PopID();
        return changed;
    }

    bool DrawVec2Control(const char* label, NE::Math::Vec2& v, float label_width, float drag_speed_per_pixel, float reset_value, int precision) {
        ImGui::PushID(label);

        const float line_start_x = ImGui::GetCursorPosX();
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(label);

        ImGui::SameLine();
        const float target_x = line_start_x + label_width;
        ImGui::SetCursorPosX(std::max(ImGui::GetCursorPosX(), target_x));

        const float avail = ImGui::GetContentRegionAvail().x;
        const float spacing = ImGui::GetStyle().ItemSpacing.x;
        const float w = std::max(1.0f, (avail - spacing * 2.0f) / 2.0f);

        bool changed = false;

        changed |= DrawAxisPill("X", "X", &v.x, IM_COL32(220, 60, 60, 255), w, drag_speed_per_pixel, reset_value, precision);
        ImGui::SameLine(0.0f, spacing);
        changed |= DrawAxisPill("Y", "Y", &v.y, IM_COL32(60, 180, 75, 255), w, drag_speed_per_pixel, reset_value, precision);

        ImGui::PopID();
        return changed;
    }
}