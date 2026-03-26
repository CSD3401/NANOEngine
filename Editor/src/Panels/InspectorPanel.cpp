#include "pch.h"
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
#include <ECS/Components/Hierarchy.hpp>
#include <ECS/Components/UIText.hpp>
#include <ECS/Components/UIButton.hpp>
#include <ECS/Components/UISlider.hpp>
#include <ECS/Components/UIToggle.hpp>
#include <ECS/Components/UILayoutGroup.hpp>
#include <ECS/Components/UIGridLayoutGroup.hpp>
#include <ECS/Components/UILayoutElement.hpp>
#include <ECS/Components/UIScrollRect.hpp>
#include <ECS/Components/UIAutoSize.hpp>
#include <ECS/Components/UIInputField.hpp>
#include <ECS/Components/UIDropdown.hpp>
#include <Core/LUIDGenerator.hpp>
#include <ECS/Components/Animator.hpp>
#include <ECS/Components/DecalProjector.hpp>
#include <ECS/Components/Camera.hpp>
#include <ECS/Components/PrefabInstance.hpp>
#include <ECS/Components/CharacterController.hpp>
#include <ECS/Components/ParticleEmitter.hpp>
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
#include <Core/SpdLogger.hpp>
#include "../Command/EditorCommands.hpp"
#include "../Layers/LayerDatabase.hpp"
#include "../Layers/LayerModal.hpp"
#include "../Lighting/LightmapAtlasAllocator.hpp"
#include <Events/EventBus.hpp>
#include "../EditorEvents.hpp"
#include "Engine.hpp"
#include <algorithm>
#include <cmath>

bool openLayerSettings = false;

namespace {
	float DegToRad(float deg) { return deg * 3.14159265358979323846f / 180.0f; }
	float RadToDeg(float rad) { return rad * 180.0f / 3.14159265358979323846f; }

