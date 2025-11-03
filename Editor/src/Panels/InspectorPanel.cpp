#include "InspectorPanel.hpp"
#include <imgui/imgui.h>
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
#include <ECS/Components/Camera.hpp>
#include <Core/Reflection.hpp>
#include <Math/Vec3.hpp>
#include "../EditorScene.hpp"
#include <Engine.hpp>
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
#include <Core/SpdLogger.hpp>

namespace {
	template<typename Owner, typename T>
	bool DrawField(const NE::Core::FieldDescriptor<Owner, T>& desc, T& value) {
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
		else if constexpr (std::is_same_v<T, std::string>) {
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

	struct FieldKey {
		uint32_t entity;
		const std::type_info* ownerType;
		size_t memberId;  // hashed member pointer

		bool operator==(const FieldKey& o) const noexcept {
			return entity == o.entity && ownerType == o.ownerType && memberId == o.memberId;
		}
	};

	struct FieldKeyHash {
		size_t operator()(const FieldKey& k) const noexcept {
			size_t h = std::hash<uint32_t>{}(k.entity);
			h ^= std::hash<const void*>{}(k.ownerType) + 0x9e3779b9 + (h << 6) + (h >> 2);
			h ^= k.memberId + 0x9e3779b9 + (h << 6) + (h >> 2);
			return h;
		}
	};

	template<class T>
	static bool Equal(const T& a, const T& b) {
		if constexpr (std::is_floating_point_v<T>) {
			return std::fabs(a - b) <= 1e-6f;
		}
		else {
			return a == b;
		}
	}
}

namespace Editor {
	std::unordered_map<std::type_index, uint8_t> componentTypeRegistry;

	static std::unordered_map<FieldKey,
		std::unique_ptr<ICommand>,
		FieldKeyHash> g_activeCommands;

	InspectorPanel::InspectorPanel() {
		m_loadedMaterial = nullptr;
		m_loadedPath = "";

		componentTypeRegistry = NE::ECS::Query::GetRegisteredComponentTypes();
	}

	void InspectorPanel::OnImGuiRender()
	{
		ImGui::Begin("Inspector", nullptr);

		//ImVec2 panelPos = ImGui::GetCursorScreenPos(); // warning unused var - RF
		//ImVec2 panelSize = ImGui::GetContentRegionAvail(); // warning unused var - RF

		if (EditorScene::s_selectedEntity) {
			uint32_t entity = EditorScene::s_selectedEntity->linkedEntity;

			bool isActive = true;
			if (ImGui::Checkbox("##", &isActive)) {
			}
			ImGui::SameLine();

			{
				using Owner = NE::ECS::Component::EntityMeta;
				using FieldT = std::string;

				const auto& metaRO = NE::ECS::Query::GetEntityMeta(entity);

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
				bool changed = ImGui::InputText("##Name", edited.data(),
					ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue);
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
								g_activeCommands.erase(it);
								return;
							}
						}
						Editor::CommandHistory::GetInstance()
							.ExecuteCommand(std::move(it->second));
						g_activeCommands.erase(it);
					}
				}
			}

