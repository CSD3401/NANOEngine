#include "InspectorPanel.hpp"
#include <imgui/imgui.h>
#include <imgui/misc/cpp/imgui_stdlib.h>
#include <EditorInterface/ECSExports.hpp>
#include <EditorInterface/RendererExports.hpp>
#include <EditorInterface/PhysicsExports.hpp>
#include "ECS/Core/Signature.hpp"
#include <ECS/Components/Transform.hpp>
#include <ECS/Components/Renderer.hpp>
#include <ECS/Components/Light.hpp>
#include <ECS/Components/Rigidbody.hpp>
#include <ECS/Components/Collider.hpp>
#include <ECS/Components/AudioSource.hpp>
#include <ECS/Components/NativeScript.hpp>
#include <ECS/Components/EntityMeta.hpp>
#include <ECS/Components/UIRectTransform.hpp>
#include <ECS/Components/UICanvas.hpp>
#include <ECS/Components/UIImage.hpp>
#include <ECS/Components/UIButton.hpp>
#include <ECS/Components/UIText.hpp>
#include <ECS/Components/Animator.hpp>
#include <ECS/Components/Camera.hpp>
#include <Core/Reflection.hpp>
#include <Math/Vec3.hpp>
#include "Math/Vec4.hpp"
#include "../EditorScene.hpp"
//#include <Engine.hpp>
#include <imgui/widgets/imsearch/imsearch.h>
#include "../EditorUI.hpp"
#include "../Command/EditorSetFieldCommand.hpp"
#include "../Command/CommandHistory.hpp"
#include <unordered_map>
#include <typeinfo>
#include <bit>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <sstream>
#include <vector>
#include "../AssetManagement/AssetManager.hpp"
#include "../AssetManagement/Settings/TextureImportSettings.hpp"
#include "../AssetManagement/Settings/ModelImportSettings.hpp"
#include <Core/SpdLogger.hpp>
#include <fstream>
#include <rapidjson/document.h>
#include <Serialisation/ReflectionJson.hpp>
#include <rapidjson/istreamwrapper.h>
#include "../EditorLayers.hpp"

namespace {
	// the widget maker
	// takes a field and draws the right UI widget for it
	// bool -> checkbox
	// int -> number dragger
	// float -> decimal dragger
	// vec3 -> 3 number inputs
	// string -> text input box
	template<typename Owner, typename T>
	bool DrawField(const NE::Core::FieldDescriptor<Owner, T>& desc, T& value) {
		if (NE::Core::HasFlag(desc.flags, NE::Core::FieldFlags::HiddenInEditor))
			return false;

		if constexpr (std::is_same_v<T, bool>) {
			return ImGui::Checkbox(desc.name.data(), &value);
		}
		else if constexpr (std::is_same_v<T, int>) {
			return ImGui::DragInt(desc.name.data(), &value);
		}
		else if constexpr (std::is_same_v<T, float>) {
			return ImGui::DragFloat(desc.name.data(), &value, 0.1f);
		}
		else if constexpr (std::is_same_v<T, NE::Math::Vec3>) {
			ImGui::BeginGroup();
			bool changed = Editor::DrawVec3Control(desc.name.data(), value, 0.0f, 75.0f);
			ImGui::EndGroup();
			return changed;
		}
		else if constexpr (std::is_same_v<T, NE::Math::Vec4>) {
			// Vec4 is used for colors - use ColorEdit4
			float color[4] = { value.x, value.y, value.z, value.w };
			if (ImGui::ColorEdit4(desc.name.data(), color, ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreviewHalf)) {
				value.x = color[0];
				value.y = color[1];
				value.z = color[2];
				value.w = color[3];
				return true;
			}
			return false;
		}
		else if constexpr (std::is_same_v<T, std::string>) {
			// String support added here -> check w irwen
			char buffer[256];
			strncpy_s(buffer, sizeof(buffer), value.c_str(), sizeof(buffer));
			buffer[sizeof(buffer) - 1] = '\0';

			if (ImGui::InputText(desc.name.data(), buffer, sizeof(buffer))) {
				value = buffer;
				return true;
			}
			return false;
		}
		else {
			ImGui::Text("%s (unsupported)", desc.name.data());
			return false;
		}
	}

	// Helpers for scripting field parsing/formatting
	static NE::Math::Vec3 Vec3FromString(const std::string& s) {
		NE::Math::Vec3 v{ 0.f,0.f,0.f };
		std::istringstream iss(s);
		iss >> v.x >> v.y >> v.z;
		return v;
	}
	static std::string Vec3ToString(const NE::Math::Vec3& v) {
		std::ostringstream oss;
		oss << v.x << ' ' << v.y << ' ' << v.z;
		return oss.str();
	}

	// when you change sth in the inspector, this creates an undo/redo command
	template <typename Owner, typename T>
	static void SubmitSetFieldCommand(uint32_t entity,
		const NE::Core::FieldDescriptor<Owner, T>& desc,
		const T& before,
		const T& after) {
		using Cmd = Editor::SetFieldCommand<Owner, T>;

		auto getter = [=](uint32_t e) -> Owner& {
			if constexpr (std::is_same_v<Owner, NE::ECS::Component::Transform>) {
				return NE::ECS::Command::GetEntityTransform(e);
			}
			else if constexpr (std::is_same_v<Owner, NE::ECS::Component::Collider>) {
				return NE::ECS::Command::GetEntityCollider(e);
			}
			else if constexpr (std::is_same_v<Owner, NE::ECS::Component::Rigidbody>) {
				return NE::ECS::Command::GetEntityRigidbody(e);
			}
			else if constexpr (std::is_same_v<Owner, NE::ECS::Component::Renderer>) {
				return NE::ECS::Command::GetEntityRenderer(e);
			}
			else if constexpr (std::is_same_v<Owner, NE::ECS::Component::Light>) {
				return NE::ECS::Command::GetEntityLight(e);
			}
			else {
				static_assert(sizeof(Owner) == 0, "No getter defined for this component type.");
			}
			};

		auto cmd = std::make_unique<Cmd>(entity,
			std::string(desc.name),
			desc.member,
			before,
			after,
			getter);

		Editor::CommandHistory::GetInstance().ExecuteCommand(std::move(cmd));
	}

	// converts a member pointer into a unique hash number for fast look ups
	template <class Owner, class T>
	struct MemberPointerHasher {
		size_t operator()(T Owner::* mp) const noexcept {
			auto bytes = std::bit_cast<std::array<std::byte, sizeof(mp)>>(mp);
			size_t h = 1469598103934665603ull;
			for (std::byte b : bytes) {
				h ^= static_cast<unsigned char>(b);
				h *= 1099511628211ull;
			}
			return h;
		}
	};

	// check if 2 field keys are identical by comparing entity ID, component type, field ID
	struct FieldKey {
		uint32_t entity;
		const std::type_info* ownerType;
		size_t memberId;  // hashed member pointer

		bool operator==(const FieldKey& o) const noexcept {
			return entity == o.entity && ownerType == o.ownerType && memberId == o.memberId;
		}
	};

	// combines all parts of a field key into a single hash number for use in unordered maps
	struct FieldKeyHash {
		size_t operator()(const FieldKey& k) const noexcept {
			size_t h = std::hash<uint32_t>{}(k.entity);
			h ^= std::hash<const void*>{}(k.ownerType) + 0x9e3779b9 + (h << 6) + (h >> 2);
			h ^= k.memberId + 0x9e3779b9 + (h << 6) + (h >> 2);
			return h;
		}
	};

	// compares 2 value for equality, with special handling for floats
	template<class T>
	static bool Equal(const T& a, const T& b) {
		if constexpr (std::is_floating_point_v<T>) {
			return std::fabs(a - b) <= 1e-6f;
		}
		else {
			return a == b;
		}
	}

	bool LoadModelImportSettings(std::string metaPath, Editor::ModelImportSettings& settings) {
		using namespace rapidjson;
		using namespace NE::Serialization;

		if (!std::filesystem::exists(metaPath))
			return false;

		std::ifstream ifs(metaPath);
		if (!ifs) {
			SPD_WARNING("Failed to read meta file: " << metaPath);
			return false;
		}

		IStreamWrapper isw(ifs);
		Document doc;
		doc.ParseStream(isw);

		if (doc.HasParseError() || !doc.IsObject()) {
			SPD_WARNING("Failed to parse JSON in meta file: " << metaPath);
			return false;
		}

		if (!doc.HasMember("modelImport") || !doc["modelImport"].IsObject())
			return true;

		const auto& jSettings = doc["modelImport"];
		from_json(jSettings, settings);

		return true;
	}

	bool LoadTextureImportSettings(std::string metaPath, Editor::TextureImportSettings& settings) {
		using namespace rapidjson;
		using namespace NE::Serialization;

		if (!std::filesystem::exists(metaPath))
			return false;

		std::ifstream ifs(metaPath);
		if (!ifs) {
			SPD_WARNING("Failed to read meta file: " << metaPath);
			return false;
		}

		IStreamWrapper isw(ifs);
		Document doc;
		doc.ParseStream(isw);

		if (doc.HasParseError() || !doc.IsObject()) {
			SPD_WARNING("Failed to parse JSON in meta file: " << metaPath);
			return false;
		}

		if (!doc.HasMember("textureImport") || !doc["textureImport"].IsObject())
			return true;

		const auto& jSettings = doc["textureImport"];
		from_json(jSettings, settings);

		return true;
	}

	// helpers for UI
	// get the correct material path based on canvas render mode
	std::string GetUIMaterialPathForRenderMode(NE::ECS::Component::UICanvas::RenderMode mode) {
		switch (mode) {
		case NE::ECS::Component::UICanvas::RenderMode::SCREEN_SPACE_OVERLAY:
			return "Assets/UI_Overlay.nanomat";
		case NE::ECS::Component::UICanvas::RenderMode::SCREEN_SPACE_CAMERA:
			return "Assets/UI_Camera.nanomat";
		case NE::ECS::Component::UICanvas::RenderMode::WORLD_SPACE:
			return "Assets/UI_World.nanomat";
		default:
			return "Assets/UI_Overlay.nanomat";
		}
	}

	// check if an entity is a child of a specific canvas
	bool IsChildOfCanvas(uint32_t entity, uint32_t canvasEntity) {
		if (!NE::ECS::Query::HasUIRectTransform(entity)) return false;

		auto& rect = NE::ECS::Query::GetUIRectTransform(entity);
		uint32_t current = rect.parent;

		// Walk up hierarchy
		while (current != NE::ECS::NO_ENTITY) {
			if (current == canvasEntity) return true;

			if (!NE::ECS::Query::HasUIRectTransform(current)) break;
			current = NE::ECS::Query::GetUIRectTransform(current).parent;
		}

		// Direct child check
		return (rect.parent == canvasEntity);
	}

	// rebuild materials for all children of a canvas
	void RebuildChildMaterials(uint32_t canvasEntity, const std::string& materialUUID) {
		// Get all entities in the scene
		auto allEntities = NE::GetNumEntities();

		for (uint32_t entity : allEntities) {
			// Skip if not a UI element
			if (!NE::ECS::Query::HasUIRectTransform(entity)) continue;
			if (!NE::ECS::Query::HasUIImage(entity)) continue;

			// Check if this entity is a child of the canvas
			if (IsChildOfCanvas(entity, canvasEntity)) {
				auto& img = NE::ECS::Command::GetUIImage(entity);

				// Only reassign if the entity has a texture
				if (!img.textureUUID.empty()) {
					NE::Renderer::Command::AssignUITexture(entity, img.textureUUID, materialUUID);

					SPD_DEBUG("[InspectorPanel] Rebuilt material for entity {} with new render mode material",
						entity);
				}
			}
		}
	}
}

namespace Editor {
	std::unordered_map<std::type_index, uint8_t> componentTypeRegistry;

	static std::unordered_map<FieldKey,
		std::unique_ptr<ICommand>,
		FieldKeyHash> g_activeCommands;

	InspectorPanel::InspectorPanel() {
		componentTypeRegistry = NE::ECS::Query::GetRegisteredComponentTypes();
	}

