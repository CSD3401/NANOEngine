#include "ScenePanel.hpp"
#include "Math/Vec3.hpp"
#include <imgui/imgui.h>
#include <unordered_set>
#include <ECS/Core/Entity.hpp>
#include "../AssetManagement/AssetManager.hpp"
#include <ECS/Components/EntityMeta.hpp>
#include <EditorInterface/RendererExports.hpp>
#include "../EditorUI.hpp"
#include "../EditorScene.hpp"
#include "Engine.hpp"
#include <imgui/widgets/imguizmo/ImGuizmo.h>
#include <EditorInterface/ECSExports.hpp>
#include <ECS/Components/Transform.hpp>
#include <ECS/Components/Hierarchy.hpp>
#include <ECS/Components/UIRectTransform.hpp>
#include <ECS/Components/UICanvas.hpp>
#include "../Command/EditorSetTransformCommand.hpp"
#include "../Command/CommandHistory.hpp"
#include "Graphics/Core/UIRenderer.hpp"
#include "../UIGizmoHandler.hpp"
#include <limits>
#include "../Util/DrawSelectedCollider.hpp"
#include <algorithm>

namespace {
	// helper function for ui
	// calculate world position by walking up parent hierarchy
	ImVec2 CalculateUIWorldPosition(uint32_t entity) {
		auto& rect = NE::ECS::Query::GetUIRectTransform(entity);

		float worldX = rect.x;
		float worldY = rect.y;

		// Walk up parent chain
		uint32_t currentParent = rect.parent;
		while (currentParent != std::numeric_limits<uint32_t>::max()) {
			if (!NE::ECS::Query::HasUIRectTransform(currentParent)) {
				break;
			}

			auto& parentRect = NE::ECS::Query::GetUIRectTransform(currentParent);
			worldX += parentRect.x;
			worldY += parentRect.y;

			currentParent = parentRect.parent;
		}

		return ImVec2(worldX, worldY);
	}
}

namespace Editor {
	static std::unique_ptr<Editor::SetTransformCommand> s_gizmoCmd;
	static bool s_gizmoActive = false;

	// --- Multi-selection gizmo support --- //
	struct GizmoMultiTarget {
		uint32_t entity;
		NE::Math::Mat4 startWorld;   // world at gizmo start
		NE::Math::Mat4 parentWorld;  // parent world at gizmo start (identity if no parent)
		std::unique_ptr<Editor::SetTransformCommand> cmd;
	};

	static std::vector<GizmoMultiTarget> s_gizmoTargets;
	static NE::Math::Mat4 s_gizmoPivotStartWorld;

	static bool s_usingUIGizmo = false;
	static bool s_showSelectedCollider = false;
	// TEMP TO BE MOVED TO SHARED MATH LIB
	float Radians(float deg) {
		return deg * 3.14159265358979323846f / 180.0f;
	}

	ScenePanel::ScenePanel() {
		NE::Math::Vec3 position = { 0.0f, 0.0f, 10.0f };
		NE::Math::Vec3 target = { 0.0f, 0.0f, 0.0f };
		NE::Math::Vec3 up = { 0.0f, 1.0f, 0.0f };

		m_fov = 60;
		m_aspectRatio = 1920.f / 1080.f;
		m_nearPlane = 0.1f;
		m_farPlane = 1000.0f;

		EditorScene::m_editorCamera.SetPerspective(m_fov, m_aspectRatio, m_nearPlane, m_farPlane);
		EditorScene::m_editorCamera.SetPosition(position);
		EditorScene::m_editorCamera.LookAt(target, up);

		// Give address of the editor camera to the scene camera tweener
		sceneCameraTweener.SetSceneCamera(&EditorScene::m_editorCamera);

		//NE::UpdateEditorCameraData();
	}