	// Convert an FOV value between vertical/horizontal for a given aspect ratio while preserving the view.
	float ConvertFovDegrees(float fovDeg, float aspect, bool fromVerticalToHorizontal) {
		aspect = std::max(1e-6f, aspect);
		fovDeg = std::clamp(fovDeg, 1.0f, 179.0f);

		const float half = std::tan(DegToRad(fovDeg) * 0.5f);
		const float halfOut = fromVerticalToHorizontal ? (half * aspect) : (half / aspect);
		return RadToDeg(2.0f * std::atan(halfOut));
	}

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
			bool changed = Editor::DrawVec3Control(desc.name.data(), value, 75.0f, 0.01f);
			ImGui::EndGroup();
			return changed;
		} 
		else if constexpr (std::is_same_v<T, NE::Math::Vec2>) {
			ImGui::BeginGroup();
			bool changed = Editor::DrawVec2Control(desc.name.data(), value, 75.0f, 0.01f);
			ImGui::EndGroup();
			return changed;
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
		else if constexpr (std::is_same_v <T, NE::Math::Vec4>) {
			ImGui::BeginGroup();
			bool changed = Editor::DrawVec4Control(desc.name.data(), value, 75.0f, 0.01f);
			ImGui::EndGroup();
			return changed;
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
			}
			else if constexpr (std::is_same_v<Owner, NE::ECS::Component::DecalProjector>) {
				return NE::ECS::Command::GetDecalProjector(e);
			}
			else if constexpr (std::is_same_v<Owner, NE::ECS::Component::ParticleEmitter>) {
				return NE::ECS::Command::GetParticleEmitter(e);
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
		if (!NE::ECS::Query::HasHierarchy(entity)) return false;

		uint32_t current = NE::ECS::Query::GetEntityHierarchy(entity).parent;

		// Walk up hierarchy
		while (current != NE::ECS::NO_ENTITY) {
			if (current == canvasEntity) return true;

			if (!NE::ECS::Query::HasHierarchy(current)) break;
			current = NE::ECS::Query::GetEntityHierarchy(current).parent;
		}

		// Direct child check
		uint32_t parent = NE::ECS::Query::GetEntityHierarchy(entity).parent;
		return (parent == canvasEntity);
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
			{ NE::ECS::Query::GetDecalProjectorComponentType(),		"Decal Projector",		&InspectorPanel::DrawDecalProjectorComponent		},
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
			{ NE::ECS::Query::GetUITextComponentType(),				"Text",					&InspectorPanel::DrawTextComponent					},
			{ NE::ECS::Query::GetUIButtonComponentType(),			"Button",				&InspectorPanel::DrawButtonComponent				},
			{ NE::ECS::Query::GetUISliderComponentType(),			"Slider",				&InspectorPanel::DrawSliderComponent				},
			{ NE::ECS::Query::GetUIToggleComponentType(),			"Toggle",				&InspectorPanel::DrawToggleComponent				},
			{ NE::ECS::Query::GetUILayoutGroupComponentType(),		"Layout Group",			&InspectorPanel::DrawLayoutGroupComponent           },
			{ NE::ECS::Query::GetUIGridLayoutGroupComponentType(),	"Grid Layout Group",	&InspectorPanel::DrawGridLayoutGroupComponent       },
			{ NE::ECS::Query::GetUILayoutElementComponentType(),	"Layout Element",		&InspectorPanel::DrawLayoutElementComponent         },
			{ NE::ECS::Query::GetUIScrollRectComponentType(),		"Scroll Rect",			&InspectorPanel::DrawScrollRectComponent            },
			{ NE::ECS::Query::GetUIAutoSizeComponentType(),			"Auto Size",			&InspectorPanel::DrawAutoSizeComponent              },
			{ NE::ECS::Query::GetUIInputFieldComponentType(),		"Input Field",			&InspectorPanel::DrawInputFieldComponent			},
			{ NE::ECS::Query::GetUIDropdownComponentType(),			"Dropdown",				&InspectorPanel::DrawDropdownComponent				},
			{ NE::ECS::Query::GetScriptComponentType(),				"Script",				&InspectorPanel::DrawScriptComponent				},
			{ NE::ECS::Query::GetParticleEmitterComponentType(),	"Particle Emitter",		&InspectorPanel::DrawParticleEmitterComponent		},
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
				if (ImGui::MenuItem("Decal Projector")) {
					NE::ECS::Command::AddDecalProjectorComponent(EditorScene::s_selection.GetLastClicked());
				}
				if (ImGui::MenuItem("Particle Emitter")) {
					NE::ECS::Command::AddParticleEmitterComponent(EditorScene::s_selection.GetLastClicked());
				}

				ImGui::Separator();
				ImGui::TextDisabled("UI");

				if (ImGui::MenuItem("UI Canvas")) {
					uint32_t entity = EditorScene::s_selection.GetLastClicked();
					if (!NE::ECS::Query::HasUIRectTransform(entity)) {
						NE::ECS::Component::UIRectTransform rect{};
						NE::ECS::Command::AddUIRectTransformComponent(entity, rect);
					}
					NE::ECS::Component::UICanvas canvas{};
					NE::ECS::Command::AddUICanvasComponent(entity, canvas);
				}
				if (ImGui::MenuItem("UI Image")) {
					uint32_t entity = EditorScene::s_selection.GetLastClicked();
					if (!NE::ECS::Query::HasUIRectTransform(entity)) {
						NE::ECS::Component::UIRectTransform rect{};
						NE::ECS::Command::AddUIRectTransformComponent(entity, rect);
					}
					NE::ECS::Component::UIImage img{};
					NE::ECS::Command::AddUIImageComponent(entity, img);
				}
				if (ImGui::MenuItem("UI Text")) {
					uint32_t entity = EditorScene::s_selection.GetLastClicked();
					if (!NE::ECS::Query::HasUIRectTransform(entity)) {
						NE::ECS::Component::UIRectTransform rect{};
						NE::ECS::Command::AddUIRectTransformComponent(entity, rect);
					}
					NE::ECS::Component::UIText text{};
					text.luid = NE::Core::LUIDGenerator::Generate("tx");
					NE::ECS::Command::AddUITextComponent(entity, text);
				}
				if (ImGui::MenuItem("UI Button")) {
					uint32_t entity = EditorScene::s_selection.GetLastClicked();
					if (!NE::ECS::Query::HasUIRectTransform(entity)) {
						NE::ECS::Component::UIRectTransform rect{};
						NE::ECS::Command::AddUIRectTransformComponent(entity, rect);
					}
					if (!NE::ECS::Query::HasUIImage(entity)) {
						NE::ECS::Component::UIImage img{};
						NE::ECS::Command::AddUIImageComponent(entity, img);
					}
					NE::ECS::Component::UIButton button{};
					button.luid = NE::Core::LUIDGenerator::Generate("bt");
					NE::ECS::Command::AddUIButtonComponent(entity, button);
				}
				if (ImGui::MenuItem("UI Slider")) {
					uint32_t entity = EditorScene::s_selection.GetLastClicked();
					// Slider entity: track background (grey bar)
					if (!NE::ECS::Query::HasUIRectTransform(entity)) {
						NE::ECS::Component::UIRectTransform rect{};
						rect.width = 200.0f;
						rect.height = 20.0f;
						NE::ECS::Command::AddUIRectTransformComponent(entity, rect);
					}
					if (!NE::ECS::Query::HasUIImage(entity)) {
						NE::ECS::Component::UIImage img{};
						img.color = NE::Math::Vec4{ 0.7f, 0.7f, 0.7f, 1.0f }; // Grey track
						NE::ECS::Command::AddUIImageComponent(entity, img);
					}

					NE::ECS::Component::UISlider slider{};
					slider.luid = NE::Core::LUIDGenerator::Generate("sl");

					// Create Fill child entity
					uint32_t fillEnt = NE::ECS::Command::CreateEntityNoComponents();
					{
						NE::ECS::Component::EntityMeta meta{};
						meta.name = "Fill";
						meta.luid = NE::Core::LUIDGenerator::Generate("em");
						NE::ECS::Command::AddEntityMetaComponent(fillEnt, meta);
						NE::ECS::Component::Hierarchy hr{};
						hr.luid = NE::Core::LUIDGenerator::Generate("hr");
						NE::ECS::Command::AddHierarchyComponent(fillEnt, hr);
						NE::ECS::Component::UIRectTransform fillRect{};
						fillRect.width = 0.0f;   // Starts empty (value = 0)
						fillRect.height = 20.0f;
						fillRect.x = -200.0f;    // UIEventSystem formula: -sliderWidth + width*0.5 = -200
						fillRect.y = -10.0f;     // UIEventSystem formula: -sliderHeight * 0.5
						NE::ECS::Command::AddUIRectTransformComponent(fillEnt, fillRect);
						NE::ECS::Component::UIImage fillImg{};
						fillImg.color = NE::Math::Vec4{ 0.2f, 0.6f, 1.0f, 1.0f }; // Blue fill
						fillImg.raycastTarget = false;
						NE::ECS::Command::AddUIImageComponent(fillEnt, fillImg);
						NE::ECS::Command::SetParent(fillEnt, entity, -1, false);
					}
					slider.fillRect = fillEnt;

					// Create Handle child entity
					uint32_t handleEnt = NE::ECS::Command::CreateEntityNoComponents();
					{
						NE::ECS::Component::EntityMeta meta{};
						meta.name = "Handle";
						meta.luid = NE::Core::LUIDGenerator::Generate("em");
						NE::ECS::Command::AddEntityMetaComponent(handleEnt, meta);
						NE::ECS::Component::Hierarchy hr{};
						hr.luid = NE::Core::LUIDGenerator::Generate("hr");
						NE::ECS::Command::AddHierarchyComponent(handleEnt, hr);
						NE::ECS::Component::UIRectTransform handleRect{};
						handleRect.width = 20.0f;
						handleRect.height = 20.0f;
						handleRect.x = -190.0f;  // UIEventSystem formula at value=0: -sliderWidth + handleWidth*0.5
						handleRect.y = -10.0f;   // UIEventSystem formula: -sliderHeight * 0.5
						NE::ECS::Command::AddUIRectTransformComponent(handleEnt, handleRect);
						NE::ECS::Component::UIImage handleImg{};
						handleImg.color = NE::Math::Vec4{ 1.0f, 1.0f, 1.0f, 1.0f }; // White handle
						handleImg.raycastTarget = false;
						NE::ECS::Command::AddUIImageComponent(handleEnt, handleImg);
						NE::ECS::Command::SetParent(handleEnt, entity, -1, false);
					}
					slider.handleRect = handleEnt;

					NE::ECS::Command::AddUISliderComponent(entity, slider);
				}
				if (ImGui::MenuItem("UI Toggle")) {
					uint32_t entity = EditorScene::s_selection.GetLastClicked();
					if (!NE::ECS::Query::HasUIRectTransform(entity)) {
						NE::ECS::Component::UIRectTransform rect{};
						NE::ECS::Command::AddUIRectTransformComponent(entity, rect);
					}
					NE::ECS::Component::UIToggle toggle{};
					toggle.luid = NE::Core::LUIDGenerator::Generate("tg");
					NE::ECS::Command::AddUIToggleComponent(entity, toggle);
				}

				ImGui::Separator();
				ImGui::TextDisabled("UI Layout");

				if (ImGui::MenuItem("UI Horizontal Layout Group")) {
					uint32_t entity = EditorScene::s_selection.GetLastClicked();
					if (!NE::ECS::Query::HasUIRectTransform(entity)) {
						NE::ECS::Component::UIRectTransform rect{};
						NE::ECS::Command::AddUIRectTransformComponent(entity, rect);
					}
					NE::ECS::Component::UILayoutGroup comp{};
					comp.isHorizontal = true;
					comp.childAlignment = NE::ECS::Component::UILayoutGroup::ChildAlignment::MiddleLeft;
					NE::ECS::Command::AddUILayoutGroupComponent(entity, comp);
				}
				if (ImGui::MenuItem("UI Vertical Layout Group")) {
					uint32_t entity = EditorScene::s_selection.GetLastClicked();
					if (!NE::ECS::Query::HasUIRectTransform(entity)) {
						NE::ECS::Component::UIRectTransform rect{};
						NE::ECS::Command::AddUIRectTransformComponent(entity, rect);
					}
					NE::ECS::Component::UILayoutGroup comp{};
					comp.isHorizontal = false;
					comp.childAlignment = NE::ECS::Component::UILayoutGroup::ChildAlignment::UpperCenter;
					NE::ECS::Command::AddUILayoutGroupComponent(entity, comp);
				}
				if (ImGui::MenuItem("UI Grid Layout Group")) {
					uint32_t entity = EditorScene::s_selection.GetLastClicked();
					if (!NE::ECS::Query::HasUIRectTransform(entity)) {
						NE::ECS::Component::UIRectTransform rect{};
						NE::ECS::Command::AddUIRectTransformComponent(entity, rect);
					}
					NE::ECS::Component::UIGridLayoutGroup comp{};
					NE::ECS::Command::AddUIGridLayoutGroupComponent(entity, comp);
				}
				if (ImGui::MenuItem("UI Layout Element")) {
					uint32_t entity = EditorScene::s_selection.GetLastClicked();
					if (!NE::ECS::Query::HasUIRectTransform(entity)) {
						NE::ECS::Component::UIRectTransform rect{};
						NE::ECS::Command::AddUIRectTransformComponent(entity, rect);
					}
					NE::ECS::Component::UILayoutElement comp{};
					NE::ECS::Command::AddUILayoutElementComponent(entity, comp);
				}
				if (ImGui::MenuItem("UI Scroll Rect")) {
					uint32_t entity = EditorScene::s_selection.GetLastClicked();
					if (!NE::ECS::Query::HasUIRectTransform(entity)) {
						NE::ECS::Component::UIRectTransform rect{};
						NE::ECS::Command::AddUIRectTransformComponent(entity, rect);
					}
					NE::ECS::Component::UIScrollRect comp{};
					NE::ECS::Command::AddUIScrollRectComponent(entity, comp);
				}
				if (ImGui::MenuItem("UI Auto Size")) {
					uint32_t entity = EditorScene::s_selection.GetLastClicked();
					if (!NE::ECS::Query::HasUIRectTransform(entity)) {
						NE::ECS::Component::UIRectTransform rect{};
						NE::ECS::Command::AddUIRectTransformComponent(entity, rect);
					}
					NE::ECS::Component::UIAutoSize comp{};
					NE::ECS::Command::AddUIAutoSizeComponent(entity, comp);
				}
				if (ImGui::MenuItem("UI Dropdown")) {
					uint32_t entity = EditorScene::s_selection.GetLastClicked();
					if (!NE::ECS::Query::HasUIRectTransform(entity)) {
						NE::ECS::Component::UIRectTransform rect{};
						rect.width = 200.0f;
						rect.height = 30.0f;
						NE::ECS::Command::AddUIRectTransformComponent(entity, rect);
					}
					if (!NE::ECS::Query::HasUIImage(entity)) {
						NE::ECS::Component::UIImage img{};
						img.color = NE::Math::Vec4{ 1.0f, 1.0f, 1.0f, 1.0f };
						NE::ECS::Command::AddUIImageComponent(entity, img);
					}
					NE::ECS::Component::UIDropdown comp{};
					comp.luid = NE::Core::LUIDGenerator::Generate("dd");

					// Create caption text child
					uint32_t captionEnt = NE::ECS::Command::CreateEntityNoComponents();
					{
						NE::ECS::Component::EntityMeta meta{};
						meta.name = "Caption";
						meta.luid = NE::Core::LUIDGenerator::Generate("em");
						NE::ECS::Command::AddEntityMetaComponent(captionEnt, meta);
						NE::ECS::Component::Hierarchy hr{};
						hr.luid = NE::Core::LUIDGenerator::Generate("hr");
						NE::ECS::Command::AddHierarchyComponent(captionEnt, hr);
						NE::ECS::Component::UIRectTransform captRect{};
						captRect.width = 200.0f;
						captRect.height = 30.0f;
						NE::ECS::Command::AddUIRectTransformComponent(captionEnt, captRect);
						NE::ECS::Component::UIText txt{};
						txt.text = comp.options.empty() ? "Option A" : comp.options[0];
						txt.color = NE::Math::Vec4{ 0.0f, 0.0f, 0.0f, 1.0f };
						txt.horizontalAlign = NE::ECS::Component::UIText::Alignment::CENTER;
						txt.verticalAlign = NE::ECS::Component::UIText::VerticalAlignment::MIDDLE;
						NE::ECS::Command::AddUITextComponent(captionEnt, txt);
						NE::ECS::Command::SetParent(captionEnt, entity, -1, false);
					}
					comp.captionTextEntity = captionEnt;

					// Create options panel child (initially inactive)
					uint32_t panelEnt = NE::ECS::Command::CreateEntityNoComponents();
					{
						NE::ECS::Component::EntityMeta meta{};
						meta.name = "Options Panel";
						meta.luid = NE::Core::LUIDGenerator::Generate("em");
						meta.isActive = false;
						NE::ECS::Command::AddEntityMetaComponent(panelEnt, meta);
						NE::ECS::Component::Hierarchy hr{};
						hr.luid = NE::Core::LUIDGenerator::Generate("hr");
						NE::ECS::Command::AddHierarchyComponent(panelEnt, hr);
						NE::ECS::Component::UIRectTransform panelRect{};
						panelRect.width = 200.0f;
						panelRect.height = 90.0f;
						panelRect.y = -60.0f;  // below the dropdown button
						NE::ECS::Command::AddUIRectTransformComponent(panelEnt, panelRect);
						NE::ECS::Component::UIImage panelImg{};
						panelImg.color = NE::Math::Vec4{ 0.95f, 0.95f, 0.95f, 1.0f };
						NE::ECS::Command::AddUIImageComponent(panelEnt, panelImg);
						NE::ECS::Component::UILayoutGroup layout{};
						layout.isHorizontal = false;
						layout.spacing = 0.0f;
						NE::ECS::Command::AddUILayoutGroupComponent(panelEnt, layout);
						NE::ECS::Command::SetParent(panelEnt, entity, -1, false);
					}
					comp.optionsPanelEntity = panelEnt;

					// Create 3 default option items inside the panel
					const std::vector<std::string>& opts = comp.options;
					for (size_t i = 0; i < opts.size(); ++i) {
						uint32_t optEnt = NE::ECS::Command::CreateEntityNoComponents();
						NE::ECS::Component::EntityMeta meta{};
						meta.name = std::string("Option ") + std::to_string(i);
						meta.luid = NE::Core::LUIDGenerator::Generate("em");
						NE::ECS::Command::AddEntityMetaComponent(optEnt, meta);
						NE::ECS::Component::Hierarchy hr{};
						hr.luid = NE::Core::LUIDGenerator::Generate("hr");
						NE::ECS::Command::AddHierarchyComponent(optEnt, hr);
						NE::ECS::Component::UIRectTransform optRect{};
						optRect.width = 200.0f;
						optRect.height = 30.0f;
						NE::ECS::Command::AddUIRectTransformComponent(optEnt, optRect);
						NE::ECS::Component::UIImage optImg{};
						optImg.color = NE::Math::Vec4{ 1.0f, 1.0f, 1.0f, 1.0f };
						optImg.raycastTarget = true;
						NE::ECS::Command::AddUIImageComponent(optEnt, optImg);
						NE::ECS::Component::UIText optTxt{};
						optTxt.text = opts[i];
						optTxt.color = NE::Math::Vec4{ 0.0f, 0.0f, 0.0f, 1.0f };
						optTxt.horizontalAlign = NE::ECS::Component::UIText::Alignment::LEFT;
						optTxt.verticalAlign = NE::ECS::Component::UIText::VerticalAlignment::MIDDLE;
						NE::ECS::Command::AddUITextComponent(optEnt, optTxt);
						NE::ECS::Command::SetParent(optEnt, panelEnt, -1, false);
					}

					NE::ECS::Command::AddUIDropdownComponent(entity, comp);
				}
				if (ImGui::MenuItem("UI Input Field")) {
					uint32_t entity = EditorScene::s_selection.GetLastClicked();
					if (!NE::ECS::Query::HasUIRectTransform(entity)) {
						NE::ECS::Component::UIRectTransform rect{};
						rect.width = 200.0f;
						rect.height = 30.0f;
						NE::ECS::Command::AddUIRectTransformComponent(entity, rect);
					}
					if (!NE::ECS::Query::HasUIImage(entity)) {
						NE::ECS::Component::UIImage img{};
						img.color = NE::Math::Vec4{ 1.0f, 1.0f, 1.0f, 1.0f };
						NE::ECS::Command::AddUIImageComponent(entity, img);
					}
					if (!NE::ECS::Query::HasUIText(entity)) {
						NE::ECS::Component::UIText txt{};
						txt.text = "";
						txt.color = NE::Math::Vec4{ 0.0f, 0.0f, 0.0f, 1.0f };
						txt.horizontalAlign = NE::ECS::Component::UIText::Alignment::LEFT;
						txt.verticalAlign = NE::ECS::Component::UIText::VerticalAlignment::MIDDLE;
						NE::ECS::Command::AddUITextComponent(entity, txt);
					}
					NE::ECS::Component::UIInputField comp{};
					comp.luid = NE::Core::LUIDGenerator::Generate("if");
					NE::ECS::Command::AddUIInputFieldComponent(entity, comp);
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

		bool isActiveValue = NE::ECS::Query::GetActive(entity);
		if (DrawCheckbox("##isActive", isActiveValue)) {
			NE::ECS::Command::ToggleActive(entity, isActiveValue);
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

		bool staticLightmap = metaRO.isStatic;
		if (DrawCheckbox("Static Lightmap", staticLightmap)) {
			using Cmd = Editor::SetFieldCommand<Owner, bool>;
			auto cmd = std::make_unique<Cmd>(
				entity,
				std::string("Toggle Static Lightmap"),
				&Owner::isStatic,
				metaRO.isStatic,
				staticLightmap,
				&NE::ECS::Command::GetEntityMeta
			);
			Editor::CommandHistory::GetInstance().ExecuteCommand(std::move(cmd));
		}

		std::string allocationStatus = "Not Run";
		std::string allocationDetail;

		if (!NE::ECS::Command::GetEntityMeta(entity).isStatic) {
			allocationStatus = "Opted Out";
			allocationDetail = "Enable Static Lightmap to include this renderer in atlas allocation.";
		} else if (const auto* preview = Editor::Lightmapping::FindLightmapEntityPreviewStatus(entity)) {
			allocationStatus = Editor::Lightmapping::ToString(preview->kind);
			allocationDetail = preview->message;
			if (preview->kind == Editor::Lightmapping::LightmapEntityStatusKind::Allocated &&
				!preview->pageId.empty()) {
				allocationDetail += " UV scale: (" +
					std::to_string(preview->uvScale.x) + ", " +
					std::to_string(preview->uvScale.y) + ")";
			}
		} else if (NE::ECS::Query::HasLightmapBinding(entity)) {
			const auto& binding = NE::ECS::Query::GetLightmapBinding(entity);
			if (binding.enabled && !binding.pageId.empty()) {
				allocationStatus = "Allocated";
				allocationDetail = "Current binding points to " + binding.pageId + ".";
			} else {
				allocationStatus = "Pending";
				allocationDetail = "Entity is opted in but has no fresh allocation preview yet.";
			}
		} else {
			allocationStatus = "Pending";
			allocationDetail = "Entity is opted in but has not been allocated yet.";
		}

		ImGui::Spacing();
		ImGui::Text("Lightmap Status");
		ImGui::SameLine();
		ImGui::TextDisabled("%s", allocationStatus.c_str());
		if (!allocationDetail.empty()) {
			ImGui::TextWrapped("%s", allocationDetail.c_str());
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

	void InspectorPanel::DrawDecalProjectorComponent(uint32_t entity) {
		auto& comp = NE::ECS::Query::GetDecalProjector(entity);

		bool copyComp = false;
		bool deleteComp = false;

		const bool open = DrawComponentHeaderWithMenu(
			"Decal Projector",
			true,
			&copyComp,
			&deleteComp
		);

		NE::Renderer::Command::DrawSelectedDecalGizmos(comp, NE::ECS::Query::GetEntityTransform(entity));

		if (!open)
			return;

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

		NE::Core::ForEachFieldView<NE::ECS::Component::DecalProjector>(comp,
			[&](auto const& desc, auto const& currentValue) {
				using FieldT = std::decay_t<decltype(currentValue)>;

				FieldT edited = currentValue;

				if (DrawField(desc, edited)) {
					SubmitSetFieldCommand<NE::ECS::Component::DecalProjector, FieldT>(
						entity, desc, currentValue, edited
					);
				}
			}
		);

		if (copyComp) {

		}
		if (deleteComp) {
			NE::ECS::Command::RemoveDecalProjectorComponent(entity);
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

		NE::Renderer::Command::DrawSelectedLightGizmos(comp);

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
			}
		}

		if (comp.type == Light::Type::Area && comp.shadowUpdateMode == Light::ShadowUpdateMode::Realtime) {
			comp.shadowUpdateMode = Light::ShadowUpdateMode::StaticBake;
		}

		if (comp.type == Light::Type::Area) {
			static const char* areaShadowUpdateModeNames[] = { "NoneUpdate", "StaticBake" };
			int areaShadowUpdateMode = (comp.shadowUpdateMode == Light::ShadowUpdateMode::StaticBake) ? 1 : 0;
			if (DrawEnumPillCombo("Shadow Update Mode", areaShadowUpdateMode, areaShadowUpdateModeNames, IM_ARRAYSIZE(areaShadowUpdateModeNames), 300.0f)) {
				comp.shadowUpdateMode = (areaShadowUpdateMode == 1)
					? Light::ShadowUpdateMode::StaticBake
					: Light::ShadowUpdateMode::NoneUpdate;
			}
		} else {
			static const char* shadowUpdateModeNames[] = { "NoneUpdate", "Realtime", "StaticBake" };
			int shadowUpdateMode = static_cast<int>(comp.shadowUpdateMode);

			if (DrawEnumPillCombo("Shadow Update Mode", shadowUpdateMode, shadowUpdateModeNames, IM_ARRAYSIZE(shadowUpdateModeNames), 300.0f)) {
				comp.shadowUpdateMode =
					static_cast<Light::ShadowUpdateMode>(shadowUpdateMode);
			}
		}

		if (comp.shadowUpdateMode != Light::ShadowUpdateMode::NoneUpdate) {
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
			if (ImSearch::BeginSearch(ImSearchFlags_NoTextHighlighting)) {
				ImSearch::SearchBar();

				auto scriptNames = NE::ECS::Command::GetRegisteredScriptNames();
				for (const auto& scriptName : scriptNames) {
					ImSearch::SearchableItem(scriptName.c_str(), [&, entity, scriptName](const char*) {
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
					});
				}

				ImSearch::EndSearch();
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
		auto& comp = NE::ECS::Command::GetEntityCamera(entity);

		bool copyComp = false;
		bool deleteComp = false;

		const bool open = DrawComponentHeaderWithMenu(
			"Camera",
			true,
			&copyComp,
			&deleteComp
		);

		if (copyComp) {

		}
		if (deleteComp) {
			NE::ECS::Command::RemoveCameraComponent(entity);
		}

		if (!open)
			return;

		static const char* ProjectionTypeNames[] = { "Perspective", "Orthographic" };
		int currProjectionType = static_cast<int>(comp.projectionType);

		if (DrawEnumPillCombo("Projection", currProjectionType, ProjectionTypeNames, IM_ARRAYSIZE(ProjectionTypeNames), 100.0f)) {
			comp.projectionType = static_cast<NE::ECS::Component::Camera::ProjectionType>(currProjectionType);
			comp.isDirty = true;
		}

		static const char* fovAxis[] = { "Vertical", "Horizontal" };
		const auto prevAxis = comp.fovAxis;
		int currAxis = static_cast<int>(comp.fovAxis);

		if (DrawEnumPillCombo("FOV Axis", currAxis, fovAxis, IM_ARRAYSIZE(fovAxis), 100.0f)) {
			const auto newAxis = static_cast<NE::ECS::Component::Camera::FieldOfViewAxis>(currAxis);

			if (newAxis != prevAxis) {
				float aspect = comp.aspectRatio;
				if (comp.isMain) {
					const uint32_t w = NE::GetGameViewWidth();
					const uint32_t h = NE::GetGameViewHeight();
					aspect = (h > 0) ? (static_cast<float>(w) / static_cast<float>(h)) : (16.0f / 9.0f);
				}

				if (comp.projectionType == NE::ECS::Component::Camera::ProjectionType::Perspective) {
					if (prevAxis == NE::ECS::Component::Camera::FieldOfViewAxis::Vertical &&
						newAxis == NE::ECS::Component::Camera::FieldOfViewAxis::Horizontal) {
						comp.fovY = ConvertFovDegrees(comp.fovY, aspect, true);
					} else if (prevAxis == NE::ECS::Component::Camera::FieldOfViewAxis::Horizontal &&
						newAxis == NE::ECS::Component::Camera::FieldOfViewAxis::Vertical) {
						comp.fovY = ConvertFovDegrees(comp.fovY, aspect, false);
					}
				} else {
					aspect = std::max(1e-6f, aspect);
					if (prevAxis == NE::ECS::Component::Camera::FieldOfViewAxis::Vertical &&
						newAxis == NE::ECS::Component::Camera::FieldOfViewAxis::Horizontal) {
						comp.fovY *= aspect;
					} else if (prevAxis == NE::ECS::Component::Camera::FieldOfViewAxis::Horizontal &&
						newAxis == NE::ECS::Component::Camera::FieldOfViewAxis::Vertical) {
						comp.fovY /= aspect;
					}
				}
			}

			comp.fovAxis = newAxis;
			comp.isDirty = true;
		}

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
			}
		);

		ImGui::TreePop();
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

		if (copyComp) {

		}
		if (deleteComp) {
			NE::ECS::Command::RemoveAnimatorComponent(entity);
		}

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
					uint32_t currentParent = NE::ECS::Query::HasHierarchy(entity) ? NE::ECS::Query::GetEntityHierarchy(entity).parent : NE::ECS::NO_ENTITY;
					while (currentParent != NE::ECS::NO_ENTITY) {
						if (NE::ECS::Query::HasUICanvas(currentParent)) {
							auto& parentCanvas = NE::ECS::Query::GetUICanvas(currentParent);
							isOverlay = (parentCanvas.renderMode == NE::ECS::Component::UICanvas::RenderMode::SCREEN_SPACE_OVERLAY);
							break;
						}
						if (!NE::ECS::Query::HasHierarchy(currentParent)) break;
						currentParent = NE::ECS::Query::GetEntityHierarchy(currentParent).parent;
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
				UI_RECT_DRAG("##ScaleX", &NE::ECS::Component::UIRectTransform::scaleX, 0.01f, 0.01f, FLT_MAX, "%.2f");
				ImGui::SameLine();
				ImGui::Text("Y");
				ImGui::SameLine();
				UI_RECT_DRAG("##ScaleY", &NE::ECS::Component::UIRectTransform::scaleY, 0.01f, 0.01f, FLT_MAX, "%.2f");

				if (!isOverlay) {
					ImGui::SameLine();
					ImGui::Text("Z");
					ImGui::SameLine();
					UI_RECT_DRAG("##ScaleZ", &NE::ECS::Component::UIRectTransform::scaleZ, 0.01f, 0.01f, FLT_MAX, "%.2f");
				}

				ImGui::PopItemWidth();
			}

			// Mask Clipping section
			ImGui::Separator();
			if (ImGui::TreeNode("Mask Clipping")) {
				float lw = 150.0f;
				{
					ImGui::AlignTextToFramePadding();
					ImGui::Text("Enable Mask");
					ImGui::SameLine(lw);
					ImGui::SetNextItemWidth(-1);
					ImGui::Checkbox("##EnableMask", &comp.enableMask);
				}
				if (comp.enableMask) {
					{
						ImGui::AlignTextToFramePadding();
						ImGui::Text("Mask Pad Left");
						ImGui::SameLine(lw);
						ImGui::SetNextItemWidth(-1);
						ImGui::DragFloat("##MaskPadLeft", &comp.maskPaddingLeft, 0.5f);
					}
					{
						ImGui::AlignTextToFramePadding();
						ImGui::Text("Mask Pad Right");
						ImGui::SameLine(lw);
						ImGui::SetNextItemWidth(-1);
						ImGui::DragFloat("##MaskPadRight", &comp.maskPaddingRight, 0.5f);
					}
					{
						ImGui::AlignTextToFramePadding();
						ImGui::Text("Mask Pad Top");
						ImGui::SameLine(lw);
						ImGui::SetNextItemWidth(-1);
						ImGui::DragFloat("##MaskPadTop", &comp.maskPaddingTop, 0.5f);
					}
					{
						ImGui::AlignTextToFramePadding();
						ImGui::Text("Mask Pad Bottom");
						ImGui::SameLine(lw);
						ImGui::SetNextItemWidth(-1);
						ImGui::DragFloat("##MaskPadBottom", &comp.maskPaddingBottom, 0.5f);
					}
				}
				ImGui::TreePop();
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

			// Opacity
			ImGui::AlignTextToFramePadding();
			ImGui::Text("Opacity");
			ImGui::SameLine(labelWidth);
			ImGui::SetNextItemWidth(-1);
			ImGui::SliderFloat("##CanvasAlpha", &comp.alpha, 0.0f, 1.0f);

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
						uint32_t current = NE::ECS::Query::HasHierarchy(entity) ? NE::ECS::Query::GetEntityHierarchy(entity).parent : NE::ECS::NO_ENTITY;

						// walk up hierarchy to find canvas
						while (current != NE::ECS::NO_ENTITY) {
							if (NE::ECS::Query::HasUICanvas(current)) {
								canvasEntity = current;
								break;
							}
							if (NE::ECS::Query::HasHierarchy(current)) {
								current = NE::ECS::Query::GetEntityHierarchy(current).parent;
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

	void InspectorPanel::DrawTextComponent(uint32_t entity) {
		auto& comp = NE::ECS::Command::GetUIText(entity);

		bool copyComp = false;
		bool deleteComp = false;

		const bool open = DrawComponentHeaderWithMenu(
			"Text",
			true,
			&copyComp,
			&deleteComp
		);

		if (!open)
			return;

		float labelWidth = 120.0f;
		ImGui::Indent();

		// Text content
		{
			ImGui::AlignTextToFramePadding();
			ImGui::Text("Text");
			ImGui::SameLine(labelWidth);
			ImGui::SetNextItemWidth(-1);

			char buffer[1024];
			strncpy_s(buffer, comp.text.c_str(), sizeof(buffer));
			if (ImGui::InputTextMultiline("##Text", buffer, sizeof(buffer), ImVec2(-1, 60))) {
				comp.text = buffer;
				comp.isDirty = true;
			}
		}

		// Font Size
		{
			ImGui::AlignTextToFramePadding();
			ImGui::Text("Font Size");
			ImGui::SameLine(labelWidth);
			ImGui::SetNextItemWidth(-1);
			if (ImGui::DragFloat("##FontSize", &comp.fontSize, 1.0f, 1.0f, 200.0f)) {
				comp.isDirty = true;
			}
		}

		// Color
		{
			ImGui::AlignTextToFramePadding();
			ImGui::Text("Color");
			ImGui::SameLine(labelWidth);
			ImGui::SetNextItemWidth(-1);
			float color[4] = { comp.color.x, comp.color.y, comp.color.z, comp.color.w };
			if (ImGui::ColorEdit4("##TextColor", color, ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreviewHalf)) {
				comp.color.x = color[0];
				comp.color.y = color[1];
				comp.color.z = color[2];
				comp.color.w = color[3];
				comp.isDirty = true;
			}
		}

		// Horizontal Alignment
		{
			ImGui::AlignTextToFramePadding();
			ImGui::Text("Horizontal Align");
			ImGui::SameLine(labelWidth);
			ImGui::SetNextItemWidth(-1);

			static const char* HAlignOptions[] = { "Left", "Center", "Right" };
			int currentHAlign = static_cast<int>(comp.horizontalAlign);
			if (ImGui::Combo("##HAlign", &currentHAlign, HAlignOptions, IM_ARRAYSIZE(HAlignOptions))) {
				comp.horizontalAlign = static_cast<NE::ECS::Component::UIText::Alignment>(currentHAlign);
				comp.isDirty = true;
			}
		}

		// Vertical Alignment
		{
			ImGui::AlignTextToFramePadding();
			ImGui::Text("Vertical Align");
			ImGui::SameLine(labelWidth);
			ImGui::SetNextItemWidth(-1);

			static const char* VAlignOptions[] = { "Top", "Middle", "Bottom" };
			int currentVAlign = static_cast<int>(comp.verticalAlign);
			if (ImGui::Combo("##VAlign", &currentVAlign, VAlignOptions, IM_ARRAYSIZE(VAlignOptions))) {
				comp.verticalAlign = static_cast<NE::ECS::Component::UIText::VerticalAlignment>(currentVAlign);
				comp.isDirty = true;
			}
		}

		// Word Wrap
		{
			ImGui::AlignTextToFramePadding();
			ImGui::Text("Word Wrap");
			ImGui::SameLine(labelWidth);
			ImGui::SetNextItemWidth(-1);
			if (ImGui::Checkbox("##WordWrap", &comp.wordWrap)) {
				comp.isDirty = true;
			}
		}

		// Line Spacing
		{
			ImGui::AlignTextToFramePadding();
			ImGui::Text("Line Spacing");
			ImGui::SameLine(labelWidth);
			ImGui::SetNextItemWidth(-1);
			if (ImGui::DragFloat("##LineSpacing", &comp.lineSpacing, 0.01f, 0.1f, 10.0f)) {
				comp.isDirty = true;
			}
		}

		// Rich Text
		{
			ImGui::AlignTextToFramePadding();
			ImGui::Text("Rich Text");
			ImGui::SameLine(labelWidth);
			ImGui::SetNextItemWidth(-1);
			if (ImGui::Checkbox("##RichText", &comp.richText)) {
				comp.isDirty = true;
			}
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip("Enable tags: <color=#RRGGBB>, <size=N>, <b>, <i>");
			}
		}

		// Auto Scale (Best Fit)
		{
			ImGui::AlignTextToFramePadding();
			ImGui::Text("Auto Scale");
			ImGui::SameLine(labelWidth);
			ImGui::SetNextItemWidth(-1);
			if (ImGui::Checkbox("##AutoScale", &comp.autoScale)) {
				comp.isDirty = true;
			}
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip("Automatically scale font size to fit within bounds");
			}
		}

		// Min/Max Font Size (only show if auto-scale is enabled)
		if (comp.autoScale) {
			// Min Font Size
			{
				ImGui::AlignTextToFramePadding();
				ImGui::Text("Min Font Size");
				ImGui::SameLine(labelWidth);
				ImGui::SetNextItemWidth(-1);
				if (ImGui::DragFloat("##MinFontSize", &comp.minFontSize, 1.0f, 1.0f, comp.maxFontSize)) {
					comp.isDirty = true;
				}
			}

			// Max Font Size
			{
				ImGui::AlignTextToFramePadding();
				ImGui::Text("Max Font Size");
				ImGui::SameLine(labelWidth);
				ImGui::SetNextItemWidth(-1);
				if (ImGui::DragFloat("##MaxFontSize", &comp.maxFontSize, 1.0f, comp.minFontSize, 500.0f)) {
					comp.isDirty = true;
				}
			}
		}

		// Font (drag-drop area for .ttf/.otf files)
		{
			ImGui::AlignTextToFramePadding();
			ImGui::Text("Font");
			ImGui::SameLine(labelWidth);
			ImGui::SetNextItemWidth(-1);

			std::string fontLabel = comp.fontUUID.empty()
				? "(none)"
				: Assets::AssetManager::GetInstance().RetrieveFilename(comp.fontUUID);

			char bufFont[256];
			strncpy_s(bufFont, fontLabel.c_str(), sizeof(bufFont));
			ImGui::InputText("##Font", bufFont, sizeof(bufFont), ImGuiInputTextFlags_ReadOnly);

			if (ImGui::BeginDragDropTarget()) {
				if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload("FONT_ASSET_PATH")) {
					std::string dropped((const char*)p->Data, p->DataSize - 1);
					auto fontUUID = Assets::AssetManager::GetInstance().RetrieveUUID(dropped);

					// Validate that the asset is actually a Font type
					if (!fontUUID.empty()) {
						auto* record = Assets::AssetManager::GetInstance().GetRecord(fontUUID);
						if (record && record->type == Assets::AssetType::Font) {
							comp.fontUUID = fontUUID;
							comp.isDirty = true;
						}
					}
				}
				ImGui::EndDragDropTarget();
			}
		}

		if (deleteComp) {
			// TODO: implement remove UIText component
		}

		ImGui::Unindent();
		ImGui::TreePop();
	}

	void InspectorPanel::DrawButtonComponent(uint32_t entity) {
		auto& comp = NE::ECS::Command::GetUIButton(entity);

		bool copyComp = false;
		bool deleteComp = false;

		const bool open = DrawComponentHeaderWithMenu(
			"Button",
			true,
			&copyComp,
			&deleteComp
		);

		if (!open)
			return;

		float labelWidth = 120.0f;
		ImGui::Indent();

		// Interactable
		{
			ImGui::AlignTextToFramePadding();
			ImGui::Text("Interactable");
			ImGui::SameLine(labelWidth);
			ImGui::SetNextItemWidth(-1);
			ImGui::Checkbox("##Interactable", &comp.interactable);
		}

		// Normal Color
		{
			ImGui::AlignTextToFramePadding();
			ImGui::Text("Normal Color");
			ImGui::SameLine(labelWidth);
			ImGui::SetNextItemWidth(-1);
			float color[4] = { comp.normalColor.x, comp.normalColor.y, comp.normalColor.z, comp.normalColor.w };
			if (ImGui::ColorEdit4("##NormalColor", color, ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreviewHalf)) {
				comp.normalColor.x = color[0];
				comp.normalColor.y = color[1];
				comp.normalColor.z = color[2];
				comp.normalColor.w = color[3];
			}
		}

		// Hovered Color
		{
			ImGui::AlignTextToFramePadding();
			ImGui::Text("Hovered Color");
			ImGui::SameLine(labelWidth);
			ImGui::SetNextItemWidth(-1);
			float color[4] = { comp.hoverColor.x, comp.hoverColor.y, comp.hoverColor.z, comp.hoverColor.w };
			if (ImGui::ColorEdit4("##HoveredColor", color, ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreviewHalf)) {
				comp.hoverColor.x = color[0];
				comp.hoverColor.y = color[1];
				comp.hoverColor.z = color[2];
				comp.hoverColor.w = color[3];
			}
		}

		// Pressed Color
		{
			ImGui::AlignTextToFramePadding();
			ImGui::Text("Pressed Color");
			ImGui::SameLine(labelWidth);
			ImGui::SetNextItemWidth(-1);
			float color[4] = { comp.pressedColor.x, comp.pressedColor.y, comp.pressedColor.z, comp.pressedColor.w };
			if (ImGui::ColorEdit4("##PressedColor", color, ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreviewHalf)) {
				comp.pressedColor.x = color[0];
				comp.pressedColor.y = color[1];
				comp.pressedColor.z = color[2];
				comp.pressedColor.w = color[3];
			}
		}

		// Disabled Color
		{
			ImGui::AlignTextToFramePadding();
			ImGui::Text("Disabled Color");
			ImGui::SameLine(labelWidth);
			ImGui::SetNextItemWidth(-1);
			float color[4] = { comp.disabledColor.x, comp.disabledColor.y, comp.disabledColor.z, comp.disabledColor.w };
			if (ImGui::ColorEdit4("##DisabledColor", color, ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreviewHalf)) {
				comp.disabledColor.x = color[0];
				comp.disabledColor.y = color[1];
				comp.disabledColor.z = color[2];
				comp.disabledColor.w = color[3];
			}
		}

		// Current State (read-only, for debugging)
		{
			ImGui::AlignTextToFramePadding();
			ImGui::Text("Current State");
			ImGui::SameLine(labelWidth);
			ImGui::SetNextItemWidth(-1);

			const char* stateNames[] = { "Normal", "Hovered", "Pressed", "Disabled" };
			int currentState = static_cast<int>(comp.currentState);
			ImGui::TextDisabled("%s", stateNames[currentState]);
		}

		if (deleteComp) {
			// TODO: implement remove UIButton component
		}

		ImGui::Unindent();
		ImGui::TreePop();
	}

	void InspectorPanel::DrawSliderComponent(uint32_t entity) {
		auto& comp = NE::ECS::Command::GetUISlider(entity);

		bool copyComp = false;
		bool deleteComp = false;

		const bool open = DrawComponentHeaderWithMenu(
			"Slider",
			true,
			&copyComp,
			&deleteComp
		);

		if (!open)
			return;

		ImGui::Indent();
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.8f, 0.2f, 1.0f));
		ImGui::TextWrapped("(!) UISlider is not production-ready. Handle positioning and value clamping may behave incorrectly.");
		ImGui::PopStyleColor();
		ImGui::Spacing();

		float labelWidth = 120.0f;

		// Interactable
		{
			ImGui::AlignTextToFramePadding();
			ImGui::Text("Interactable");
			ImGui::SameLine(labelWidth);
			ImGui::SetNextItemWidth(-1);
			ImGui::Checkbox("##Interactable", &comp.interactable);
		}

		// Value
		{
			ImGui::AlignTextToFramePadding();
			ImGui::Text("Value");
			ImGui::SameLine(labelWidth);
			ImGui::SetNextItemWidth(-1);
			if (ImGui::SliderFloat("##Value", &comp.value, comp.minValue, comp.maxValue)) {
				if (comp.wholeNumbers) {
					comp.value = static_cast<float>(static_cast<int>(comp.value + 0.5f));
				}
				comp.ClampValue();
			}
		}

		// Min Value
		{
			ImGui::AlignTextToFramePadding();
			ImGui::Text("Min Value");
			ImGui::SameLine(labelWidth);
			ImGui::SetNextItemWidth(-1);
			if (ImGui::DragFloat("##MinValue", &comp.minValue, 0.1f)) {
				if (comp.minValue > comp.maxValue) comp.minValue = comp.maxValue;
				comp.ClampValue();
			}
		}

		// Max Value
		{
			ImGui::AlignTextToFramePadding();
			ImGui::Text("Max Value");
			ImGui::SameLine(labelWidth);
			ImGui::SetNextItemWidth(-1);
			if (ImGui::DragFloat("##MaxValue", &comp.maxValue, 0.1f)) {
				if (comp.maxValue < comp.minValue) comp.maxValue = comp.minValue;
				comp.ClampValue();
			}
		}

		// Whole Numbers
		{
			ImGui::AlignTextToFramePadding();
			ImGui::Text("Whole Numbers");
			ImGui::SameLine(labelWidth);
			ImGui::SetNextItemWidth(-1);
			if (ImGui::Checkbox("##WholeNumbers", &comp.wholeNumbers)) {
				if (comp.wholeNumbers) {
					comp.value = static_cast<float>(static_cast<int>(comp.value + 0.5f));
				}
			}
		}

		// Direction
		{
			ImGui::AlignTextToFramePadding();
			ImGui::Text("Direction");
			ImGui::SameLine(labelWidth);
			ImGui::SetNextItemWidth(-1);

			static const char* DirectionNames[] = { "Left To Right", "Right To Left", "Bottom To Top", "Top To Bottom" };
			int currentDirection = static_cast<int>(comp.direction);
			if (ImGui::Combo("##Direction", &currentDirection, DirectionNames, IM_ARRAYSIZE(DirectionNames))) {
				comp.direction = static_cast<NE::ECS::Component::UISlider::Direction>(currentDirection);
			}
		}

		// Fill Rect (entity reference - read only for now)
		{
			ImGui::AlignTextToFramePadding();
			ImGui::Text("Fill Rect");
			ImGui::SameLine(labelWidth);
			ImGui::SetNextItemWidth(-1);
			ImGui::TextDisabled("Entity %u", comp.fillRect);
		}

		// Handle Rect (entity reference - read only for now)
		{
			ImGui::AlignTextToFramePadding();
			ImGui::Text("Handle Rect");
			ImGui::SameLine(labelWidth);
			ImGui::SetNextItemWidth(-1);
			ImGui::TextDisabled("Entity %u", comp.handleRect);
		}

		if (deleteComp) {
			// TODO: implement remove UISlider component
		}

		ImGui::Unindent();
		ImGui::TreePop();
	}

	void InspectorPanel::DrawToggleComponent(uint32_t entity) {
		auto& comp = NE::ECS::Command::GetUIToggle(entity);

		bool copyComp = false;
		bool deleteComp = false;

		const bool open = DrawComponentHeaderWithMenu(
			"Toggle",
			true,
			&copyComp,
			&deleteComp
		);

		if (!open)
			return;

		float labelWidth = 120.0f;
		ImGui::Indent();

		// Interactable
		{
			ImGui::AlignTextToFramePadding();
			ImGui::Text("Interactable");
			ImGui::SameLine(labelWidth);
			ImGui::SetNextItemWidth(-1);
			ImGui::Checkbox("##Interactable", &comp.interactable);
		}

		// Is On
		{
			ImGui::AlignTextToFramePadding();
			ImGui::Text("Is On");
			ImGui::SameLine(labelWidth);
			ImGui::SetNextItemWidth(-1);
			ImGui::Checkbox("##IsOn", &comp.isOn);
		}

		// Toggle Group
		{
			ImGui::AlignTextToFramePadding();
			ImGui::Text("Toggle Group");
			ImGui::SameLine(labelWidth);
			ImGui::SetNextItemWidth(-1);
			int group = static_cast<int>(comp.toggleGroup);
			if (ImGui::InputInt("##ToggleGroup", &group)) {
				comp.toggleGroup = static_cast<uint32_t>(group < 0 ? 0 : group);
			}
		}

		// Graphic (checkmark entity reference - read only for now)
		{
			ImGui::AlignTextToFramePadding();
			ImGui::Text("Graphic");
			ImGui::SameLine(labelWidth);
			ImGui::SetNextItemWidth(-1);
			ImGui::TextDisabled("Entity %u", comp.graphic);
		}

		// Background (entity reference - read only for now)
		{
			ImGui::AlignTextToFramePadding();
			ImGui::Text("Background");
			ImGui::SameLine(labelWidth);
			ImGui::SetNextItemWidth(-1);
			ImGui::TextDisabled("Entity %u", comp.background);
		}

		if (deleteComp) {
			// TODO: implement remove UIToggle component
		}

		ImGui::Unindent();
		ImGui::TreePop();
	}

	void InspectorPanel::DrawLayoutGroupComponent(uint32_t entity) {
		auto& comp = NE::ECS::Command::GetUILayoutGroup(entity);

		bool copyComp = false;
		bool deleteComp = false;

		const char* title = comp.isHorizontal ? "Horizontal Layout Group" : "Vertical Layout Group";
		const bool open = DrawComponentHeaderWithMenu(
			title,
			true,
			&copyComp,
			&deleteComp
		);

		if (!open)
			return;

		float labelWidth = 150.0f;
		ImGui::Indent();

		{
			ImGui::AlignTextToFramePadding();
			ImGui::Text("Direction");
			ImGui::SameLine(labelWidth);
			ImGui::SetNextItemWidth(-1);
			static const char* DirNames[] = { "Vertical", "Horizontal" };
			int dirIdx = comp.isHorizontal ? 1 : 0;
			if (ImGui::Combo("##Direction", &dirIdx, DirNames, IM_ARRAYSIZE(DirNames))) {
				comp.isHorizontal = (dirIdx == 1);
			}
		}

		ImGui::Separator();
		ImGui::TextDisabled("Padding");
		{
			ImGui::AlignTextToFramePadding();
			ImGui::Text("Left");
			ImGui::SameLine(labelWidth);
			ImGui::SetNextItemWidth(-1);
			ImGui::DragFloat("##PaddingLeft", &comp.paddingLeft, 0.5f);
		}
		{
			ImGui::AlignTextToFramePadding();
			ImGui::Text("Right");
			ImGui::SameLine(labelWidth);
			ImGui::SetNextItemWidth(-1);
			ImGui::DragFloat("##PaddingRight", &comp.paddingRight, 0.5f);
		}
		{
			ImGui::AlignTextToFramePadding();
			ImGui::Text("Top");
			ImGui::SameLine(labelWidth);
			ImGui::SetNextItemWidth(-1);
			ImGui::DragFloat("##PaddingTop", &comp.paddingTop, 0.5f);
		}
		{
			ImGui::AlignTextToFramePadding();
			ImGui::Text("Bottom");
			ImGui::SameLine(labelWidth);
			ImGui::SetNextItemWidth(-1);
			ImGui::DragFloat("##PaddingBottom", &comp.paddingBottom, 0.5f);
		}

		ImGui::Separator();

		{
			ImGui::AlignTextToFramePadding();
			ImGui::Text("Spacing");
			ImGui::SameLine(labelWidth);
			ImGui::SetNextItemWidth(-1);
			ImGui::DragFloat("##Spacing", &comp.spacing, 0.5f);
		}

		{
			ImGui::AlignTextToFramePadding();
			ImGui::Text("Child Alignment");
			ImGui::SameLine(labelWidth);
			ImGui::SetNextItemWidth(-1);
			static const char* AlignNames[] = { "Upper Left", "Upper Center", "Upper Right", "Middle Left", "Middle Center", "Middle Right", "Lower Left", "Lower Center", "Lower Right" };
			int alignIdx = static_cast<int>(comp.childAlignment);
			if (ImGui::Combo("##ChildAlignment", &alignIdx, AlignNames, IM_ARRAYSIZE(AlignNames)))
				comp.childAlignment = static_cast<NE::ECS::Component::UILayoutGroup::ChildAlignment>(alignIdx);
		}

		{
			ImGui::AlignTextToFramePadding();
			ImGui::Text("Reverse Arrangement");
			ImGui::SameLine(labelWidth);
			ImGui::SetNextItemWidth(-1);
			ImGui::Checkbox("##ReverseArrangement", &comp.reverseArrangement);
		}

		ImGui::Separator();
		ImGui::TextDisabled("Child Controls");

		{
			ImGui::AlignTextToFramePadding();
			ImGui::Text("Control Width");
			ImGui::SameLine(labelWidth);
			ImGui::SetNextItemWidth(-1);
			ImGui::Checkbox("##ControlChildWidth", &comp.controlChildWidth);
		}
		{
			ImGui::AlignTextToFramePadding();
			ImGui::Text("Control Height");
			ImGui::SameLine(labelWidth);
			ImGui::SetNextItemWidth(-1);
			ImGui::Checkbox("##ControlChildHeight", &comp.controlChildHeight);
		}
		{
			ImGui::AlignTextToFramePadding();
			ImGui::Text("Force Expand Width");
			ImGui::SameLine(labelWidth);
			ImGui::SetNextItemWidth(-1);
			ImGui::Checkbox("##ForceExpandWidth", &comp.childForceExpandWidth);
		}
		{
			ImGui::AlignTextToFramePadding();
			ImGui::Text("Force Expand Height");
			ImGui::SameLine(labelWidth);
			ImGui::SetNextItemWidth(-1);
			ImGui::Checkbox("##ForceExpandHeight", &comp.childForceExpandHeight);
		}

		if (deleteComp) {
			// TODO: implement remove
		}

		ImGui::Unindent();
		ImGui::TreePop();
	}

	void InspectorPanel::DrawGridLayoutGroupComponent(uint32_t entity) {
		auto& comp = NE::ECS::Command::GetUIGridLayoutGroup(entity);

		bool copyComp = false;
		bool deleteComp = false;

		const bool open = DrawComponentHeaderWithMenu(
			"Grid Layout Group",
			true,
			&copyComp,
			&deleteComp
		);

		if (!open)
			return;

		float labelWidth = 150.0f;
		ImGui::Indent();

		// Padding
		ImGui::TextDisabled("Padding");
		{
			ImGui::AlignTextToFramePadding();
			ImGui::Text("Left");
			ImGui::SameLine(labelWidth);
			ImGui::SetNextItemWidth(-1);
			ImGui::DragFloat("##PaddingLeft", &comp.paddingLeft, 0.5f);
		}
		{
			ImGui::AlignTextToFramePadding();
			ImGui::Text("Right");
			ImGui::SameLine(labelWidth);
			ImGui::SetNextItemWidth(-1);
			ImGui::DragFloat("##PaddingRight", &comp.paddingRight, 0.5f);
		}
		{
			ImGui::AlignTextToFramePadding();
			ImGui::Text("Top");
			ImGui::SameLine(labelWidth);
			ImGui::SetNextItemWidth(-1);
			ImGui::DragFloat("##PaddingTop", &comp.paddingTop, 0.5f);
		}
		{
			ImGui::AlignTextToFramePadding();
			ImGui::Text("Bottom");
			ImGui::SameLine(labelWidth);
			ImGui::SetNextItemWidth(-1);
			ImGui::DragFloat("##PaddingBottom", &comp.paddingBottom, 0.5f);
		}

		ImGui::Separator();

		{
			ImGui::AlignTextToFramePadding();
			ImGui::Text("Cell Size X");
			ImGui::SameLine(labelWidth);
			ImGui::SetNextItemWidth(-1);
			ImGui::DragFloat("##CellWidth", &comp.cellWidth, 0.5f, 1.0f, 10000.0f);
		}
		{
			ImGui::AlignTextToFramePadding();
			ImGui::Text("Cell Size Y");
			ImGui::SameLine(labelWidth);
			ImGui::SetNextItemWidth(-1);
			ImGui::DragFloat("##CellHeight", &comp.cellHeight, 0.5f, 1.0f, 10000.0f);
		}

		{
			ImGui::AlignTextToFramePadding();
			ImGui::Text("Spacing X");
			ImGui::SameLine(labelWidth);
			ImGui::SetNextItemWidth(-1);
			ImGui::DragFloat("##SpacingX", &comp.spacingX, 0.5f);
		}
		{
			ImGui::AlignTextToFramePadding();
			ImGui::Text("Spacing Y");
			ImGui::SameLine(labelWidth);
			ImGui::SetNextItemWidth(-1);
			ImGui::DragFloat("##SpacingY", &comp.spacingY, 0.5f);
		}

		ImGui::Separator();

		{
			ImGui::AlignTextToFramePadding();
			ImGui::Text("Start Corner");
			ImGui::SameLine(labelWidth);
			ImGui::SetNextItemWidth(-1);
			static const char* CornerNames[] = { "Upper Left", "Upper Right", "Lower Left", "Lower Right" };
			int cornerIdx = static_cast<int>(comp.startCorner);
			if (ImGui::Combo("##StartCorner", &cornerIdx, CornerNames, IM_ARRAYSIZE(CornerNames)))
				comp.startCorner = static_cast<NE::ECS::Component::UIGridLayoutGroup::StartCorner>(cornerIdx);
		}

		{
			ImGui::AlignTextToFramePadding();
			ImGui::Text("Start Axis");
			ImGui::SameLine(labelWidth);
			ImGui::SetNextItemWidth(-1);
			static const char* AxisNames[] = { "Horizontal", "Vertical" };
			int axisIdx = static_cast<int>(comp.startAxis);
			if (ImGui::Combo("##StartAxis", &axisIdx, AxisNames, IM_ARRAYSIZE(AxisNames)))
				comp.startAxis = static_cast<NE::ECS::Component::UIGridLayoutGroup::StartAxis>(axisIdx);
		}

		{
			ImGui::AlignTextToFramePadding();
			ImGui::Text("Child Alignment");
			ImGui::SameLine(labelWidth);
			ImGui::SetNextItemWidth(-1);
			static const char* AlignNames[] = { "Upper Left", "Upper Center", "Upper Right", "Middle Left", "Middle Center", "Middle Right", "Lower Left", "Lower Center", "Lower Right" };
			int gridAlignIdx = static_cast<int>(comp.childAlignment);
			if (ImGui::Combo("##ChildAlignment", &gridAlignIdx, AlignNames, IM_ARRAYSIZE(AlignNames)))
				comp.childAlignment = static_cast<NE::ECS::Component::UIGridLayoutGroup::ChildAlignment>(gridAlignIdx);
		}

		ImGui::Separator();

		{
			ImGui::AlignTextToFramePadding();
			ImGui::Text("Constraint");
			ImGui::SameLine(labelWidth);
			ImGui::SetNextItemWidth(-1);
			static const char* ConstraintNames[] = { "Flexible", "Fixed Column Count", "Fixed Row Count" };
			int constraintIdx = static_cast<int>(comp.constraint);
			if (ImGui::Combo("##Constraint", &constraintIdx, ConstraintNames, IM_ARRAYSIZE(ConstraintNames)))
				comp.constraint = static_cast<NE::ECS::Component::UIGridLayoutGroup::Constraint>(constraintIdx);
		}

		if (comp.constraint != NE::ECS::Component::UIGridLayoutGroup::Constraint::Flexible) {
			ImGui::AlignTextToFramePadding();
			ImGui::Text("Constraint Count");
			ImGui::SameLine(labelWidth);
			ImGui::SetNextItemWidth(-1);
			ImGui::InputInt("##ConstraintCount", &comp.constraintCount);
			if (comp.constraintCount < 1) comp.constraintCount = 1;
		}

		if (deleteComp) {
			// TODO: implement remove
		}

		ImGui::Unindent();
		ImGui::TreePop();
	}

	void InspectorPanel::DrawLayoutElementComponent(uint32_t entity) {
		auto& comp = NE::ECS::Command::GetUILayoutElement(entity);

		bool copyComp = false;
		bool deleteComp = false;

		const bool open = DrawComponentHeaderWithMenu(
			"Layout Element",
			true,
			&copyComp,
			&deleteComp
		);

		if (!open)
			return;

		float labelWidth = 150.0f;
		ImGui::Indent();

		{
			ImGui::AlignTextToFramePadding();
			ImGui::Text("Ignore Layout");
			ImGui::SameLine(labelWidth);
			ImGui::SetNextItemWidth(-1);
			ImGui::Checkbox("##IgnoreLayout", &comp.ignoreLayout);
		}

		ImGui::Separator();
		ImGui::TextDisabled("Size (-1 = use rect)");

		{
			ImGui::AlignTextToFramePadding();
			ImGui::Text("Min Width");
			ImGui::SameLine(labelWidth);
			ImGui::SetNextItemWidth(-1);
			ImGui::DragFloat("##MinWidth", &comp.minWidth, 0.5f, -1.0f, 10000.0f);
		}
		{
			ImGui::AlignTextToFramePadding();
			ImGui::Text("Min Height");
			ImGui::SameLine(labelWidth);
			ImGui::SetNextItemWidth(-1);
			ImGui::DragFloat("##MinHeight", &comp.minHeight, 0.5f, -1.0f, 10000.0f);
		}
		{
			ImGui::AlignTextToFramePadding();
			ImGui::Text("Preferred Width");
			ImGui::SameLine(labelWidth);
			ImGui::SetNextItemWidth(-1);
			ImGui::DragFloat("##PreferredWidth", &comp.preferredWidth, 0.5f, -1.0f, 10000.0f);
		}
		{
			ImGui::AlignTextToFramePadding();
			ImGui::Text("Preferred Height");
			ImGui::SameLine(labelWidth);
			ImGui::SetNextItemWidth(-1);
			ImGui::DragFloat("##PreferredHeight", &comp.preferredHeight, 0.5f, -1.0f, 10000.0f);
		}
		{
			ImGui::AlignTextToFramePadding();
			ImGui::Text("Flexible Width");
			ImGui::SameLine(labelWidth);
			ImGui::SetNextItemWidth(-1);
			ImGui::DragFloat("##FlexibleWidth", &comp.flexibleWidth, 0.1f, -1.0f, 100.0f);
		}
		{
			ImGui::AlignTextToFramePadding();
			ImGui::Text("Flexible Height");
			ImGui::SameLine(labelWidth);
			ImGui::SetNextItemWidth(-1);
			ImGui::DragFloat("##FlexibleHeight", &comp.flexibleHeight, 0.1f, -1.0f, 100.0f);
		}

		if (deleteComp) {
			// TODO: implement remove
		}

		ImGui::Unindent();
		ImGui::TreePop();
	}

	void InspectorPanel::DrawScrollRectComponent(uint32_t entity) {
		auto& comp = NE::ECS::Command::GetUIScrollRect(entity);

		bool copyComp = false;
		bool deleteComp = false;

		const bool open = DrawComponentHeaderWithMenu(
			"Scroll Rect",
			true,
			&copyComp,
			&deleteComp
		);

		if (!open)
			return;

		float labelWidth = 150.0f;
		ImGui::Indent();

		// Entity references
		{
			ImGui::AlignTextToFramePadding();
			ImGui::Text("Content");
			ImGui::SameLine(labelWidth);
			ImGui::SetNextItemWidth(-1);
			int val = static_cast<int>(comp.contentEntity);
			if (ImGui::InputInt("##ContentEntity", &val)) {
				comp.contentEntity = static_cast<uint32_t>(val < 0 ? UINT32_MAX : val);
			}
		}
		{
			ImGui::AlignTextToFramePadding();
			ImGui::Text("Viewport");
			ImGui::SameLine(labelWidth);
			ImGui::SetNextItemWidth(-1);
			int val = static_cast<int>(comp.viewportEntity);
			if (ImGui::InputInt("##ViewportEntity", &val)) {
				comp.viewportEntity = static_cast<uint32_t>(val < 0 ? UINT32_MAX : val);
			}
		}

		ImGui::Separator();

		{
			ImGui::AlignTextToFramePadding();
			ImGui::Text("Horizontal");
			ImGui::SameLine(labelWidth);
			ImGui::SetNextItemWidth(-1);
			ImGui::Checkbox("##Horizontal", &comp.horizontal);
		}
		{
			ImGui::AlignTextToFramePadding();
			ImGui::Text("Vertical");
			ImGui::SameLine(labelWidth);
			ImGui::SetNextItemWidth(-1);
			ImGui::Checkbox("##Vertical", &comp.vertical);
		}

		{
			ImGui::AlignTextToFramePadding();
			ImGui::Text("Movement Type");
			ImGui::SameLine(labelWidth);
			ImGui::SetNextItemWidth(-1);
			static const char* MovementNames[] = { "Unrestricted", "Elastic", "Clamped" };
			int movIdx = static_cast<int>(comp.movementType);
			if (ImGui::Combo("##MovementType", &movIdx, MovementNames, IM_ARRAYSIZE(MovementNames)))
				comp.movementType = static_cast<NE::ECS::Component::UIScrollRect::MovementType>(movIdx);
		}

		if (comp.movementType == NE::ECS::Component::UIScrollRect::MovementType::Elastic) {
			ImGui::AlignTextToFramePadding();
			ImGui::Text("Elasticity");
			ImGui::SameLine(labelWidth);
			ImGui::SetNextItemWidth(-1);
			ImGui::DragFloat("##Elasticity", &comp.elasticity, 0.01f, 0.0f, 1.0f);
		}

		{
			ImGui::AlignTextToFramePadding();
			ImGui::Text("Inertia");
			ImGui::SameLine(labelWidth);
			ImGui::SetNextItemWidth(-1);
			ImGui::Checkbox("##Inertia", &comp.inertia);
		}

		if (comp.inertia) {
			ImGui::AlignTextToFramePadding();
			ImGui::Text("Deceleration Rate");
			ImGui::SameLine(labelWidth);
			ImGui::SetNextItemWidth(-1);
			ImGui::DragFloat("##DecelerationRate", &comp.decelerationRate, 0.01f, 0.0f, 1.0f);
		}

		{
			ImGui::AlignTextToFramePadding();
			ImGui::Text("Scroll Sensitivity");
			ImGui::SameLine(labelWidth);
			ImGui::SetNextItemWidth(-1);
			ImGui::DragFloat("##ScrollSensitivity", &comp.scrollSensitivity, 0.1f, 0.0f, 100.0f);
		}

		{
			ImGui::AlignTextToFramePadding();
			ImGui::Text("Interactable");
			ImGui::SameLine(labelWidth);
			ImGui::SetNextItemWidth(-1);
			ImGui::Checkbox("##Interactable", &comp.interactable);
		}

		ImGui::Separator();

		// Scrollbar entity references
		{
			ImGui::AlignTextToFramePadding();
			ImGui::Text("H Scrollbar");
			ImGui::SameLine(labelWidth);
			ImGui::SetNextItemWidth(-1);
			ImGui::TextDisabled("Entity %u", comp.horizontalScrollbar);
		}
		{
			ImGui::AlignTextToFramePadding();
			ImGui::Text("V Scrollbar");
			ImGui::SameLine(labelWidth);
			ImGui::SetNextItemWidth(-1);
			ImGui::TextDisabled("Entity %u", comp.verticalScrollbar);
		}

		if (deleteComp) {
			// TODO: implement remove
		}

		ImGui::Unindent();
		ImGui::TreePop();
	}

	void InspectorPanel::DrawAutoSizeComponent(uint32_t entity) {
		auto& comp = NE::ECS::Command::GetUIAutoSize(entity);

		bool copyComp = false;
		bool deleteComp = false;

		const bool open = DrawComponentHeaderWithMenu(
			"Auto Size",
			true,
			&copyComp,
			&deleteComp
		);

		if (!open)
			return;

		float labelWidth = 150.0f;
		ImGui::Indent();

		// Content Size Fitter section
		ImGui::TextDisabled("Content Size Fitter");
		static const char* fitModes[] = { "Unconstrained", "Min Size", "Preferred Size" };

		{
			ImGui::AlignTextToFramePadding();
			ImGui::Text("Horizontal Fit");
			ImGui::SameLine(labelWidth);
			ImGui::SetNextItemWidth(-1);
			int hFitIdx = static_cast<int>(comp.horizontalFit);
			if (ImGui::Combo("##HorizontalFit", &hFitIdx, fitModes, IM_ARRAYSIZE(fitModes)))
				comp.horizontalFit = static_cast<NE::ECS::Component::UIAutoSize::FitMode>(hFitIdx);
		}
		{
			ImGui::AlignTextToFramePadding();
			ImGui::Text("Vertical Fit");
			ImGui::SameLine(labelWidth);
			ImGui::SetNextItemWidth(-1);
			int vFitIdx = static_cast<int>(comp.verticalFit);
			if (ImGui::Combo("##VerticalFit", &vFitIdx, fitModes, IM_ARRAYSIZE(fitModes)))
				comp.verticalFit = static_cast<NE::ECS::Component::UIAutoSize::FitMode>(vFitIdx);
		}

		ImGui::Separator();

		// Aspect Ratio Fitter section
		ImGui::TextDisabled("Aspect Ratio Fitter");
		static const char* aspectModes[] = { "None", "Width Controls Height", "Height Controls Width", "Fit In Parent", "Envelope Parent" };

		{
			ImGui::AlignTextToFramePadding();
			ImGui::Text("Aspect Mode");
			ImGui::SameLine(labelWidth);
			ImGui::SetNextItemWidth(-1);
			int aspectIdx = static_cast<int>(comp.aspectMode);
			if (ImGui::Combo("##AspectMode", &aspectIdx, aspectModes, IM_ARRAYSIZE(aspectModes)))
				comp.aspectMode = static_cast<NE::ECS::Component::UIAutoSize::AspectMode>(aspectIdx);
		}
		{
			ImGui::AlignTextToFramePadding();
			ImGui::Text("Aspect Ratio");
			ImGui::SameLine(labelWidth);
			ImGui::SetNextItemWidth(-1);
			ImGui::DragFloat("##AspectRatio", &comp.aspectRatio, 0.01f, 0.001f, 100.0f);
		}

		if (deleteComp) {
			// TODO: implement remove
		}

		ImGui::Unindent();
		ImGui::TreePop();
	}

	void InspectorPanel::DrawDropdownComponent(uint32_t entity) {
		auto& comp = NE::ECS::Command::GetUIDropdown(entity);

		bool copyComp = false;
		bool deleteComp = false;

		const bool open = DrawComponentHeaderWithMenu(
			"Dropdown",
			true,
			&copyComp,
			&deleteComp
		);

		if (!open)
			return;

		ImGui::Indent();
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.8f, 0.2f, 1.0f));
		ImGui::TextWrapped("(!) UIDropdown is not production-ready. Option panel management and selection state may behave incorrectly.");
		ImGui::PopStyleColor();
		ImGui::Spacing();

		float labelWidth = 150.0f;

		// Options list
		ImGui::Separator();
		ImGui::TextDisabled("Options");
		{
			for (int i = 0; i < static_cast<int>(comp.options.size()); ++i) {
				char label[32];
				snprintf(label, sizeof(label), "Option %d", i);
				ImGui::AlignTextToFramePadding();
				ImGui::Text("%s", label);
				ImGui::SameLine(labelWidth);
				ImGui::SetNextItemWidth(-40.0f);
				char id[64]; snprintf(id, sizeof(id), "##DropOpt%d", i);
				char buf[256];
				strncpy_s(buf, comp.options[i].c_str(), sizeof(buf) - 1);
				buf[sizeof(buf) - 1] = '\0';
				if (ImGui::InputText(id, buf, sizeof(buf))) {
					comp.options[i] = buf;
				}
				ImGui::SameLine();
				char rmId[64]; snprintf(rmId, sizeof(rmId), "X##DropRm%d", i);
				if (ImGui::SmallButton(rmId)) {
					comp.options.erase(comp.options.begin() + i);
					if (comp.selectedIndex >= static_cast<int>(comp.options.size()))
						comp.selectedIndex = static_cast<int>(comp.options.size()) - 1;
					break;
				}
			}
			if (ImGui::SmallButton("+ Add Option")) {
				comp.options.push_back("New Option");
			}
		}

		ImGui::Separator();
		ImGui::TextDisabled("State");

		{
			ImGui::AlignTextToFramePadding();
			ImGui::Text("Selected Index");
			ImGui::SameLine(labelWidth);
			ImGui::SetNextItemWidth(-1);
			ImGui::DragInt("##SelectedIdx", &comp.selectedIndex, 1.0f, 0,
				static_cast<int>(comp.options.size()) - 1);
		}
		{
			ImGui::AlignTextToFramePadding();
			ImGui::Text("Interactable");
			ImGui::SameLine(labelWidth);
			ImGui::Checkbox("##DropInteractable", &comp.interactable);
		}

		ImGui::Separator();
		ImGui::TextDisabled("Entity References");

		{
			ImGui::AlignTextToFramePadding();
			ImGui::Text("Caption Text");
			ImGui::SameLine(labelWidth);
			ImGui::SetNextItemWidth(-1);
			int cEnt = static_cast<int>(comp.captionTextEntity == UINT32_MAX ? -1 : comp.captionTextEntity);
			if (ImGui::DragInt("##DropCaption", &cEnt, 1.0f, -1, 99999)) {
				comp.captionTextEntity = cEnt < 0 ? UINT32_MAX : static_cast<uint32_t>(cEnt);
			}
		}
		{
			ImGui::AlignTextToFramePadding();
			ImGui::Text("Options Panel");
			ImGui::SameLine(labelWidth);
			ImGui::SetNextItemWidth(-1);
			int pEnt = static_cast<int>(comp.optionsPanelEntity == UINT32_MAX ? -1 : comp.optionsPanelEntity);
			if (ImGui::DragInt("##DropPanel", &pEnt, 1.0f, -1, 99999)) {
				comp.optionsPanelEntity = pEnt < 0 ? UINT32_MAX : static_cast<uint32_t>(pEnt);
			}
		}

		ImGui::Separator();
		ImGui::TextDisabled("Colors");

		{
			ImGui::AlignTextToFramePadding();
			ImGui::Text("Normal");
			ImGui::SameLine(labelWidth);
			ImGui::SetNextItemWidth(-1);
			ImGui::ColorEdit4("##DropNormal", &comp.normalColor.r);
		}
		{
			ImGui::AlignTextToFramePadding();
			ImGui::Text("Highlighted");
			ImGui::SameLine(labelWidth);
			ImGui::SetNextItemWidth(-1);
			ImGui::ColorEdit4("##DropHighlight", &comp.highlightedColor.r);
		}
		{
			ImGui::AlignTextToFramePadding();
			ImGui::Text("Pressed");
			ImGui::SameLine(labelWidth);
			ImGui::SetNextItemWidth(-1);
			ImGui::ColorEdit4("##DropPressed", &comp.pressedColor.r);
		}
		{
			ImGui::AlignTextToFramePadding();
			ImGui::Text("Disabled");
			ImGui::SameLine(labelWidth);
			ImGui::SetNextItemWidth(-1);
			ImGui::ColorEdit4("##DropDisabled", &comp.disabledColor.r);
		}
		{
			ImGui::AlignTextToFramePadding();
			ImGui::Text("Option Normal");
			ImGui::SameLine(labelWidth);
			ImGui::SetNextItemWidth(-1);
			ImGui::ColorEdit4("##DropOptNormal", &comp.optionNormalColor.r);
		}
		{
			ImGui::AlignTextToFramePadding();
			ImGui::Text("Option Highlight");
			ImGui::SameLine(labelWidth);
			ImGui::SetNextItemWidth(-1);
			ImGui::ColorEdit4("##DropOptHighlight", &comp.optionHighlightedColor.r);
		}

		ImGui::Separator();
		ImGui::TextDisabled("Events");

		{
			ImGui::AlignTextToFramePadding();
			ImGui::Text("On Changed ID");
			ImGui::SameLine(labelWidth);
			ImGui::SetNextItemWidth(-1);
			int val = static_cast<int>(comp.onValueChangedEventId);
			if (ImGui::DragInt("##DropOnChangedID", &val, 1.0f, 0, 99999)) {
				comp.onValueChangedEventId = static_cast<uint32_t>(val);
			}
		}

		// Runtime info (read-only)
		if (comp.isExpanded) {
			ImGui::Separator();
			ImGui::TextDisabled("Runtime");
			ImGui::Text("Expanded: true");
			if (comp.hoveredOptionIndex >= 0)
				ImGui::Text("Hovered Option: %d", comp.hoveredOptionIndex);
		}

		if (deleteComp) {
			// TODO: implement remove
		}

		ImGui::Unindent();
		ImGui::TreePop();
	}

	void InspectorPanel::DrawInputFieldComponent(uint32_t entity) {
		auto& comp = NE::ECS::Command::GetUIInputField(entity);

		bool copyComp = false;
		bool deleteComp = false;

		const bool open = DrawComponentHeaderWithMenu(
			"Input Field",
			true,
			&copyComp,
			&deleteComp
		);

		if (!open)
			return;

		float labelWidth = 150.0f;
		ImGui::Indent();

		// Text content
		{
			ImGui::AlignTextToFramePadding();
			ImGui::Text("Text");
			ImGui::SameLine(labelWidth);
			ImGui::SetNextItemWidth(-1);
			char buf[1024];
			strncpy_s(buf, comp.text.c_str(), sizeof(buf) - 1);
			buf[sizeof(buf) - 1] = '\0';
			if (ImGui::InputText("##InputFieldText", buf, sizeof(buf))) {
				comp.text = buf;
			}
		}
		{
			ImGui::AlignTextToFramePadding();
			ImGui::Text("Placeholder");
			ImGui::SameLine(labelWidth);
			ImGui::SetNextItemWidth(-1);
			char buf[512];
			strncpy_s(buf, comp.placeholderText.c_str(), sizeof(buf) - 1);
			buf[sizeof(buf) - 1] = '\0';
			if (ImGui::InputText("##Placeholder", buf, sizeof(buf))) {
				comp.placeholderText = buf;
			}
		}

		ImGui::Separator();
		ImGui::TextDisabled("Configuration");

		{
			ImGui::AlignTextToFramePadding();
			ImGui::Text("Content Type");
			ImGui::SameLine(labelWidth);
			ImGui::SetNextItemWidth(-1);
			const char* contentTypes[] = { "Standard", "Integer", "Decimal", "Alpha Numeric", "Password" };
			int ct = static_cast<int>(comp.contentType);
			if (ImGui::Combo("##ContentType", &ct, contentTypes, IM_ARRAYSIZE(contentTypes))) {
				comp.contentType = static_cast<NE::ECS::Component::UIInputField::ContentType>(ct);
			}
		}
		{
			ImGui::AlignTextToFramePadding();
			ImGui::Text("Line Type");
			ImGui::SameLine(labelWidth);
			ImGui::SetNextItemWidth(-1);
			const char* lineTypes[] = { "Single Line", "Multi Line" };
			int lt = static_cast<int>(comp.lineType);
			if (ImGui::Combo("##LineType", &lt, lineTypes, IM_ARRAYSIZE(lineTypes))) {
				comp.lineType = static_cast<NE::ECS::Component::UIInputField::LineType>(lt);
			}
		}
		{
			ImGui::AlignTextToFramePadding();
			ImGui::Text("Character Limit");
			ImGui::SameLine(labelWidth);
			ImGui::SetNextItemWidth(-1);
			ImGui::DragInt("##CharLimit", &comp.characterLimit, 1.0f, 0, 10000);
		}
		{
			ImGui::AlignTextToFramePadding();
			ImGui::Text("Interactable");
			ImGui::SameLine(labelWidth);
			ImGui::Checkbox("##Interactable", &comp.interactable);
		}
		{
			ImGui::AlignTextToFramePadding();
			ImGui::Text("Read Only");
			ImGui::SameLine(labelWidth);
			ImGui::Checkbox("##ReadOnly", &comp.readOnly);
		}

		ImGui::Separator();
		ImGui::TextDisabled("Colors");

		{
			ImGui::AlignTextToFramePadding();
			ImGui::Text("Normal");
			ImGui::SameLine(labelWidth);
			ImGui::SetNextItemWidth(-1);
			ImGui::ColorEdit4("##NormalColor", &comp.normalColor.r);
		}
		{
			ImGui::AlignTextToFramePadding();
			ImGui::Text("Selected");
			ImGui::SameLine(labelWidth);
			ImGui::SetNextItemWidth(-1);
			ImGui::ColorEdit4("##SelectedColor", &comp.selectedColor.r);
		}
		{
			ImGui::AlignTextToFramePadding();
			ImGui::Text("Disabled");
			ImGui::SameLine(labelWidth);
			ImGui::SetNextItemWidth(-1);
			ImGui::ColorEdit4("##DisabledColor", &comp.disabledColor.r);
		}
		{
			ImGui::AlignTextToFramePadding();
			ImGui::Text("Text Color");
			ImGui::SameLine(labelWidth);
			ImGui::SetNextItemWidth(-1);
			ImGui::ColorEdit4("##TextColor", &comp.textColor.r);
		}
		{
			ImGui::AlignTextToFramePadding();
			ImGui::Text("Placeholder Color");
			ImGui::SameLine(labelWidth);
			ImGui::SetNextItemWidth(-1);
			ImGui::ColorEdit4("##PlaceholderColor", &comp.placeholderColor.r);
		}

		ImGui::Separator();
		ImGui::TextDisabled("Events");

		{
			ImGui::AlignTextToFramePadding();
			ImGui::Text("On Changed ID");
			ImGui::SameLine(labelWidth);
			ImGui::SetNextItemWidth(-1);
			int val = static_cast<int>(comp.onValueChangedEventId);
			if (ImGui::DragInt("##OnChangedID", &val, 1.0f, 0, 99999)) {
				comp.onValueChangedEventId = static_cast<uint32_t>(val);
			}
		}
		{
			ImGui::AlignTextToFramePadding();
			ImGui::Text("On Submit ID");
			ImGui::SameLine(labelWidth);
			ImGui::SetNextItemWidth(-1);
			int val = static_cast<int>(comp.onSubmitEventId);
			if (ImGui::DragInt("##OnSubmitID", &val, 1.0f, 0, 99999)) {
				comp.onSubmitEventId = static_cast<uint32_t>(val);
			}
		}

		// Runtime info (read-only)
		if (comp.isFocused) {
			ImGui::Separator();
			ImGui::TextDisabled("Runtime");
			ImGui::Text("Cursor: %d", comp.cursorPosition);
			if (comp.selectionStart >= 0 && comp.selectionStart != comp.selectionEnd) {
				ImGui::Text("Selection: %d - %d", comp.selectionStart, comp.selectionEnd);
			}
		}

		if (deleteComp) {
			// TODO: implement remove
		}

		ImGui::Unindent();
		ImGui::TreePop();
	}

	void InspectorPanel::DrawParticleEmitterComponent(uint32_t entity)
	{
		using ParticleEmitter = NE::ECS::Component::ParticleEmitter;

		auto& comp = NE::ECS::Command::GetParticleEmitter(entity);

		bool copyComp = false;
		bool deleteComp = false;

		const bool open = DrawComponentHeaderWithMenu(
			"Particle Emitter",
			true,
			&copyComp,
			&deleteComp
		);

		if (!open)
			return;

		// =====================================================
		// Material field
		// =====================================================
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
				comp.materialUUID = uuid;
				comp.material = NE::Renderer::Command::GetMaterial(uuid);
				comp.isDirty = true;
			}
		);

		if (openMaterialPopup)
			ImGui::OpenPopup("AssetPicker_ParticleMaterial");

		ImGui::SetNextWindowSizeConstraints(
			ImVec2(0.f, 0.f),
			ImVec2(350.f, 500.f)
		);

		if (ImGui::BeginPopup("AssetPicker_ParticleMaterial")) {
			ImGui::Text("Select a Material");
			ImGui::Separator();
			auto& materialList = Assets::AssetManager::GetInstance().GetAssetsOfType(Assets::AssetType::Material);

			if (ImSearch::BeginSearch()) {
				ImSearch::SearchBar();
				for (const auto& [materialName, uuid] : materialList) {
					ImSearch::SearchableItem(materialName.c_str(), [&, materialName](const char*) {
						if (ImGui::Selectable(materialName.c_str())) {
							comp.materialUUID = uuid;
							comp.material = NE::Renderer::Command::GetMaterial(uuid);
							comp.isDirty = true;
							ImGui::CloseCurrentPopup();
						}
						});
				}
				ImSearch::EndSearch();
			}
			ImGui::EndPopup();
		}

		// =====================================================
		// Shape enum
		// =====================================================
		static const char* ShapeTypeNames[] = { "Point", "Sphere", "Cone", "Box" };
		int currShape = static_cast<int>(comp.shape);

		if (DrawEnumPillCombo("Shape", currShape, ShapeTypeNames, IM_ARRAYSIZE(ShapeTypeNames), 100.0f)) {
			auto newShape = static_cast<ParticleEmitter::ShapeType>(currShape);
			if (newShape != comp.shape) {
				comp.shape = newShape;
				comp.isDirty = true;
			}
		}

		// =====================================================
		// Draw reflected fields, skipping fields handled manually
		// =====================================================
		NE::Core::ForEachFieldView<ParticleEmitter>(comp,
			[&](auto const& desc, auto const& currentValue) {
				using FieldT = std::decay_t<decltype(currentValue)>;

				const std::string_view name = desc.name;

				if (name == "shape" ||
					name == "sphereRadius" ||
					name == "coneAngle" ||
					name == "boxExtents" ||
					name == "materialUUID" ||
					name == "modelUUID")
				{
					return;
				}

				FieldT edited = currentValue;

				if (DrawField(desc, edited)) {
					SubmitSetFieldCommand<ParticleEmitter, FieldT>(
						entity, desc, currentValue, edited
					);

					auto& emitter = NE::ECS::Command::GetParticleEmitter(entity);
					emitter.isDirty = true;

					if constexpr (std::is_same_v<FieldT, uint32_t>) {
						if (name == "maxParticles") {
							emitter.EnsureCapacity();
						}
					}
				}
			}
		);

		// =====================================================
		// Shape-specific settings
		// =====================================================
		switch (comp.shape)
		{
		case ParticleEmitter::ShapeType::Sphere:
		{
			float edited = comp.sphereRadius;
			if (DrawFloatControl("Sphere Radius", edited, 1)) {
				comp.sphereRadius = edited;
				comp.isDirty = true;
			}
			break;
		}
		case ParticleEmitter::ShapeType::Cone:
		{
			float edited = comp.coneAngle;
			if (DrawFloatControl("Cone Angle", edited, 1)) {
				comp.coneAngle = edited;
				comp.isDirty = true;
			}
			break;
		}
		case ParticleEmitter::ShapeType::Box:
		{
			NE::Math::Vec3 edited = comp.boxExtents;
			if (DrawVec3Control("Box Extents", edited, 100.0f, 0.01f, 0.0f, 2)) {
				comp.boxExtents = edited;
				comp.isDirty = true;
			}
			break;
		}
		case ParticleEmitter::ShapeType::Point:
		default:
			break;
		}

		if (copyComp) {
		}
		if (deleteComp) {
			NE::ECS::Command::RemoveParticleEmitterComponent(entity);
		}

		ImGui::TreePop();
	}
}