	void InspectorPanel::OnImGuiRender()
	{
		ImGui::Begin("Inspector", nullptr);

		if (EditorScene::s_selectedEntity)
		{
			uint32_t entity = EditorScene::s_selectedEntity->linkedEntity;

			// Entity name field only (isActive is now handled by EntityMeta component)
			{
				using Owner = NE::ECS::Component::EntityMeta;
				using FieldT = std::string;

				auto& metaRO = NE::ECS::Command::GetEntityMeta(entity);

				bool isActiveValue = metaRO.isActive;
				if (ImGui::Checkbox("isActive", &isActiveValue)) {
					//metaRO.isActive = isActiveValue;

					// DONE HERE FOR NOW, SHOULD BE DONE IN SYSTEMS !! OR ELSEWHERE
					EditorScene::SetAllDescendantsActive(entity, isActiveValue);
					NE::MarkSceneDirty();
				}

				ImGui::SameLine();

				FieldKey nameKey{
					entity,
					&typeid(Owner),
					MemberPointerHasher<Owner, FieldT>{}(&Owner::name)
				};

				std::string currentText;
				if (auto it = g_activeCommands.find(nameKey); it != g_activeCommands.end()) {
					if (auto* live = dynamic_cast<Editor::SetFieldCommand<Owner, FieldT>*>(it->second.get())) {
						currentText = live->After();
					}
				}
				if (currentText.empty()) currentText = metaRO.name;

				std::string edited = currentText;

				ImGui::PushID("EntityName");
				bool changed = ImGui::InputText(
					"##Name",
					&edited,
					ImGuiInputTextFlags_AutoSelectAll
				);
				bool activated = ImGui::IsItemActivated();
				bool active = ImGui::IsItemActive();
				bool deactivated = ImGui::IsItemDeactivatedAfterEdit();
				ImGui::PopID();

				if (activated && !g_activeCommands.contains(nameKey)) {
					using Cmd = Editor::SetFieldCommand<Owner, FieldT>;
					auto cmd = std::make_unique<Cmd>(
						entity,
						std::string("Rename Entity"),
						&Owner::name,
						metaRO.name,
						metaRO.name,
						&NE::ECS::Command::GetEntityMeta
					);
					g_activeCommands[nameKey] = std::move(cmd);
				}

				// Safety net: if the Activated frame was missed but we're changing, create it now
				if ((active && changed) && !g_activeCommands.contains(nameKey)) {
					using Cmd = Editor::SetFieldCommand<Owner, FieldT>;
					auto cmd = std::make_unique<Cmd>(
						entity, std::string("Rename Entity"),
						&Owner::name, metaRO.name, metaRO.name,
						&NE::ECS::Command::GetEntityMeta);
					g_activeCommands[nameKey] = std::move(cmd);
				}

				// During edit: coalesce by updating After() and applying immediately
				if (active && changed) {
					auto it = g_activeCommands.find(nameKey);
					if (it != g_activeCommands.end()) {
						using Cmd = Editor::SetFieldCommand<Owner, FieldT>;
						Cmd tmp(
							entity, std::string{}, &Owner::name,
							metaRO.name,
							edited,
							&NE::ECS::Command::GetEntityMeta
						);
						it->second->CoalesceFrom(tmp);
					}
				}

				if (deactivated) {
					auto it = g_activeCommands.find(nameKey);
					if (it != g_activeCommands.end()) {
						if (auto* c = dynamic_cast<Editor::SetFieldCommand<Owner, FieldT>*>(it->second.get())) {
							if (c->Before() == c->After()) {
								// No net change: just drop the command
								g_activeCommands.erase(it);
							}
							else {
								// There *was* a change: commit it
								Editor::CommandHistory::GetInstance()
									.ExecuteCommand(std::move(it->second));
								g_activeCommands.erase(it);
							}
						}
						else {
							// Fallback: if not a SetFieldCommand, just execute & erase
							Editor::CommandHistory::GetInstance()
								.ExecuteCommand(std::move(it->second));
							g_activeCommands.erase(it);
						}
					}
				}

				//const char* previewTag[1] = { "Under Dev" };

				//ImGui::PushID("EntityTag");
				//if (ImGui::BeginCombo("Tag", *previewTag)) {
				//	int tagCount = 0;
				//	for (int i = 0; i < tagCount; ++i) {
				//		bool selected = (i == 0);

				//		if (selected)
				//			ImGui::SetItemDefaultFocus();
				//	}
				//	ImGui::EndCombo();
				//}
				//ImGui::PopID();

				//ImGui::SameLine();

				//using LayerFieldT = uint8_t;

				//int currentLayer = (int)metaRO.layer;
				//int newLayer = currentLayer;

				//const char* preview = Editor::Layers::GetLayerName(currentLayer);

				//ImGui::PushID("EntityLayer");
				//if (ImGui::BeginCombo("Layer", preview)) {
				//	int layerCount = Editor::Layers::GetLayerCount();
				//	for (int i = 0; i < layerCount; ++i) {
				//		bool selected = (i == currentLayer);
				//		if (ImGui::Selectable(Editor::Layers::GetLayerName(i), selected)) {
				//			newLayer = i;
				//		}
				//		if (selected)
				//			ImGui::SetItemDefaultFocus();
				//	}
				//	ImGui::EndCombo();
				//}
				//ImGui::PopID();

				//if (newLayer != currentLayer) {
				//	FieldKey layerKey{
				//		entity,
				//		&typeid(Owner),
				//		MemberPointerHasher<Owner, LayerFieldT>{}(&Owner::layer)
				//	};

				//	using Cmd = Editor::SetFieldCommand<Owner, LayerFieldT>;
				//	auto cmd = std::make_unique<Cmd>(
				//		entity,
				//		std::string("Change Layer"),
				//		&Owner::layer,
				//		metaRO.layer,
				//		static_cast<LayerFieldT>(newLayer),
				//		&NE::ECS::Command::GetEntityMeta
				//	);

				//	Editor::CommandHistory::GetInstance().ExecuteCommand(std::move(cmd));
				//}

				// =========================================
// Shared width for both Tag & Layer combos
// =========================================
				const float comboWidth = 140.0f;

				// =========================================
				// TAG COMBO
				// =========================================
				const char* previewTag = "Under Dev";   // Should later come from metaRO.tag

				ImGui::PushID("EntityTag");
				ImGui::PushItemWidth(comboWidth);

				if (ImGui::BeginCombo("Tag", previewTag)) {
					// Currently empty, but structure ready
					// Example:
					// for (int i = 0; i < tagCount; ++i) { ... }
					ImGui::TextDisabled("[No Tags Yet]");
					ImGui::EndCombo();
				}

				ImGui::PopItemWidth();
				ImGui::PopID();

				ImGui::SameLine();

				// =========================================
				// LAYER COMBO
				// =========================================
				using LayerFieldT = uint8_t;

				int currentLayer = (int)metaRO.layer;
				int newLayer = currentLayer;

				const char* preview = Editor::Layers::GetLayerName(currentLayer);

				ImGui::PushID("EntityLayer");
				ImGui::PushItemWidth(comboWidth);

				if (ImGui::BeginCombo("Layer", preview)) {
					int layerCount = Editor::Layers::GetLayerCount();
					for (int i = 0; i < layerCount; ++i) {
						bool selected = (i == currentLayer);
						if (ImGui::Selectable(Editor::Layers::GetLayerName(i), selected))
							newLayer = i;

						if (selected)
							ImGui::SetItemDefaultFocus();
					}
					ImGui::EndCombo();
				}

				ImGui::PopItemWidth();
				ImGui::PopID();

				// =========================================
				// CHANGE COMMAND
				// =========================================
				if (newLayer != currentLayer) {
					FieldKey layerKey{
						entity,
						&typeid(Owner),
						MemberPointerHasher<Owner, LayerFieldT>{}(&Owner::layer)
					};

					using Cmd = Editor::SetFieldCommand<Owner, LayerFieldT>;
					auto cmd = std::make_unique<Cmd>(
						entity,
						std::string("Change Layer"),
						&Owner::layer,
						metaRO.layer,
						static_cast<LayerFieldT>(newLayer),
						&NE::ECS::Command::GetEntityMeta
					);

					Editor::CommandHistory::GetInstance().ExecuteCommand(std::move(cmd));
				}

				if (metaRO.prefabID != "") {
					ImGui::Text("Prefab");
					ImGui::SameLine();
					ImGui::Text(metaRO.prefabID.c_str());
				}
			}

			NE::ECS::Signature sig(NE::ECS::Query::GetEntitySignature(entity));
			for (const auto& [typeIdx, compType] : componentTypeRegistry)
			{
				if (!sig.test(compType)) continue;

				if (typeIdx == typeid(NE::ECS::Component::Transform)) {
					auto& comp = NE::ECS::Query::GetEntityTransform(entity);
					ImGui::SeparatorText("Transform");
					//NE::Core::ForEachFieldView<NE::ECS::Component::Transform>(comp,
					//    [&](auto const& desc, auto const& currentValue) {
					//        using FieldT = std::decay_t<decltype(currentValue)>;

					//        FieldT edited = currentValue;

					//        if (DrawField(desc, edited)) {
					//            SubmitSetFieldCommand(entity, desc, currentValue, edited);
					//        }
					//    });

					NE::Core::ForEachFieldView<NE::ECS::Component::Transform>(comp,
						[&](auto const& desc, auto const& currentValue) {
							using Owner = NE::ECS::Component::Transform;
							using FieldT = std::decay_t<decltype(currentValue)>;

							FieldT edited = currentValue;

							ImGui::PushID(desc.name.data());
							const bool changed = DrawField(desc, edited);
							const bool activated = ImGui::IsItemActivated();
							const bool active = ImGui::IsItemActive();
							const bool deactivated = ImGui::IsItemDeactivatedAfterEdit();
							ImGui::PopID();

							FieldKey key{
								entity,
								&typeid(Owner),
								MemberPointerHasher<Owner, FieldT>{}(desc.member)
							};

							if (activated) {
								using Cmd = Editor::SetFieldCommand<Owner, FieldT>;
								auto cmd = std::make_unique<Cmd>(
									entity,
									std::string("Set Transform") + desc.name.data(),
									desc.member,
									currentValue,
									currentValue,
									&NE::ECS::Command::GetEntityTransform
								);
								g_activeCommands[key] = std::move(cmd);
							}

							if (active && changed) {
								auto it = g_activeCommands.find(key);
								if (it != g_activeCommands.end()) {
									using Cmd = Editor::SetFieldCommand<Owner, FieldT>;
									Cmd tmp(
										entity,
										std::string{},
										desc.member,
										currentValue,
										edited,
										&NE::ECS::Command::GetEntityTransform
									);
									it->second->CoalesceFrom(tmp);
								}
							}

							if (deactivated) {
								auto it = g_activeCommands.find(key);
								if (it != g_activeCommands.end()) {
									auto* asSet = dynamic_cast<Editor::SetFieldCommand<Owner, FieldT>*>(it->second.get());
									if (asSet && Equal(asSet->Before(), asSet->After())) {
										g_activeCommands.erase(it);
									}
									else {
										Editor::CommandHistory::GetInstance()
											.ExecuteCommand(std::move(it->second));
										g_activeCommands.erase(it);
									}
								}
							}
						});
				}
				else if (typeIdx == typeid(NE::ECS::Component::Renderer))
				{
					auto& comp = NE::ECS::Query::GetEntityRenderer(entity);
					ImGui::SeparatorText("Renderer");
					// Model field
					bool openPopup = false;
					DrawAssetField("Model", AssetManager::GetInstance().RetrieveFileName(comp.modelUUID), "+", 0.f, &openPopup);
					if (openPopup) {
						ImGui::OpenPopup("AssetPicker_Model");
					}

					if (ImGui::BeginDragDropTarget()) {
						if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload("ASSET_MESH_PATH")) {
							std::string dropped((const char*)p->Data, p->DataSize - 1);
							auto uuid = AssetManager::GetInstance().RetrieveUUID(dropped);
							NE::Renderer::Command::AssignModel(entity, uuid);
						}
						ImGui::EndDragDropTarget();
					}

					static std::string searchQuery;
					if (ImGui::BeginPopup("AssetPicker_Model")) {
						ImGui::Text("Select a Model");
						ImGui::Separator();
						//auto& modelList = AssetManager::GetInstance().GetInstance().GetAssetsOfType<AssetType::Mesh>();
						auto& modelList = AssetManager::GetInstance().GetAssetsOfType<AssetType::Mesh>();

						if (ImSearch::BeginSearch()) {
							ImSearch::SearchBar();
							for (const auto& [modelName, uuid] : modelList) {
								ImSearch::SearchableItem(modelName.c_str(), [&, modelName](const char*) {
									if (ImGui::Selectable(modelName.c_str())) {
										NE::Renderer::Command::AssignModel(entity, uuid);
										ImGui::CloseCurrentPopup();
									}
									});
							}

							ImSearch::EndSearch();
						}
						ImGui::EndPopup();
					}

					// Material field
					char bufMat[256];
					strncpy_s(bufMat, AssetManager::GetInstance().RetrieveFileName(comp.materialUUID).c_str(), sizeof(bufMat));
					ImGui::InputText("Material", bufMat, sizeof(bufMat));

					if (ImGui::BeginDragDropTarget()) {
						if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload("MATERIAL_PATH")) {
							std::string dropped((const char*)p->Data, p->DataSize - 1);
							auto uuid = AssetManager::GetInstance().RetrieveUUID(dropped);
							NE::Renderer::Command::AssignMaterial(entity, uuid);
						}
						ImGui::EndDragDropTarget();
					}

					static const char* ShadowCastModeNames[] = { "Off", "On", "TwoSided", "ShadowsOnly" };
					int currentCastMode = static_cast<int>(comp.shadowCastMode);
					auto& tempR = NE::ECS::Command::GetEntityRenderer(entity);
					if (ImGui::Combo("Shadow Cast Mode", &currentCastMode, ShadowCastModeNames, IM_ARRAYSIZE(ShadowCastModeNames))) {
						tempR.shadowCastMode = static_cast<NE::ECS::Component::Renderer::ShadowCastMode>(currentCastMode);

						NE::MarkSceneDirty();
					}

					if (Editor::DrawCheckbox("Receive Shadows", tempR.receiveShadows)) {
						NE::MarkSceneDirty();
					}
				}
				else if (typeIdx == typeid(NE::ECS::Component::Light))
				{
					auto& comp = NE::ECS::Query::GetEntityLight(entity);
					ImGui::SeparatorText("Light");

					static const char* LightTypeNames[] = { "Directional", "Point", "Spot" };
					int currentType = static_cast<int>(comp.type);
					if (ImGui::Combo("Type", &currentType, LightTypeNames, IM_ARRAYSIZE(LightTypeNames))) {
						auto& tempLight = NE::ECS::Command::GetEntityLight(entity);
						tempLight.type = static_cast<NE::ECS::Component::Light::Type>(currentType);

						// Mark scene dirty when light type changes
						NE::MarkSceneDirty();
						SPD_DEBUG("[DirtyFlag] Light type changed - Scene marked DIRTY");
					}

					static const char* shadowTypeNames[] = { "None", "Hard", "Soft" };
					int shadowType = static_cast<int>(comp.shadowType);
					if (ImGui::Combo("Shadow Type", &shadowType, shadowTypeNames, IM_ARRAYSIZE(shadowTypeNames))) {
						auto& tempLight = NE::ECS::Command::GetEntityLight(entity);
						tempLight.shadowType = static_cast<NE::ECS::Component::Light::ShadowType>(shadowType);

						// Mark scene dirty when light type changes
						NE::MarkSceneDirty();
						SPD_DEBUG("[DirtyFlag] Light type changed - Scene marked DIRTY");
					}

					static const char* shadowUpdateModeNames[] = { "NoneUpdate", "Realtime", "StaticBake" };
					int shadowUpdateMode = static_cast<int>(comp.shadowUpdateMode);
					if (ImGui::Combo("Shadow Update Mode", &shadowUpdateMode, shadowUpdateModeNames, IM_ARRAYSIZE(shadowUpdateModeNames))) {
						auto& tempLight = NE::ECS::Command::GetEntityLight(entity);
						tempLight.shadowUpdateMode = static_cast<NE::ECS::Component::Light::ShadowUpdateMode>(shadowUpdateMode);

						// Mark scene dirty when light type changes
						NE::MarkSceneDirty();
						SPD_DEBUG("[DirtyFlag] Light type changed - Scene marked DIRTY");
					}

					NE::Core::ForEachFieldView<NE::ECS::Component::Light>(comp,
						[&](auto const& desc, auto const& currentValue) {
							using FieldT = std::decay_t<decltype(currentValue)>;

							FieldT edited = currentValue;

							if (DrawField(desc, edited)) {
								SubmitSetFieldCommand<NE::ECS::Component::Light, FieldT>(
									entity, desc, currentValue, edited
								);
							}
						});
				}
				else if (typeIdx == typeid(NE::ECS::Component::Collider))
				{
					auto& comp = NE::ECS::Command::GetEntityCollider(entity);
					ImGui::SeparatorText("Collider");

					// Dropdown shapes
					static const char* ShapeTypeNames[] = { "Box", "Sphere", "Capsule", "Mesh", "None" };
					int currShape = static_cast<int>(comp.shapeType);
					if (ImGui::Combo("Shape Type", &currShape, ShapeTypeNames, IM_ARRAYSIZE(ShapeTypeNames)))
					{
						auto newShapeType = static_cast<NE::ECS::Component::Collider::ShapeType>(currShape);

						// Create a field descriptor for shapeType
						using ColliderType = NE::ECS::Component::Collider;
						NE::Core::FieldDescriptor<ColliderType, ColliderType::ShapeType> shapeDesc{
							"Shape Type", &ColliderType::shapeType
						};

						// Submit command
						SubmitSetFieldCommand<ColliderType, ColliderType::ShapeType>(
							entity, shapeDesc, comp.shapeType, newShapeType
						);

						// Also mark the collider as dirty
						comp.isShapeDirty = true;

						// NOTE: SubmitSetFieldCommand already marks scene dirty via SetFieldCommand
					}

					// Collider fields - shape properties
					NE::Core::ForEachFieldView<NE::ECS::Component::Collider>(comp,
						[&](auto const& desc, auto const& currentValue) {
							using FieldT = std::decay_t<decltype(currentValue)>;
							using ColliderType = NE::ECS::Component::Collider;

							FieldT edited = currentValue;

							if (DrawField(desc, edited))
							{
								SubmitSetFieldCommand<ColliderType, FieldT>(
									entity, desc, currentValue, edited);

								// Mark properties as dirty when any collider field changes
								comp.isPropertiesDirty = true;
							}
						});

				}
				else if (typeIdx == typeid(NE::ECS::Component::Rigidbody))
				{
					auto& comp = NE::ECS::Command::GetEntityRigidbody(entity);
					ImGui::SeparatorText("Rigidbody");

					// Motion Type dropdown (primary control)
					static const char* MotionTypeNames[] = { "Static", "Kinematic", "Dynamic" };
					int currentMotionType = static_cast<int>(comp.motionType);
					if (ImGui::Combo("Motion Type", &currentMotionType, MotionTypeNames, IM_ARRAYSIZE(MotionTypeNames))) {
						auto& tempRb = NE::ECS::Command::GetEntityRigidbody(entity);

						// Log the motion type change
						SPD_DEBUG("Motion Type changed for entity {}: {} -> {}",
							entity,
							MotionTypeNames[static_cast<int>(comp.motionType)],
							MotionTypeNames[currentMotionType]);

						tempRb.motionType = static_cast<uint8_t>(currentMotionType);

						// Update isStatic to stay in sync
						tempRb.isStatic = (currentMotionType == 0);

						SPD_DEBUG("  Updated: motionType={}, isStatic={}",
							static_cast<int>(tempRb.motionType),
							tempRb.isStatic);

						// Mark scene dirty when motion type changes
						NE::MarkSceneDirty();
						SPD_DEBUG("[DirtyFlag] Rigidbody motion type changed - Scene marked DIRTY");
					}

					// Help text
					ImGui::TextDisabled("Static: Ground/Walls (Layer 0)");
					ImGui::TextDisabled("Dynamic: Player/Physics Objects (Layer 1)");
					ImGui::TextDisabled("Kinematic: Moving Platforms (Layer 1)");
					ImGui::Spacing();

					// Other Rigidbody fields (mass, useGravity, etc.)
					  // Note: isStatic is NOT shown here - Motion Type controls it
					NE::Core::ForEachFieldView<NE::ECS::Component::Rigidbody>(comp,
						[&](auto const& desc, auto const& currentValue) {
							using FieldT = std::decay_t<decltype(currentValue)>;

							// Skip isStatic field (controlled by Motion Type dropdown)
							if (std::string(desc.name) == "isStatic") {
								return;
							}

							FieldT edited = currentValue;

							if (DrawField(desc, edited)) {
								SubmitSetFieldCommand<NE::ECS::Component::Rigidbody, FieldT>(
									entity, desc, currentValue, edited
								);
							}
						});

					if (ImGui::TreeNode("Constraints")) {
						bool changedX = Editor::DrawCheckbox("X", comp.constrainX);
						bool changedY = Editor::DrawCheckbox("Y", comp.constrainY);
						bool changedZ = Editor::DrawCheckbox("Z", comp.constrainZ);

						if (changedX || changedY || changedZ) {
							NE::Physics::Command::LockConstraints(
								entity,
								comp.constrainX,
								comp.constrainY,
								comp.constrainZ
							);
						}

						ImGui::TreePop();
					}

				}
				else if (typeIdx == typeid(NE::ECS::Component::AudioSource))
				{
					auto& comp = NE::ECS::Query::GetEntityAudioSource(entity);
					ImGui::SeparatorText("AudioSource");

					bool openPopup = false;
					DrawAssetField("Audio", comp.modelPath.string(), "+", 0.f, &openPopup);
					if (openPopup) {
						ImGui::OpenPopup("AudioPicker_Model");
					}

					//static std::string searchQuery;
					//if (ImGui::BeginPopup("AudioPicker_Model")) {
					//	ImGui::Text("Select Audio");
					//	ImGui::Separator();
					//	auto& assets = NE::GetAllModels();

					//	if (ImSearch::BeginSearch()) {
					//		ImSearch::SearchBar();

					//		// warning entity in capture clause not used -RF
					//		for (const auto& [name, asset] : assets) {
					//			ImSearch::SearchableItem(name.c_str(),
					//				[name/*, &entity*/](const char*) {
					//					if (ImGui::Selectable(name.c_str())) {
					//						//NE::Renderer::Command::AssignModel(entity, name); // need to add undo redo
					//						printf("Audio Adding Works?");
					//						ImGui::CloseCurrentPopup();
					//					}
					//				});
					//		}

					//		ImSearch::EndSearch();
					//	}
					//	ImGui::EndPopup();
					//}

					// This renders all the external properties of AudioSource but cant edit atm
					//NE::Core::ForEachFieldView<NE::ECS::Component::AudioSource>(comp,
					//    [&](auto const& desc, auto const& currentValue) {
					//        using FieldT = std::decay_t<decltype(currentValue)>;

					//        // make a local editable copy
					//        FieldT edited = currentValue;

					//        // render widget; returns true if user changed it
					//        if (DrawField(desc, edited)) {
					//            // don't write to comp.* here; push a command to the engine:
					//            //SubmitSetFieldCommand(entity, desc, edited);
					//        }
					//    });
				}
				else if (typeIdx == typeid(NE::ECS::Component::NativeScript))
				{
					auto& comp = NE::ECS::Query::GetEntityScript(entity);
					ImGui::SeparatorText("Script");

					// Display current script name or "None"
					std::string currentScript = comp.ScriptName.empty() ? "None" : comp.ScriptName;

					ImGui::Text("Current Script: %s", currentScript.c_str());

					// Script selection dropdown
					if (ImGui::BeginCombo("Script Type", currentScript.c_str())) {
						// "None" option to remove script
						if (ImGui::Selectable("None", comp.ScriptName.empty())) {
							NE::ECS::Command::RemoveEntityScript(entity);
							NE::MarkSceneDirty();
							SPD_DEBUG("[DirtyFlag] Script removed - Scene marked DIRTY");
						}

						// List all registered scripts
						auto scriptNames = NE::ECS::Command::GetRegisteredScriptNames();
						for (const auto& scriptName : scriptNames) {
							bool isSelected = (comp.ScriptName == scriptName);
							if (ImGui::Selectable(scriptName.c_str(), isSelected)) {
								NE::ECS::Command::SetEntityScript(entity, scriptName);
								NE::MarkSceneDirty();
								SPD_DEBUG("[DirtyFlag] Script changed - Scene marked DIRTY");
							}
							if (isSelected) {
								ImGui::SetItemDefaultFocus();
							}
						}
						ImGui::EndCombo();
					}

					// Display script status
					if (!comp.ScriptName.empty()) {
						ImGui::Separator();

						// Check if script is registered in the DLL
						bool isRegistered = NE::ECS::Command::IsScriptRegistered(comp.ScriptName);

						// Check if script instance exists before accessing it
						if (!comp.Instance) {
							// More detailed error message based on registration status
							if (!isRegistered) {
								ImGui::TextColored(ImVec4(1.0f, 0.2f, 0.2f, 1.0f), "Status: Script not found in DLL");
								ImGui::TextWrapped("The script '%s' no longer exists in the compiled game code.", comp.ScriptName.c_str());
								ImGui::TextWrapped("It may have been deleted or renamed.");

								// Offer to remove the invalid script
								ImGui::Spacing();
								if (ImGui::Button("Remove Invalid Script")) {
									NE::ECS::Command::RemoveEntityScript(entity);
									NE::MarkSceneDirty();
									SPD_DEBUG("[DirtyFlag] Invalid script removed - Scene marked DIRTY");
								}
								ImGui::SameLine();
								ImGui::TextDisabled("(?)");
								if (ImGui::IsItemHovered()) {
									ImGui::SetTooltip("This will clear the script component and put it in 'No Script' state");
								}
							}
							else {
								ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "Status: Script not instantiated");
								ImGui::TextWrapped("The script instance failed to initialize. Try reloading the scene or removing and re-adding the script.");
							}
						}
						else {
							// Script enabled/disabled checkbox
							bool enabled = comp.Instance->IsEnabled();
							if (ImGui::Checkbox("Enabled", &enabled)) {
								comp.Instance->SetEnabled(enabled);
								NE::MarkSceneDirty();
								SPD_DEBUG("[DirtyFlag] Script enabled/disabled - Scene marked DIRTY");
							}

							ImGui::Text("Status: Active");
							ImGui::Text("Entity ID: %u", comp.Instance->GetEntity());

							// --- Scripting Fields UI ---
							auto fieldNames = comp.Instance->GetExposedFieldNames();
						if (!fieldNames.empty()) {
							ImGui::SeparatorText("Script Fields");

							// ? NEW: Group struct fields under collapsible headers
							std::unordered_map<std::string, std::vector<std::string>> structGroups;
							std::vector<std::string> normalFields;

							// Separate struct fields (contain '.') from normal fields
							for (const auto& fname : fieldNames) {
								if (fname.find('.') != std::string::npos) {
									// Extract struct name (e.g., "stats" from "stats.health")
									size_t dotPos = fname.find('.');
									std::string structName = fname.substr(0, dotPos);
									structGroups[structName].push_back(fname);
								}
								else {
									normalFields.push_back(fname);
								}
							}

							// Render normal fields first
							for (const auto& fname : normalFields) {
								std::string ftype = comp.Instance->GetFieldType(fname);
								std::string fval = comp.Instance->GetFieldValueAsString(fname);

								ImGui::PushID(fname.c_str());

								bool fieldChanged = false;

								if (ftype == "bool") {
									bool v = (fval == "1" || fval == "true");
									if (ImGui::Checkbox(fname.c_str(), &v)) {
										comp.Instance->SetFieldValueFromString(fname, v ? "1" : "0");
										fieldChanged = true;
									}
								}
								else if (ftype == "int") {
									int v = 0; if (!fval.empty()) v = std::stoi(fval);
									if (ImGui::DragInt(fname.c_str(), &v)) {
										comp.Instance->SetFieldValueFromString(fname, std::to_string(v));
										fieldChanged = true;
									}
								}
								else if (ftype == "float") {
									float v = 0.f; if (!fval.empty()) v = std::stof(fval);
									if (ImGui::DragFloat(fname.c_str(), &v, 0.01f)) {
										comp.Instance->SetFieldValueFromString(fname, std::to_string(v));
										fieldChanged = true;
									}
								}
								else if (ftype == "vec3") {
									NE::Math::Vec3 vv = Vec3FromString(fval);
									if (Editor::DrawVec3Control(fname.c_str(), vv, 0.0f, 100.0f)) {
										comp.Instance->SetFieldValueFromString(fname, Vec3ToString(vv));
										fieldChanged = true;
									}
								}
								else if (ftype == "enum") {
									// Enum dropdown support
									auto enumOptions = comp.Instance->GetEnumOptions(fname);
									if (!enumOptions.empty()) {
										int currentValue = 0;
										if (!fval.empty()) {
											try {
												currentValue = std::stoi(fval);
											}
											catch (...) {
												currentValue = 0;
											}
										}

										// Clamp to valid range
										if (currentValue < 0 || currentValue >= static_cast<int>(enumOptions.size())) {
											currentValue = 0;
										}

										if (ImGui::BeginCombo(fname.c_str(), enumOptions[currentValue].c_str())) {
											for (int i = 0; i < static_cast<int>(enumOptions.size()); ++i) {
												bool isSelected = (currentValue == i);
												if (ImGui::Selectable(enumOptions[i].c_str(), isSelected)) {
													comp.Instance->SetFieldValueFromString(fname, std::to_string(i));
													fieldChanged = true;
												}
												if (isSelected) {
													ImGui::SetItemDefaultFocus();
												}
											}
											ImGui::EndCombo();
										}
									}
									else {
										// Fallback if no enum options provided
										ImGui::Text("%s: %s (enum - no options)", fname.c_str(), fval.c_str());
									}
								}
								else if (ftype.starts_with("componentref:")) {
									// Component reference field
									// Extract component type (e.g., "Transform" from "componentref:Transform")
									std::string componentType = ftype.substr(13); // Skip "componentref:"

									// Get current pointer value and try to find the entity name
									std::string displayName = "None";
									uint32_t assignedEntityId = NE::ECS::NO_ENTITY;
									std::string noEntityStr = std::to_string(NE::ECS::NO_ENTITY);
									if (!fval.empty() && fval != noEntityStr) {
										try {
											// Now fval is entity ID, not a pointer!
											assignedEntityId = static_cast<uint32_t>(std::stoul(fval));

											// Verify entity still exists and has the component
											NE::ECS::Signature entitySig(NE::ECS::Query::GetEntitySignature(assignedEntityId));
											bool isValid = false;

											if (componentType == "Transform") {
												isValid = entitySig.test(NE::ECS::Query::GetRegisteredComponentTypes()[typeid(NE::ECS::Component::Transform)]);
											}
											else if (componentType == "Rigidbody") {
												isValid = entitySig.test(NE::ECS::Query::GetRegisteredComponentTypes()[typeid(NE::ECS::Component::Rigidbody)]);
											}
											else if (componentType == "AudioSource") {
												isValid = entitySig.test(NE::ECS::Query::GetRegisteredComponentTypes()[typeid(NE::ECS::Component::AudioSource)]);
											}

											if (isValid) {
												const auto& entityMeta = NE::ECS::Query::GetEntityMeta(assignedEntityId);
												displayName = entityMeta.name.empty() ? "Entity" : entityMeta.name;
											}
											else {
												displayName = "[Invalid Reference]";
												assignedEntityId = NE::ECS::NO_ENTITY;
											}
										}
										catch (...) {
											displayName = "[Error]";
										}
									}

									// Display the component reference field
									ImGui::Text("%s (%s)", fname.c_str(), componentType.c_str());

									ImGui::PushID((fname + "_compref").c_str());

									// Button shows entity name or status
									if (ImGui::Button(displayName.c_str(), ImVec2(200, 0))) {
										// Future: could select the referenced entity in hierarchy
										if (assignedEntityId != NE::ECS::NO_ENTITY) {
											SPD_DEBUG("Referenced entity: {} (ID: {})", displayName, assignedEntityId);
										}
									}

									// Drag-drop support - accept entity drops
									if (ImGui::BeginDragDropTarget()) {
										// Try to accept HIER_DRAG_ID (from Hierarchy panel)
										const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("HIER_DRAG_ID");
										if (payload && payload->DataSize == sizeof(uint32_t)) {
											uint32_t droppedEntity = *(const uint32_t*)payload->Data;

											// Verify entity has the required component
											bool hasComponent = false;
											NE::ECS::Signature entitySig(NE::ECS::Query::GetEntitySignature(droppedEntity));

											if (componentType == "Transform") {
												hasComponent = entitySig.test(NE::ECS::Query::GetRegisteredComponentTypes()[typeid(NE::ECS::Component::Transform)]);
											}
											else if (componentType == "Rigidbody") {
												hasComponent = entitySig.test(NE::ECS::Query::GetRegisteredComponentTypes()[typeid(NE::ECS::Component::Rigidbody)]);
											}
											else if (componentType == "AudioSource") {
												hasComponent = entitySig.test(NE::ECS::Query::GetRegisteredComponentTypes()[typeid(NE::ECS::Component::AudioSource)]);
											}

											if (hasComponent) {
												const auto& entityMeta = NE::ECS::Query::GetEntityMeta(droppedEntity);
												std::string entityName = entityMeta.name.empty() ? "Entity" : entityMeta.name;

												// Store entity ID (not pointer!)
												bool success = comp.Instance->SetFieldValueFromString(fname, std::to_string(droppedEntity));

												if (success) {
													comp.Instance->_RefreshComponentReferences();
													fieldChanged = true;
												}

											}
										}
										ImGui::EndDragDropTarget();
									}

									// Clear button
									ImGui::SameLine();
									if (ImGui::Button("X")) {
										comp.Instance->SetFieldValueFromString(fname, noEntityStr);
										comp.Instance->_RefreshComponentReferences(); // Clear the pointer too
										fieldChanged = true;
									}

									ImGui::PopID();
								}
								else if (ftype == "materialref") {
									// Material reference field
									// Get current material UUID
									std::string materialUUID = fval;
									std::string displayName = materialUUID.empty() ? "None" : AssetManager::GetInstance().RetrieveFileName(materialUUID);

									// Display the material reference field
									ImGui::Text("%s (Material)", fname.c_str());

									ImGui::PushID((fname + "_matref").c_str());

									// Button shows material name or "None" - make it a drop target
									ImGui::Button(displayName.c_str(), ImVec2(200, 0));

									// Drag-drop support - accept material drops from asset browser
									// NOTE: Must be called right after the button, while it's still the active item
									if (ImGui::BeginDragDropTarget()) {
										const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("MATERIAL_PATH");
										if (payload && payload->DataSize > 0) {
											std::string droppedPath((const char*)payload->Data, payload->DataSize - 1);
											SPD_DEBUG("[MaterialRef] Dropped path: {}", droppedPath);

											std::string droppedUUID = AssetManager::GetInstance().RetrieveUUID(droppedPath);
											SPD_DEBUG("[MaterialRef] Retrieved UUID: {}", droppedUUID);

											if (!droppedUUID.empty()) {
												SPD_DEBUG("[MaterialRef] Calling SetFieldValueFromString for field '{}' with UUID '{}'", fname, droppedUUID);
												bool success = comp.Instance->SetFieldValueFromString(fname, droppedUUID);
												SPD_DEBUG("[MaterialRef] SetFieldValueFromString returned: {}", success);
												if (success) {
													fieldChanged = true;
													SPD_DEBUG("[MaterialRef] Material {} assigned to field {}", droppedUUID, fname);
												}
												else {
													SPD_ERROR("[MaterialRef] Failed to assign material {} to field {}", droppedUUID, fname);
												}
											}
											else {
												SPD_ERROR("[MaterialRef] Empty UUID retrieved from path: {}", droppedPath);
											}
										}
										else {
											if (payload) {
												SPD_DEBUG("[MaterialRef] Payload received but DataSize is: {}", payload->DataSize);
											}
											else {
												SPD_DEBUG("[MaterialRef] No MATERIAL_PATH payload accepted");
											}
										}
										ImGui::EndDragDropTarget();
									}

									// Clear button
									ImGui::SameLine();
									if (ImGui::Button("X")) {
										comp.Instance->SetFieldValueFromString(fname, "");
										fieldChanged = true;
									}

									ImGui::PopID();
								}
								else if (ftype == "prefabref") {
									// Prefab reference field
									// Get current prefab UUID
									std::string prefabName = fval;
									//std::string displayName = prefabUUID.empty() ? "None" : AssetManager::GetInstance().RetrieveFileName(prefabUUID);

									// Display the prefab reference field
									ImGui::Text("%s (Prefab)", fname.c_str());

									ImGui::PushID((fname + "_prefabref").c_str());

									// Button shows prefab name or "None" - make it a drop target
									ImGui::Button(prefabName.c_str(), ImVec2(200, 0));

									// Drag-drop support - accept prefab drops from asset browser
									// NOTE: Must be called right after the button, while it's still the active item
									if (ImGui::BeginDragDropTarget()) {
										const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("PREFAB_ASSET_PATH");
										if (payload && payload->DataSize > 0) {
											std::string droppedPath((const char*)payload->Data, payload->DataSize - 1);
											SPD_DEBUG("[PrefabRef] Dropped path: {}", droppedPath);

											/*std::string droppedUUID = AssetManager::GetInstance().RetrieveUUID(droppedPath);
											SPD_DEBUG("[PrefabRef] Retrieved UUID: {}", droppedUUID);*/

											bool success = comp.Instance->SetFieldValueFromString(fname, droppedPath);
											SPD_DEBUG("[PrefabRef] SetFieldValueFromString returned: {}", success);
											if (success) {
												fieldChanged = true;
												SPD_DEBUG("[PrefabRef] Prefab " << droppedPath << " assigned to " << fname);
											}
											else {
												SPD_ERROR("[PrefabRef] Fail to get Prefab " << droppedPath << " assigned to " << fname);
											}

											/*if (!droppedUUID.empty()) {
												SPD_DEBUG("[PrefabRef] Calling SetFieldValueFromString for field '{}' with UUID '{}'", fname, droppedUUID);
												bool success = comp.Instance->SetFieldValueFromString(fname, droppedPath);
												SPD_DEBUG("[PrefabRef] SetFieldValueFromString returned: {}", success);
												if (success) {
													fieldChanged = true;
													SPD_DEBUG("[PrefabRef] Prefab {} assigned to field {}", droppedUUID, fname);
												} else {
													SPD_ERROR("[PrefabRef] Failed to assign prefab {} to field {}", droppedUUID, fname);
												}
											} else {
												SPD_ERROR("[PrefabRef] Empty UUID retrieved from path: {}", droppedPath);
											}*/
										}
										else {
											if (payload) {
												SPD_DEBUG("[PrefabRef] Payload received but DataSize is: {}", payload->DataSize);
											}
											else {
												SPD_DEBUG("[PrefabRef] No PREFAB_ASSET_PATH payload accepted");
											}
										}
										ImGui::EndDragDropTarget();
									}

									// Clear button
									ImGui::SameLine();
									if (ImGui::Button("X")) {
										comp.Instance->SetFieldValueFromString(fname, "");
										fieldChanged = true;
									}

									ImGui::PopID();
								}
								else if (ftype.starts_with("vector<")) {
									// Array/Vector support (int, float, bool, string)
									// NOTE: Nested struct vectors not yet supported - will be added in future commit
									size_t arraySize = comp.Instance->GetArraySize(fname);

									if (ImGui::TreeNode(fname.c_str(), "%s [%zu]", fname.c_str(), arraySize)) {
										// Add element button
										if (ImGui::Button("+##add")) {
											comp.Instance->AddArrayElement(fname);
											fieldChanged = true;
										}
										ImGui::SameLine();
										ImGui::Text("Add Element");

										// Display each element
										for (size_t i = 0; i < arraySize; ++i) {
											ImGui::PushID(static_cast<int>(i));

											std::string elemValue = comp.Instance->GetArrayElement(fname, i);

											// Determine element type from vector<T>
											std::string elementType = ftype.substr(7, ftype.length() - 8); // Extract T from "vector<T>"

											ImGui::Text("[%zu]", i);
											ImGui::SameLine();

											bool elemChanged = false;
											if (elementType == "int") {
												int val = elemValue.empty() ? 0 : std::stoi(elemValue);
												if (ImGui::DragInt("##elem", &val)) {
													comp.Instance->SetArrayElement(fname, i, std::to_string(val));
													elemChanged = true;
												}
											}
											else if (elementType == "float") {
												float val = elemValue.empty() ? 0.0f : std::stof(elemValue);
												if (ImGui::DragFloat("##elem", &val, 0.01f)) {
													comp.Instance->SetArrayElement(fname, i, std::to_string(val));
													elemChanged = true;
												}
											}
											else if (elementType == "bool") {
												bool val = (elemValue == "1" || elemValue == "true");
												if (ImGui::Checkbox("##elem", &val)) {
													comp.Instance->SetArrayElement(fname, i, val ? "1" : "0");
													elemChanged = true;

													// Verification removed for release
													//std::string verifyValue = comp.Instance->GetArrayElement(fname, i);
													//SPD_DEBUG(" Verification: flags[" << i << "] is now '" << verifyValue << "'");
												}
											}
											else if (elementType == "string") {
												// String support for vector<string>
												char buf[256];
												strncpy_s(buf, elemValue.c_str(), sizeof(buf));
												buf[sizeof(buf) - 1] = '\0';
												if (ImGui::InputText("##elem", buf, sizeof(buf))) {
													comp.Instance->SetArrayElement(fname, i, std::string(buf));
													elemChanged = true;
												}
											}
											else if (elementType == "materialref") {
												// Material reference support for vector<materialref>
												std::string materialUUID = elemValue;
												std::string displayName = materialUUID.empty() ? "None" : AssetManager::GetInstance().RetrieveFileName(materialUUID);

												// Button shows material name or "None" - make it a drop target
												ImGui::Button(displayName.c_str(), ImVec2(150, 0));

												// Drag-drop support - accept material drops
												// NOTE: Must be called right after the button, while it's still the active item
												if (ImGui::BeginDragDropTarget()) {
													const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("MATERIAL_PATH");
													if (payload && payload->DataSize > 0) {
														std::string droppedPath((const char*)payload->Data, payload->DataSize - 1);
														SPD_DEBUG("[MaterialRef Vector] Dropped path: {}", droppedPath);

														std::string droppedUUID = AssetManager::GetInstance().RetrieveUUID(droppedPath);
														SPD_DEBUG("[MaterialRef Vector] Retrieved UUID: {}", droppedUUID);

														if (!droppedUUID.empty()) {
															SPD_DEBUG("[MaterialRef Vector] Setting element {} of field '{}' to UUID '{}'", i, fname, droppedUUID);
															bool success = comp.Instance->SetArrayElement(fname, i, droppedUUID);
															SPD_DEBUG("[MaterialRef Vector] SetArrayElement returned: {}", success);
															if (success) {
																elemChanged = true;
																SPD_DEBUG("[MaterialRef Vector] Successfully assigned material to vector element");
															}
															else {
																SPD_ERROR("[MaterialRef Vector] Failed to set array element");
															}
														}
														else {
															SPD_ERROR("[MaterialRef Vector] Empty UUID retrieved from path: {}", droppedPath);
														}
													}
													else {
														if (payload) {
															SPD_DEBUG("[MaterialRef Vector] Payload received but DataSize is: {}", payload->DataSize);
														}
														else {
															SPD_DEBUG("[MaterialRef Vector] No MATERIAL_PATH payload accepted");
														}
													}
													ImGui::EndDragDropTarget();
												}

												// Clear button
												ImGui::SameLine();
												if (ImGui::Button("X##clear")) {
													comp.Instance->SetArrayElement(fname, i, "");
													elemChanged = true;
												}
											}
											else if (elementType == "prefabref") {
												// Prefab reference support for vector<prefabref>
												std::string prefabUUID = elemValue;
												std::string displayName = prefabUUID.empty() ? "None" : AssetManager::GetInstance().RetrieveFileName(prefabUUID);

												// Button shows prefab name or "None" - make it a drop target
												ImGui::Button(displayName.c_str(), ImVec2(150, 0));

												// Drag-drop support - accept prefab drops
												// NOTE: Must be called right after the button, while it's still the active item
												if (ImGui::BeginDragDropTarget()) {
													const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("PREFAB_ASSET_PATH");
													if (payload && payload->DataSize > 0) {
														std::string droppedPath((const char*)payload->Data, payload->DataSize - 1);
														SPD_DEBUG("[PrefabRef Vector] Dropped path: {}", droppedPath);

														std::string droppedUUID = AssetManager::GetInstance().RetrieveUUID(droppedPath);
														SPD_DEBUG("[PrefabRef Vector] Retrieved UUID: {}", droppedUUID);

														if (!droppedUUID.empty()) {
															SPD_DEBUG("[PrefabRef Vector] Setting element {} of field '{}' to UUID '{}'", i, fname, droppedUUID);
															bool success = comp.Instance->SetArrayElement(fname, i, droppedUUID);
															SPD_DEBUG("[PrefabRef Vector] SetArrayElement returned: {}", success);
															if (success) {
																elemChanged = true;
																SPD_DEBUG("[PrefabRef Vector] Successfully assigned prefab to vector element");
															}
															else {
																SPD_ERROR("[PrefabRef Vector] Failed to set array element");
															}
														}
														else {
															SPD_ERROR("[PrefabRef Vector] Empty UUID retrieved from path: {}", droppedPath);
														}
													}
													else {
														if (payload) {
															SPD_DEBUG("[PrefabRef Vector] Payload received but DataSize is: {}", payload->DataSize);
														}
														else {
															SPD_DEBUG("[PrefabRef Vector] No PREFAB_ASSET_PATH payload accepted");
														}
													}
													ImGui::EndDragDropTarget();
												}

												// Clear button
												ImGui::SameLine();
												if (ImGui::Button("X##clear")) {
													comp.Instance->SetArrayElement(fname, i, "");
													elemChanged = true;
												}
											}
											else if (elementType == "entity") {
												// Entity reference support for vector<entity>
												std::string entityIdStr = elemValue;
												std::string displayName = "None";
												uint32_t assignedEntityId = NE::ECS::NO_ENTITY;
												std::string noEntityStr = std::to_string(NE::ECS::NO_ENTITY);

												if (!entityIdStr.empty() && entityIdStr != noEntityStr) {
													try {
														assignedEntityId = static_cast<uint32_t>(std::stoul(entityIdStr));

														// Verify entity still exists
														if (assignedEntityId != NE::ECS::NO_ENTITY) {
															const auto& entityMeta = NE::ECS::Query::GetEntityMeta(assignedEntityId);
															displayName = entityMeta.name.empty() ? "Entity" : entityMeta.name;
														}
													}
													catch (...) {
														displayName = "[Error]";
													}
												}

												// Button shows entity name or "None" - make it a drop target
												ImGui::Button(displayName.c_str(), ImVec2(150, 0));

												// Drag-drop support - accept entity drops from hierarchy
												if (ImGui::BeginDragDropTarget()) {
													const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("HIER_DRAG_ID");
													if (payload && payload->DataSize == sizeof(uint32_t)) {
														uint32_t droppedEntity = *(const uint32_t*)payload->Data;
														SPD_DEBUG("[Entity Vector] Dropped entity: {}", droppedEntity);

														bool success = comp.Instance->SetArrayElement(fname, i, std::to_string(droppedEntity));
														if (success) {
															elemChanged = true;
															SPD_DEBUG("[Entity Vector] Successfully assigned entity to vector element");
														}
														else {
															SPD_ERROR("[Entity Vector] Failed to set array element");
														}
													}
													ImGui::EndDragDropTarget();
												}

												// Clear button
												ImGui::SameLine();
												if (ImGui::Button("X##clear")) {
													comp.Instance->SetArrayElement(fname, i, noEntityStr);
													elemChanged = true;
												}
											}
											else {
												// Unknown type fallback - treat as string
												char buf[256];
												strncpy_s(buf, elemValue.c_str(), sizeof(buf));
												buf[sizeof(buf) - 1] = '\0';
												if (ImGui::InputText("##elem", buf, sizeof(buf))) {
													comp.Instance->SetArrayElement(fname, i, std::string(buf));
													elemChanged = true;
												}
											}

											ImGui::SameLine();
											if (ImGui::Button("-##remove")) {
												comp.Instance->RemoveArrayElement(fname, i);
												fieldChanged = true;
											}

											if (elemChanged) {
												fieldChanged = true;
											}

											ImGui::PopID();
										}

										ImGui::TreePop();
									}
								}
								else if (fname.find('.') != std::string::npos) {
									// Struct field (contains dot notation)
									// NOTE: Nested struct serialization not fully supported yet - will be added in future commit
									   // Display as normal field, but with indentation
									ImGui::Indent();

									if (ftype == "int") {
										int v = 0;
										if (!fval.empty()) v = std::stoi(fval);
										if (ImGui::DragInt(fname.c_str(), &v)) {
											comp.Instance->SetFieldValueFromString(fname, std::to_string(v));
											fieldChanged = true;
										}
									}
									else if (ftype == "float") {
										float v = 0.0f;
										if (!fval.empty()) v = std::stof(fval);
										if (ImGui::DragFloat(fname.c_str(), &v, 0.01f)) {
											comp.Instance->SetFieldValueFromString(fname, std::to_string(v));
											fieldChanged = true;
										}
									}
									else if (ftype == "bool") {
										bool v = (fval == "1" || fval == "true");
										if (ImGui::Checkbox(fname.c_str(), &v)) {
											comp.Instance->SetFieldValueFromString(fname, v ? "1" : "0");
											fieldChanged = true;
										}
									}

									ImGui::Unindent();
								}
								else { // treat as string
									char buf[256];
									strncpy_s(buf, fval.c_str(), sizeof(buf));
									if (ImGui::InputText(fname.c_str(), buf, sizeof(buf))) {
										comp.Instance->SetFieldValueFromString(fname, std::string(buf));
										fieldChanged = true;
									}
								}

								// Call OnValidate() when a field changes (editor-only)
								if (fieldChanged) {
									comp.Instance->OnValidate();
									NE::MarkSceneDirty();
									// printf("[DirtyFlag] Script field changed - Scene marked DIRTY\n"); // Too spammy
								}

								ImGui::PopID();
							}

							//  NOW RENDER STRUCT GROUPS
							for (const auto& [structName, fields] : structGroups) {
								if (ImGui::TreeNode(structName.c_str())) {
									for (const auto& fname : fields) {
										std::string ftype = comp.Instance->GetFieldType(fname);
										std::string fval = comp.Instance->GetFieldValueAsString(fname);

										// Extract field name after dot (e.g., "health" from "stats.health")
										size_t dotPos = fname.find('.');
										std::string fieldName = fname.substr(dotPos + 1);

										ImGui::PushID(fname.c_str());
										bool fieldChanged = false;

										if (ftype == "int") {
											int v = 0;
											if (!fval.empty()) v = std::stoi(fval);
											if (ImGui::DragInt(fieldName.c_str(), &v)) {
												comp.Instance->SetFieldValueFromString(fname, std::to_string(v));
												fieldChanged = true;
											}
										}
										else if (ftype == "float") {
											float v = 0.0f;
											if (!fval.empty()) v = std::stof(fval);
											if (ImGui::DragFloat(fieldName.c_str(), &v, 0.01f)) {
												comp.Instance->SetFieldValueFromString(fname, std::to_string(v));
												fieldChanged = true;
											}
										}
										else if (ftype == "bool") {
											bool v = (fval == "1" || fval == "true");
											if (ImGui::Checkbox(fieldName.c_str(), &v)) {
												comp.Instance->SetFieldValueFromString(fname, v ? "1" : "0");
												fieldChanged = true;
											}
										}

										if (fieldChanged) {
											comp.Instance->OnValidate();
										}

										ImGui::PopID();
									}
									ImGui::TreePop();
								}
							}
						}
						} // End of comp.Instance else block
					}
				}
				else if (typeIdx == typeid(NE::ECS::Component::Camera))
				{
					auto& comp = NE::ECS::Query::GetEntityCamera(entity);
					ImGui::SeparatorText("Camera");

					NE::Core::ForEachFieldView<NE::ECS::Component::Camera>(comp,
						[&](auto const& desc, auto const& currentValue) {
							using Owner = NE::ECS::Component::Camera;
							using FieldT = std::decay_t<decltype(currentValue)>;

							FieldT edited = currentValue;

							// --- draw widget, track edit lifecycle ---
							ImGui::PushID(desc.name.data());
							const bool changed = DrawField(desc, edited);  // your field drawer
							const bool activated = ImGui::IsItemActivated();
							const bool active = ImGui::IsItemActive();
							const bool deactivated = ImGui::IsItemDeactivatedAfterEdit();
							ImGui::PopID();

							// Key to coalesce continuous edits (dragging slider, etc.)
							FieldKey key{
								entity,
								&typeid(Owner),
								MemberPointerHasher<Owner, FieldT>{}(desc.member)
							};

							// 1) Begin an active command when editing starts
							if (activated) {
								using Cmd = Editor::SetFieldCommand<Owner, FieldT>;
								auto cmd = std::make_unique<Cmd>(
									entity,
									std::string("Set Camera ") + desc.name.data(),
									desc.member,
									currentValue,  // before
									currentValue,  // after (will change while dragging / on release)
									&NE::ECS::Command::GetEntityCamera
								);
								g_activeCommands[key] = std::move(cmd);
							}

							// 2) While dragging, coalesce into the active command
							if (active && changed) {
								auto it = g_activeCommands.find(key);
								if (it != g_activeCommands.end()) {
									using Cmd = Editor::SetFieldCommand<Owner, FieldT>;
									Cmd tmp(
										entity,
										std::string{},     // no label for interim updates
										desc.member,
										currentValue,      // before (ignored by CoalesceFrom)
										edited,            // new after value
										&NE::ECS::Command::GetEntityCamera
									);
									it->second->CoalesceFrom(tmp);
								}
							}

							// 3) When edit ends, either discard (no net change) or commit
							if (deactivated) {
								auto it = g_activeCommands.find(key);
								if (it != g_activeCommands.end()) {
									// Ensure final 'edited' value is applied at the end,
									// even if no coalescing happened while active (e.g. checkboxes).
									if (changed) {
										using Cmd = Editor::SetFieldCommand<Owner, FieldT>;
										Cmd tmp(
											entity,
											std::string{},    // no label
											desc.member,
											currentValue,     // before (ignored by CoalesceFrom)
											edited,           // final value
											&NE::ECS::Command::GetEntityCamera
										);
										it->second->CoalesceFrom(tmp);
									}

									// If no net change, drop it; else execute & mark camera dirty
									if (auto* asSet =
										dynamic_cast<Editor::SetFieldCommand<Owner, FieldT>*>(it->second.get());
										asSet && Equal(asSet->Before(), asSet->After())) {
										g_activeCommands.erase(it);
									}
									else {
										Editor::CommandHistory::GetInstance().ExecuteCommand(std::move(it->second));
										g_activeCommands.erase(it);

										// Ensure projection is rebuilt after param changes
										auto& cam = NE::ECS::Command::GetEntityCamera(entity);
										cam.isDirty = true;  // projection rebuild flag
									}
								}
							}
						});
				}
				else if (typeIdx == typeid(NE::ECS::Component::Animator)) {
					auto& comp = NE::ECS::Command::GetEntityAnimator(entity);
					ImGui::SeparatorText("Animator");

					NE::Core::ForEachFieldView<NE::ECS::Component::Animator>(comp,
						[&](auto const& desc, auto const& currentValue) {
							using Owner = NE::ECS::Component::Animator;
							using FieldT = std::decay_t<decltype(currentValue)>;

							FieldT edited = currentValue;

							ImGui::PushID(desc.name.data());
							const bool changed = DrawField(desc, edited);
							const bool activated = ImGui::IsItemActivated();
							const bool active = ImGui::IsItemActive();
							const bool deactivated = ImGui::IsItemDeactivatedAfterEdit();
							ImGui::PopID();

							FieldKey key{
								entity,
								&typeid(Owner),
								MemberPointerHasher<Owner, FieldT>{}(desc.member)
							};

							if (activated) {
								using Cmd = Editor::SetFieldCommand<Owner, FieldT>;
								auto cmd = std::make_unique<Cmd>(
									entity,
									std::string("Set Animator ") + desc.name.data(),
									desc.member,
									currentValue,
									currentValue,
									&NE::ECS::Command::GetEntityAnimator
								);
								g_activeCommands[key] = std::move(cmd);
							}

							if (active && changed) {
								auto it = g_activeCommands.find(key);
								if (it != g_activeCommands.end()) {
									using Cmd = Editor::SetFieldCommand<Owner, FieldT>;
									Cmd tmp(
										entity,
										std::string{},
										desc.member,
										currentValue,
										edited,
										&NE::ECS::Command::GetEntityAnimator
									);
									it->second->CoalesceFrom(tmp);
								}
							}

							if (deactivated) {
								auto it = g_activeCommands.find(key);
								if (it != g_activeCommands.end()) {
									auto* asSet = dynamic_cast<Editor::SetFieldCommand<Owner, FieldT>*>(it->second.get());
									if (asSet && Equal(asSet->Before(), asSet->After())) {
										g_activeCommands.erase(it);
									}
									else {
										Editor::CommandHistory::GetInstance()
											.ExecuteCommand(std::move(it->second));
										g_activeCommands.erase(it);
									}
								}
							}
						});
				}
				else if (typeIdx == typeid(NE::ECS::Component::UIRectTransform))
				{
					auto& comp = NE::ECS::Command::GetUIRectTransform(entity);

					if (ImGui::CollapsingHeader("Rect Transform", ImGuiTreeNodeFlags_DefaultOpen))
					{
						ImGui::Indent();

						// Check render mode - walk up hierarchy to find canvas
						bool isOverlay = false;
						{
							if (NE::ECS::Query::HasUICanvas(entity))
							{
								auto& compCanvas = NE::ECS::Command::GetUICanvas(entity);
								isOverlay = (compCanvas.renderMode == NE::ECS::Component::UICanvas::RenderMode::SCREEN_SPACE_OVERLAY);
							}
							else
							{
								uint32_t currentParent = comp.parent;
								while (currentParent != NE::ECS::NO_ENTITY)
								{
									if (NE::ECS::Query::HasUICanvas(currentParent))
									{
										auto& parentCanvas = NE::ECS::Query::GetUICanvas(currentParent);
										isOverlay = (parentCanvas.renderMode == NE::ECS::Component::UICanvas::RenderMode::SCREEN_SPACE_OVERLAY);
										break;
									}
									if (!NE::ECS::Query::HasUIRectTransform(currentParent)) break;
									currentParent = NE::ECS::Query::GetUIRectTransform(currentParent).parent;
								}
							}
						}

						float itemWidth = 70.0f;
						float spacing = 10.0f;

						// Helper macro for UIRectTransform fields
						#define UI_RECT_DRAG(label, fieldPtr, speed, minVal, maxVal, format) \
						do { \
							using Owner = NE::ECS::Component::UIRectTransform; \
							using FieldT = std::decay_t<decltype(comp.*fieldPtr)>; \
							ImGui::PushID(label); \
							FieldT before = comp.*fieldPtr; \
							bool changed = ImGui::DragFloat(label, &(comp.*fieldPtr), speed, minVal, maxVal, format); \
							const bool activated = ImGui::IsItemActivated(); \
							const bool deactivated = ImGui::IsItemDeactivatedAfterEdit(); \
							ImGui::PopID(); \
							FieldKey key{ entity, &typeid(Owner), MemberPointerHasher<Owner, FieldT>{}(fieldPtr) }; \
							if (activated) { \
								using Cmd = Editor::SetFieldCommand<Owner, FieldT>; \
								auto cmd = std::make_unique<Cmd>(entity, "UI Rect: Transform", fieldPtr, before, before, &NE::ECS::Command::GetUIRectTransform); \
								g_activeCommands[key] = std::move(cmd); \
							} \
							if (changed) { \
								NE::MarkSceneDirty(); \
								auto it = g_activeCommands.find(key); \
								if (it != g_activeCommands.end()) { \
									using Cmd = Editor::SetFieldCommand<Owner, FieldT>; \
									Cmd tmp(entity, std::string{}, fieldPtr, before, comp.*fieldPtr, &NE::ECS::Command::GetUIRectTransform); \
									it->second->CoalesceFrom(tmp); \
								} \
							} \
							if (deactivated) { \
								auto it = g_activeCommands.find(key); \
								if (it != g_activeCommands.end()) { \
									auto* asSet = dynamic_cast<Editor::SetFieldCommand<Owner, FieldT>*>(it->second.get()); \
									if (asSet && Equal(asSet->Before(), asSet->After())) { \
										g_activeCommands.erase(it); \
									} else { \
										Editor::CommandHistory::GetInstance().ExecuteCommand(std::move(it->second)); \
										g_activeCommands.erase(it); \
									} \
								} \
							} \
						} while(0)

						#define UI_RECT_SLIDER(label, fieldPtr, minVal, maxVal, format) \
						do { \
							using Owner = NE::ECS::Component::UIRectTransform; \
							using FieldT = std::decay_t<decltype(comp.*fieldPtr)>; \
							ImGui::PushID(label); \
							FieldT before = comp.*fieldPtr; \
							bool changed = ImGui::SliderFloat(label, &(comp.*fieldPtr), minVal, maxVal, format); \
							const bool activated = ImGui::IsItemActivated(); \
							const bool deactivated = ImGui::IsItemDeactivatedAfterEdit(); \
							ImGui::PopID(); \
							FieldKey key{ entity, &typeid(Owner), MemberPointerHasher<Owner, FieldT>{}(fieldPtr) }; \
							if (activated) { \
								using Cmd = Editor::SetFieldCommand<Owner, FieldT>; \
								auto cmd = std::make_unique<Cmd>(entity, "UI Rect: Transform", fieldPtr, before, before, &NE::ECS::Command::GetUIRectTransform); \
								g_activeCommands[key] = std::move(cmd); \
							} \
							if (changed) { \
								NE::MarkSceneDirty(); \
								auto it = g_activeCommands.find(key); \
								if (it != g_activeCommands.end()) { \
									using Cmd = Editor::SetFieldCommand<Owner, FieldT>; \
									Cmd tmp(entity, std::string{}, fieldPtr, before, comp.*fieldPtr, &NE::ECS::Command::GetUIRectTransform); \
									it->second->CoalesceFrom(tmp); \
								} \
							} \
							if (deactivated) { \
								auto it = g_activeCommands.find(key); \
								if (it != g_activeCommands.end()) { \
									auto* asSet = dynamic_cast<Editor::SetFieldCommand<Owner, FieldT>*>(it->second.get()); \
									if (asSet && Equal(asSet->Before(), asSet->After())) { \
										g_activeCommands.erase(it); \
									} else { \
										Editor::CommandHistory::GetInstance().ExecuteCommand(std::move(it->second)); \
										g_activeCommands.erase(it); \
									} \
								} \
							} \
						} while(0)		

						// Position section
						{
							ImGui::AlignTextToFramePadding();
							ImGui::Text("Position");
							ImGui::SameLine(100);

							ImGui::BeginGroup();
							ImGui::TextDisabled("Pos X");
							ImGui::SetNextItemWidth(itemWidth);
							UI_RECT_DRAG("##PosX", &NE::ECS::Component::UIRectTransform::x, 0.1f, 0.0f, 0.0f, "%.1f");
							ImGui::EndGroup();

							ImGui::SameLine(0, spacing);

							ImGui::BeginGroup();
							ImGui::TextDisabled("Pos Y");
							ImGui::SetNextItemWidth(itemWidth);
							UI_RECT_DRAG("##PosY", &NE::ECS::Component::UIRectTransform::y, 0.1f, 0.0f, 0.0f, "%.1f");
							ImGui::EndGroup();

							if (!isOverlay)
							{
								ImGui::SameLine(0, spacing);

								ImGui::BeginGroup();
								ImGui::TextDisabled("Pos Z");
								ImGui::SetNextItemWidth(itemWidth);
								UI_RECT_DRAG("##PosZ", &NE::ECS::Component::UIRectTransform::z, 0.1f, 0.0f, 0.0f, "%.1f");
								ImGui::EndGroup();
							}
						}

						ImGui::Spacing();

						// Size/Offset section - changes based on anchor mode
						{
							bool isStretchedX = (comp.anchorMinX != comp.anchorMaxX);
							bool isStretchedY = (comp.anchorMinY != comp.anchorMaxY);

							if (isStretchedX || isStretchedY) {
								// Stretched mode: Show Left/Right/Top/Bottom offsets (like Unity)
								ImGui::AlignTextToFramePadding();
								ImGui::Text("Offsets");
								ImGui::SameLine(100);

								if (isStretchedX && isStretchedY) {
									// Stretch Both: Arrange in columns (Left/Top, Right/Bottom)
									// First row: Left and Right
									float leftColumnX = ImGui::GetCursorPosX();
									
									ImGui::BeginGroup();
									ImGui::TextDisabled("Left");
									ImGui::SetNextItemWidth(itemWidth);
									UI_RECT_DRAG("##OffsetLeft", &NE::ECS::Component::UIRectTransform::offsetMinX, 1.0f, 0.0f, 0.0f, "%.0f");
									ImGui::EndGroup();

									ImGui::SameLine(0, spacing);

									ImGui::BeginGroup();
									ImGui::TextDisabled("Right");
									ImGui::SetNextItemWidth(itemWidth);
									UI_RECT_DRAG("##OffsetRight", &NE::ECS::Component::UIRectTransform::offsetMaxX, 1.0f, 0.0f, 0.0f, "%.0f");
									ImGui::EndGroup();

									// Second row: Top and Bottom, aligned with Left and Right columns
									ImGui::SetCursorPosX(leftColumnX);
									ImGui::BeginGroup();
									ImGui::TextDisabled("Top");
									ImGui::SetNextItemWidth(itemWidth);
									UI_RECT_DRAG("##OffsetTop", &NE::ECS::Component::UIRectTransform::offsetMaxY, 1.0f, 0.0f, 0.0f, "%.0f");
									ImGui::EndGroup();

									ImGui::SameLine(0, spacing);

									ImGui::BeginGroup();
									ImGui::TextDisabled("Bottom");
									ImGui::SetNextItemWidth(itemWidth);
									UI_RECT_DRAG("##OffsetBottom", &NE::ECS::Component::UIRectTransform::offsetMinY, 1.0f, 0.0f, 0.0f, "%.0f");
									ImGui::EndGroup();
								}
								else {
									// Single-axis stretch: Show non-stretched dimension first, then offsets
									// First row: Show non-stretched dimension (Width or Height)
									if (!isStretchedX) {
										// Point anchor X: Show Width
										ImGui::BeginGroup();
										ImGui::TextDisabled("Width");
										ImGui::SetNextItemWidth(itemWidth);
										UI_RECT_DRAG("##Width", &NE::ECS::Component::UIRectTransform::width, 1.0f, 1.0f, 10000.0f, "%.0f");
										ImGui::EndGroup();
									}
									else if (!isStretchedY) {
										// Point anchor Y: Show Height (for Stretch Horizontal)
										ImGui::BeginGroup();
										ImGui::TextDisabled("Height");
										ImGui::SetNextItemWidth(itemWidth);
										UI_RECT_DRAG("##Height", &NE::ECS::Component::UIRectTransform::height, 1.0f, 1.0f, 10000.0f, "%.0f");
										ImGui::EndGroup();
									}

									// Second row: Show stretched dimension offsets
									if (isStretchedX) {
										// Horizontal stretch: Show Left and Right
										if (!isStretchedY) {
											// If only horizontal stretch, show on same line after Height
											ImGui::SameLine(0, spacing);
										}

										ImGui::BeginGroup();
										ImGui::TextDisabled("Left");
										ImGui::SetNextItemWidth(itemWidth);
										UI_RECT_DRAG("##OffsetLeft", &NE::ECS::Component::UIRectTransform::offsetMinX, 1.0f, 0.0f, 0.0f, "%.0f");
										ImGui::EndGroup();

										ImGui::SameLine(0, spacing);

										ImGui::BeginGroup();
										ImGui::TextDisabled("Right");
										ImGui::SetNextItemWidth(itemWidth);
										UI_RECT_DRAG("##OffsetRight", &NE::ECS::Component::UIRectTransform::offsetMaxX, 1.0f, 0.0f, 0.0f, "%.0f");
										ImGui::EndGroup();
									}

									if (isStretchedY) {
										// Vertical stretch: Show Top and Bottom
										if (!isStretchedX) {
											// If only vertical stretch, show on same line after Width
											ImGui::SameLine(0, spacing);
										}

										ImGui::BeginGroup();
										ImGui::TextDisabled("Top");
										ImGui::SetNextItemWidth(itemWidth);
										UI_RECT_DRAG("##OffsetTop", &NE::ECS::Component::UIRectTransform::offsetMaxY, 1.0f, 0.0f, 0.0f, "%.0f");
										ImGui::EndGroup();

										ImGui::SameLine(0, spacing);

										ImGui::BeginGroup();
										ImGui::TextDisabled("Bottom");
										ImGui::SetNextItemWidth(itemWidth);
										UI_RECT_DRAG("##OffsetBottom", &NE::ECS::Component::UIRectTransform::offsetMinY, 1.0f, 0.0f, 0.0f, "%.0f");
										ImGui::EndGroup();
									}
								}
							}
							else {
								// Point anchor mode: Show Width and Height (normal mode)
								ImGui::AlignTextToFramePadding();
								ImGui::Text("Size");
								ImGui::SameLine(100);

								ImGui::BeginGroup();
								ImGui::TextDisabled("Width");
								ImGui::SetNextItemWidth(itemWidth);
								UI_RECT_DRAG("##Width", &NE::ECS::Component::UIRectTransform::width, 1.0f, 1.0f, 10000.0f, "%.0f");
								ImGui::EndGroup();

								ImGui::SameLine(0, spacing);

								ImGui::BeginGroup();
								ImGui::TextDisabled("Height");
								ImGui::SetNextItemWidth(itemWidth);
								UI_RECT_DRAG("##Height", &NE::ECS::Component::UIRectTransform::height, 1.0f, 1.0f, 10000.0f, "%.0f");
								ImGui::EndGroup();
							}
						}

						// Anchor section
						{
							ImGui::Text("Anchors");
							ImGui::SameLine(100);

							const char* presetNames[] = {
								"Top Left", "Top Center", "Top Right",
								"Middle Left", "Center", "Middle Right",
								"Bottom Left", "Bottom Center", "Bottom Right",
								"Stretch Horizontal", "Stretch Vertical", "Stretch Both"
							};

							// Detect current preset from anchor values
							int currentPreset = -1;
							bool isStretchedX = (comp.anchorMinX != comp.anchorMaxX);
							bool isStretchedY = (comp.anchorMinY != comp.anchorMaxY);
							
							if (isStretchedX && isStretchedY) {
								currentPreset = 11; // Stretch Both
							}
							else if (isStretchedX) {
								currentPreset = 9; // Stretch Horizontal
							}
							else if (isStretchedY) {
								currentPreset = 10; // Stretch Vertical
							}
							else {
								// Point anchor - determine which preset
								float anchorX = comp.anchorMinX;
								float anchorY = comp.anchorMinY;
								
								if (anchorX == 0.0f && anchorY == 1.0f) currentPreset = 0; // Top Left
								else if (anchorX == 0.5f && anchorY == 1.0f) currentPreset = 1; // Top Center
								else if (anchorX == 1.0f && anchorY == 1.0f) currentPreset = 2; // Top Right
								else if (anchorX == 0.0f && anchorY == 0.5f) currentPreset = 3; // Middle Left
								else if (anchorX == 0.5f && anchorY == 0.5f) currentPreset = 4; // Center
								else if (anchorX == 1.0f && anchorY == 0.5f) currentPreset = 5; // Middle Right
								else if (anchorX == 0.0f && anchorY == 0.0f) currentPreset = 6; // Bottom Left
								else if (anchorX == 0.5f && anchorY == 0.0f) currentPreset = 7; // Bottom Center
								else if (anchorX == 1.0f && anchorY == 0.0f) currentPreset = 8; // Bottom Right
								else currentPreset = 4; // Default to Center if no match
							}

							ImGui::SetNextItemWidth(150);
							int newPreset = currentPreset;
							if (ImGui::Combo("##AnchorPresets", &newPreset, presetNames, IM_ARRAYSIZE(presetNames)))
							{
								if (newPreset != currentPreset) {
									// Store old values for potential position adjustment
									float oldAnchorMinX = comp.anchorMinX;
									float oldAnchorMaxX = comp.anchorMaxX;
									float oldAnchorMinY = comp.anchorMinY;
									float oldAnchorMaxY = comp.anchorMaxY;
									
									// Set new anchor values
									switch (newPreset)
									{
									case 0: comp.anchorMinX = comp.anchorMaxX = 0.0f; comp.anchorMinY = comp.anchorMaxY = 1.0f; break;
									case 1: comp.anchorMinX = comp.anchorMaxX = 0.5f; comp.anchorMinY = comp.anchorMaxY = 1.0f; break;
									case 2: comp.anchorMinX = comp.anchorMaxX = 1.0f; comp.anchorMinY = comp.anchorMaxY = 1.0f; break;
									case 3: comp.anchorMinX = comp.anchorMaxX = 0.0f; comp.anchorMinY = comp.anchorMaxY = 0.5f; break;
									case 4: comp.anchorMinX = comp.anchorMaxX = 0.5f; comp.anchorMinY = comp.anchorMaxY = 0.5f; break;
									case 5: comp.anchorMinX = comp.anchorMaxX = 1.0f; comp.anchorMinY = comp.anchorMaxY = 0.5f; break;
									case 6: comp.anchorMinX = comp.anchorMaxX = 0.0f; comp.anchorMinY = comp.anchorMaxY = 0.0f; break;
									case 7: comp.anchorMinX = comp.anchorMaxX = 0.5f; comp.anchorMinY = comp.anchorMaxY = 0.0f; break;
									case 8: comp.anchorMinX = comp.anchorMaxX = 1.0f; comp.anchorMinY = comp.anchorMaxY = 0.0f; break;
									case 9: comp.anchorMinX = 0.0f; comp.anchorMaxX = 1.0f; comp.anchorMinY = comp.anchorMaxY = 0.5f; break;
									case 10: comp.anchorMinX = comp.anchorMaxX = 0.5f; comp.anchorMinY = 0.0f; comp.anchorMaxY = 1.0f; break;
									case 11: comp.anchorMinX = 0.0f; comp.anchorMaxX = 1.0f; comp.anchorMinY = 0.0f; comp.anchorMaxY = 1.0f; break;
									}
									
									// Reset offsets when switching to/from stretch mode
									bool wasStretchedX = (oldAnchorMinX != oldAnchorMaxX);
									bool wasStretchedY = (oldAnchorMinY != oldAnchorMaxY);
									bool nowStretchedX = (comp.anchorMinX != comp.anchorMaxX);
									bool nowStretchedY = (comp.anchorMinY != comp.anchorMaxY);
									
									if (nowStretchedX && !wasStretchedX) {
										// Switching to stretch X - reset X offsets to 0 so element stretches to full width
										comp.offsetMinX = 0.0f;
										comp.offsetMaxX = 0.0f;
										// Also reset X position since it's not used for stretch anchors
										comp.x = 0.0f;
									}
									if (nowStretchedY && !wasStretchedY) {
										// Switching to stretch Y - reset Y offsets to 0 so element stretches to full height
										comp.offsetMinY = 0.0f;
										comp.offsetMaxY = 0.0f;
										// Also reset Y position since it's not used for stretch anchors
										comp.y = 0.0f;
									}
									// Also reset offsets if already in stretch mode (e.g., switching between stretch presets)
									if (nowStretchedX) {
										comp.offsetMinX = 0.0f;
										comp.offsetMaxX = 0.0f;
									}
									if (nowStretchedY) {
										comp.offsetMinY = 0.0f;
										comp.offsetMaxY = 0.0f;
									}
									if (!nowStretchedX && wasStretchedX) {
										// Switching from stretch X - reset X position
										comp.x = 0.0f;
									}
									if (!nowStretchedY && wasStretchedY) {
										// Switching from stretch Y - reset Y position
										comp.y = 0.0f;
									}
									
									NE::MarkSceneDirty();
								}
							}

							ImGui::Indent(16.0f);

							ImGui::AlignTextToFramePadding();
							ImGui::Text("Min");
							ImGui::SameLine(100);

							ImGui::PushItemWidth(70);
							ImGui::Text("X");
							ImGui::SameLine();
							UI_RECT_DRAG("##AnchorMinX", &NE::ECS::Component::UIRectTransform::anchorMinX, 0.01f, 0.0f, 1.0f, "%.2f");
							ImGui::SameLine();
							ImGui::Text("Y");
							ImGui::SameLine();
							UI_RECT_DRAG("##AnchorMinY", &NE::ECS::Component::UIRectTransform::anchorMinY, 0.01f, 0.0f, 1.0f, "%.2f");
							ImGui::PopItemWidth();

							ImGui::AlignTextToFramePadding();
							ImGui::Text("Max");
							ImGui::SameLine(100);

							ImGui::PushItemWidth(70);
							ImGui::Text("X");
							ImGui::SameLine();
							UI_RECT_DRAG("##AnchorMaxX", &NE::ECS::Component::UIRectTransform::anchorMaxX, 0.01f, 0.0f, 1.0f, "%.2f");
							ImGui::SameLine();
							ImGui::Text("Y");
							ImGui::SameLine();
							UI_RECT_DRAG("##AnchorMaxY", &NE::ECS::Component::UIRectTransform::anchorMaxY, 0.01f, 0.0f, 1.0f, "%.2f");
							ImGui::PopItemWidth();
							ImGui::Unindent(16.0f);
						}

						// Pivot section
						{
							ImGui::AlignTextToFramePadding();
							ImGui::Text("Pivot");
							ImGui::SameLine(100);

							ImGui::PushItemWidth(70);
							ImGui::Text("X");
							ImGui::SameLine();
							UI_RECT_SLIDER("##PivotX", &NE::ECS::Component::UIRectTransform::pivotX, 0.0f, 1.0f, "%.2f");
							ImGui::SameLine();
							ImGui::Text("Y");
							ImGui::SameLine();
							UI_RECT_SLIDER("##PivotY", &NE::ECS::Component::UIRectTransform::pivotY, 0.0f, 1.0f, "%.2f");
							ImGui::PopItemWidth();
						}

						// Rotation section
						{
							ImGui::AlignTextToFramePadding();
							ImGui::Text("Rotation");
							ImGui::SameLine(100);

							constexpr float ROTATION_DRAG_SPEED = 5.0f;

							ImGui::PushItemWidth(70);
							if (!isOverlay)
							{
								ImGui::Text("X");
								ImGui::SameLine();
								UI_RECT_DRAG("##RotX", &NE::ECS::Component::UIRectTransform::rotationX, ROTATION_DRAG_SPEED, 0.0f, 0.0f, "%.1f");
								ImGui::SameLine();
								ImGui::Text("Y");
								ImGui::SameLine();
								UI_RECT_DRAG("##RotY", &NE::ECS::Component::UIRectTransform::rotationY, ROTATION_DRAG_SPEED, 0.0f, 0.0f, "%.1f");							
								ImGui::SameLine();
							}
							ImGui::Text("Z");
							ImGui::SameLine();
							UI_RECT_DRAG("##RotZ", &NE::ECS::Component::UIRectTransform::rotationZ,	ROTATION_DRAG_SPEED, 0.0f, 0.0f, "%.1f");					
							ImGui::PopItemWidth();
						}

						// Scale section
						{
							ImGui::AlignTextToFramePadding();
							ImGui::Text("Scale");
							ImGui::SameLine(100);

							ImGui::PushItemWidth(itemWidth);

							ImGui::Text("X");
							ImGui::SameLine();
							UI_RECT_DRAG("##ScaleX", &NE::ECS::Component::UIRectTransform::scaleX, 0.01f, 0.01f, 10.0f, "%.2f");
							ImGui::SameLine();
							ImGui::Text("Y");
							ImGui::SameLine();
							UI_RECT_DRAG("##ScaleY", &NE::ECS::Component::UIRectTransform::scaleY, 0.01f, 0.01f, 10.0f, "%.2f");

							if (!isOverlay)
							{
								ImGui::SameLine();
								ImGui::Text("Z");
								ImGui::SameLine();
								UI_RECT_DRAG("##ScaleZ", &NE::ECS::Component::UIRectTransform::scaleZ, 0.01f, 0.01f, 10.0f, "%.2f");
							}

							ImGui::PopItemWidth();
						}

						#undef UI_RECT_DRAG
						#undef UI_RECT_SLIDER

						ImGui::Unindent();
					}
				}
				else if (typeIdx == typeid(NE::ECS::Component::UICanvas))
				{
					auto& comp = NE::ECS::Command::GetUICanvas(entity);

					if (ImGui::CollapsingHeader("Canvas", ImGuiTreeNodeFlags_DefaultOpen))
					{
						ImGui::Indent();

						const float labelWidth = 140.0f;

						// render Mode dropdown
						ImGui::AlignTextToFramePadding();
						ImGui::Text("Render Mode");
						ImGui::SameLine(labelWidth);
						ImGui::SetNextItemWidth(-1);
						static const char* RenderModes[] = {
							"Screen Space - Overlay",
							"Screen Space - Camera",
							"World Space"
						};
						int currentMode = static_cast<int>(comp.renderMode);
						if (ImGui::Combo("##RenderMode", &currentMode, RenderModes, IM_ARRAYSIZE(RenderModes)))
						{
							auto oldMode = comp.renderMode;
							comp.renderMode = static_cast<decltype(comp.renderMode)>(currentMode);
							std::string materialPath = GetUIMaterialPathForRenderMode(comp.renderMode);
							std::string materialUUID = AssetManager::GetInstance().RetrieveUUID(materialPath);

							if (materialUUID.empty()) {
								SPD_ERROR("[InspectorPanel] Failed to find material for render mode: {}", materialPath);
								SPD_ERROR("Make sure UI_Overlay.nanomat, UI_Camera.nanomat, and UI_World.nanomat exist in Assets/");
							}
							else {
								// Rebuild all child materials with the new shader
								RebuildChildMaterials(entity, materialUUID);

								SPD_INFO("[InspectorPanel] Canvas render mode changed: {} -> {}",
									static_cast<int>(oldMode), currentMode);
								SPD_INFO("Assigned material: {}", materialPath);
							}

							NE::MarkSceneDirty();
						}

						// pixel perfect toggle (if in overlay mode or camera mode)
						if (comp.renderMode == NE::ECS::Component::UICanvas::RenderMode::SCREEN_SPACE_OVERLAY ||
							comp.renderMode == NE::ECS::Component::UICanvas::RenderMode::SCREEN_SPACE_CAMERA)
						{
							ImGui::AlignTextToFramePadding();
							ImGui::Text("Pixel Perfect");
							ImGui::SameLine(labelWidth);
							ImGui::SetNextItemWidth(-1);
							if (ImGui::Checkbox("##PixelPerfect", &comp.pixelPerfect))
							{
								NE::MarkSceneDirty();
							}
						}

						// show plane distqance for camera mode only
						if (comp.renderMode == NE::ECS::Component::UICanvas::RenderMode::SCREEN_SPACE_CAMERA) {
							ImGui::AlignTextToFramePadding();
							ImGui::Text("Plane Distance");
							ImGui::SameLine(labelWidth);
							ImGui::SetNextItemWidth(-1);
							if (ImGui::DragFloat("##PlaneDistance", &comp.planeDistance, 1.0f, 0.1f, 1000.0f))
							{
								NE::MarkSceneDirty();
							}
						}

						// Sort Order
						ImGui::AlignTextToFramePadding();
						ImGui::Text("Sort Order");
						ImGui::SameLine(labelWidth);
						ImGui::SetNextItemWidth(-1);
						if (ImGui::DragInt("##SortOrder", &comp.sortingOrder))
						{
							NE::MarkSceneDirty();
						}

						ImGui::Unindent();
					}

					// scalar section
					if (ImGui::CollapsingHeader("Canvas Scaler", ImGuiTreeNodeFlags_DefaultOpen))
					{
						ImGui::Indent();

						const float labelWidth = 140.0f;

						// UI scale mode
						ImGui::AlignTextToFramePadding();
						ImGui::Text("UI Scale Mode");
						ImGui::SameLine(labelWidth);
						ImGui::SetNextItemWidth(-1);
						static const char* ScaleModes[] = {
							"Constant Pixel Size",
							"Scale With Screen Size",
							"Constant Physical Size"
						};
						int currentScaleMode = static_cast<int>(comp.scaleMode);
						if (ImGui::Combo("##UIScaleMode", &currentScaleMode, ScaleModes, IM_ARRAYSIZE(ScaleModes))) {
							comp.scaleMode = static_cast<decltype(comp.scaleMode)>(currentScaleMode);
						}

						ImGui::Spacing();

						// show different options based on UI Scale Mode
						switch (comp.scaleMode) {
						case NE::ECS::Component::UICanvas::ScaleMode::CONSTANT_PIXEL_SIZE:
						{
							ImGui::AlignTextToFramePadding();
							ImGui::Text("Scale Factor");
							ImGui::SameLine(labelWidth);
							ImGui::SetNextItemWidth(-1);
							ImGui::DragFloat("##ScaleFactor", &comp.scaleFactor, 0.01f, 0.01f, 10.0f);

							//ImGui::TextDisabled("Reference Pixels Per Unit");
							//float refPixels = 100.0f; // Add this to your component if needed
							//ImGui::DragFloat("##RefPixels", &refPixels, 1.0f, 1.0f, 1000.0f);
							break;
						}

						case NE::ECS::Component::UICanvas::ScaleMode::SCALE_WITH_SCREEN_SIZE:
						{
							ImGui::Text("Reference Resolution");
							ImGui::Indent();

							ImGui::BeginGroup();
							{
								ImGui::Columns(3, "RefResColumns", false);
								ImGui::SetColumnWidth(0, 20.0f);
								ImGui::SetColumnWidth(1, 100.0f);

								ImGui::Text("X"); ImGui::NextColumn();
								ImGui::SetNextItemWidth(-1);
								ImGui::DragFloat("##RefResX", &comp.referenceWidth, 1.0f, 1.0f, 10000.0f);
								ImGui::NextColumn(); ImGui::NextColumn();

								ImGui::Text("Y"); ImGui::NextColumn();
								ImGui::SetNextItemWidth(-1);
								ImGui::DragFloat("##RefResY", &comp.referenceHeight, 1.0f, 1.0f, 10000.0f);
								ImGui::NextColumn();

								ImGui::Columns(1);
							}
							ImGui::EndGroup();

							ImGui::Unindent();

							//ImGui::Spacing();
							//ImGui::Text("Screen Match Mode");
							//static const char* MatchModes[] = { "Match Width Or Height", "Expand", "Shrink" };
							//int matchMode = 0; // Add this to your component if needed
							//ImGui::Combo("##ScreenMatchMode", &matchMode, MatchModes, IM_ARRAYSIZE(MatchModes));

							//ImGui::DragFloat("Match", &comp.screenMatchMode, 0.01f, 0.0f, 1.0f);
							//ImGui::SameLine();
							//ImGui::TextDisabled("(0=Width, 1=Height)");
							break;
						}

						case NE::ECS::Component::UICanvas::ScaleMode::CONSTANT_PHYSICAL_SIZE:
						{
							//static const char* PhysicalUnits[] = {
							//    "Centimeters",
							//    "Millimeters",
							//    "Inches",
							//    "Points",
							//    "Picas"
							//};
							//int currentUnit = static_cast<int>(comp.physicalUnit);
							//ImGui::Combo("Physical Unit", &currentUnit, PhysicalUnits, IM_ARRAYSIZE(PhysicalUnits));
							//comp.physicalUnit = static_cast<NE::ECS::Component::UICanvas::PhysicalUnit>(currentUnit);

							//ImGui::DragFloat("Fallback Screen DPI", &comp.fallbackScreenDPI, 1.0f, 1.0f, 1000.0f);
							//ImGui::DragFloat("Default Sprite DPI", &comp.defaultSpriteDPI, 1.0f, 1.0f, 1000.0f);
							break;
						}
						}

						ImGui::Unindent();
					}
				}
				else if (typeIdx == typeid(NE::ECS::Component::UIImage))
				{
					auto& comp = NE::ECS::Command::GetUIImage(entity);

					if (ImGui::CollapsingHeader("UI Image", ImGuiTreeNodeFlags_DefaultOpen))
					{
						ImGui::Indent();

						const float labelWidth = 160.0f;

						// texture assignment
						{
							// source image
							ImGui::AlignTextToFramePadding();
							ImGui::Text("Source Image");
							ImGui::SameLine(labelWidth);

							ImGui::SetNextItemWidth(-1);

							std::string texLabel = comp.textureUUID.empty()
								? ""
								: AssetManager::GetInstance().RetrieveFileName(comp.textureUUID);

							char bufTex[256];
							strncpy_s(bufTex, texLabel.c_str(), sizeof(bufTex));
							ImGui::InputText("Source Image", bufTex, sizeof(bufTex));

							if (ImGui::BeginDragDropTarget())
							{
								if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload("TEXTURE_ASSET_PATH"))
								{
									std::string dropped((const char*)p->Data, p->DataSize - 1); // dropped = "Assets/Textures/MyTexture.jpg"
									auto textureUUID = AssetManager::GetInstance().RetrieveUUID(dropped); // convert file path to UUID --> uuid = "abc123def456" (from MyTexture.jpg.meta file)

									// find parent canvas to determine render mode
									auto& rectTransform = NE::ECS::Command::GetUIRectTransform(entity);
									uint32_t canvasEntity = entity;
									uint32_t current = rectTransform.parent;

									// walk up hierarchy to find canvas
									while (current != NE::ECS::NO_ENTITY)
									{
										if (NE::ECS::Query::HasUICanvas(current))
										{
											canvasEntity = current;
											break;
										}
										if (NE::ECS::Query::HasUIRectTransform(current))
										{
											current = NE::ECS::Query::GetUIRectTransform(current).parent;
										}
										else
										{
											break;
										}
									}

									// get render mode from canvas
									int renderMode = 0;
									if (NE::ECS::Query::HasUICanvas(canvasEntity))
									{
										auto& canvas = NE::ECS::Command::GetUICanvas(canvasEntity);
										renderMode = static_cast<int>(canvas.renderMode);
									}

									// determine material file path based on render mode
									std::string materialPath;
									switch (renderMode) {
									case 0: materialPath = "Assets/UI_Overlay.nanomat"; break;
									case 1: materialPath = "Assets/UI_Camera.nanomat"; break;
									case 2: materialPath = "Assets/UI_World.nanomat"; break;
									default: materialPath = "Assets/UI_Overlay.nanomat"; break;
									}

									// convert material path to UUID
									std::string materialUUID = AssetManager::GetInstance().RetrieveUUID(materialPath);

									if (materialUUID.empty())
									{
										SPD_ERROR("[InspectorPanel] Failed to retrieve material UUID for: " << materialPath);
									}
									else if (textureUUID.empty())
									{
										SPD_ERROR("[InspectorPanel] Failed to retrieve texture UUID for: " << dropped);
									}
									else
									{
										// call assignment function with both UUIDs
										NE::Renderer::Command::AssignUITexture(entity, textureUUID, materialUUID);
									}
								}
								ImGui::EndDragDropTarget();
							}

							// Right-click to clear texture
							if (ImGui::BeginPopupContextItem("##TextureContext"))
							{
								if (ImGui::MenuItem("Clear"))
								{
									comp.textureUUID.clear();
									comp.material.reset();  // Clear material

									// Mark dirty
									if (NE::GetEngineState() == NE::EngineState::Edit)
									{
										if constexpr (requires { comp.isDirty; }) comp.isDirty = true;
										NE::MarkSceneDirty();
									}
								}
								ImGui::EndPopup();
							}
						}

						// color
						{
							ImGui::AlignTextToFramePadding();
							ImGui::Text("Color");
							ImGui::SameLine(labelWidth);
							ImGui::SetNextItemWidth(-1);
							float color[4] = { comp.color.x, comp.color.y, comp.color.z, comp.color.w };

							if (ImGui::ColorEdit4("##Color", color, ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreviewHalf)) {
								comp.color.x = color[0];
								comp.color.y = color[1];
								comp.color.z = color[2];
								comp.color.w = color[3];
							}
						}

						// image type
						{
							ImGui::AlignTextToFramePadding();
							ImGui::Text("Image Type");
							ImGui::SameLine(labelWidth);
							ImGui::SetNextItemWidth(-1);

							static const char* ImageTypes[] = { "Simple", "Sliced", "Tiled", "Filled" };
							int currentImageType = static_cast<int>(comp.imageType);
							if (ImGui::Combo("##ImageType", &currentImageType, ImageTypes, IM_ARRAYSIZE(ImageTypes))) {
								comp.imageType = static_cast<NE::ECS::Component::UIImage::ImageType>(currentImageType);
								comp.isDirty = true;
								NE::MarkSceneDirty();
							}

							// type specific options
							ImGui::Indent(16.0f);

							switch (comp.imageType)
							{
							case NE::ECS::Component::UIImage::ImageType::SIMPLE:
							{
								// simple image options
								ImGui::Text("Preserve Aspect");
								ImGui::SameLine(labelWidth); // adjust for indent
								ImGui::SetNextItemWidth(-1);
								if (ImGui::Checkbox("##PreserveAspect", &comp.preserveAspect)) {
									comp.isDirty = true;
									NE::MarkSceneDirty();
								}

								// reset fill amount when switching to simple mode
								if (comp.fillAmount < 1.0f)
								{
									comp.fillAmount = 1.0f;
									comp.isDirty = true;
								}
								break;
							}

							case NE::ECS::Component::UIImage::ImageType::SLICED:
							{
								// 9-slice borders
								//ImGui::Text("Border Left");
								//ImGui::SameLine(labelWidth);
								//ImGui::SetNextItemWidth(-1);
								//if (ImGui::DragFloat("##BorderLeft", &comp.borderLeft, 1.0f, 0.0f, 1000.0f)) {
								//	comp.isDirty = true;
								//	NE::MarkSceneDirty();
								//}

								//ImGui::Text("Border Right");
								//ImGui::SameLine(labelWidth);
								//ImGui::SetNextItemWidth(-1);
								//if (ImGui::DragFloat("##BorderRight", &comp.borderRight, 1.0f, 0.0f, 1000.0f)) {
								//	comp.isDirty = true;
								//	NE::MarkSceneDirty();
								//}

								//ImGui::Text("Border Top");
								//ImGui::SameLine(labelWidth);
								//ImGui::SetNextItemWidth(-1);
								//if (ImGui::DragFloat("##BorderTop", &comp.borderTop, 1.0f, 0.0f, 1000.0f)) {
								//	comp.isDirty = true;
								//	NE::MarkSceneDirty();
								//}

								//ImGui::Text("Border Bottom");
								//ImGui::SameLine(labelWidth);
								//ImGui::SetNextItemWidth(-1);
								//if (ImGui::DragFloat("##BorderBottom", &comp.borderBottom, 1.0f, 0.0f, 1000.0f)) {
								//	comp.isDirty = true;
								//	NE::MarkSceneDirty();
								//}

								// reset fill amount when switching to simple mode
								if (comp.fillAmount < 1.0f)
								{
									comp.fillAmount = 1.0f;
									comp.isDirty = true;
								}
								break;
							}

							case NE::ECS::Component::UIImage::ImageType::TILED:
							{
								// tiled options
								ImGui::Text("Pixels Per Unit Multiplier");
								ImGui::SameLine(labelWidth);
								ImGui::SetNextItemWidth(-1);
								if (ImGui::DragFloat("##PixelsPerUnitMultiplier", &comp.pixelsPerUnitMultiplier, 0.1f, 0.1f, 10.0f)) {
									comp.isDirty = true;
									NE::MarkSceneDirty();
								}

								// reset fill amount when switching to simple mode
								if (comp.fillAmount < 1.0f)
								{
									comp.fillAmount = 1.0f;
									comp.isDirty = true;
								}
								break;
							}

							case NE::ECS::Component::UIImage::ImageType::FILLED:
							{
								// fill Method dropdown
								ImGui::Text("Fill Method");
								ImGui::SameLine(labelWidth);
								ImGui::SetNextItemWidth(-1);

								static const char* FillMethods[] = {
									"Horizontal",
									"Vertical",
									"Radial 90",
									"Radial 180",
									"Radial 360"
								};

								int currentFillMethod = static_cast<int>(comp.fillMethod);
								if (ImGui::Combo("##FillMethod", &currentFillMethod, FillMethods, IM_ARRAYSIZE(FillMethods))) {
									comp.fillMethod = static_cast<NE::ECS::Component::UIImage::FillMethod>(currentFillMethod);
									comp.isDirty = true;
									NE::MarkSceneDirty();
								}

								// fill Origin (context-dependent)
								ImGui::Text("Fill Origin");
								ImGui::SameLine(labelWidth);
								ImGui::SetNextItemWidth(-1);

								if (comp.fillMethod == NE::ECS::Component::UIImage::FillMethod::HORIZONTAL)
								{
									static const char* HOrigins[] = { "Left", "Right" };
									int origin = static_cast<int>(comp.fillOrigin);
									if (ImGui::Combo("##FillOrigin", &origin, HOrigins, IM_ARRAYSIZE(HOrigins))) {
										comp.fillOrigin = static_cast<NE::ECS::Component::UIImage::FillOrigin>(origin);
										comp.isDirty = true;
										NE::MarkSceneDirty();
									}
								}
								else if (comp.fillMethod == NE::ECS::Component::UIImage::FillMethod::VERTICAL)
								{
									static const char* VOrigins[] = { "Bottom", "Top" };
									int origin = static_cast<int>(comp.fillOrigin);
									if (ImGui::Combo("##FillOrigin", &origin, VOrigins, IM_ARRAYSIZE(VOrigins))) {
										comp.fillOrigin = static_cast<NE::ECS::Component::UIImage::FillOrigin>(origin);
										comp.isDirty = true;
										NE::MarkSceneDirty();
									}
								}
								else // radial fills
								{
									static const char* RadialOrigins[] = {
										"Bottom",
										"Right",
										"Top",
										"Left"
									};

									int origin = static_cast<int>(comp.fillOrigin);
									if (ImGui::Combo("##FillOrigin", &origin, RadialOrigins, IM_ARRAYSIZE(RadialOrigins))) {
										comp.fillOrigin = static_cast<NE::ECS::Component::UIImage::FillOrigin>(origin);
										comp.isDirty = true;
										NE::MarkSceneDirty();
									}
								}

								// fill Amount slider
								ImGui::Text("Fill Amount");
								ImGui::SameLine(labelWidth);
								ImGui::SetNextItemWidth(-1);
								if (ImGui::SliderFloat("##FillAmount", &comp.fillAmount, 0.0f, 1.0f))
								{
									comp.ClampFillAmount();
									comp.isDirty = true;
									NE::MarkSceneDirty();
								}

								// clockwise toggle 
								if (comp.fillMethod == NE::ECS::Component::UIImage::FillMethod::RADIAL_90 ||
									comp.fillMethod == NE::ECS::Component::UIImage::FillMethod::RADIAL_180 ||
									comp.fillMethod == NE::ECS::Component::UIImage::FillMethod::RADIAL_360)
								{
									ImGui::Text("Clockwise");
									ImGui::SameLine(labelWidth);
									ImGui::SetNextItemWidth(-1);
									if (ImGui::Checkbox("##Clockwise", &comp.fillClockwise))
									{
										comp.isDirty = true;
										NE::MarkSceneDirty();
									}
								}

								// preserve aspect
								ImGui::Text("Preserve Aspect");
								ImGui::SameLine(labelWidth);
								ImGui::SetNextItemWidth(-1);
								if (ImGui::Checkbox("##PreserveAspect", &comp.preserveAspect))
								{
									comp.isDirty = true;
									NE::MarkSceneDirty();
								}
								break;
							}
							}

							ImGui::Unindent(16.0f);
							ImGui::Unindent();
						}
					}
				}
				else if (typeIdx == typeid(NE::ECS::Component::UIButton))
				{
					if (!NE::ECS::Query::HasUIButton(entity)) continue;
					auto& comp = NE::ECS::Command::GetUIButton(entity);

					if (ImGui::CollapsingHeader("Button", ImGuiTreeNodeFlags_DefaultOpen))
					{
						ImGui::Indent();

						const float labelWidth = 140.0f;

						// Interactable
						ImGui::AlignTextToFramePadding();
						ImGui::Text("Interactable");
						ImGui::SameLine(labelWidth);
						ImGui::SetNextItemWidth(-1);
						if (ImGui::Checkbox("##Interactable", &comp.interactable))
						{
							if (!comp.interactable) {
								comp.currentState = NE::ECS::Component::UIButton::State::DISABLED;
							} else {
								comp.currentState = NE::ECS::Component::UIButton::State::NORMAL;
							}
							NE::MarkSceneDirty();
						}

						// Transition Type
						ImGui::AlignTextToFramePadding();
						ImGui::Text("Transition");
						ImGui::SameLine(labelWidth);
						ImGui::SetNextItemWidth(-1);
						static const char* TransitionTypes[] = {
							"Color Tint",
							"Sprite Swap",
							"Animation"
						};
						int currentTransition = static_cast<int>(comp.transitionType);
						if (ImGui::Combo("##Transition", &currentTransition, TransitionTypes, IM_ARRAYSIZE(TransitionTypes)))
						{
							comp.transitionType = static_cast<decltype(comp.transitionType)>(currentTransition);
							NE::MarkSceneDirty();
						}

						// Color States
						ImGui::Spacing();
						ImGui::Text("Colors");
						ImGui::Indent();

						// Normal Color - label on left, color picker on right
						ImGui::AlignTextToFramePadding();
						ImGui::Text("Normal");
						ImGui::SameLine(labelWidth);
						float normalColor[4] = { comp.normalColor.x, comp.normalColor.y, comp.normalColor.z, comp.normalColor.w };
						ImGui::SetNextItemWidth(-1);
						if (ImGui::ColorEdit4("##NormalColor", normalColor, ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreviewHalf)) {
							comp.normalColor.x = normalColor[0];
							comp.normalColor.y = normalColor[1];
							comp.normalColor.z = normalColor[2];
							comp.normalColor.w = normalColor[3];
							NE::MarkSceneDirty();
						}

						// Hover Color
						ImGui::AlignTextToFramePadding();
						ImGui::Text("Hovered");
						ImGui::SameLine(labelWidth);
						float hoverColor[4] = { comp.hoverColor.x, comp.hoverColor.y, comp.hoverColor.z, comp.hoverColor.w };
						ImGui::SetNextItemWidth(-1);
						if (ImGui::ColorEdit4("##HoverColor", hoverColor, ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreviewHalf)) {
							comp.hoverColor.x = hoverColor[0];
							comp.hoverColor.y = hoverColor[1];
							comp.hoverColor.z = hoverColor[2];
							comp.hoverColor.w = hoverColor[3];
							NE::MarkSceneDirty();
						}

						// Pressed Color
						ImGui::AlignTextToFramePadding();
						ImGui::Text("Pressed");
						ImGui::SameLine(labelWidth);
						float pressedColor[4] = { comp.pressedColor.x, comp.pressedColor.y, comp.pressedColor.z, comp.pressedColor.w };
						ImGui::SetNextItemWidth(-1);
						if (ImGui::ColorEdit4("##PressedColor", pressedColor, ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreviewHalf)) {
							comp.pressedColor.x = pressedColor[0];
							comp.pressedColor.y = pressedColor[1];
							comp.pressedColor.z = pressedColor[2];
							comp.pressedColor.w = pressedColor[3];
							NE::MarkSceneDirty();
						}

						// Disabled Color
						ImGui::AlignTextToFramePadding();
						ImGui::Text("Disabled");
						ImGui::SameLine(labelWidth);
						float disabledColor[4] = { comp.disabledColor.x, comp.disabledColor.y, comp.disabledColor.z, comp.disabledColor.w };
						ImGui::SetNextItemWidth(-1);
						if (ImGui::ColorEdit4("##DisabledColor", disabledColor, ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreviewHalf)) {
							comp.disabledColor.x = disabledColor[0];
							comp.disabledColor.y = disabledColor[1];
							comp.disabledColor.z = disabledColor[2];
							comp.disabledColor.w = disabledColor[3];
							NE::MarkSceneDirty();
						}

						ImGui::Unindent();
						ImGui::Unindent();
					}
				}
			{
				if (!NE::ECS::Query::HasUIText(entity)) continue;
				auto& comp = NE::ECS::Command::GetUIText(entity);

				if (ImGui::CollapsingHeader("Text", ImGuiTreeNodeFlags_DefaultOpen))
				{
					ImGui::Indent();

					const float labelWidth = 160.0f;

					// Text Content
					ImGui::AlignTextToFramePadding();
					ImGui::Text("Text");
					ImGui::SameLine(labelWidth);
					ImGui::SetNextItemWidth(-1);
					char textBuffer[1024];
					strncpy_s(textBuffer, sizeof(textBuffer), comp.text.c_str(), sizeof(textBuffer));
					textBuffer[sizeof(textBuffer) - 1] = '\0';
					if (ImGui::InputTextMultiline("##Text", textBuffer, sizeof(textBuffer), ImVec2(-1, ImGui::GetTextLineHeight() * 3))) {
						comp.text = textBuffer;
						NE::MarkSceneDirty();
					}

					// Font Size
					ImGui::AlignTextToFramePadding();
					ImGui::Text("Font Size");
					ImGui::SameLine(labelWidth);
					ImGui::SetNextItemWidth(-1);
					if (ImGui::DragFloat("##FontSize", &comp.fontSize, 1.0f, 1.0f, 200.0f)) {
						NE::MarkSceneDirty();
					}

					// Color
					ImGui::AlignTextToFramePadding();
					ImGui::Text("Color");
					ImGui::SameLine(labelWidth);
					float textColor[4] = { comp.color.x, comp.color.y, comp.color.z, comp.color.w };
					ImGui::SetNextItemWidth(-1);
					if (ImGui::ColorEdit4("##TextColor", textColor, ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreviewHalf)) {
						comp.color.x = textColor[0];
						comp.color.y = textColor[1];
						comp.color.z = textColor[2];
						comp.color.w = textColor[3];
						NE::MarkSceneDirty();
					}

					// Font Style
					ImGui::AlignTextToFramePadding();
					ImGui::Text("Font Style");
					ImGui::SameLine(labelWidth);
					ImGui::SetNextItemWidth(-1);
					static const char* FontStyles[] = { "Normal", "Bold", "Italic", "Bold And Italic" };
					int currentFontStyle = static_cast<int>(comp.fontStyle);
					if (ImGui::Combo("##FontStyle", &currentFontStyle, FontStyles, IM_ARRAYSIZE(FontStyles))) {
						comp.fontStyle = static_cast<decltype(comp.fontStyle)>(currentFontStyle);
						NE::MarkSceneDirty();
					}

					// Horizontal Alignment
					ImGui::AlignTextToFramePadding();
					ImGui::Text("Horizontal Align");
					ImGui::SameLine(labelWidth);
					ImGui::SetNextItemWidth(-1);
					static const char* HAligns[] = { "Left", "Center", "Right", "Justify" };
					int currentHAlign = static_cast<int>(comp.horizontalAlign);
					if (ImGui::Combo("##HAlign", &currentHAlign, HAligns, IM_ARRAYSIZE(HAligns))) {
						comp.horizontalAlign = static_cast<decltype(comp.horizontalAlign)>(currentHAlign);
						NE::MarkSceneDirty();
					}

					// Vertical Alignment
					ImGui::AlignTextToFramePadding();
					ImGui::Text("Vertical Align");
					ImGui::SameLine(labelWidth);
					ImGui::SetNextItemWidth(-1);
					static const char* VAligns[] = { "Top", "Middle", "Bottom" };
					int currentVAlign = static_cast<int>(comp.verticalAlign);
					if (ImGui::Combo("##VAlign", &currentVAlign, VAligns, IM_ARRAYSIZE(VAligns))) {
						comp.verticalAlign = static_cast<decltype(comp.verticalAlign)>(currentVAlign);
						NE::MarkSceneDirty();
					}

					// Word Wrap
					ImGui::AlignTextToFramePadding();
					ImGui::Text("Word Wrap");
					ImGui::SameLine(labelWidth);
					ImGui::SetNextItemWidth(-1);
					if (ImGui::Checkbox("##WordWrap", &comp.wordWrap)) {
						NE::MarkSceneDirty();
					}

					// Horizontal Overflow
					ImGui::AlignTextToFramePadding();
					ImGui::Text("Horizontal Overflow");
					ImGui::SameLine(labelWidth);
					ImGui::SetNextItemWidth(-1);
					static const char* HOverflows[] = { "Wrap", "Visible", "Truncate" };
					int currentHOverflow = static_cast<int>(comp.horizontalOverflow);
					if (ImGui::Combo("##HOverflow", &currentHOverflow, HOverflows, IM_ARRAYSIZE(HOverflows))) {
						comp.horizontalOverflow = static_cast<decltype(comp.horizontalOverflow)>(currentHOverflow);
						NE::MarkSceneDirty();
					}

					// Vertical Overflow
					ImGui::AlignTextToFramePadding();
					ImGui::Text("Vertical Overflow");
					ImGui::SameLine(labelWidth);
					ImGui::SetNextItemWidth(-1);
					static const char* VOverflows[] = { "Wrap", "Visible", "Truncate" };
					int currentVOverflow = static_cast<int>(comp.verticalOverflow);
					if (ImGui::Combo("##VOverflow", &currentVOverflow, VOverflows, IM_ARRAYSIZE(VOverflows))) {
						comp.verticalOverflow = static_cast<decltype(comp.verticalOverflow)>(currentVOverflow);
						NE::MarkSceneDirty();
					}

					// Best Fit
					ImGui::AlignTextToFramePadding();
					ImGui::Text("Best Fit");
					ImGui::SameLine(labelWidth);
					ImGui::SetNextItemWidth(-1);
					if (ImGui::Checkbox("##BestFit", &comp.bestFit)) {
						NE::MarkSceneDirty();
					}

					if (comp.bestFit) {
						ImGui::Indent();
						// Min Size
						ImGui::AlignTextToFramePadding();
						ImGui::Text("Min Size");
						ImGui::SameLine(labelWidth);
						ImGui::SetNextItemWidth(-1);
						if (ImGui::DragFloat("##MinSize", &comp.minSize, 1.0f, 1.0f, 200.0f)) {
							NE::MarkSceneDirty();
						}

						// Max Size
						ImGui::AlignTextToFramePadding();
						ImGui::Text("Max Size");
						ImGui::SameLine(labelWidth);
						ImGui::SetNextItemWidth(-1);
						if (ImGui::DragFloat("##MaxSize", &comp.maxSize, 1.0f, 1.0f, 200.0f)) {
							NE::MarkSceneDirty();
						}
						ImGui::Unindent();
					}

					// Line Spacing
					ImGui::AlignTextToFramePadding();
					ImGui::Text("Line Spacing");
					ImGui::SameLine(labelWidth);
					ImGui::SetNextItemWidth(-1);
					if (ImGui::DragFloat("##LineSpacing", &comp.lineSpacing, 0.1f, 0.1f, 5.0f)) {
						NE::MarkSceneDirty();
					}

					// Character Spacing
					ImGui::AlignTextToFramePadding();
					ImGui::Text("Character Spacing");
					ImGui::SameLine(labelWidth);
					ImGui::SetNextItemWidth(-1);
					if (ImGui::DragFloat("##CharSpacing", &comp.characterSpacing, 1.0f, -50.0f, 50.0f)) {
						NE::MarkSceneDirty();
					}

					// Raycast Target
					ImGui::AlignTextToFramePadding();
					ImGui::Text("Raycast Target");
					ImGui::SameLine(labelWidth);
					ImGui::SetNextItemWidth(-1);
					if (ImGui::Checkbox("##RaycastTarget", &comp.raycastTarget)) {
						NE::MarkSceneDirty();
					}

					ImGui::Unindent();
				}
			}
		}

		if (ImGui::Button("Add Component"))
			{
				ImGui::OpenPopup("ComponentList");
			}

			if (ImGui::BeginPopup("ComponentList")) { // automate this next time with a registry
				if (ImGui::MenuItem("Renderer")) {
					NE::ECS::Command::AddRendererComponent(EditorScene::s_selectedEntity->linkedEntity);
					NE::MarkSceneDirty();
					SPD_DEBUG("[DirtyFlag] Added Renderer component - Scene marked DIRTY");
				}
				if (ImGui::MenuItem("Rigidbody")) {
					NE::ECS::Command::AddColliderComponent(EditorScene::s_selectedEntity->linkedEntity);
					NE::ECS::Command::AddRigidbodyComponent(EditorScene::s_selectedEntity->linkedEntity);
					NE::MarkSceneDirty();
					SPD_DEBUG("[DirtyFlag] Added Rigidbody/Collider components - Scene marked DIRTY");
				}
				if (ImGui::MenuItem("Collider")) {
					NE::ECS::Command::AddColliderComponent(EditorScene::s_selectedEntity->linkedEntity);
					NE::MarkSceneDirty();
					SPD_DEBUG("[DirtyFlag] Added Collider component - Scene marked DIRTY");
				}
				if (ImGui::MenuItem("Light")) {
					NE::ECS::Command::AddLightComponent(EditorScene::s_selectedEntity->linkedEntity);
					NE::MarkSceneDirty();
					SPD_DEBUG("[DirtyFlag] Added Light component - Scene marked DIRTY");
				}
				if (ImGui::MenuItem("AudioSource")) {
					NE::ECS::Command::AddAudioSourceComponent(EditorScene::s_selectedEntity->linkedEntity);
					NE::MarkSceneDirty();
					SPD_DEBUG("[DirtyFlag] Added AudioSource component - Scene marked DIRTY");
				}
				if (ImGui::MenuItem("Script")) {
					NE::ECS::Command::AddScriptComponent(EditorScene::s_selectedEntity->linkedEntity);
					NE::MarkSceneDirty();
					SPD_DEBUG("[DirtyFlag] Added Script component - Scene marked DIRTY");
				}
				if (ImGui::MenuItem("Camera")) {
					NE::ECS::Command::AddCameraComponent(EditorScene::s_selectedEntity->linkedEntity);
					NE::MarkSceneDirty();
					SPD_DEBUG("[DirtyFlag] Added Camera component - Scene marked DIRTY");
				}
				if (ImGui::MenuItem("Animator")) {
					NE::ECS::Command::AddAnimatorComponent(EditorScene::s_selectedEntity->linkedEntity);
					NE::MarkSceneDirty();
					SPD_DEBUG("[DirtyFlag] Added Animator component - Scene marked DIRTY");
				}

				ImGui::EndPopup();
			}
		}
		else if (EditorScene::selectedAsset != "")
		{
			std::filesystem::path assetPath = EditorScene::selectedAsset;

			if (assetPath.extension() == ".png" || assetPath.extension() == ".jpg") {
				RenderTextureImportSettings(assetPath.string() + ".meta");
			}
			else if (assetPath.extension() == ".obj" || assetPath.extension() == ".fbx") {
				RenderModelImportSettings(assetPath.string() + ".meta");
			}
			else if (assetPath.extension() == ".nanomat") {
				//RenderMaterialSettings();
				if (!m_materialEditor || m_lastPath != assetPath.string()) {
					m_materialEditor = std::make_unique<MaterialEditor>();
					if (m_materialEditor->LoadMaterial(assetPath.string(), AssetManager::GetInstance().RetrieveUUID(assetPath.string())))
						m_lastPath = assetPath.string();
					else
						m_materialEditor.reset();
				}

				if (m_materialEditor)
					m_materialEditor->RenderSettings();
			}
		}
		ImGui::End();
	}

	void InspectorPanel::RenderTextureImportSettings(std::string metaPath) {
		static std::string s_LastMetaPath;
		static TextureImportSettings s_Settings{};
		static bool s_Loaded = false;
		static bool s_dirty = false;

		if (!s_Loaded || metaPath != s_LastMetaPath) {
			if (!LoadTextureImportSettings(metaPath, s_Settings)) {
				ImGui::TextUnformatted("Failed to load texture import settings.");
				return;
			}

			s_LastMetaPath = metaPath;
			s_Loaded = true;
		}

		static const char* TextureTypeNames[] = { "Default", "Normal Map", "Sprite" };
		static const char* TextureShapeNames[] = { "2D", "Cube", "2D Array" };
		static const char* TextureWrapMode[] = { "Repeat", "Clamp", "Mirror", "MirrorOnce", "PerAxis" };
		static const char* TextureFilterMode[] = { "Point", "Bilinear", "Trilinear" };
		static const char* AlphaSourceNames[] = { "InputTextureAlpha", "GrayscaleSource", "None" };

		// ----- Texture Type -----
		int currentType = static_cast<int>(s_Settings.type); // assuming enum starts at 0
		if (ImGui::Combo("Texture Type", &currentType, TextureTypeNames, IM_ARRAYSIZE(TextureTypeNames))) {
			s_Settings.type = static_cast<TexType>(currentType);
			s_dirty = true;
		}

		// ----- Shape -----
		int currentShape = static_cast<int>(s_Settings.shape); // TextureShape enum
		if (ImGui::Combo("Texture Shape", &currentShape, TextureShapeNames, IM_ARRAYSIZE(TextureShapeNames))) {
			s_Settings.shape = static_cast<TexShape>(currentShape);
			s_dirty = true;
		}

		// ----- sRGB -----
		bool isSRGB = s_Settings.sRGB;
		if (Editor::DrawCheckbox("sRGB (Color Texture)", isSRGB)) {
			s_Settings.sRGB = isSRGB;
			s_dirty = true;
		}

		// ----- Alpha Source -----
		int currentAlpha = static_cast<int>(s_Settings.alphaSource); // AlphaSource enum
		if (ImGui::Combo("Alpha Source", &currentAlpha, AlphaSourceNames, IM_ARRAYSIZE(AlphaSourceNames))) {
			s_Settings.alphaSource = static_cast<TexAlphaSource>(currentAlpha);
			s_dirty = true;
		}

		// ----- Advanced -----
		if (ImGui::TreeNode("Advanced")) {
			bool generateMips = s_Settings.mips.generateMipmap;
			bool preserveCoverage = s_Settings.mips.preserveCoverage;

			if (Editor::DrawCheckbox("Generate Mipmaps", generateMips)) {
				s_Settings.mips.generateMipmap = generateMips;
				s_dirty = true;
			}

			if (Editor::DrawCheckbox("Preserve Coverage", preserveCoverage)) {
				s_Settings.mips.preserveCoverage = preserveCoverage;
				s_dirty = true;
			}

			ImGui::TreePop();
		}

		// ----- Filter -----
		int currentFilter = static_cast<int>(s_Settings.filterMode); // FilterMode enum
		if (ImGui::Combo("Filter Mode", &currentFilter, TextureFilterMode, IM_ARRAYSIZE(TextureFilterMode))) {
			s_Settings.filterMode = static_cast<TexFilterMode>(currentFilter);
			s_dirty = true;
		}

		// ----- Wrap -----
		int currentWrap = static_cast<int>(s_Settings.wrapMode); // WrapMode enum
		if (ImGui::Combo("Wrap Mode", &currentWrap, TextureWrapMode, IM_ARRAYSIZE(TextureWrapMode))) {
			s_Settings.wrapMode = static_cast<TexWrapMode>(currentWrap);
			s_dirty = true;
		}

		ImGui::BeginDisabled(!s_dirty);
		if (ImGui::Button("Apply")) {
			if (!AssetManager::GetInstance().SaveTextureImportSettings(metaPath, s_Settings)) {
				SPD_WARNING("Failed to save texture import settings for: " << metaPath);
			}
			else {
				AssetManager::GetInstance().ReimportAsset(metaPath);
			}

			s_dirty = false;
		}
		ImGui::EndDisabled();
	}

	void InspectorPanel::RenderModelImportSettings(const std::string& metaPath) {
		//static std::string s_LastMetaPath;
		//static ModelImportSettings s_Settings{};
		//static bool s_Loaded = false;

		ModelImportSettings settings{};
		if (!LoadModelImportSettings(metaPath, settings)) {
			ImGui::TextUnformatted("Failed to load model import settings.");
			return;
		}

		static int s_CurrentImportTab = 0;

		const char* tabNames[] = { "Model", "Rig", "Animation", "Materials" };
		constexpr int tabCount = IM_ARRAYSIZE(tabNames);

		ImGuiStyle& style = ImGui::GetStyle();
		float fullWidth = ImGui::GetContentRegionAvail().x;

		float totalButtonsWidth = 0.0f;
		for (int i = 0; i < tabCount; ++i) {
			ImVec2 textSize = ImGui::CalcTextSize(tabNames[i]);
			float btnWidth = textSize.x + style.FramePadding.x * 2.0f;
			totalButtonsWidth += btnWidth;
			if (i + 1 < tabCount)
				totalButtonsWidth += style.ItemInnerSpacing.x;
		}

		float cursorX = (fullWidth - totalButtonsWidth) * 0.5f;
		if (cursorX < 0.0f) cursorX = 0.0f;
		ImGui::SetCursorPosX(ImGui::GetCursorPosX() + cursorX);

		for (int i = 0; i < tabCount; ++i) {
			bool isActive = (s_CurrentImportTab == i);

			if (isActive)
				ImGui::PushStyleColor(ImGuiCol_Button, style.Colors[ImGuiCol_ButtonActive]);
			else
				ImGui::PushStyleColor(ImGuiCol_Button, style.Colors[ImGuiCol_Button]);

			if (ImGui::Button(tabNames[i]))
				s_CurrentImportTab = i;

			ImGui::PopStyleColor();

			if (i + 1 < tabCount)
				ImGui::SameLine();
		}

		ImGui::Separator();

		auto DrawComboEnum = [](const char* label, int& currentIndex, const char* const* names, int count) {
			ImGui::Combo(label, &currentIndex, names, count);
			};

		switch (s_CurrentImportTab) {
		case 0:
		{
			if (ImGui::CollapsingHeader("Scene", ImGuiTreeNodeFlags_DefaultOpen)) {
				// SceneImportSettings
				ImGui::DragFloat("Scale Factor", &settings.scene.scaleFactor, 0.01f, 0.0001f, 100.0f);
				Editor::DrawCheckbox("Convert Units", settings.scene.convertUnits);
				Editor::DrawCheckbox("Import Blend Shapes", settings.scene.importBlendShapes);
				Editor::DrawCheckbox("Import Cameras", settings.scene.importCameras);
				Editor::DrawCheckbox("Import Lights", settings.scene.importLights);
				Editor::DrawCheckbox("Preserve Hierarchy", settings.scene.preserveHierarchy);
			}

			if (ImGui::CollapsingHeader("Mesh", ImGuiTreeNodeFlags_DefaultOpen)) {
				// MeshImportSettings
				const char* MeshOptNames[] = { "None", "Everything", "Polygon Order", "Vertex Order" };
				int meshOptIndex = static_cast<int>(settings.mesh.meshOptimizationMode);
				DrawComboEnum("Mesh Optimization", meshOptIndex, MeshOptNames, IM_ARRAYSIZE(MeshOptNames));
				settings.mesh.meshOptimizationMode =
					static_cast<MeshImportSettings::MeshOptimizationMode>(meshOptIndex);

				Editor::DrawCheckbox("Generate Colliders", settings.mesh.generateColliders);
				Editor::DrawCheckbox("Generate Mesh LODs", settings.mesh.generateMeshLODs);

				Editor::DrawCheckbox("Keep Quads", settings.mesh.keepQuads);
				Editor::DrawCheckbox("Weld Vertices", settings.mesh.weldVertices);

				const char* IndexFormatNames[] = { "Auto", "UInt16", "UInt32" };
				int indexFmtIndex = static_cast<int>(settings.mesh.indexFormat);
				DrawComboEnum("Index Format", indexFmtIndex, IndexFormatNames, IM_ARRAYSIZE(IndexFormatNames));
				settings.mesh.indexFormat = static_cast<MeshImportSettings::IndexFormat>(indexFmtIndex);

				const char* NormalModeNames[] = { "Import", "Calculate", "None" };
				int normalIndex = static_cast<int>(settings.mesh.normalMode);
				DrawComboEnum("Normals", normalIndex, NormalModeNames, IM_ARRAYSIZE(NormalModeNames));
				settings.mesh.normalMode = static_cast<MeshImportSettings::NormalMode>(normalIndex);

				ImGui::DragFloat("Smoothing Angle", &settings.mesh.smoothingAngle, 1.0f, 0.0f, 180.0f);

				const char* TangentModeNames[] = { "Import", "Calculate (MikkTSpace)", "None" };
				int tangentIndex = static_cast<int>(settings.mesh.tangentMode);
				DrawComboEnum("Tangents", tangentIndex, TangentModeNames, IM_ARRAYSIZE(TangentModeNames));
				settings.mesh.tangentMode = static_cast<MeshImportSettings::TangentMode>(tangentIndex);

				Editor::DrawCheckbox("Swap UVs", settings.mesh.swapUVs);
			}
			break;
		}

		case 1: // ----- RIG -----
		{
			if (ImGui::CollapsingHeader("Rig", ImGuiTreeNodeFlags_DefaultOpen)) {
				const char* AnimTypeNames[] = { "None", "Generic", "Humanoid" };
				int animTypeIndex = static_cast<int>(settings.rig.animationType);
				DrawComboEnum("Animation Type", animTypeIndex, AnimTypeNames, IM_ARRAYSIZE(AnimTypeNames));
				settings.rig.animationType = static_cast<RigImportSettings::AnimationType>(animTypeIndex);

				Editor::DrawCheckbox("Strip Unused Bones", settings.rig.stripBones);
			}
			break;
		}

		case 2: // ----- ANIMATION -----
		{
			if (ImGui::CollapsingHeader("Animation", ImGuiTreeNodeFlags_DefaultOpen)) {
				Editor::DrawCheckbox("Import Animations", settings.animation.importAnimations);
				Editor::DrawCheckbox("Import Constraints", settings.animation.importConstraints);
				Editor::DrawCheckbox("Import Animated Custom Properties", settings.animation.importAnimatedCustomProperties);
				Editor::DrawCheckbox("Auto Split Clips", settings.animation.autoSplitClips);

				ImGui::DragFloat("Sample Rate", &settings.animation.sampleRate, 1.0f, 0.0f, 480.0f, "%.1f");

				Editor::DrawCheckbox("Import Root Motion", settings.animation.importRootMotion);
				Editor::DrawCheckbox("Lock Root Position XZ", settings.animation.lockRootPositionXZ);
				Editor::DrawCheckbox("Lock Root Rotation Y", settings.animation.lockRootRotationY);
			}
			break;
		}

		case 3: // ----- MATERIALS -----
		{
			if (ImGui::CollapsingHeader("Materials", ImGuiTreeNodeFlags_DefaultOpen)) {
				Editor::DrawCheckbox("Import Materials", settings.material.importMaterials);
				Editor::DrawCheckbox("Try Reuse Existing Materials", settings.material.tryReuseExistingMaterials);

				const char* MaterialModeNames[] = { "Per Submesh", "Per Mesh", "Per File" };
				int matModeIndex = static_cast<int>(settings.material.creationMode);
				DrawComboEnum("Material Creation", matModeIndex, MaterialModeNames, IM_ARRAYSIZE(MaterialModeNames));
				settings.material.creationMode =
					static_cast<MaterialImportSettings::MaterialCreationMode>(matModeIndex);
			}
			break;
		}
		}

		ImGui::Spacing();
		ImGui::Separator();

		if (ImGui::Button("Apply")) {
			//SaveModelImportSettings(metaPath, settings);
		}
	}
}