			NE::ECS::Signature sig(NE::ECS::Query::GetEntitySignature(entity));
			for (const auto& [typeIdx, compType] : componentTypeRegistry) {
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
				else if (typeIdx == typeid(NE::ECS::Component::Renderer)) {
					auto& comp = NE::ECS::Query::GetEntityRenderer(entity);
					ImGui::SeparatorText("Renderer");
					//char buf[256];
					//strncpy_s(buf, comp.modelPath.string().c_str(), sizeof(buf));
					//ImGui::InputText("Model", buf, sizeof(buf));
					//comp.modelPath = buf;
					bool openPopup = false;
					DrawAssetField("Model", comp.modelPath.string(), "+", 0.f, &openPopup);
					if (openPopup) {
						ImGui::OpenPopup("AssetPicker_Model");
					}

					if (ImGui::BeginDragDropTarget()) {
						if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload("ASSET_PATH")) {
							std::string dropped((const char*)p->Data, p->DataSize - 1);

							NE::Renderer::Command::AssignModel(entity, dropped);
						}
						ImGui::EndDragDropTarget();
					}

					static std::string searchQuery;
					if (ImGui::BeginPopup("AssetPicker_Model")) {
						ImGui::Text("Select a Model");
						ImGui::Separator();
						auto& assets = NE::GetAllModels();

						if (ImSearch::BeginSearch()) {
							ImSearch::SearchBar();

							for (const auto& [name, asset] : assets) {
								ImSearch::SearchableItem(name.c_str(),
									[name, &entity](const char*) {
										if (ImGui::Selectable(name.c_str())) {
											NE::Renderer::Command::AssignModel(entity, name); // need to add undo redo
											ImGui::CloseCurrentPopup();
										}
									});
							}

							ImSearch::EndSearch();
						}
						ImGui::EndPopup();
					}

					char bufMat[256];
					strncpy_s(bufMat, comp.materialPath.string().c_str(), sizeof(bufMat));
					ImGui::InputText("Material", bufMat, sizeof(bufMat));

					//comp.materialPath = bufMat;
					if (ImGui::BeginDragDropTarget()) {
						if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload("MATERIAL_PATH")) {
							std::string dropped((const char*)p->Data, p->DataSize - 1);
							//NE::AssignRendererMaterial(comp, dropped);
							NE::Renderer::Command::AssignMaterial(entity, dropped);
						}
						ImGui::EndDragDropTarget();
					}
				}
				else if (typeIdx == typeid(NE::ECS::Component::Light)) {
					auto& comp = NE::ECS::Query::GetEntityLight(entity);
					ImGui::SeparatorText("Light");

					static const char* LightTypeNames[] = { "Directional", "Point", "Spot" };
					int currentType = static_cast<int>(comp.type);
					if (ImGui::Combo("Type", &currentType, LightTypeNames, IM_ARRAYSIZE(LightTypeNames))) {
						//comp.type = static_cast<NE::ECS::Component::Light::Type>(currentType);
						// temp
						auto& tempLight = NE::ECS::Command::GetEntityLight(entity);
						tempLight.type = static_cast<NE::ECS::Component::Light::Type>(currentType);
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
				else if (typeIdx == typeid(NE::ECS::Component::Collider)) {
					// START COLLIDER

					auto& comp = NE::ECS::Command::GetEntityCollider(entity);
					ImGui::SeparatorText("Collider");

					// Dropdown shapes
					static const char* ShapeTypeNames[] = { "Box", "Sphere", "Capsule", "None" };
					int currShape = static_cast<int>(comp.shapeType);
					if (ImGui::Combo("Shape Type", &currShape, ShapeTypeNames, IM_ARRAYSIZE(ShapeTypeNames)))
					{
						auto& tempCollider = NE::ECS::Command::GetEntityCollider(entity);
						tempCollider.shapeType = static_cast<NE::ECS::Component::Collider::ShapeType>(currShape);
					}
					// Collider fields
					NE::Core::ForEachFieldView<NE::ECS::Component::Collider>(comp,
						[&](auto const& desc, auto const& currentValue) {
							using FieldT = std::decay_t<decltype(currentValue)>;

							FieldT edited = currentValue;

							if (DrawField(desc, edited))
							{
								SubmitSetFieldCommand<NE::ECS::Component::Collider, FieldT>(
									entity, desc, currentValue, edited);
							}
						});

					// Store original values to detect changes
					auto originalShapeType = comp.shapeType;
					auto originalHalfExtents = comp.halfExtents;
					auto originalRadius = comp.radius;
					auto originalHeight = comp.height;

					// Check if entity has physics body component
					bool hasPhysicsBody = NE::Physics::Query::HasPhysicsBody(entity);
					uint32_t currentBodyID = NE::Physics::Query::GetPhysicsBodyId(entity);

					// Calculate full size from half extents
					NE::Math::Vec3 fullsize = {
						comp.halfExtents.x * 2.0f,
						comp.halfExtents.y * 2.0f,
						comp.halfExtents.z * 2.0f
					};

					if (!hasPhysicsBody)
					{
						if (ImGui::Button("Create Physics Body"))
						{
							NE::Physics::Command::CreatePhysicsBody(entity);
						}
						if (comp.shapeType != NE::ECS::Component::Collider::ShapeType::Box)
						{
							ImGui::TextDisabled("Only support box collider currently");
						}
					}

					else
					{
						// Entity has valid physics body
						ImGui::Text("Physics Body Exists ID: %u", currentBodyID);

						// Check if properties changed
						bool colliderChanged = (originalShapeType != comp.shapeType ||
							originalHalfExtents != comp.halfExtents ||
							originalRadius != comp.radius ||
							originalHeight != comp.height);

						if (comp.shapeType == NE::ECS::Component::Collider::ShapeType::Box)
						{
							if (ImGui::Button("Update Physics Body") || colliderChanged)
							{
								//NE::Physics::PhysicsManager::UpdateBoxSize(currentBodyID, fullsize);
								NE::Physics::Command::UpdatePhysicsBody(entity);
							}
						}
						else
						{
							ImGui::TextDisabled("Physics body exists but shape type changed!\n");
						}

						ImGui::SameLine();
						if (ImGui::Button("Remove Physics Body"))
						{
							NE::Physics::Command::RemovePhysicsBody(entity);
						}

						ImGui::Spacing();
						if (ImGui::Button("Activate Body"))
						{
							//NE::Physics::PhysicsManager::ActivateBodies();
							NE::Physics::Command::ActivateBodies();
						}
						ImGui::SameLine();
						if (ImGui::Button("DeActivate Body"))
						{
							//NE::Physics::PhysicsManager::DeactivateBodies();
							NE::Physics::Command::DeactivateBodies();
						}
					}

					ImGui::Spacing();
					ImGui::Separator();

					if (hasPhysicsBody)
					{
						ImGui::TextDisabled("Physics : ACTIVE");

						// Show physics transform
						NE::Math::Vec3 physicsPos, physicsRot;
						//NE::Physics::PhysicsManager::GetTransform(currentBodyID, physicsPos, physicsRot);
						NE::Physics::Query::GetPhysicsTransform(currentBodyID, physicsPos, physicsRot);
						ImGui::Text("Physics Position: (%.2f, %.2f, %.2f)",
							physicsPos.x, physicsPos.y, physicsPos.z);
					}
					else
					{
						ImGui::TextDisabled("Physics : INACTIVE");
					}

					// END
				}
				else if (typeIdx == typeid(NE::ECS::Component::Rigidbody)) {
					auto& comp = NE::ECS::Query::GetEntityRigidbody(entity);
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
					if (ImGui::BeginPopup("AudioPicker_Model")) {
						ImGui::Text("Select Audio");
						ImGui::Separator();
						auto& assets = NE::GetAllModels();

						if (ImSearch::BeginSearch()) {
							ImSearch::SearchBar();

							// warning entity in capture clause not used -RF
							for (const auto& [name, asset] : assets) {
								ImSearch::SearchableItem(name.c_str(),
									[name/*, &entity*/](const char*) {
										if (ImGui::Selectable(name.c_str())) {
											//NE::Renderer::Command::AssignModel(entity, name); // need to add undo redo
											printf("Audio Adding Works?");
											ImGui::CloseCurrentPopup();
										}
									});
							}

							ImSearch::EndSearch();
						}
						ImGui::EndPopup();
					}

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
				else if (typeIdx == typeid(NE::ECS::Component::NativeScript)) {
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
						}

						// List all registered scripts
						auto scriptNames = NE::ECS::Command::GetRegisteredScriptNames();
						for (const auto& scriptName : scriptNames) {
							bool isSelected = (comp.ScriptName == scriptName);
							if (ImGui::Selectable(scriptName.c_str(), isSelected)) {
								NE::ECS::Command::SetEntityScript(entity, scriptName);
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

						// Script enabled/disabled checkbox
						if (comp.Instance) {
							bool enabled = comp.Instance->IsEnabled();
							if (ImGui::Checkbox("Enabled", &enabled)) {
								comp.Instance->SetEnabled(enabled);
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
						}
						else {
							ImGui::Text("Status: Not Instantiated");
						}

						// Show if script is properly registered
						bool isRegistered = NE::ECS::Command::IsScriptRegistered(comp.ScriptName);
						if (!isRegistered) {
							ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Warning: Script not registered!");
						}
					}
				}
				else if (typeIdx == typeid(NE::ECS::Component::Camera)) {
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
									currentValue,  // after (will change while dragging)
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
									// If no net change, drop it; else execute & mark camera dirty
									if (auto* asSet = dynamic_cast<Editor::SetFieldCommand<Owner, FieldT>*>(it->second.get());
										asSet && Equal(asSet->Before(), asSet->After())) {
										g_activeCommands.erase(it);
									}
									else {
										Editor::CommandHistory::GetInstance().ExecuteCommand(std::move(it->second));
										g_activeCommands.erase(it);

										// Ensure projection is rebuilt after param changes
										// (Either handle in SetFieldCommand::Apply, or do it here.)
										auto& cam = NE::ECS::Command::GetEntityCamera(entity);
										cam.isDirty = true;  // projection rebuild flag
									}
								}
							}
						});
				}
			}

			if (ImGui::Button("Add Component")) {
				ImGui::OpenPopup("ComponentList");
			}

			if (ImGui::BeginPopup("ComponentList")) { // automate this next time with a registry
				if (ImGui::MenuItem("Renderer")) {
					NE::ECS::Command::AddRendererComponent(EditorScene::s_selectedEntity->linkedEntity);
				}
				if (ImGui::MenuItem("Rigidbody")) {
					NE::ECS::Command::AddColliderComponent(EditorScene::s_selectedEntity->linkedEntity);
					NE::ECS::Command::AddRigidbodyComponent(EditorScene::s_selectedEntity->linkedEntity);
				}
				if (ImGui::MenuItem("Collider")) {
					NE::ECS::Command::AddColliderComponent(EditorScene::s_selectedEntity->linkedEntity);
				}
				if (ImGui::MenuItem("Light")) {
					NE::ECS::Command::AddLightComponent(EditorScene::s_selectedEntity->linkedEntity);
				}
				if (ImGui::MenuItem("AudioSource")) {
					NE::ECS::Command::AddAudioSourceComponent(EditorScene::s_selectedEntity->linkedEntity);
				}
				if (ImGui::MenuItem("Script")) {
					NE::ECS::Command::AddScriptComponent(EditorScene::s_selectedEntity->linkedEntity);
				}
				if (ImGui::MenuItem("Camera")) {
					NE::ECS::Command::AddCameraComponent(EditorScene::s_selectedEntity->linkedEntity);
				}
				ImGui::EndPopup();
			}
		}
		else if (EditorScene::selectedMaterial != "") {
			if (m_loadedPath != EditorScene::selectedMaterial) {
				try {
					m_loadedMaterial = NE::GetMaterial(EditorScene::selectedMaterial);
					m_loadedPath = EditorScene::selectedMaterial;
				}
				catch (...) {
					m_loadedMaterial.reset();
					m_loadedPath.clear();
				}
			}

			if (m_loadedMaterial) {
				bool openPopup = false;
				DrawAssetField("Shader", m_loadedMaterial->GetPipeline()->GetSpecification().shaderName, "+", 0.f, &openPopup);
				if (openPopup) {
					ImGui::OpenPopup("AssetPicker_Shader");
				}

				static std::string searchQuery;
				if (ImGui::BeginPopup("AssetPicker_Shader")) {
					ImGui::Text("Select a Shader");
					ImGui::Separator();
					auto& assets = NE::GetAllShaders();

					if (ImSearch::BeginSearch()) {
						ImSearch::SearchBar();

						for (const auto& [name, asset] : assets) {
							ImSearch::SearchableItem(name.c_str(),
								[name, this](const char*) {
									if (ImGui::Selectable(name.c_str())) {
										m_loadedMaterial->SetShader(name);
										ImGui::CloseCurrentPopup();
									}
								});
						}

						ImSearch::EndSearch();
					}
					ImGui::EndPopup();
				}

				ImGui::SeparatorText("Material Uniforms");

				for (auto& [name, val] : m_loadedMaterial->GetFloatUniforms()) {
					//float v = val;
					if (Editor::DrawFloatControl(name.c_str(), val, 0.1f)) {
						//m_loadedMaterial->SetUniformFloat(name, v);
					}
				}

				for (auto& [name, val] : m_loadedMaterial->GetVec3Uniforms()) {
					NE::Math::Vec3 v = val;
					if (Editor::DrawVec3Control(name.c_str(), v, 0.0f, 100.0f)) {
						m_loadedMaterial->SetUniformVec3(name, v);
					}
				}

				for (auto& [name, val] : m_loadedMaterial->GetIntUniforms()) {
					int i = val;
					//Editor::DrawIntControl(name.c_str(), i);
					if (ImGui::DragInt(name.c_str(), &i)) {
						m_loadedMaterial->SetUniformInt(name, i);
					}
				}

				if (ImGui::Button("Save Material", { 100.f, 30.f })) {
					m_loadedMaterial->SaveMaterial("");
				}

				ImGui::SeparatorText("Material Textures");

				//for (auto& [uName, tex] : m_loadedMaterial->GetTextures()) {
				//    // Preview + picker (96px thumb)
				//    DrawTextureField(
				//        uName.c_str(), tex, 96.0f,
				//        [this, &tex, &uName](const std::string& id) {
				//            auto t = NE::GetTexture(id);
				//            m_loadedMaterial->SetTexture(uName, t);

				//            // for keeping u_HasBaseMap in sync for toggle
				//            std::string has = "u_Has" + uName.substr(2);
				//            auto& ints = m_loadedMaterial->GetIntUniforms();
				//            if (ints.find(has) != ints.end())
				//                m_loadedMaterial->SetUniformInt(has, t ? 1 : 0);
				//        }
				//    );
				//}
			}
		}

		ImGui::End();
	}
}