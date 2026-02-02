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
#include <Scripting/ScriptingEngine.hpp>
#include <ECS/Components/EntityMeta.hpp>
#include <ECS/Components/UIRectTransform.hpp>
#include <ECS/Components/UICanvas.hpp>
#include <ECS/Components/UIImage.hpp>
#include <ECS/Components/Animator.hpp>
#include <ECS/Components/Camera.hpp>
#include <ECS/Components/PrefabInstance.hpp>
#include <ECS/Components/CharacterController.hpp>
#include <Core/Reflection.hpp>
#include <Math/Vec3.hpp>
#include "Math/Vec4.hpp"
#include "../EditorScene.hpp"
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
#include "../Serialization/JSONReflection.hpp"
#include <rapidjson/istreamwrapper.h>
#include "../Command/EditorCommands.hpp"
#include "../Layers/LayerDatabase.hpp"
#include "../Layers/LayerModal.hpp"
#include <Events/EventBus.hpp>
#include "../EditorEvents.hpp"

bool openLayerSettings = false;

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
		} else if constexpr (std::is_same_v<T, int>) {
			return ImGui::DragInt(desc.name.data(), &value);
		} else if constexpr (std::is_same_v<T, float>) {
			return ImGui::DragFloat(desc.name.data(), &value, 0.1f);
		} else if constexpr (std::is_same_v<T, NE::Math::Vec3>) {
			ImGui::BeginGroup();
			bool changed = Editor::DrawVec3Control(desc.name.data(), value, 0.0f, 75.0f);
			ImGui::EndGroup();
			return changed;
		} else if constexpr (std::is_same_v<T, std::string>) {
			// String support added here -> check w irwen
			char buffer[256];
			strncpy_s(buffer, sizeof(buffer), value.c_str(), sizeof(buffer));
			buffer[sizeof(buffer) - 1] = '\0';

			if (ImGui::InputText(desc.name.data(), buffer, sizeof(buffer))) {
				value = buffer;
				return true;
			}
			return false;
		} else {
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
			} else if constexpr (std::is_same_v<Owner, NE::ECS::Component::Collider>) {
				return NE::ECS::Command::GetEntityCollider(e);
			} else if constexpr (std::is_same_v<Owner, NE::ECS::Component::Rigidbody>) {
				return NE::ECS::Command::GetEntityRigidbody(e);
			} else if constexpr (std::is_same_v<Owner, NE::ECS::Component::Renderer>) {
				return NE::ECS::Command::GetEntityRenderer(e);
			} else if constexpr (std::is_same_v<Owner, NE::ECS::Component::Light>) {
				return NE::ECS::Command::GetEntityLight(e);
			} else if constexpr (std::is_same_v<Owner, NE::ECS::Component::CharacterController>) {
				return NE::ECS::Command::GetCharacterController(e);
			} else if constexpr (std::is_same_v<Owner, NE::ECS::Component::Animator>) {
				return NE::ECS::Command::GetEntityAnimator(e);
			} else {
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

	// just for collider component only due to std::variant usage
	template <typename Alt, typename FieldT>
	static void SubmitSetColliderVariantFieldCommand(
		uint32_t entity,
		std::string_view fieldName,
		FieldT Alt::* member,
		const FieldT& before,
		const FieldT& after)
	{
		using Cmd = Editor::SetColliderVariantFieldCommand<Alt, FieldT>;

		auto cmd = std::make_unique<Cmd>(
			entity,
			std::string(fieldName),
			member,
			before,
			after
		);

		Editor::CommandHistory::GetInstance().ExecuteCommand(std::move(cmd));
	}

	template <typename Alt, typename FieldT>
	static void SubmitSetLightVariantFieldCommand(
		uint32_t entity,
		std::string_view fieldName,
		FieldT Alt::* member,
		const FieldT& before,
		const FieldT& after)
	{
		using Cmd = Editor::SetLightVariantFieldCommand<Alt, FieldT>;

		auto cmd = std::make_unique<Cmd>(
			entity,
			std::string(fieldName),
			member,
			before,
			after
		);

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

	// temp
	// Returns true if layer changed
	bool DrawLayerCombo_UnityStyle(
		const char* label,
		NE::Core::LayerID& ioLayer,
		Editor::Layers::LayerDatabase & db,
		float comboWidth = 180.0f)
	{
		bool changed = false;

		// Preview text
		std::string_view currentName = db.GetName(ioLayer);
		const char* preview = (!currentName.empty()) ? currentName.data() : "<Unassigned>";

		ImGui::PushID(label);
		ImGui::PushItemWidth(comboWidth);
		if (Editor::BeginPillCombo(label, preview)) {
			db.ForEachUsed([&](NE::Core::LayerID id, std::string_view name) {
				const bool selected = (id == ioLayer);

				std::string label = std::to_string((int)id);
				label += ": ";
				label += name;

				if (ImGui::Selectable(label.c_str(), selected)) {
					ioLayer = id;
					changed = true;
				}
				if (selected) ImGui::SetItemDefaultFocus();
				});

			ImGui::Separator();

			if (ImGui::Selectable("Add Layer...")) {
				openLayerSettings = true;
			}

			Editor::EndPillCombo();
		}

		ImGui::PopItemWidth();
		ImGui::PopID();
		return changed;
	}

	bool DrawComponentHeaderWithMenu(
		const char* label,
		bool defaultOpen,
		bool* outCopy,
		bool* outDelete
	) {
		if (outCopy)   *outCopy = false;
		if (outDelete) *outDelete = false;

		ImGui::PushID(label);

		const ImGuiTreeNodeFlags baseFlags =
			ImGuiTreeNodeFlags_Framed |
			ImGuiTreeNodeFlags_SpanAvailWidth |
			ImGuiTreeNodeFlags_AllowItemOverlap |
			ImGuiTreeNodeFlags_FramePadding;

		ImGuiTreeNodeFlags flags = baseFlags;
		if (defaultOpen) flags |= ImGuiTreeNodeFlags_DefaultOpen;

		const bool open = ImGui::TreeNodeEx("##component_node", flags, "%s", label);

		const float lineRight = ImGui::GetItemRectMax().x;
		const float lineLeft = ImGui::GetItemRectMin().x;

		const float btnW = ImGui::GetFrameHeight();
		const float padR = ImGui::GetStyle().FramePadding.x;
		float btnX = lineRight - btnW - padR;

		if (btnX < lineLeft) btnX = lineLeft;

		ImGui::SameLine();
		ImGui::SetCursorScreenPos(ImVec2(btnX, ImGui::GetItemRectMin().y + 1.0f));

		if (ImGui::SmallButton("*")) {
			ImGui::OpenPopup("ComponentMenu");
		}

		if (ImGui::BeginPopup("ComponentMenu")) {
			if (ImGui::MenuItem("Copy Component")) {
				if (outCopy) *outCopy = true;
			}
			if (ImGui::MenuItem("Delete")) {
				if (outDelete) *outDelete = true;
			}
			ImGui::EndPopup();
		}

		ImGui::PopID();
		return open;
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

	uint32_t FNV1a32(std::string_view s) {
		uint32_t h = 2166136261u;
		for (unsigned char c : s) { h ^= c; h *= 16777619u; }
		return h;
	}

	uint32_t MakeFieldId(const char* componentName, std::string_view fieldName) {
		std::string full;
		full.reserve(std::strlen(componentName) + 1 + fieldName.size());
		full.append(componentName);
		full.push_back('.');
		full.append(fieldName.data(), fieldName.size());
		return FNV1a32(full);
	}
}

namespace Editor {
	static std::unordered_map<FieldKey,
		std::unique_ptr<ICommand>,
		FieldKeyHash> g_activeCommands;

	InspectorPanel::InspectorPanel() {

		m_drawers = {
			{ NE::ECS::Query::GetEntityMetaComponentType(),			"EntityMeta",			&InspectorPanel::DrawEntityMetaComponent			},
			{ NE::ECS::Query::GetPrefabInstanceComponentType(),		"PrefabInstance",		&InspectorPanel::DrawPrefabInstanceComponent		},
			{ NE::ECS::Query::GetTransformComponentType(),			"Transform",			&InspectorPanel::DrawTransformComponent				},
			{ NE::ECS::Query::GetRendererComponentType(),			"Renderer",				&InspectorPanel::DrawRendererComponent				},
			{ NE::ECS::Query::GetLightComponentType(),				"Light",				&InspectorPanel::DrawLightComponent					},
			{ NE::ECS::Query::GetColliderComponentType(),			"Collider",				&InspectorPanel::DrawColliderComponent				},
			{ NE::ECS::Query::GetRigidbodyComponentType(),			"Rigidbody",			&InspectorPanel::DrawRigidbodyComponent				},
			{ NE::ECS::Query::GetCharacterControllerComponentType(),"CharacterController",	&InspectorPanel::DrawCharacterControllerComponent	},
			{ NE::ECS::Query::GetAudioSourceComponentType(),		"Audio Source",			&InspectorPanel::DrawAudioSourceComponent			},
			{ NE::ECS::Query::GetEntityCameraComponentType(),		"Camera",				&InspectorPanel::DrawCameraComponent				},
			{ NE::ECS::Query::GetEntityAnimatorComponentType(),		"Animator",				&InspectorPanel::DrawAnimatorComponent				},
			{ NE::ECS::Query::GetUIRectTransformComponentType(),	"Rect Transform",		&InspectorPanel::DrawRectTransformComponent			},
			{ NE::ECS::Query::GetUICanvasComponentType(),			"Canvas",				&InspectorPanel::DrawCanvasComponent				},
			{ NE::ECS::Query::GetUIImageComponentType(),			"Image",				&InspectorPanel::DrawImageComponent					},
			{ NE::ECS::Query::GetScriptComponentType(),				"Script",				&InspectorPanel::DrawScriptComponent				}
		};
	}

	void InspectorPanel::OnImGuiRender() {
		ImGui::Begin("Inspector", nullptr);

		if (EditorScene::s_selection.GetLastClicked() != NE::ECS::NO_ENTITY) {
			uint32_t entity = EditorScene::s_selection.GetLastClicked();

			NE::ECS::Signature sig(NE::ECS::Query::GetEntitySignature(entity));
			for (const Drawer& d : m_drawers) {
				if (!sig.test(d.id)) continue;
				(this->*d.draw)(entity);
			}

			ImGui::Separator();

			float windowWidth = ImGui::GetWindowSize().x;
			float buttonWidth = ImGui::CalcTextSize("Add Component").x + ImGui::GetStyle().FramePadding.x * 2.0f;
			float centeredPosX = (windowWidth - buttonWidth) * 0.5f;

			ImGui::SetCursorPosX(centeredPosX);

			if (ImGui::Button("Add Component")) {
				ImGui::OpenPopup("ComponentList");
			}

			if (ImGui::BeginPopup("ComponentList")) { // automate this next time with a registry
				if (ImGui::MenuItem("Renderer")) {
					NE::ECS::Command::AddRendererComponent(EditorScene::s_selection.GetLastClicked());
				}
				if (ImGui::MenuItem("Rigidbody")) {
					NE::ECS::Command::AddColliderComponent(EditorScene::s_selection.GetLastClicked());
					NE::ECS::Command::AddRigidbodyComponent(EditorScene::s_selection.GetLastClicked());
				}
				if (ImGui::MenuItem("Character Controller")) {
					NE::ECS::Command::AddCharacterControllerComponent(EditorScene::s_selection.GetLastClicked(), NE::ECS::Component::CharacterController{});
				}
				if (ImGui::MenuItem("Collider")) {
					NE::ECS::Command::AddColliderComponent(EditorScene::s_selection.GetLastClicked());
				}
				if (ImGui::MenuItem("Light")) {
					NE::ECS::Command::AddLightComponent(EditorScene::s_selection.GetLastClicked());
				}
				if (ImGui::MenuItem("AudioSource")) {
					NE::ECS::Command::AddAudioSourceComponent(EditorScene::s_selection.GetLastClicked());
				}
				if (ImGui::MenuItem("Script")) {
					NE::ECS::Command::AddScriptComponent(EditorScene::s_selection.GetLastClicked());
				}
				if (ImGui::MenuItem("Camera")) {
					NE::ECS::Command::AddCameraComponent(EditorScene::s_selection.GetLastClicked());
				}
				if (ImGui::MenuItem("Animator")) {
					NE::ECS::Command::AddAnimatorComponent(EditorScene::s_selection.GetLastClicked());
				}

				ImGui::EndPopup();
			}
		} else if (EditorScene::selectedAsset != "") {
			std::filesystem::path assetPath = EditorScene::selectedAsset;

			if (assetPath.extension() == ".png" || assetPath.extension() == ".jpg") {
				if (!m_textureEditor || m_lastPath != assetPath.string()) {
					m_textureEditor = std::make_unique<TextureSettingsEditor>();
					if (m_textureEditor->LoadTextureSettings(assetPath.string(), Assets::AssetManager::GetInstance().RetrieveUUID(assetPath.string())))
						m_lastPath = assetPath.string();
					else {
						ImGui::TextUnformatted("Failed to load texture import settings.");
						m_textureEditor.reset();
					}
				}

				if (m_textureEditor)
					m_textureEditor->RenderSettings();
			} else if (assetPath.extension() == ".obj" || assetPath.extension() == ".fbx") {
				if (!m_modelEditor || m_lastPath != assetPath.string()) {
					m_modelEditor = std::make_unique<ModelSettingsEditor>();
					if (m_modelEditor->LoadModelSettings(assetPath.string(), Assets::AssetManager::GetInstance().RetrieveUUID(assetPath.string())))
						m_lastPath = assetPath.string();
					else {
						ImGui::TextUnformatted("Failed to load model import settings.");
						m_modelEditor.reset();
					}
				}

				if (m_modelEditor)
					m_modelEditor->RenderSettings();
			} else if (assetPath.extension() == ".nanomat") {
				if (!m_materialEditor || m_lastPath != assetPath.string()) {
					m_materialEditor = std::make_unique<MaterialEditor>();
					if (m_materialEditor->LoadMaterial(assetPath.string(), Assets::AssetManager::GetInstance().RetrieveUUID(assetPath.string())))
						m_lastPath = assetPath.string();
					else
						m_materialEditor.reset();
				}

				if (m_materialEditor)
					m_materialEditor->RenderSettings();
			}
		}

		auto r = Layers::DrawLayerModal(EditorScene::layerDatabase, "LayerSettings");
		if (r.applied) {
			// Save to disk, bake runtime collision table, etc.
		}
		ImGui::End();

	}

	void InspectorPanel::DrawEntityMetaComponent(uint32_t entity) {
		using Owner = NE::ECS::Component::EntityMeta;
		using FieldT = std::string;

		auto& metaRO = NE::ECS::Command::GetEntityMeta(entity);

		bool isActiveValue = metaRO.isActive;
		if (DrawCheckbox("##isActive", isActiveValue)) {
			NE::ECS::Command::SetActive(entity, isActiveValue);
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
					} else {
						// There *was* a change: commit it
						Editor::CommandHistory::GetInstance()
							.ExecuteCommand(std::move(it->second));
						g_activeCommands.erase(it);
					}
				} else {
					// Fallback: if not a SetFieldCommand, just execute & erase
					Editor::CommandHistory::GetInstance()
						.ExecuteCommand(std::move(it->second));
					g_activeCommands.erase(it);
				}
			}
		}

		const float comboWidth = 140.0f;

		const char* previewTag = "Under Dev";

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

		// Draw Layer
		NE::Core::LayerID layer = static_cast<int>(NE::ECS::Query::GetLayer(entity));
		NE::Core::LayerID before = layer;

		if (DrawLayerCombo_UnityStyle("Layer", layer, EditorScene::layerDatabase, comboWidth)) {
			auto cmd = std::make_unique<Editor::SetEntityLayerCommand>(
				entity,
				static_cast<uint8_t>(before),
				static_cast<uint8_t>(layer)
			);
			Editor::CommandHistory::GetInstance().ExecuteCommand(std::move(cmd));
		}

		if (openLayerSettings) {
			ImGui::OpenPopup("LayerSettings");
			openLayerSettings = false;
		}

		//if (metaRO.prefabID != "") {
		//	ImGui::Text("Prefab");
		//	ImGui::SameLine();
		//	ImGui::Text(metaRO.prefabID.c_str());
		//}
	}

	void InspectorPanel::DrawPrefabInstanceComponent(uint32_t entity) {
		auto& comp = NE::ECS::Query::GetPrefabInstance(entity);

		bool openPopup = false;
		DrawAssetField("Prefab", Assets::AssetManager::GetInstance().RetrieveFilename(comp.prefabUUID), &openPopup);

		ImGui::Button("Overrides");
		ImGui::SameLine();
		ImGui::Button("Select");
		ImGui::SameLine();
		ImGui::Button("Open");
		//if (openPopup) {
		//	ImGui::OpenPopup("AssetPicker_Model");
		//}
	}

	void InspectorPanel::DrawTransformComponent(uint32_t entity) {
		auto& comp = NE::ECS::Query::GetEntityTransform(entity);
		ImGui::SeparatorText("Transform");

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
						} else {
							Editor::CommandHistory::GetInstance()
								.ExecuteCommand(std::move(it->second));
							g_activeCommands.erase(it);


							const uint32_t compTypeId = NE::ECS::Query::GetTransformComponentType();
							const uint32_t fieldId = MakeFieldId("Transform", desc.name);
							NANOEngine::Events::EventBus::Get().Dispatch(
								NANOEngine::Events::EventDomain::Editor,
								Events::AutoKeyRecordEvent{ compTypeId, fieldId }
							);
						}
					}
				}
			});
	}

	void InspectorPanel::DrawRendererComponent(uint32_t entity) {
		auto& comp = NE::ECS::Query::GetEntityRenderer(entity);

		bool copyComp = false;
		bool deleteComp = false;

		const bool open = DrawComponentHeaderWithMenu(
			"Renderer",
			true,
			&copyComp,
			&deleteComp
		);

		if (!open)
			return;

		// Model field
		bool openModelPopup = false;
		DrawAssetField(
			"Model",
			Assets::AssetManager::GetInstance().RetrieveFilename(comp.modelUUID),
			true,
			&openModelPopup,
			ImVec2(0, 0),
			28.0f,
			"ASSET_SUBMESH",
			[&](const ImGuiPayload* p) {
				std::string dropped((const char*)p->Data, p->DataSize ? p->DataSize - 1 : 0);
				auto it = std::find(dropped.begin(), dropped.end(), ':');
				if (it != dropped.end()) {
					std::string meshPath(dropped.begin(), it);
					std::string submeshName(it + 1, dropped.end());
					auto uuid = Assets::AssetManager::GetInstance().RetrieveUUID(meshPath);
					NE::Renderer::Command::AssignModel(entity, uuid, std::stoi(submeshName));
				}
			}
		);

		if (openModelPopup) ImGui::OpenPopup("AssetPicker_Model");
		
		ImGui::SetNextWindowSizeConstraints(
			ImVec2(0.f, 0.f),
			ImVec2(350.f, 500.f)
		);

		static std::string searchQuery;
		if (ImGui::BeginPopup("AssetPicker_Model")) {
			ImGui::Text("Select a Model");
			ImGui::Separator();
			auto& modelList = Assets::AssetManager::GetInstance().GetAssetsOfType(Assets::AssetType::Model);

			if (ImSearch::BeginSearch()) {
				ImSearch::SearchBar();
				for (const auto& [modelName, uuid] : modelList) {
					ImSearch::SearchableItem(modelName.c_str(), [&, modelName](const char*) {
						if (ImGui::Selectable(modelName.c_str())) {
							NE::Renderer::Command::AssignModel(entity, uuid, 0);
							ImGui::CloseCurrentPopup();
						}
						});
				}

				ImSearch::EndSearch();
			}
			ImGui::EndPopup();
		}

		bool openMaterialPopup = false;
		DrawAssetField(
			"Material",
			Assets::AssetManager::GetInstance().RetrieveFilename(comp.materialUUID),
			true,
			&openMaterialPopup,
			ImVec2(0, 0),
			28.0f,
			"MATERIAL_PATH",
			[&](const ImGuiPayload* p) {
				std::string dropped((const char*)p->Data, p->DataSize ? p->DataSize - 1 : 0);
				auto uuid = Assets::AssetManager::GetInstance().RetrieveUUID(dropped);
				NE::Renderer::Command::AssignMaterial(entity, uuid);
			}
		);

		if (openMaterialPopup) ImGui::OpenPopup("AssetPicker_Material");

		ImGui::SetNextWindowSizeConstraints(
			ImVec2(0.f, 0.f),
			ImVec2(350.f, 500.f)
		);

		if (ImGui::BeginPopup("AssetPicker_Material")) {
			ImGui::Text("Select a Material");
			ImGui::Separator();
			auto& materialList = Assets::AssetManager::GetInstance().GetAssetsOfType(Assets::AssetType::Material);

			if (ImSearch::BeginSearch()) {
				ImSearch::SearchBar();
				for (const auto& [materialName, uuid] : materialList) {
					ImSearch::SearchableItem(materialName.c_str(), [&, materialName](const char*) {
						if (ImGui::Selectable(materialName.c_str())) {
							NE::Renderer::Command::AssignMaterial(entity, uuid);
							ImGui::CloseCurrentPopup();
						}
						});
				}

				ImSearch::EndSearch();
			}
			ImGui::EndPopup();
		}

		static const char* ShadowCastModeNames[] = { "Off", "On", "TwoSided", "ShadowsOnly" };
		int currentCastMode = static_cast<int>(comp.shadowCastMode);
		auto& tempR = NE::ECS::Command::GetEntityRenderer(entity);
		if (ImGui::Combo("Shadow Cast Mode", &currentCastMode, ShadowCastModeNames, IM_ARRAYSIZE(ShadowCastModeNames))) {
			tempR.shadowCastMode = static_cast<NE::ECS::Component::Renderer::ShadowCastMode>(currentCastMode);
		}

		if (Editor::DrawCheckbox("Receive Shadows", tempR.receiveShadows)) {
		}
		//ImGui::InputInt("Submesh Index", &tempR.subMeshIndex);

		if (copyComp) {

		}
		if (deleteComp) {
			NE::ECS::Command::RemoveRendererComponent(entity);
		}

		ImGui::TreePop();
	}

	void InspectorPanel::DrawRigidbodyComponent(uint32_t entity) {
		auto& comp = NE::ECS::Command::GetEntityRigidbody(entity);

		bool copyComp = false;
		bool deleteComp = false;

		const bool open = DrawComponentHeaderWithMenu(
			"Rigidbody",
			true,
			&copyComp,
			&deleteComp
		);

		if (!open)
			return;

		NE::Core::ForEachFieldView<NE::ECS::Component::Rigidbody>(comp,
			[&](auto const& desc, auto const& currentValue) {
				using FieldT = std::decay_t<decltype(currentValue)>;

				FieldT edited = currentValue;

				if (DrawField(desc, edited)) {
					SubmitSetFieldCommand<NE::ECS::Component::Rigidbody, FieldT>(
						entity, desc, currentValue, edited
					);
				}
			});

		if (ImGui::TreeNode("Constraints")) {
			ImGui::TextUnformatted("Position");
			ImGui::SameLine();
			Editor::DrawCheckbox("X##Pos", comp.freezePosX);
			ImGui::SameLine();
			Editor::DrawCheckbox("Y##Pos", comp.freezePosY);
			ImGui::SameLine();
			Editor::DrawCheckbox("Z##Pos", comp.freezePosZ);

			ImGui::TextUnformatted("Rotation");
			ImGui::SameLine();
			Editor::DrawCheckbox("X##Rot", comp.freezeRotX);
			ImGui::SameLine();
			Editor::DrawCheckbox("Y##Rot", comp.freezeRotY);
			ImGui::SameLine();
			Editor::DrawCheckbox("Z##Rot", comp.freezeRotZ);

			ImGui::TreePop();
		}

		if (copyComp) {

		}
		if (deleteComp) {
			NE::ECS::Command::RemoveRigidbodyComponent(entity);
		}

		ImGui::TreePop();
	}

	void InspectorPanel::DrawColliderComponent(uint32_t entity) {
		using Collider = NE::ECS::Component::Collider;

		auto& comp = NE::ECS::Command::GetEntityCollider(entity);

		bool copyComp = false;
		bool deleteComp = false;

		const bool open = DrawComponentHeaderWithMenu(
			"Collider",
			true,
			&copyComp,
			&deleteComp
		);

		if (!open)
			return;

		static const char* ColliderTypeNames[] = { "Box", "Sphere", "Capsule", "Cylinder", "Mesh" };
		int currCollider = static_cast<int>(comp.type);

		if (DrawEnumPillCombo("Collider Type", currCollider, ColliderTypeNames, IM_ARRAYSIZE(ColliderTypeNames), 100.0f)) {
			auto newType =
				static_cast<Collider::ColliderType>(currCollider);

			if (newType != comp.type) {
				comp.type = newType;

				switch (newType) {
				case Collider::ColliderType::Box:
					comp.data.emplace<Collider::BoxColliderData>();
					break;
				case Collider::ColliderType::Sphere:
					comp.data.emplace<Collider::SphereColliderData>();
					break;
				case Collider::ColliderType::Capsule:
					comp.data.emplace<Collider::CapsuleColliderData>();
					break;
				case Collider::ColliderType::Cylinder:
					comp.data.emplace<Collider::CylinderColliderData>();
					break;
				case Collider::ColliderType::Mesh:
					comp.data.emplace<Collider::MeshColliderData>();
					break;
				}

				comp.isDirty = true;
			}
		}

		NE::Core::ForEachFieldView<Collider>(comp,
			[&](auto const& desc, auto const& currentValue) {
				using FieldT = std::decay_t<decltype(currentValue)>;

				FieldT edited = currentValue;

				if (DrawField(desc, edited)) {
					SubmitSetFieldCommand<Collider, FieldT>(
						entity, desc, currentValue, edited
					);
				}
			}
		);

		std::visit([&](auto& shape) {
			using Alt = std::decay_t<decltype(shape)>;

			NE::Core::ForEachFieldView<Alt>(shape, [&](auto const& desc, auto const& currentValue) {
				using FieldT = std::decay_t<decltype(currentValue)>;

				FieldT edited = currentValue;
				if (DrawField(desc, edited)) {
					SubmitSetColliderVariantFieldCommand<Alt, FieldT>(
						entity,
						desc.name,
						desc.member,
						currentValue,
						edited
					);
				}
				});

			}, comp.data
		);

		NE::Physics::Command::DrawSelectedCollider(entity);

		if (copyComp) {

		}
		if (deleteComp) {
			NE::ECS::Command::RemoveColliderComponent(entity);
		}

		ImGui::TreePop();
	}

	void InspectorPanel::DrawLightComponent(uint32_t entity) {
		using Light = NE::ECS::Component::Light;

		auto& comp = NE::ECS::Command::GetEntityLight(entity);

		bool copyComp = false;
		bool deleteComp = false;

		const bool open = DrawComponentHeaderWithMenu(
			"Light",
			true,
			&copyComp,
			&deleteComp
		);

		if (!open)
			return;

		static const char* LightTypeNames[] = { "Directional", "Point", "Spot", "Area" };
		int currentType = static_cast<int>(comp.type);

		if (DrawEnumPillCombo("Type", currentType, LightTypeNames, IM_ARRAYSIZE(LightTypeNames), 300.0f)) {
			auto newType =
				static_cast<Light::Type>(currentType);

			if (newType != comp.type) {
				comp.type = newType;

				switch (newType) {
				case Light::Type::Directional:
					comp.data.emplace<Light::DirectionalLightData>();
					break;
				case Light::Type::Point:
					comp.data.emplace<Light::PointLightData>();
					break;
				case Light::Type::Spot:
					comp.data.emplace<Light::SpotLightData>();
					break;
				case Light::Type::Area:
					comp.data.emplace<Light::AreaLightData>();
					break;
				}

				comp.isDirty = true;
			}
		}

		static const char* shadowUpdateModeNames[] = { "NoneUpdate", "Realtime", "StaticBake" };
		int shadowUpdateMode = static_cast<int>(comp.shadowUpdateMode);

		if (DrawEnumPillCombo("Shadow Update Mode", shadowUpdateMode, shadowUpdateModeNames, IM_ARRAYSIZE(shadowUpdateModeNames), 300.0f)) {
			comp.shadowUpdateMode =
				static_cast<Light::ShadowUpdateMode>(shadowUpdateMode);
		}

		if (shadowUpdateMode != 0) {
			static const char* shadowTypeNames[] = { "None", "Hard", "Soft" };
			int shadowType = static_cast<int>(comp.shadowType);

			if (DrawEnumPillCombo("Shadow Type", shadowType, shadowTypeNames, IM_ARRAYSIZE(shadowTypeNames), 300.0f)) {
				comp.shadowType =
					static_cast<Light::ShadowType>(shadowType);
			}

		}

		std::visit([&](auto& shape) {
			using Alt = std::decay_t<decltype(shape)>;

			NE::Core::ForEachFieldView<Alt>(shape, [&](auto const& desc, auto const& currentValue) {
				using FieldT = std::decay_t<decltype(currentValue)>;

				FieldT edited = currentValue;
				if (DrawField(desc, edited)) {
					SubmitSetLightVariantFieldCommand<Alt, FieldT>(
						entity,
						desc.name,
						desc.member,
						currentValue,
						edited
					);
				}
				});

			}, comp.data
		);

		NE::Core::ForEachFieldView<Light>(comp,
			[&](auto const& desc, auto const& currentValue) {
				using FieldT = std::decay_t<decltype(currentValue)>;

				FieldT edited = currentValue;

				if (DrawField(desc, edited)) {
					SubmitSetFieldCommand<Light, FieldT>(
						entity, desc, currentValue, edited
					);
				}
			}
		);


		if (copyComp) {

		}
		if (deleteComp) {
			NE::ECS::Command::RemoveLightComponent(entity);
		}

		ImGui::TreePop();
	}

	void InspectorPanel::DrawAudioSourceComponent(uint32_t entity) {
		auto& comp = NE::ECS::Query::GetEntityAudioSource(entity);
		ImGui::SeparatorText("AudioSource");

		bool openPopup = false;
		DrawAssetField("Audio", comp.modelPath.string(), &openPopup);
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

	void InspectorPanel::DrawScriptComponent(uint32_t entity) {
		auto& comp = NE::ECS::Command::GetEntityScript(entity);
		ImGui::SeparatorText("Scripts");

		// Get all script instances for this entity
		const auto* scriptInstances = NE::Scripting::ScriptingEngine::GetInstance().GetScriptInstances(entity);

		// Display list of scripts
		if (!comp.ScriptNames.empty()) {
			// Build script list string for display
			std::string scriptList;
			for (size_t i = 0; i < comp.ScriptNames.size(); ++i) {
				if (i > 0) scriptList += ", ";
				scriptList += comp.ScriptNames[i];
			}
			ImGui::Text("Scripts: %s", scriptList.c_str());
		} else {
			ImGui::Text("Scripts: None");
		}

		// Script selection dropdown (add new script)
		ImGui::Text("Add Script:");
		if (ImGui::BeginCombo("##ScriptType", "Add Script...")) {
			// List all registered scripts
			auto scriptNames = NE::ECS::Command::GetRegisteredScriptNames();
			for (const auto& scriptName : scriptNames) {
				// Check if script is already attached
				bool alreadyAttached = false;
				for (const auto& attachedName : comp.ScriptNames) {
					if (attachedName == scriptName) {
						alreadyAttached = true;
						break;
					}
				}

				if (alreadyAttached) {
					ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
					ImGui::Selectable(scriptName.c_str(), false, ImGuiSelectableFlags_Disabled);
					ImGui::PopStyleColor();
				}
				else if (ImGui::Selectable(scriptName.c_str())) {
					// Add this script to the list
					NE::ECS::Command::AddEntityScript(entity, scriptName);
				}
			}
			ImGui::EndCombo();
		}

		ImGui::SameLine();
		if (ImGui::Button("Clear All") && !comp.ScriptNames.empty()) {
			NE::ECS::Command::RemoveEntityScript(entity);
		}

		// Display script status and fields for each script
		if (!comp.ScriptNames.empty() && scriptInstances && !scriptInstances->empty()) {
			ImGui::Separator();

			for (size_t scriptIdx = 0; scriptIdx < comp.ScriptNames.size() && scriptIdx < scriptInstances->size(); ++scriptIdx) {
				const std::string& scriptName = comp.ScriptNames[scriptIdx];
				NE::Scripting::IScript* scriptInstance = (*scriptInstances)[scriptIdx];

				// Helper lambda to update field value in both script instance and serialized fields
				auto UpdateFieldValue = [&](const std::string& fieldName, const std::string& value) -> bool {
					// Update the script instance
					bool success = scriptInstance->SetFieldValueFromString(fieldName, value);
					if (success) {
						// Also update the component's serialized fields for persistence
						// IMPORTANT: Read back the actual serialized value (may be LUID, not entity ID)
						std::string key = NE::ECS::Component::NativeScript::GetFieldKey(scriptName, fieldName);
						std::string actualValue = scriptInstance->GetFieldValueAsString(fieldName);
						comp.SerializedFields[key] = actualValue;

						// Mark scene as dirty so save is enabled
						EditorScene::isDirty = true;
					}
					return success;
				};

				// Helper lambda to sync array field after modifications (add/remove/element change)
				auto SyncArrayField = [&](const std::string& fieldName) {
					std::string key = NE::ECS::Component::NativeScript::GetFieldKey(scriptName, fieldName);
					std::string currentValue = scriptInstance->GetFieldValueAsString(fieldName);
					comp.SerializedFields[key] = currentValue;
				};

				// Create a unique ID for this script's header
				std::string headerLabel = scriptName + "##script_" + std::to_string(scriptIdx);

				// Push ID for this script
				ImGui::PushID(static_cast<int>(scriptIdx));

				// Collapsing header for each script (default open)
				ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.2f, 0.3f, 0.4f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.3f, 0.4f, 0.5f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.4f, 0.5f, 0.6f, 1.0f));
				bool headerOpen = ImGui::CollapsingHeader(headerLabel.c_str(), ImGuiTreeNodeFlags_DefaultOpen);
				ImGui::PopStyleColor(3);

				if (headerOpen) {
					// Remove button at the top of the collapsible content
					if (ImGui::Button(("Remove##" + std::to_string(scriptIdx)).c_str())) {
						// Remove this specific script by index
						NE::ECS::Command::RemoveEntityScriptByIndex(entity, scriptIdx);
					}
					ImGui::Separator();

					if (!scriptInstance) {
					// Check if script is registered in the DLL
					bool isRegistered = NE::ECS::Command::IsScriptRegistered(scriptName);

					// More detailed error message based on registration status
					if (!isRegistered) {
						ImGui::TextColored(ImVec4(1.0f, 0.2f, 0.2f, 1.0f), "Status: Script not found in DLL");
						ImGui::TextWrapped("The script '%s' no longer exists in the compiled game code.", scriptName.c_str());
						ImGui::TextWrapped("It may have been deleted or renamed.");
					} else {
						ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "Status: Script not instantiated");
						ImGui::TextWrapped("The script instance failed to initialize. Try reloading the scene or removing and re-adding the script.");
					}
				}
				else {
					// Script enabled/disabled checkbox
					bool enabled = scriptInstance->IsEnabled();
					ImGui::PushID(("enabled_" + std::to_string(scriptIdx)).c_str());
					if (ImGui::Checkbox("Enabled", &enabled)) {
						scriptInstance->SetEnabled(enabled);
					}
					ImGui::PopID();

					ImGui::Text("Entity ID: %u", scriptInstance->GetEntity());

					// --- Scripting Fields UI ---
					auto fieldNames = scriptInstance->GetExposedFieldNames();
					if (!fieldNames.empty()) {
						ImGui::SeparatorText("Script Fields");

						// Group struct fields under collapsible headers
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
							std::string ftype = scriptInstance->GetFieldType(fname);
							std::string fval = scriptInstance->GetFieldValueAsString(fname);

							ImGui::PushID(fname.c_str());

							bool fieldChanged = false;

							if (ftype == "bool") {
								bool v = (fval == "1" || fval == "true");
								if (ImGui::Checkbox(fname.c_str(), &v)) {
									UpdateFieldValue(fname, v ? "1" : "0");
									fieldChanged = true;
								}
							}
							else if (ftype == "int") {
								int v = 0; if (!fval.empty()) v = std::stoi(fval);
								if (ImGui::DragInt(fname.c_str(), &v)) {
									UpdateFieldValue(fname, std::to_string(v));
									fieldChanged = true;
								}
							}
							else if (ftype == "float") {
								float v = 0.f; if (!fval.empty()) v = std::stof(fval);
								if (ImGui::DragFloat(fname.c_str(), &v, 0.01f)) {
									UpdateFieldValue(fname, std::to_string(v));
									fieldChanged = true;
								}
							}
							else if (ftype == "vec3") {
								NE::Math::Vec3 vv = Vec3FromString(fval);
								if (Editor::DrawVec3Control(fname.c_str(), vv, 0.0f, 100.0f)) {
									UpdateFieldValue(fname, Vec3ToString(vv));
									fieldChanged = true;
								}
							}
							else if (ftype == "enum") {
								// Enum dropdown support
								auto enumOptions = scriptInstance->GetEnumOptions(fname);
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
												UpdateFieldValue(fname, std::to_string(i));
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
							else if (ftype == "transformref" || ftype == "rigidbodyref" || ftype == "rendererref") {
								// Component reference field - display entity name and allow drag-drop
								std::string componentType = (ftype == "transformref") ? "Transform" :
								                             (ftype == "rigidbodyref") ? "Rigidbody" : "Renderer";
								std::string displayName = "None";
								uint32_t assignedEntityId = NE::ECS::NO_ENTITY;
								std::string noEntityStr = std::to_string(NE::ECS::NO_ENTITY);

								// Try to resolve the stored value (could be Entity ID or component LUID)
								if (!fval.empty() && fval != "0" && fval != noEntityStr) {
									try {
										uint64_t storedValue = std::stoull(fval);

										// Check if it looks like an Entity ID (small value)
										if (storedValue < static_cast<uint64_t>(NE::ECS::NO_ENTITY)) {
											assignedEntityId = static_cast<uint32_t>(storedValue);
											if (assignedEntityId != NE::ECS::NO_ENTITY) {
												const auto& entityMeta = NE::ECS::Query::GetEntityMeta(assignedEntityId);
												displayName = entityMeta.name.empty() ? ("Entity " + std::to_string(assignedEntityId)) : entityMeta.name;
											}
										}
										else {
											// Large value - likely a LUID, resolve it to entity
											assignedEntityId = NE::ECS::Query::ResolveComponentLuidToEntity(storedValue);
											if (assignedEntityId != NE::ECS::NO_ENTITY) {
												const auto& entityMeta = NE::ECS::Query::GetEntityMeta(assignedEntityId);
												displayName = entityMeta.name.empty() ? ("Entity " + std::to_string(assignedEntityId)) : entityMeta.name;
											}
											else {
												displayName = "[Invalid Reference]";
											}
										}
									}
									catch (...) {
										displayName = "[Error]";
									}
								}

								ImGui::Text("%s (%s)", fname.c_str(), componentType.c_str());
								ImGui::PushID((fname + "_compref").c_str());

								ImGui::Button(displayName.c_str(), ImVec2(200, 0));

								if (ImGui::BeginDragDropTarget()) {
									const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ENTITY_DRAG");
									if (payload && payload->DataSize == sizeof(uint32_t)) {
										uint32_t droppedEntity = *(const uint32_t*)payload->Data;

										// Validate that the entity has the required component
										bool hasComponent = false;
										if (ftype == "transformref") {
											// Transform is always present
											hasComponent = true;
										}
										else if (ftype == "rigidbodyref") {
											hasComponent = NE::ECS::Query::HasComponent<NE::ECS::Component::Rigidbody>(droppedEntity);
										}
										else if (ftype == "rendererref") {
											hasComponent = NE::ECS::Query::HasComponent<NE::ECS::Component::Renderer>(droppedEntity);
										}

										if (hasComponent) {
											// Pass entity ID - deserialization code will resolve to component LUID
											bool success = UpdateFieldValue(fname, std::to_string(droppedEntity));
											if (success) {
												fieldChanged = true;
											}
										}
										else {
											SPD_WARNING("Entity does not have required " << componentType << " component");
										}
									}
									ImGui::EndDragDropTarget();
								}

								ImGui::SameLine();
								if (ImGui::Button("X")) {
									UpdateFieldValue(fname, std::to_string(NE::ECS::NO_ENTITY));
									fieldChanged = true;
								}

								ImGui::PopID();
							}
							else if (ftype.starts_with("componentref:")) {
								// Generic component reference fallback
								ImGui::Text("%s (ComponentRef - unsupported type)", fname.c_str());
							}
							else if (ftype == "materialref") {
								// Material reference field
								std::string materialUUID = fval;
								std::string displayName = materialUUID.empty() ? "None" : Assets::AssetManager::GetInstance().RetrieveFilename(materialUUID);

								ImGui::Text("%s (Material)", fname.c_str());

								ImGui::PushID((fname + "_matref").c_str());

								ImGui::Button(displayName.c_str(), ImVec2(200, 0));

								if (ImGui::BeginDragDropTarget()) {
									const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("MATERIAL_PATH");
									if (payload && payload->DataSize > 0) {
										std::string droppedPath((const char*)payload->Data, payload->DataSize - 1);
										std::string droppedUUID = Assets::AssetManager::GetInstance().RetrieveUUID(droppedPath);

										if (!droppedUUID.empty()) {
											bool success = UpdateFieldValue(fname, droppedUUID);
											if (success) {
												fieldChanged = true;
											}
										}
									}
									ImGui::EndDragDropTarget();
							}

							ImGui::SameLine();
							if (ImGui::Button("X")) {
								UpdateFieldValue(fname, "");
								fieldChanged = true;
							}

							ImGui::PopID();
						}
						else if (ftype == "prefabref") {
							// Prefab reference field
							std::string prefabName = fval;

							ImGui::Text("%s (Prefab)", fname.c_str());

							ImGui::PushID((fname + "_prefabref").c_str());

							ImGui::Button(prefabName.c_str(), ImVec2(200, 0));

							if (ImGui::BeginDragDropTarget()) {
								const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("PREFAB_ASSET_PATH");
								if (payload && payload->DataSize > 0) {
									std::string droppedPath((const char*)payload->Data, payload->DataSize - 1);
									bool success = UpdateFieldValue(fname, droppedPath);
									if (success) {
										fieldChanged = true;
									}
								}
								ImGui::EndDragDropTarget();
							}

							ImGui::SameLine();
							if (ImGui::Button("X")) {
								UpdateFieldValue(fname, "");
								fieldChanged = true;
							}

							ImGui::PopID();
						}
						else if (ftype == "gameobjectref") {
							// GameObject reference field - drag entity from hierarchy
							std::string displayName = "None";
							uint32_t assignedEntityId = NE::ECS::NO_ENTITY;
							std::string noEntityStr = std::to_string(NE::ECS::NO_ENTITY);

							// Try to resolve the stored LUID
							if (!fval.empty() && fval != "0" && fval != noEntityStr) {
								try {
									uint64_t luid = std::stoull(fval);
									assignedEntityId = NE::ECS::Query::ResolveEntityMetaLuidToEntity(luid);

									if (assignedEntityId != NE::ECS::NO_ENTITY) {
										const auto& entityMeta = NE::ECS::Query::GetEntityMeta(assignedEntityId);
										displayName = entityMeta.name.empty() ? ("Entity " + std::to_string(assignedEntityId)) : entityMeta.name;
									}
								}
								catch (...) {
									displayName = "[Error]";
								}
							}

							ImGui::Text("%s (GameObject)", fname.c_str());
							ImGui::PushID((fname + "_goref").c_str());

							ImGui::Button(displayName.c_str(), ImVec2(200, 0));

							if (ImGui::BeginDragDropTarget()) {
								const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ENTITY_DRAG");
								if (payload && payload->DataSize == sizeof(uint32_t)) {
									uint32_t droppedEntity = *(const uint32_t*)payload->Data;

									// Get the EntityMeta LUID to store (stable across sessions)
									uint64_t luid = 0;
									if (NE::ECS::Query::HasEntityMeta(droppedEntity)) {
										const auto& meta = NE::ECS::Query::GetEntityMeta(droppedEntity);
										luid = meta.luid;
									}

									// Assign the LUID
									bool success = UpdateFieldValue(fname, std::to_string(luid));
									if (success) {
										fieldChanged = true;
									}
								}
								ImGui::EndDragDropTarget();
							}

							ImGui::SameLine();
							if (ImGui::Button("X")) {
								UpdateFieldValue(fname, std::to_string(NE::ECS::NO_ENTITY));
								fieldChanged = true;
							}

							ImGui::PopID();
						}
						else if (ftype == "layerref") {
							// Layer reference field - show layer dropdown from LayerDatabase
							NE::Core::LayerID currentLayerId = 0;
							if (!fval.empty()) {
								try {
									currentLayerId = static_cast<NE::Core::LayerID>(std::stoi(fval));
								} catch (...) {
									currentLayerId = 0;
								}
							}

							// Get layer name for preview
							std::string_view layerName = EditorScene::layerDatabase.GetName(currentLayerId);
							std::string preview = layerName.empty() ? "<Unassigned>" : std::string(layerName);

							ImGui::PushID((fname + "_layerref").c_str());

							ImGui::Text("%s", fname.c_str());
							ImGui::SameLine();

							ImGui::PushItemWidth(140.0f);
							if (Editor::BeginPillCombo("##layercombo", preview.c_str())) {
								EditorScene::layerDatabase.ForEachUsed([&](NE::Core::LayerID id, std::string_view name) {
									const bool selected = (id == currentLayerId);

									std::string label = std::to_string(static_cast<int>(id));
									label += ": ";
									label += name;

									if (ImGui::Selectable(label.c_str(), selected)) {
										UpdateFieldValue(fname, std::to_string(static_cast<int>(id)));
										fieldChanged = true;
									}
									if (selected) ImGui::SetItemDefaultFocus();
								});

								Editor::EndPillCombo();
							}
							ImGui::PopItemWidth();

							ImGui::PopID();
						}
						else if (ftype.starts_with("vector<")) {
							// Array/Vector support
							size_t arraySize = scriptInstance->GetArraySize(fname);

							if (ImGui::TreeNode(fname.c_str(), "%s [%zu]", fname.c_str(), arraySize)) {
								if (ImGui::Button("+##add")) {
									scriptInstance->AddArrayElement(fname);
									SyncArrayField(fname);
									fieldChanged = true;
								}
								ImGui::SameLine();
								ImGui::Text("Add Element");

								for (size_t i = 0; i < arraySize; ++i) {
									ImGui::PushID(static_cast<int>(i));

									std::string elemValue = scriptInstance->GetArrayElement(fname, i);
									std::string elementType = ftype.substr(7, ftype.length() - 8);

									ImGui::Text("[%zu]", i);
									ImGui::SameLine();

									bool elemChanged = false;
									if (elementType == "int") {
										int val = elemValue.empty() ? 0 : std::stoi(elemValue);
										if (ImGui::DragInt("##elem", &val)) {
											scriptInstance->SetArrayElement(fname, i, std::to_string(val));
											elemChanged = true;
										}
									}
									else if (elementType == "float") {
										float val = elemValue.empty() ? 0.0f : std::stof(elemValue);
										if (ImGui::DragFloat("##elem", &val, 0.01f)) {
											scriptInstance->SetArrayElement(fname, i, std::to_string(val));
											elemChanged = true;
										}
									}
									else if (elementType == "bool") {
										bool val = (elemValue == "1" || elemValue == "true");
										if (ImGui::Checkbox("##elem", &val)) {
											scriptInstance->SetArrayElement(fname, i, val ? "1" : "0");
											elemChanged = true;
										}
									}
									else if (elementType == "string") {
										char buf[256];
										strncpy_s(buf, elemValue.c_str(), sizeof(buf));
										buf[sizeof(buf) - 1] = '\0';
										if (ImGui::InputText("##elem", buf, sizeof(buf))) {
											scriptInstance->SetArrayElement(fname, i, std::string(buf));
											elemChanged = true;
										}
									}
									else if (elementType == "materialref") {
										std::string materialUUID = elemValue;
										std::string displayName = materialUUID.empty() ? "None" : Assets::AssetManager::GetInstance().RetrieveFilename(materialUUID);

										ImGui::Button(displayName.c_str(), ImVec2(150, 0));

										if (ImGui::BeginDragDropTarget()) {
											const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("MATERIAL_PATH");
											if (payload && payload->DataSize > 0) {
												std::string droppedPath((const char*)payload->Data, payload->DataSize - 1);
												std::string droppedUUID = Assets::AssetManager::GetInstance().RetrieveUUID(droppedPath);
												if (!droppedUUID.empty()) {
													bool success = scriptInstance->SetArrayElement(fname, i, droppedUUID);
													if (success) elemChanged = true;
												}
											}
											ImGui::EndDragDropTarget();
										}

										ImGui::SameLine();
										if (ImGui::Button("X##clear")) {
											scriptInstance->SetArrayElement(fname, i, "");
											elemChanged = true;
										}
									}
									else if (elementType == "prefabref") {
										std::string prefabUUID = elemValue;
										std::string displayName = prefabUUID.empty() ? "None" : Assets::AssetManager::GetInstance().RetrieveFilename(prefabUUID);

										ImGui::Button(displayName.c_str(), ImVec2(150, 0));

										if (ImGui::BeginDragDropTarget()) {
											const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("PREFAB_ASSET_PATH");
											if (payload && payload->DataSize > 0) {
												std::string droppedPath((const char*)payload->Data, payload->DataSize - 1);
												std::string droppedUUID = Assets::AssetManager::GetInstance().RetrieveUUID(droppedPath);
												if (!droppedUUID.empty()) {
													bool success = scriptInstance->SetArrayElement(fname, i, droppedUUID);
													if (success) elemChanged = true;
												}
											}
											ImGui::EndDragDropTarget();
										}

										ImGui::SameLine();
										if (ImGui::Button("X##clear")) {
											scriptInstance->SetArrayElement(fname, i, "");
											elemChanged = true;
										}
									}
									else if (elementType == "transformref" || elementType == "rigidbodyref" || elementType == "rendererref") {
										// Component reference in array
										std::string componentType = (elementType == "transformref") ? "Transform" :
										                             (elementType == "rigidbodyref") ? "Rigidbody" : "Renderer";
										std::string displayName = "None";
										uint32_t assignedEntityId = NE::ECS::NO_ENTITY;
										std::string noEntityStr = std::to_string(NE::ECS::NO_ENTITY);

										// Try to resolve the stored value (could be Entity ID or component LUID)
										if (!elemValue.empty() && elemValue != "0" && elemValue != noEntityStr) {
											try {
												uint64_t storedValue = std::stoull(elemValue);
												// Check if it looks like an Entity ID (small value)
												if (storedValue < static_cast<uint64_t>(NE::ECS::NO_ENTITY)) {
													assignedEntityId = static_cast<uint32_t>(storedValue);
													if (assignedEntityId != NE::ECS::NO_ENTITY) {
														const auto& entityMeta = NE::ECS::Query::GetEntityMeta(assignedEntityId);
														displayName = entityMeta.name.empty() ? ("Entity " + std::to_string(assignedEntityId)) : entityMeta.name;
													}
												}
												else {
													// Large value - likely a LUID, resolve it to entity
													assignedEntityId = NE::ECS::Query::ResolveComponentLuidToEntity(storedValue);
													if (assignedEntityId != NE::ECS::NO_ENTITY) {
														const auto& entityMeta = NE::ECS::Query::GetEntityMeta(assignedEntityId);
														displayName = entityMeta.name.empty() ? ("Entity " + std::to_string(assignedEntityId)) : entityMeta.name;
													}
													else {
														displayName = "[Invalid Reference]";
													}
												}
											}
											catch (...) {
												displayName = "[Error]";
											}
										}

										ImGui::Button(displayName.c_str(), ImVec2(150, 0));

										if (ImGui::BeginDragDropTarget()) {
											const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ENTITY_DRAG");
											if (payload && payload->DataSize == sizeof(uint32_t)) {
												uint32_t droppedEntity = *(const uint32_t*)payload->Data;

												// Validate component
												bool hasComponent = false;
												if (elementType == "transformref") {
													hasComponent = true;
												}
												else if (elementType == "rigidbodyref") {
													hasComponent = NE::ECS::Query::HasComponent<NE::ECS::Component::Rigidbody>(droppedEntity);
												}
												else if (elementType == "rendererref") {
													hasComponent = NE::ECS::Query::HasComponent<NE::ECS::Component::Renderer>(droppedEntity);
												}

												if (hasComponent) {
													bool success = scriptInstance->SetArrayElement(fname, i, std::to_string(droppedEntity));
													if (success) elemChanged = true;
												}
												else {
													SPD_WARNING("Entity does not have required " << componentType << " component");
												}
											}
											ImGui::EndDragDropTarget();
										}

										ImGui::SameLine();
										if (ImGui::Button("X##clear")) {
											scriptInstance->SetArrayElement(fname, i, noEntityStr);
											elemChanged = true;
										}
									}
									else if (elementType == "entity") {
										std::string entityIdStr = elemValue;
										std::string displayName = "None";
										uint32_t assignedEntityId = NE::ECS::NO_ENTITY;
										std::string noEntityStr = std::to_string(NE::ECS::NO_ENTITY);

										if (!entityIdStr.empty() && entityIdStr != noEntityStr) {
											try {
												uint64_t luid = std::stoull(entityIdStr);
												assignedEntityId = NE::ECS::Query::ResolveEntityMetaLuidToEntity(luid);

												if (assignedEntityId != NE::ECS::NO_ENTITY) {
													const auto& entityMeta = NE::ECS::Query::GetEntityMeta(assignedEntityId);
													displayName = entityMeta.name.empty() ? "Entity" : entityMeta.name;
												}
											}
											catch (...) {
												displayName = "[Error]";
											}
										}

										ImGui::Button(displayName.c_str(), ImVec2(150, 0));

										if (ImGui::BeginDragDropTarget()) {
											const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ENTITY_DRAG");
											if (payload && payload->DataSize == sizeof(uint32_t)) {
												uint32_t droppedEntity = *(const uint32_t*)payload->Data;
												// Get the EntityMeta LUID to store (stable across sessions)
												uint64_t luid = 0;
												if (NE::ECS::Query::HasEntityMeta(droppedEntity)) {
													const auto& meta = NE::ECS::Query::GetEntityMeta(droppedEntity);
													luid = meta.luid;
												}
												bool success = scriptInstance->SetArrayElement(fname, i, std::to_string(luid));
												if (success) elemChanged = true;
											}
											ImGui::EndDragDropTarget();
										}

										ImGui::SameLine();
										if (ImGui::Button("X##clear")) {
											scriptInstance->SetArrayElement(fname, i, noEntityStr);
											elemChanged = true;
										}
									}
									else if (elementType == "gameobjectref") {
										// GameObject reference in array - same as entity but with consistent naming
										std::string entityIdStr = elemValue;
										std::string displayName = "None";
										uint32_t assignedEntityId = NE::ECS::NO_ENTITY;
										std::string noEntityStr = std::to_string(NE::ECS::NO_ENTITY);

										if (!entityIdStr.empty() && entityIdStr != "0" && entityIdStr != noEntityStr) {
											try {
												uint64_t luid = std::stoull(entityIdStr);
												assignedEntityId = NE::ECS::Query::ResolveEntityMetaLuidToEntity(luid);

												if (assignedEntityId != NE::ECS::NO_ENTITY) {
													const auto& entityMeta = NE::ECS::Query::GetEntityMeta(assignedEntityId);
													displayName = entityMeta.name.empty() ? ("Entity " + std::to_string(assignedEntityId)) : entityMeta.name;
												}
											}
											catch (...) {
												displayName = "[Error]";
											}
										}

										ImGui::Button(displayName.c_str(), ImVec2(150, 0));

										if (ImGui::BeginDragDropTarget()) {
											const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ENTITY_DRAG");
											if (payload && payload->DataSize == sizeof(uint32_t)) {
												uint32_t droppedEntity = *(const uint32_t*)payload->Data;
												// Get the EntityMeta LUID to store (stable across sessions)
												uint64_t luid = 0;
												if (NE::ECS::Query::HasEntityMeta(droppedEntity)) {
													const auto& meta = NE::ECS::Query::GetEntityMeta(droppedEntity);
													luid = meta.luid;
												}
												bool success = scriptInstance->SetArrayElement(fname, i, std::to_string(luid));
												if (success) elemChanged = true;
											}
											ImGui::EndDragDropTarget();
										}

										ImGui::SameLine();
										if (ImGui::Button("X##clear")) {
											scriptInstance->SetArrayElement(fname, i, noEntityStr);
											elemChanged = true;
										}
									}
									else if (elementType == "layerref") {
										// Layer reference in array - show layer dropdown
										NE::Core::LayerID currentLayerId = 0;
										if (!elemValue.empty()) {
											try {
												currentLayerId = static_cast<NE::Core::LayerID>(std::stoi(elemValue));
											} catch (...) {
												currentLayerId = 0;
											}
										}

										// Get layer name for preview
										std::string_view layerName = EditorScene::layerDatabase.GetName(currentLayerId);
										std::string preview = layerName.empty() ? "<Unassigned>" : std::string(layerName);

										ImGui::PushItemWidth(120.0f);
										if (Editor::BeginPillCombo("##layercombo", preview.c_str())) {
											EditorScene::layerDatabase.ForEachUsed([&](NE::Core::LayerID id, std::string_view name) {
												const bool selected = (id == currentLayerId);

												std::string label = std::to_string(static_cast<int>(id));
												label += ": ";
												label += name;

												if (ImGui::Selectable(label.c_str(), selected)) {
													scriptInstance->SetArrayElement(fname, i, std::to_string(static_cast<int>(id)));
													elemChanged = true;
												}
												if (selected) ImGui::SetItemDefaultFocus();
											});

											Editor::EndPillCombo();
										}
										ImGui::PopItemWidth();

										ImGui::SameLine();
										if (ImGui::Button("X##clear")) {
											scriptInstance->SetArrayElement(fname, i, "0");
											elemChanged = true;
										}
									}
									else if (elementType == "prefabref") {
										// Prefab reference in array
										std::string prefabPath = elemValue;
										std::string displayName = prefabPath.empty() ? "None" : prefabPath;

										ImGui::Button(displayName.c_str(), ImVec2(150, 0));

										if (ImGui::BeginDragDropTarget()) {
											const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("PREFAB_ASSET_PATH");
											if (payload && payload->DataSize > 0) {
												std::string droppedPath((const char*)payload->Data, payload->DataSize - 1);
												bool success = scriptInstance->SetArrayElement(fname, i, droppedPath);
												if (success) elemChanged = true;
											}
											ImGui::EndDragDropTarget();
										}

										ImGui::SameLine();
										if (ImGui::Button("X##clear")) {
											scriptInstance->SetArrayElement(fname, i, "");
											elemChanged = true;
										}
									}
									else if (elementType == "audiosourceref") {
										// AudioSource reference in array
										std::string displayName = "None";
										uint32_t assignedEntityId = NE::ECS::NO_ENTITY;
										std::string noEntityStr = std::to_string(NE::ECS::NO_ENTITY);

										// Try to resolve the stored value (could be Entity ID or component LUID)
										if (!elemValue.empty() && elemValue != "0" && elemValue != noEntityStr) {
											try {
												uint64_t storedValue = std::stoull(elemValue);
												// Check if it looks like an Entity ID (small value)
												if (storedValue < static_cast<uint64_t>(NE::ECS::NO_ENTITY)) {
													assignedEntityId = static_cast<uint32_t>(storedValue);
													if (assignedEntityId != NE::ECS::NO_ENTITY) {
														const auto& entityMeta = NE::ECS::Query::GetEntityMeta(assignedEntityId);
														displayName = entityMeta.name.empty() ? ("Entity " + std::to_string(assignedEntityId)) : entityMeta.name;
													}
												}
												else {
													// Large value - likely a LUID, resolve it to entity
													assignedEntityId = NE::ECS::Query::ResolveComponentLuidToEntity(storedValue);
													if (assignedEntityId != NE::ECS::NO_ENTITY) {
														const auto& entityMeta = NE::ECS::Query::GetEntityMeta(assignedEntityId);
														displayName = entityMeta.name.empty() ? ("Entity " + std::to_string(assignedEntityId)) : entityMeta.name;
													}
													else {
														displayName = "[Invalid Reference]";
													}
												}
											}
											catch (...) {
												displayName = "[Error]";
											}
										}

										ImGui::Button(displayName.c_str(), ImVec2(150, 0));

										if (ImGui::BeginDragDropTarget()) {
											const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ENTITY_DRAG");
											if (payload && payload->DataSize == sizeof(uint32_t)) {
												uint32_t droppedEntity = *(const uint32_t*)payload->Data;

												// Validate that the entity has AudioSource component
												if (NE::ECS::Query::HasComponent<NE::ECS::Component::AudioSource>(droppedEntity)) {
													bool success = scriptInstance->SetArrayElement(fname, i, std::to_string(droppedEntity));
													if (success) elemChanged = true;
												}
												else {
													SPD_WARNING("Entity does not have required AudioSource component");
												}
											}
											ImGui::EndDragDropTarget();
										}

										ImGui::SameLine();
										if (ImGui::Button("X##clear")) {
											scriptInstance->SetArrayElement(fname, i, noEntityStr);
											elemChanged = true;
										}
									}
									else if (elementType == "enum") {
										// Enum dropdown in array
										auto enumOptions = scriptInstance->GetEnumOptions(fname);
										if (!enumOptions.empty()) {
											int currentValue = 0;
											if (!elemValue.empty()) {
												try {
													currentValue = std::stoi(elemValue);
												} catch (...) {
													currentValue = 0;
												}
											}

											// Clamp to valid range
											if (currentValue < 0 || currentValue >= static_cast<int>(enumOptions.size())) {
												currentValue = 0;
											}

											if (ImGui::BeginCombo("##elem", enumOptions[currentValue].c_str())) {
												for (int j = 0; j < static_cast<int>(enumOptions.size()); ++j) {
													bool isSelected = (currentValue == j);
													if (ImGui::Selectable(enumOptions[j].c_str(), isSelected)) {
														scriptInstance->SetArrayElement(fname, i, std::to_string(j));
														elemChanged = true;
													}
													if (isSelected) {
														ImGui::SetItemDefaultFocus();
													}
												}
												ImGui::EndCombo();
											}
										} else {
											ImGui::Text("[%zu]: (enum - no options)", i);
										}
									}
									else {
										// Unknown type fallback - treat as string
										char buf[256];
										strncpy_s(buf, elemValue.c_str(), sizeof(buf));
										buf[sizeof(buf) - 1] = '\0';
										if (ImGui::InputText("##elem", buf, sizeof(buf))) {
											scriptInstance->SetArrayElement(fname, i, std::string(buf));
											elemChanged = true;
										}
									}

									ImGui::SameLine();
									if (ImGui::Button("-##remove")) {
										scriptInstance->RemoveArrayElement(fname, i);
										SyncArrayField(fname);
										fieldChanged = true;
									}

									if (elemChanged) {
										SyncArrayField(fname);
										fieldChanged = true;
									}

									ImGui::PopID();
								}

								ImGui::TreePop();
							}
						}
						else if (fname.find('.') != std::string::npos) {
							// Struct field (contains dot notation) - handled in structGroups below
							// This section is a fallback for individual struct fields
							ImGui::Indent();

							if (ftype == "int") {
								int v = 0;
								if (!fval.empty()) v = std::stoi(fval);
								if (ImGui::DragInt(fname.c_str(), &v)) {
									UpdateFieldValue(fname, std::to_string(v));
									fieldChanged = true;
								}
							}
							else if (ftype == "float") {
								float v = 0.0f;
								if (!fval.empty()) v = std::stof(fval);
								if (ImGui::DragFloat(fname.c_str(), &v, 0.01f)) {
									UpdateFieldValue(fname, std::to_string(v));
									fieldChanged = true;
								}
							}
							else if (ftype == "bool") {
								bool v = (fval == "1" || fval == "true");
								if (ImGui::Checkbox(fname.c_str(), &v)) {
									UpdateFieldValue(fname, v ? "1" : "0");
									fieldChanged = true;
								}
							}

							ImGui::Unindent();
						}
						else { // treat as string
							char buf[256];
							strncpy_s(buf, fval.c_str(), sizeof(buf));
							if (ImGui::InputText(fname.c_str(), buf, sizeof(buf))) {
								UpdateFieldValue(fname, std::string(buf));
								fieldChanged = true;
							}
						}

						// Call OnValidate() when a field changes
						if (fieldChanged) {
							scriptInstance->OnValidate();
						}

						ImGui::PopID();
					}

					// NOW RENDER STRUCT GROUPS
					for (const auto& pair : structGroups) {
						const std::string& structName = pair.first;
						const std::vector<std::string>& fields = pair.second;

						if (ImGui::TreeNode(structName.c_str())) {
							for (const auto& fname : fields) {
								std::string ftype = scriptInstance->GetFieldType(fname);
								std::string fval = scriptInstance->GetFieldValueAsString(fname);

								size_t dotPos = fname.find('.');
								std::string fieldName = fname.substr(dotPos + 1);

								ImGui::PushID(fname.c_str());
								bool fieldChanged = false;

								if (ftype == "int") {
									int v = 0;
									if (!fval.empty()) v = std::stoi(fval);
									if (ImGui::DragInt(fieldName.c_str(), &v)) {
										UpdateFieldValue(fname, std::to_string(v));
										fieldChanged = true;
									}
								}
								else if (ftype == "float") {
									float v = 0.0f;
									if (!fval.empty()) v = std::stof(fval);
									if (ImGui::DragFloat(fieldName.c_str(), &v, 0.01f)) {
										UpdateFieldValue(fname, std::to_string(v));
										fieldChanged = true;
									}
								}
								else if (ftype == "bool") {
									bool v = (fval == "1" || fval == "true");
									if (ImGui::Checkbox(fieldName.c_str(), &v)) {
										UpdateFieldValue(fname, v ? "1" : "0");
										fieldChanged = true;
									}
								}

								if (fieldChanged) {
									scriptInstance->OnValidate();
								}

								ImGui::PopID();
							}
							ImGui::TreePop();
						}
					}
				}
				} // End of else block (scriptInstance exists)

				} // End of if (headerOpen) - collapsible header

				ImGui::PopID(); // Pop scriptIdx scope
			}
		}
	}

	void InspectorPanel::DrawCameraComponent(uint32_t entity) {
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
						} else {
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

	void InspectorPanel::DrawAnimatorComponent(uint32_t entity) {

		auto& comp = NE::ECS::Query::GetEntityAnimator(entity);

		bool copyComp = false;
		bool deleteComp = false;

		const bool open = DrawComponentHeaderWithMenu(
			"Animator",
			true,
			&copyComp,
			&deleteComp
		);

		if (!open)
			return;
		
		bool openAnimClipPopup = false;
		DrawAssetField("Animation Clip", Assets::AssetManager::GetInstance().RetrieveFilename(comp.animClipUUID), &openAnimClipPopup);
		if (openAnimClipPopup) {
			ImGui::OpenPopup("AssetPicker_Anim");
		}

		if (ImGui::BeginDragDropTarget()) {
			if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload("ASSET_ANIM_PATH")) {
				std::string dropped((const char*)p->Data, p->DataSize - 1);
				auto uuid = Assets::AssetManager::GetInstance().RetrieveUUID(dropped);
				NE::ECS::Command::AssignAnimClip(entity, uuid);
			}
			ImGui::EndDragDropTarget();
		}

		static std::string searchQuery;
		if (ImGui::BeginPopup("AssetPicker_Anim")) {
			ImGui::Text("Select a Animation Clip");
			ImGui::Separator();
			auto& modelList = Assets::AssetManager::GetInstance().GetAssetsOfType(Assets::AssetType::AnimationClip);

			if (ImSearch::BeginSearch()) {
				ImSearch::SearchBar();
				for (const auto& [modelName, uuid] : modelList) {
					ImSearch::SearchableItem(modelName.c_str(), [&, modelName](const char*) {
						if (ImGui::Selectable(modelName.c_str())) {
							NE::ECS::Command::AssignAnimClip(entity, uuid);
							ImGui::CloseCurrentPopup();
						}
						});
				}

				ImSearch::EndSearch();
			}
			ImGui::EndPopup();
		}

		NE::Core::ForEachFieldView<NE::ECS::Component::Animator>(comp,
			[&](auto const& desc, auto const& currentValue) {
				using FieldT = std::decay_t<decltype(currentValue)>;

				FieldT edited = currentValue;

				if (DrawField(desc, edited)) {
					SubmitSetFieldCommand<NE::ECS::Component::Animator, FieldT>(
						entity, desc, currentValue, edited
					);
				}
			});

		if (copyComp) {

		}
		if (deleteComp) {
			NE::ECS::Command::RemoveRendererComponent(entity);
		}

		ImGui::TreePop();
	}

	void InspectorPanel::DrawRectTransformComponent(uint32_t entity) {
		auto& comp = NE::ECS::Command::GetUIRectTransform(entity);

		if (ImGui::CollapsingHeader("Rect Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
			ImGui::Indent();

			// Check render mode - walk up hierarchy to find canvas
			bool isOverlay = false;
			{
				if (NE::ECS::Query::HasUICanvas(entity)) {
					auto& compCanvas = NE::ECS::Command::GetUICanvas(entity);
					isOverlay = (compCanvas.renderMode == NE::ECS::Component::UICanvas::RenderMode::SCREEN_SPACE_OVERLAY);
				} else {
					uint32_t currentParent = comp.parent;
					while (currentParent != NE::ECS::NO_ENTITY) {
						if (NE::ECS::Query::HasUICanvas(currentParent)) {
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

				if (!isOverlay) {
					ImGui::SameLine(0, spacing);

					ImGui::BeginGroup();
					ImGui::TextDisabled("Pos Z");
					ImGui::SetNextItemWidth(itemWidth);
					UI_RECT_DRAG("##PosZ", &NE::ECS::Component::UIRectTransform::z, 0.1f, 0.0f, 0.0f, "%.1f");
					ImGui::EndGroup();
				}
			}

			ImGui::Spacing();

			// Size section
			{
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

				static int currentPreset = 4;

				ImGui::SetNextItemWidth(150);
				if (ImGui::Combo("##AnchorPresets", &currentPreset, presetNames, IM_ARRAYSIZE(presetNames))) {
					switch (currentPreset) {
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
				if (!isOverlay) {
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
				UI_RECT_DRAG("##RotZ", &NE::ECS::Component::UIRectTransform::rotationZ, ROTATION_DRAG_SPEED, 0.0f, 0.0f, "%.1f");
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

				if (!isOverlay) {
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

	void InspectorPanel::DrawCanvasComponent(uint32_t entity) {
		auto& comp = NE::ECS::Command::GetUICanvas(entity);

		if (ImGui::CollapsingHeader("Canvas", ImGuiTreeNodeFlags_DefaultOpen)) {
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
			if (ImGui::Combo("##RenderMode", &currentMode, RenderModes, IM_ARRAYSIZE(RenderModes))) {
				auto oldMode = comp.renderMode;
				comp.renderMode = static_cast<decltype(comp.renderMode)>(currentMode);
				std::string materialPath = GetUIMaterialPathForRenderMode(comp.renderMode);
				std::string materialUUID = Assets::AssetManager::GetInstance().RetrieveUUID(materialPath);

				if (materialUUID.empty()) {
					SPD_ERROR("[InspectorPanel] Failed to find material for render mode: {}", materialPath);
					SPD_ERROR("Make sure UI_Overlay.nanomat, UI_Camera.nanomat, and UI_World.nanomat exist in Assets/");
				} else {
					// Rebuild all child materials with the new shader
					RebuildChildMaterials(entity, materialUUID);

					SPD_INFO("[InspectorPanel] Canvas render mode changed: {} -> {}",
						static_cast<int>(oldMode), currentMode);
					SPD_INFO("Assigned material: {}", materialPath);
				}
			}

			// pixel perfect toggle (if in overlay mode or camera mode)
			if (comp.renderMode == NE::ECS::Component::UICanvas::RenderMode::SCREEN_SPACE_OVERLAY ||
				comp.renderMode == NE::ECS::Component::UICanvas::RenderMode::SCREEN_SPACE_CAMERA) {
				ImGui::AlignTextToFramePadding();
				ImGui::Text("Pixel Perfect");
				ImGui::SameLine(labelWidth);
				ImGui::SetNextItemWidth(-1);
				if (ImGui::Checkbox("##PixelPerfect", &comp.pixelPerfect)) {
				}
			}

			// show plane distqance for camera mode only
			if (comp.renderMode == NE::ECS::Component::UICanvas::RenderMode::SCREEN_SPACE_CAMERA) {
				ImGui::AlignTextToFramePadding();
				ImGui::Text("Plane Distance");
				ImGui::SameLine(labelWidth);
				ImGui::SetNextItemWidth(-1);
				if (ImGui::DragFloat("##PlaneDistance", &comp.planeDistance, 1.0f, 0.1f, 1000.0f)) {
				}
			}

			// Sort Order
			ImGui::AlignTextToFramePadding();
			ImGui::Text("Sort Order");
			ImGui::SameLine(labelWidth);
			ImGui::SetNextItemWidth(-1);
			if (ImGui::DragInt("##SortOrder", &comp.sortingOrder)) {
			}

			ImGui::Unindent();
		}

		// scalar section
		if (ImGui::CollapsingHeader("Canvas Scaler", ImGuiTreeNodeFlags_DefaultOpen)) {
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

	void InspectorPanel::DrawImageComponent(uint32_t entity) {
		auto& comp = NE::ECS::Command::GetUIImage(entity);

		if (ImGui::CollapsingHeader("UI Image", ImGuiTreeNodeFlags_DefaultOpen)) {
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
					: Assets::AssetManager::GetInstance().RetrieveFilename(comp.textureUUID);

				char bufTex[256];
				strncpy_s(bufTex, texLabel.c_str(), sizeof(bufTex));
				ImGui::InputText("Source Image", bufTex, sizeof(bufTex));

				if (ImGui::BeginDragDropTarget()) {
					if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload("TEXTURE_ASSET_PATH")) {
						std::string dropped((const char*)p->Data, p->DataSize - 1); // dropped = "Assets/Textures/MyTexture.jpg"
						auto textureUUID = Assets::AssetManager::GetInstance().RetrieveUUID(dropped); // convert file path to UUID --> uuid = "abc123def456" (from MyTexture.jpg.meta file)

						// find parent canvas to determine render mode
						auto& rectTransform = NE::ECS::Command::GetUIRectTransform(entity);
						uint32_t canvasEntity = entity;
						uint32_t current = rectTransform.parent;

						// walk up hierarchy to find canvas
						while (current != NE::ECS::NO_ENTITY) {
							if (NE::ECS::Query::HasUICanvas(current)) {
								canvasEntity = current;
								break;
							}
							if (NE::ECS::Query::HasUIRectTransform(current)) {
								current = NE::ECS::Query::GetUIRectTransform(current).parent;
							} else {
								break;
							}
						}

						// get render mode from canvas
						int renderMode = 0;
						if (NE::ECS::Query::HasUICanvas(canvasEntity)) {
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
						std::string materialUUID = Assets::AssetManager::GetInstance().RetrieveUUID(materialPath);

						if (materialUUID.empty()) {
							SPD_ERROR("[InspectorPanel] Failed to retrieve material UUID for: " << materialPath);
						} else if (textureUUID.empty()) {
							SPD_ERROR("[InspectorPanel] Failed to retrieve texture UUID for: " << dropped);
						} else {
							// call assignment function with both UUIDs
							NE::Renderer::Command::AssignUITexture(entity, textureUUID, materialUUID);
						}
					}
					ImGui::EndDragDropTarget();
				}

				// Right-click to clear texture
				if (ImGui::BeginPopupContextItem("##TextureContext")) {
					if (ImGui::MenuItem("Clear")) {
						comp.textureUUID.clear();
						comp.material.reset();
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
				}

				// type specific options
				ImGui::Indent(16.0f);

				switch (comp.imageType) {
				case NE::ECS::Component::UIImage::ImageType::SIMPLE:
				{
					// simple image options
					ImGui::Text("Preserve Aspect");
					ImGui::SameLine(labelWidth); // adjust for indent
					ImGui::SetNextItemWidth(-1);
					if (ImGui::Checkbox("##PreserveAspect", &comp.preserveAspect)) {
						comp.isDirty = true;
					}

					// reset fill amount when switching to simple mode
					if (comp.fillAmount < 1.0f) {
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
					if (comp.fillAmount < 1.0f) {
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
					}

					// reset fill amount when switching to simple mode
					if (comp.fillAmount < 1.0f) {
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
					}

					// fill Origin (context-dependent)
					ImGui::Text("Fill Origin");
					ImGui::SameLine(labelWidth);
					ImGui::SetNextItemWidth(-1);

					if (comp.fillMethod == NE::ECS::Component::UIImage::FillMethod::HORIZONTAL) {
						static const char* HOrigins[] = { "Left", "Right" };
						int origin = static_cast<int>(comp.fillOrigin);
						if (ImGui::Combo("##FillOrigin", &origin, HOrigins, IM_ARRAYSIZE(HOrigins))) {
							comp.fillOrigin = static_cast<NE::ECS::Component::UIImage::FillOrigin>(origin);
							comp.isDirty = true;
						}
					} else if (comp.fillMethod == NE::ECS::Component::UIImage::FillMethod::VERTICAL) {
						static const char* VOrigins[] = { "Bottom", "Top" };
						int origin = static_cast<int>(comp.fillOrigin);
						if (ImGui::Combo("##FillOrigin", &origin, VOrigins, IM_ARRAYSIZE(VOrigins))) {
							comp.fillOrigin = static_cast<NE::ECS::Component::UIImage::FillOrigin>(origin);
							comp.isDirty = true;
						}
					} else // radial fills
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
						}
					}

					// fill Amount slider
					ImGui::Text("Fill Amount");
					ImGui::SameLine(labelWidth);
					ImGui::SetNextItemWidth(-1);
					if (ImGui::SliderFloat("##FillAmount", &comp.fillAmount, 0.0f, 1.0f)) {
						comp.ClampFillAmount();
						comp.isDirty = true;
					}

					// clockwise toggle 
					if (comp.fillMethod == NE::ECS::Component::UIImage::FillMethod::RADIAL_90 ||
						comp.fillMethod == NE::ECS::Component::UIImage::FillMethod::RADIAL_180 ||
						comp.fillMethod == NE::ECS::Component::UIImage::FillMethod::RADIAL_360) {
						ImGui::Text("Clockwise");
						ImGui::SameLine(labelWidth);
						ImGui::SetNextItemWidth(-1);
						if (ImGui::Checkbox("##Clockwise", &comp.fillClockwise)) {
							comp.isDirty = true;
						}
					}

					// preserve aspect
					ImGui::Text("Preserve Aspect");
					ImGui::SameLine(labelWidth);
					ImGui::SetNextItemWidth(-1);
					if (ImGui::Checkbox("##PreserveAspect", &comp.preserveAspect)) {
						comp.isDirty = true;
					}
					break;
				}
				}

				ImGui::Unindent(16.0f);
				ImGui::Unindent();
			}
		}
	}

	void InspectorPanel::DrawCharacterControllerComponent(uint32_t entity) {
		auto& comp = NE::ECS::Query::GetCharacterController(entity);

		bool copyComp = false;
		bool deleteComp = false;

		const bool open = DrawComponentHeaderWithMenu(
			"CharacterController",
			true,
			&copyComp,
			&deleteComp
		);

		if (!open)
			return;

		NE::Core::ForEachFieldView<NE::ECS::Component::CharacterController>(comp,
			[&](auto const& desc, auto const& currentValue) {
				using FieldT = std::decay_t<decltype(currentValue)>;

				FieldT edited = currentValue;

				if (DrawField(desc, edited)) {
					SubmitSetFieldCommand<NE::ECS::Component::CharacterController, FieldT>(
						entity, desc, currentValue, edited
					);
				}
			}
		);

		if (copyComp) {

		}
		if (deleteComp) {
			//NE::ECS::Command::RemoveRigidbodyComponent(entity);
		}

		ImGui::TreePop();
	}
}