	void ScenePanel::OnImGuiRender()
	{
		using namespace NE::Math;

		ImGui::Begin("Scene", nullptr,
			ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse
			| ImGuiWindowFlags_MenuBar);

		ImVec2 panelPos = ImGui::GetCursorScreenPos();
		ImVec2 panelSize = ImGui::GetContentRegionAvail();

		float deltaTime = ImGui::GetIO().DeltaTime;

		if (ImGui::BeginMenuBar()) {
			//if (ImGui::BeginMenu("Toggle Grid")) {
			//	ImGui::Text("[Under Development]");

			//	ImGui::EndMenu();
			//}

			//if (ImGui::BeginMenu("Camera Settings")) {
			//	bool changed = false;
			//	ImGui::Text("Scene Camera");
			//	changed |= Editor::DrawFloatSliderWithValue("Field of View", m_fov, 4.f, 120.f, 0.01f);
			//	ImGui::Text("Clipping Planes");
			//	ImGui::SameLine();
			//	changed |= Editor::DrawFloatControl("Near", m_nearPlane);
			//	changed |= Editor::DrawFloatControl("Far", m_farPlane);

			//	if (changed) {
			//		EditorScene::m_editorCamera.SetPerspective(m_fov, m_aspectRatio, m_nearPlane, m_farPlane);
			//	}

			//	ImGui::EndMenu();
			//}

			ImGuiStyle& style = ImGui::GetStyle();

			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.f, style.ItemSpacing.y));
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6.f, 2.f));
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1, 1, 1, 0.10f));
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1, 1, 1, 0.20f));

			bool openGrid = ImGui::Button("Toggle Grid");
			ImVec2 gridMin = ImGui::GetItemRectMin();
			ImVec2 gridMax = ImGui::GetItemRectMax();

			ImGui::SameLine();

			bool openCamera = ImGui::Button("Camera Settings");
			ImVec2 camMin = ImGui::GetItemRectMin();
			ImVec2 camMax = ImGui::GetItemRectMax();

			bool openView = ImGui::Button("Collider Draw");
			ImVec2 viewMin = ImGui::GetItemRectMin();
			ImVec2 viewMax = ImGui::GetItemRectMax();

			if (openGrid)   ImGui::OpenPopup("ToggleGridPopup");
			if (openCamera) ImGui::OpenPopup("CameraSettingsPopup");
			if (openView) ImGui::OpenPopup("ViewPopup");

			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));
			ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
			ImGui::PushStyleVar(ImGuiStyleVar_PopupBorderSize, 1.0f);

			ImGui::SetNextWindowPos(ImVec2(gridMin.x, gridMax.y), ImGuiCond_Appearing);
			if (ImGui::BeginPopup("ToggleGridPopup")) {
				ImGui::Text("[Under Development]");
				ImGui::EndPopup();
			}

			ImGui::SetNextWindowPos(ImVec2(camMin.x, camMax.y), ImGuiCond_Appearing);
			ImGui::SetNextWindowSize(ImVec2(400.f, 240.f));
			if (ImGui::BeginPopup("CameraSettingsPopup")) {
				bool changed = false;

				ImGui::Text("Scene Camera");
				changed |= Editor::DrawFloatSliderWithField(
					"Field of View", m_fov, 4.f, 120.f, 0.01f, true
				);

				ImGui::Spacing();
				ImGui::Text("Clipping Planes");
				ImGui::Indent(50.f);
				changed |= Editor::DrawFloatField("Near", m_nearPlane, 0.01f, true);
				changed |= Editor::DrawFloatField("Far", m_farPlane, 0.01f, true);
				ImGui::Unindent(50.f);

				if (changed) {
					EditorScene::m_editorCamera.SetPerspective(
						m_fov, m_aspectRatio, m_nearPlane, m_farPlane
					);
				}

				ImGui::Text("Navigation");
				Editor::DrawCheckbox("Camera Easing", m_cameraUseEasing);
				Editor::DrawCheckbox("Camera Acceleration", m_cameraUseAcceleration);
				Editor::DrawFloatSliderWithField(
					"Camera Speed", m_cameraSpeed, m_cameraMinSpeed, m_cameraMaxSpeed, 0.01f, true
				);
				ImGui::Indent(50.f);
				Editor::DrawFloatField("Min", m_cameraMinSpeed, 0.01f, true);
				Editor::DrawFloatField("Max", m_cameraMaxSpeed, 0.01f, true);
				ImGui::Unindent(50.f);

				ImGui::EndPopup();
			}
			ImGui::SetNextWindowPos(ImVec2(viewMin.x, viewMax.y), ImGuiCond_Appearing);
			if (ImGui::BeginPopup("ViewPopup")) {
				ImGui::Checkbox("Show Collider (Selected)", &s_showSelectedCollider);
				ImGui::EndPopup();
			}
			ImGui::PopStyleVar(3);
			ImGui::PopStyleColor(3);
			ImGui::PopStyleVar(2);

			ImGui::EndMenuBar();
		}

		ImGui::Image(
			(ImTextureID)(uintptr_t)NE::GetSceneColorAttachment(),
			panelSize,
			ImVec2(0, 1),
			ImVec2(1, 0)
		);
		if (!ImGuizmo::IsUsingAny() && m_dragSelecting) {
			ImDrawList* dl = ImGui::GetWindowDrawList();

			ImVec2 p0 = m_dragStartScreen;
			ImVec2 p1 = m_dragEndScreen;

			// Normalize corners
			ImVec2 min(
				std::min(p0.x, p1.x),
				std::min(p0.y, p1.y)
			);
			ImVec2 max(
				std::max(p0.x, p1.x),
				std::max(p0.y, p1.y)
			);

			dl->AddRect(min, max,
				IM_COL32(0, 150, 255, 200)); // outline
			dl->AddRectFilled(min, max,
				IM_COL32(0, 150, 255, 40));  // translucent fill
		}

		if (ImGui::BeginDragDropTarget()) {
			if (const ImGuiPayload* prefabPayload = ImGui::AcceptDragDropPayload("PREFAB_ASSET_PATH")) {
				//std::string dropped((const char*)prefabPayload->Data, prefabPayload->DataSize - 1);
				//std::string uuid = AssetManager::GetInstance().RetrieveUUID(dropped);
				//Vec3 camForwardPos = EditorScene::m_editorCamera.GetPosition() + EditorScene::m_editorCamera.GetForward() * 6.0f;
				//std::vector<uint32_t> newEntities = NE::DeserializePrefab(dropped, uuid, camForwardPos);

				//if (newEntities.empty()) {
				//	ImGui::EndDragDropTarget();
				//	return;
				//}

				//std::unordered_set<uint32_t> newSet(newEntities.begin(), newEntities.end());

				//for (uint32_t entt : newEntities) {
				//	Editor::EditorScene::s_entities.push_back(Editor::EditorEntity{ entt });

				//	Editor::Node node{};
				//	node.id = entt;

				//	uint32_t parent = NE::ECS::Query::GetParent(entt);
				//	node.parent = parent;

				//	if (parent == NE::ECS::NO_ENTITY) {
				//		node.orderKey = static_cast<float>(Editor::EditorScene::s_roots.size());
				//		Editor::EditorScene::s_roots.push_back(entt);
				//	} else {
				//		auto& childrenVec = Editor::EditorScene::s_children[parent];
				//		node.orderKey = static_cast<float>(childrenVec.size());
				//		childrenVec.push_back(entt);
				//	}

				//	Editor::EditorScene::s_nodes[entt] = node;
				//}

				//uint32_t prefabRoot = NE::ECS::NO_ENTITY;
				//for (uint32_t entt : newEntities) {
				//	uint32_t parent = NE::ECS::Query::GetParent(entt);
				//	if (parent == NE::ECS::NO_ENTITY || !newSet.count(parent)) {
				//		prefabRoot = entt;
				//		NE::ECS::Command::GetEntityMeta(entt).prefabID = AssetManager::GetInstance().RetrieveUUID(dropped);
				//		break;
				//	}
				//}

				//if (prefabRoot != NE::ECS::NO_ENTITY) {
				//	Editor::EditorScene::s_selectedEntity = nullptr;
				//	for (auto& ee : Editor::EditorScene::s_entities) {
				//		if (ee.linkedEntity == prefabRoot) {
				//			Editor::EditorScene::s_selectedEntity = &ee;
				//			break;
				//		}
				//	}
				//}

				//// Clear asset selection since we just selected an entity
				//Editor::EditorScene::selectedAsset.clear();
			} else if (const ImGuiPayload* materialPayload = ImGui::AcceptDragDropPayload("MATERIAL_PATH")) {
				std::string dropped((const char*)materialPayload->Data, materialPayload->DataSize - 1);
				std::string uuid = Assets::AssetManager::GetInstance().RetrieveUUID(dropped);

				if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
					ImVec2 mousePos = ImGui::GetMousePos();
					if (mousePos.x >= panelPos.x && mousePos.x < panelPos.x + panelSize.x &&
						mousePos.y >= panelPos.y && mousePos.y < panelPos.y + panelSize.y) {
						float localX = mousePos.x - panelPos.x;
						float localY = mousePos.y - panelPos.y;
						float spMouseX = localX / panelSize.x;
						float spMouseY = localY / panelSize.y;

						uint32_t x = static_cast<int>(spMouseX * 1920.f); // temp hardcoded
						uint32_t y = static_cast<int>(1080 - 1 - (spMouseY * 1080)); // temp hardcoded

						uint32_t id = NE::GetPickedEntity(x, y);

						if (id != NE::ECS::NO_ENTITY)
							NE::Renderer::Command::AssignMaterial(id, uuid);
					}
				}
			} else if (const ImGuiPayload* modalPayload = ImGui::AcceptDragDropPayload("ASSET_MESH_PATH")) {
				std::string dropped((const char*)modalPayload->Data, modalPayload->DataSize - 1);
				std::string uuid = Assets::AssetManager::GetInstance().RetrieveUUID(dropped);

				if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
					ImVec2 mousePos = ImGui::GetMousePos();
					if (mousePos.x >= panelPos.x && mousePos.x < panelPos.x + panelSize.x &&
						mousePos.y >= panelPos.y && mousePos.y < panelPos.y + panelSize.y) {
						float localX = mousePos.x - panelPos.x;
						float localY = mousePos.y - panelPos.y;
						float spMouseX = localX / panelSize.x;
						float spMouseY = localY / panelSize.y;

						uint32_t x = static_cast<int>(spMouseX * 1920.f);
						uint32_t y = static_cast<int>(1080 - 1 - (spMouseY * 1080));

						uint32_t id = NE::GetPickedEntity(x, y);

						if (id != NE::ECS::NO_ENTITY)
							NE::Renderer::Command::AssignModel(id, uuid);
					}
				}
			}
			ImGui::EndDragDropTarget();
		}
		// --- Floating Play Controls ---
		{
			// Centered at the top of the viewport
			ImVec2 overlaySize(200, 40);
			ImVec2 overlayPos(panelPos.x + panelSize.x * 0.5f - overlaySize.x * 0.5f,
				panelPos.y + 10.0f);

			ImGui::SetNextWindowPos(overlayPos);
			ImGui::SetNextWindowSize(overlaySize);
			ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));
			ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0.5f)); // translucent

			if (ImGui::Begin("PlayControls", nullptr,
				ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
				ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
				ImGuiWindowFlags_NoScrollWithMouse)) {
				static bool playing = false;
				static bool paused = false;

				if (ImGui::Button("Play")) {
					playing = true;
					paused = false;
					NE::EditorPlay();
				}
				ImGui::SameLine();
				if (ImGui::Button("Pause")) {
					if (playing) paused = !paused;
				}
				ImGui::SameLine();
				if (ImGui::Button("Stop")) {
					playing = false;
					paused = false;

					NE::EditorEdit();
					EditorScene::s_rootOrder.clear();
				}
			}
			ImGui::End();
			ImGui::PopStyleColor();
			ImGui::PopStyleVar(2);
		}

		// Handle input only when scene panel is focused
		if (ImGui::IsWindowFocused()) {
			ImGuiIO& io = ImGui::GetIO();

			if (!ImGuizmo::IsUsingAny() && !s_usingUIGizmo) {
				ImVec2 mousePos = io.MousePos;

				bool insideScene =
					mousePos.x >= panelPos.x && mousePos.x < panelPos.x + panelSize.x &&
					mousePos.y >= panelPos.y && mousePos.y < panelPos.y + panelSize.y;

				if (insideScene) {
					if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
						m_dragSelecting = true;
						m_dragStartScreen = mousePos;
						m_dragEndScreen = mousePos;
					}

					if (m_dragSelecting && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
						m_dragEndScreen = mousePos;
					}

					// On release: perform selection
					if (m_dragSelecting && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
						m_dragSelecting = false;

						// Avoid treating a tiny drag as a click
						ImVec2 delta = ImVec2(
							m_dragEndScreen.x - m_dragStartScreen.x,
							m_dragEndScreen.y - m_dragStartScreen.y
						);

						const float minBox = 4.0f; // pixels
						bool isBox = (fabsf(delta.x) > minBox || fabsf(delta.y) > minBox);

						if (isBox) {
							SelectEntitiesInRect(m_dragStartScreen, m_dragEndScreen,
								panelPos, panelSize);
						} else {
							float localX = mousePos.x - panelPos.x;
							float localY = mousePos.y - panelPos.y;
							float spMouseX = localX / panelSize.x;
							float spMouseY = localY / panelSize.y;

							uint32_t x = static_cast<int>(spMouseX * 1920.f); // temp hardcoded
							uint32_t y = static_cast<int>(1080 - 1 - (spMouseY * 1080)); // temp hardcoded

							uint32_t id = NE::GetPickedEntity(x, y);

							EditorScene::s_selection.Clear();
							EditorScene::selectedAsset = "";

							if (id != NE::ECS::NO_ENTITY) {
								EditorScene::s_selection.SetSingle(id);
							}
						}
					}
				} else {
					// If we left the panel, cancel drag
					if (m_dragSelecting && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
						m_dragSelecting = false;
				}
			}

			if (ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
				if (!m_rightMouseHeld) {
					m_lastMousePos = io.MousePos;
					m_rightMouseHeld = true;
					m_currentMoveSpeed = 0.0f;
					m_lastMoveDir = Vec3(0.0f);
				}

				Vec3 move(0.0f);
				if (ImGui::IsKeyDown(ImGuiKey_W) || ImGui::IsKeyDown(ImGuiKey_UpArrow))      move.z += 1.0f;
				if (ImGui::IsKeyDown(ImGuiKey_S) || ImGui::IsKeyDown(ImGuiKey_DownArrow))    move.z -= 1.0f;
				if (ImGui::IsKeyDown(ImGuiKey_A) || ImGui::IsKeyDown(ImGuiKey_LeftArrow))    move.x -= 1.0f;
				if (ImGui::IsKeyDown(ImGuiKey_D) || ImGui::IsKeyDown(ImGuiKey_RightArrow))   move.x += 1.0f;
				if (ImGui::IsKeyDown(ImGuiKey_Q) || ImGui::IsKeyDown(ImGuiKey_LeftBracket))  move.y += 1.0f;
				if (ImGui::IsKeyDown(ImGuiKey_E) || ImGui::IsKeyDown(ImGuiKey_RightBracket)) move.y -= 1.0f;

				bool hasInput = (move.LengthSquared() > 0.0f);
				bool boost = ImGui::IsKeyDown(ImGuiKey_LeftShift) || ImGui::IsKeyDown(ImGuiKey_RightShift);

				if (hasInput) {
					if (m_cameraUseAcceleration) {
						if (m_currentMoveSpeed <= 0.0f) {
							m_currentMoveSpeed = boost ? m_cameraMaxSpeed : m_cameraMinSpeed;
						}

						m_currentMoveSpeed += m_cameraAcceleration * deltaTime;

						if (!boost && m_currentMoveSpeed > m_cameraMaxSpeed) {
							m_currentMoveSpeed = m_cameraMaxSpeed;
						}
					} else {
						m_currentMoveSpeed = boost ? m_cameraMaxSpeed : m_cameraSpeed;
					}
				} else {
					// No input but RMB still held
					if (m_cameraUseEasing) {
						m_currentMoveSpeed -= m_cameraDeceleration * deltaTime;
						if (m_currentMoveSpeed < 0.0f)
							m_currentMoveSpeed = 0.0f;
					} else {
						m_currentMoveSpeed = 0.0f;
					}
				}

				// Mouse wheel controls next time
				//if (io.MouseWheel != 0.0f) {
				//	m_cameraSpeed += io.MouseWheel * 2.0f;
				//	m_cameraSpeed = std::clamp(m_cameraSpeed, m_cameraMinSpeed, m_cameraMaxSpeed);
				//}

				if (hasInput) {
					move.Normalize();
					Vec3 forward = EditorScene::m_editorCamera.GetForward();
					Vec3 right = forward.Cross(Vec3(0, 1, 0)).Normalized();
					m_lastMoveDir = right * move.x + forward * move.z + Vec3(0, 1, 0) * move.y;
				}

				if (m_currentMoveSpeed > 0.0f && m_lastMoveDir.LengthSquared() > 0.0f) {
					Vec3 offset = m_lastMoveDir.Normalized() * m_currentMoveSpeed * deltaTime;
					EditorScene::m_editorCamera.SetPosition(
						EditorScene::m_editorCamera.GetPosition() + offset
					);
				}

				ImVec2 delta = { io.MousePos.x - m_lastMousePos.x, io.MousePos.y - m_lastMousePos.y };
				m_lastMousePos = io.MousePos;

				m_cameraYaw += delta.x * m_mouseSensitivity;
				m_cameraPitch -= delta.y * m_mouseSensitivity;

				if (m_cameraPitch > 89.0f) m_cameraPitch = 89.0f;
				if (m_cameraPitch < -89.0f) m_cameraPitch = -89.0f;
			} else {
				m_rightMouseHeld = false;
				m_currentMoveSpeed = 0.0f;
				m_lastMoveDir = Vec3(0.0f);
			}
		}

		// transform gizmos
		if (!EditorScene::s_selection.Empty()) {
			static ImGuizmo::OPERATION currentOperation = ImGuizmo::TRANSLATE;
			if (ImGui::IsKeyPressed(ImGuiKey_W)) currentOperation = ImGuizmo::TRANSLATE;
			if (ImGui::IsKeyPressed(ImGuiKey_E)) currentOperation = ImGuizmo::ROTATE;
			if (ImGui::IsKeyPressed(ImGuiKey_R)) currentOperation = ImGuizmo::SCALE;

			ImGuizmo::SetOrthographic(false);
			ImGuizmo::SetDrawlist();
			ImGuizmo::SetRect(panelPos.x, panelPos.y, panelSize.x, panelSize.y);

			using Owner = NE::ECS::Component::Transform;
			const uint32_t last = EditorScene::s_selection.GetPrimary();

			bool lastHasTransform = (last != NE::ECS::NO_ENTITY) && NE::ECS::Query::HasTransform(last);
			bool lastHasUIRectTransform = (last != NE::ECS::NO_ENTITY) && NE::ECS::Query::HasUIRectTransform(last);

			if (lastHasTransform) {
				auto topLevel = EditorScene::s_selection.GetTopLevelSelection(
					[](uint32_t e) {
						const auto& h = NE::ECS::Query::GetEntityHierarchy(e);
						return h.parent;
					});

				std::vector<uint32_t> transformEntities;
				transformEntities.reserve(topLevel.size());
				for (uint32_t e : topLevel) {
					if (NE::ECS::Query::HasTransform(e))
						transformEntities.push_back(e);
				}

				if (!transformEntities.empty()) {
					// 1. Compute center of selection in world space
					NE::Math::Vec3 center(0.0f, 0.0f, 0.0f);
					for (uint32_t e : transformEntities) {
						const auto& t = NE::ECS::Query::GetEntityTransform(e);
						center += t.worldMatrix.GetTranslation();
					}
					center /= static_cast<float>(transformEntities.size());

					// 2. Build group gizmo matrix at that center (identity rot/scale)
					NE::Math::Mat4 groupWorld;
					groupWorld.SetToIdentity();
					groupWorld.SetTranslation(center);

					float groupMatrix[16];
					memcpy(groupMatrix, groupWorld.Data(), sizeof(float) * 16);

					bool editedThisFrame = ImGuizmo::Manipulate(
						EditorScene::m_editorCamera.GetViewMatrix().Data(),
						EditorScene::m_editorCamera.GetProjectionMatrix().Data(),
						currentOperation, ImGuizmo::LOCAL, groupMatrix
					);
					bool isUsing = ImGuizmo::IsUsing();

					// Write back to Mat4
					memcpy(groupWorld.Data(), groupMatrix, sizeof(float) * 16);

					// Command mask (pos/rot/scale)
					static uint8_t s_gizmoMask = 0;
					auto opToMask = [](ImGuizmo::OPERATION op) {
						using Cmd = Editor::SetTransformCommand;
						switch (op) {
						case ImGuizmo::TRANSLATE: return Cmd::Pos;
						case ImGuizmo::ROTATE:    return Cmd::Rot;
						case ImGuizmo::SCALE:     return Cmd::Scl;
						default:                  return Cmd::Pos;
						}
						};

					// 3. On gizmo start: record start world & parent world per entity
					if (!s_gizmoActive && isUsing) {
						s_gizmoActive = true;
						s_gizmoMask = opToMask(currentOperation);

						s_gizmoTargets.clear();
						s_gizmoPivotStartWorld = NE::Math::Mat4{};
						s_gizmoPivotStartWorld.SetToIdentity();
						s_gizmoPivotStartWorld.SetTranslation(center); // initial group center

						for (uint32_t e : transformEntities) {
							auto before = NE::ECS::Query::GetEntityTransform(e);

							NE::Math::Mat4 parentWorld;
							parentWorld.SetToIdentity();
							const auto& h = NE::ECS::Query::GetEntityHierarchy(e);
							if (h.parent != NE::ECS::NO_ENTITY) {
								const auto& parentT = NE::ECS::Query::GetEntityTransform(h.parent);
								parentWorld = parentT.worldMatrix;
							}

							auto cmd = std::make_unique<Editor::SetTransformCommand>(
								e, "Gizmo: Transform", before, before,
								&NE::ECS::Command::GetEntityTransform, s_gizmoMask
							);

							GizmoMultiTarget tgt;
							tgt.entity = e;
							tgt.startWorld = before.worldMatrix;
							tgt.parentWorld = parentWorld;
							tgt.cmd = std::move(cmd);

							s_gizmoTargets.push_back(std::move(tgt));
						}
					}

					// 4. While dragging: delta = newGroup * inverse(oldGroup)
					if (s_gizmoActive && isUsing && editedThisFrame && !s_gizmoTargets.empty()) {
						NE::Math::Mat4 invGroupStart = s_gizmoPivotStartWorld.Inverse();
						NE::Math::Mat4 delta = groupWorld * invGroupStart;

						for (auto& tgt : s_gizmoTargets) {
							NE::Math::Mat4 newWorld = delta * tgt.startWorld;

							NE::Math::Mat4 invParent = tgt.parentWorld.Inverse();
							NE::Math::Mat4 newLocal = invParent * newWorld;

							// Decompose local matrix
							float tr[3], rotDeg[3], sc[3];
							float localMatrix[16];
							memcpy(localMatrix, newLocal.Data(), sizeof(float) * 16);
							ImGuizmo::DecomposeMatrixToComponents(localMatrix, tr, rotDeg, sc);

							auto current = NE::ECS::Query::GetEntityTransform(tgt.entity);
							auto after = current;

							if (s_gizmoMask & Editor::SetTransformCommand::Pos)
								after.localPosition = { tr[0], tr[1], tr[2] };
							if (s_gizmoMask & Editor::SetTransformCommand::Rot)
								after.localRotationEuler = {
									Radians(rotDeg[0]),
									Radians(rotDeg[1]),
									Radians(rotDeg[2])
							};
							if (s_gizmoMask & Editor::SetTransformCommand::Scl)
								after.localScale = { sc[0], sc[1], sc[2] };

							tgt.cmd->SetAfter(after);
						}
					}

					// 5. On gizmo release: push changed commands
					if (s_gizmoActive && !isUsing) {
						auto eq = [](auto a, auto b) {
							return std::fabs(a.x - b.x) <= 1e-6f &&
								std::fabs(a.y - b.y) <= 1e-6f &&
								std::fabs(a.z - b.z) <= 1e-6f;
							};

						for (auto& tgt : s_gizmoTargets) {
							if (!tgt.cmd)
								continue;

							const auto& B = tgt.cmd->Before();
							const auto& A = tgt.cmd->After();

							bool changed = false;
							if (s_gizmoMask & Editor::SetTransformCommand::Pos)
								changed |= !eq(B.localPosition, A.localPosition);
							if (s_gizmoMask & Editor::SetTransformCommand::Rot)
								changed |= !eq(B.localRotationEuler, A.localRotationEuler);
							if (s_gizmoMask & Editor::SetTransformCommand::Scl)
								changed |= !eq(B.localScale, A.localScale);

							if (changed) {
								Editor::CommandHistory::GetInstance().ExecuteCommand(std::move(tgt.cmd));
							}
						}

						s_gizmoTargets.clear();
						s_gizmoActive = false;
					}
				}
			} else if (lastHasUIRectTransform) {
				auto& rectTransform = NE::ECS::Command::GetUIRectTransform(last);

				// Get the canvas parent to check render mode
				uint32_t canvasEntityId = std::numeric_limits<uint32_t>::max();
				NE::ECS::Component::UICanvas* canvas = nullptr;

				// First check if this entity itself is a canvas
				if (NE::ECS::Query::HasUICanvas(last)) {
					canvasEntityId = last;
					canvas = &NE::ECS::Command::GetUICanvas(last);
				} else {
					// Walk up parent chain to find canvas
					uint32_t currentParent = rectTransform.parent;
					while (currentParent != std::numeric_limits<uint32_t>::max()) {
						if (NE::ECS::Query::HasUICanvas(currentParent)) {
							canvasEntityId = currentParent;
							canvas = &NE::ECS::Command::GetUICanvas(currentParent);
							break;
						}
						if (!NE::ECS::Query::HasUIRectTransform(currentParent)) break;
						currentParent = NE::ECS::Query::GetUIRectTransform(currentParent).parent;
					}
				}

				if (!canvas) {
					// No canvas parent found, skip
					ImGui::End();
					return;
				}

				// Setup operation keys for 3D gizmo
				UIGizmoHandler::SetOperation(currentOperation);

				// World space canvas (3D gizmo)
				if (canvas->renderMode == NE::ECS::Component::UICanvas::RenderMode::WORLD_SPACE) {
					NE::Math::Mat4 view = EditorScene::m_editorCamera.GetViewMatrix();
					NE::Math::Mat4 proj = EditorScene::m_editorCamera.GetProjectionMatrix();

					Editor::UIGizmoHandler::Update3DGizmo(last, view, proj, panelPos, panelSize);
					s_usingUIGizmo = Editor::UIGizmoHandler::IsGizmoActive();
				}
				// Screen space canvas (2D gizmo with corner/edge handles)
				else if (canvas->renderMode == NE::ECS::Component::UICanvas::RenderMode::SCREEN_SPACE_OVERLAY ||
					canvas->renderMode == NE::ECS::Component::UICanvas::RenderMode::SCREEN_SPACE_CAMERA) {
					// Begin 2D gizmo if not already active
					if (!UIGizmoHandler::IsGizmoActive()) {
						UIGizmoHandler::Begin2DGizmo(last);
						s_usingUIGizmo = true;
					}

					// Update 2D gizmo
					if (UIGizmoHandler::IsGizmoActive()) {
						UIGizmoHandler::Update2DGizmo(last, panelPos, panelSize, 1920.f, 1080.f);
					}

					// End 2D gizmo on mouse release
					if (!ImGui::IsMouseDown(ImGuiMouseButton_Left) && UIGizmoHandler::IsGizmoActive()) {
						UIGizmoHandler::End2DGizmo(last);
						s_usingUIGizmo = false;
					}
				}
			}
		}

		// Calculate camera's look direction regardless of input
		Vec3 dir;
		dir.x = cosf(Radians(m_cameraYaw)) * cosf(Radians(m_cameraPitch));
		dir.y = sinf(Radians(m_cameraPitch));
		dir.z = sinf(Radians(m_cameraYaw)) * cosf(Radians(m_cameraPitch));
		EditorScene::m_editorCamera.LookAt(EditorScene::m_editorCamera.GetPosition() + dir, Vec3(0, 1, 0));

		NE::UpdateEditorCameraData();

		ImGui::End();
	}

	void ScenePanel::SelectEntitiesInRect(ImVec2 startScreen,
		ImVec2 endScreen,
		ImVec2 panelPos,
		ImVec2 panelSize)
	{
		ImVec2 s0 = startScreen;
		ImVec2 s1 = endScreen;

		ImVec2 scrMin(
			std::min(s0.x, s1.x),
			std::min(s0.y, s1.y));
		ImVec2 scrMax(
			std::max(s0.x, s1.x),
			std::max(s0.y, s1.y));

		auto ClampToPanel = [&](const ImVec2& p) -> ImVec2 {
			ImVec2 result = p;
			result.x = std::clamp(result.x, panelPos.x, panelPos.x + panelSize.x);
			result.y = std::clamp(result.y, panelPos.y, panelPos.y + panelSize.y);
			result.x -= panelPos.x;
			result.y -= panelPos.y;
			return result;
			};

		ImVec2 locMin = ClampToPanel(scrMin);
		ImVec2 locMax = ClampToPanel(scrMax);

		if (locMax.x <= locMin.x || locMax.y <= locMin.y)
			return;

		const float pickWidth = 1920.0f;
		const float pickHeight = 1080.0f;

		auto ToPickCoords = [&](const ImVec2& local) -> ImVec2 {
			float spX = local.x / panelSize.x;
			float spY = local.y / panelSize.y;

			float px = spX * pickWidth;
			float py = (1.0f - spY) * pickHeight - 1.0f;

			return ImVec2(px, py);
			};

		ImVec2 pMin = ToPickCoords(locMin);
		ImVec2 pMax = ToPickCoords(locMax);

		float minXf = std::min(pMin.x, pMax.x);
		float maxXf = std::max(pMin.x, pMax.x);
		float minYf = std::min(pMin.y, pMax.y);
		float maxYf = std::max(pMin.y, pMax.y);

		uint32_t minX = static_cast<uint32_t>(std::max(0.0f, std::floor(minXf)));
		uint32_t maxX = static_cast<uint32_t>(std::min(pickWidth - 1.0f, std::floor(maxXf)));
		uint32_t minY = static_cast<uint32_t>(std::max(0.0f, std::floor(minYf)));
		uint32_t maxY = static_cast<uint32_t>(std::min(pickHeight - 1.0f, std::floor(maxYf)));

		if (maxX < minX || maxY < minY)
			return;

		uint32_t w = maxX - minX + 1;
		uint32_t h = maxY - minY + 1;

		std::vector<uint32_t> ids = NE::GetPickedEntities(minX, minY, w, h);

		std::unordered_set<uint32_t> uniqueIds;
		for (uint32_t id : ids) {
			if (id != NE::ECS::NO_ENTITY)
				uniqueIds.insert(id);
		}

		ImGuiIO& io = ImGui::GetIO();
		bool additive = io.KeyCtrl;

		if (!additive) {
			EditorScene::s_selection.Clear();
			EditorScene::selectedAsset.clear();
		}

		for (uint32_t id : uniqueIds) {
			EditorScene::s_selection.Add(id);
		}
	}
}