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
#include <ECS/Components/UIRectTransform.hpp>
#include <ECS/Components/UICanvas.hpp>
#include "../Command/EditorSetTransformCommand.hpp"
#include "../Command/CommandHistory.hpp"
#include "Graphics/Core/UIRenderer.hpp"
#include "../UIGizmoHandler.hpp"
#include <limits>
#include "../Util/DrawSelectedCollider.hpp"

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
		if (s_showSelectedCollider && Editor::EditorScene::s_selectedEntity) {
			const uint32_t eid = Editor::EditorScene::s_selectedEntity->linkedEntity;
			EditorHelpers::DrawSelectedBoxColliderOverlay(
				eid,
				panelPos, panelSize,
				Editor::EditorScene::m_editorCamera.GetViewMatrix(),
				Editor::EditorScene::m_editorCamera.GetProjectionMatrix(),
				1.8f
			);
		}
		if (ImGui::BeginDragDropTarget()) {
			if (const ImGuiPayload* prefabPayload = ImGui::AcceptDragDropPayload("PREFAB_ASSET_PATH")) {
				std::string dropped((const char*)prefabPayload->Data, prefabPayload->DataSize - 1);
				std::string uuid = AssetManager::GetInstance().RetrieveUUID(dropped);
				Vec3 camForwardPos = EditorScene::m_editorCamera.GetPosition() + EditorScene::m_editorCamera.GetForward() * 6.0f;
				std::vector<uint32_t> newEntities = NE::DeserializePrefab(dropped, uuid, camForwardPos);

				if (newEntities.empty()) {
					ImGui::EndDragDropTarget();
					return;
				}

				std::unordered_set<uint32_t> newSet(newEntities.begin(), newEntities.end());

				for (uint32_t entt : newEntities) {
					Editor::EditorScene::s_entities.push_back(Editor::EditorEntity{ entt });

					Editor::Node node{};
					node.id = entt;

					uint32_t parent = NE::ECS::Query::GetParent(entt);
					node.parent = parent;

					if (parent == NE::ECS::NO_ENTITY) {
						node.orderKey = static_cast<float>(Editor::EditorScene::s_roots.size());
						Editor::EditorScene::s_roots.push_back(entt);
					} else {
						auto& childrenVec = Editor::EditorScene::s_children[parent];
						node.orderKey = static_cast<float>(childrenVec.size());
						childrenVec.push_back(entt);
					}

					Editor::EditorScene::s_nodes[entt] = node;
				}

				uint32_t prefabRoot = NE::ECS::NO_ENTITY;
				for (uint32_t entt : newEntities) {
					uint32_t parent = NE::ECS::Query::GetParent(entt);
					if (parent == NE::ECS::NO_ENTITY || !newSet.count(parent)) {
						prefabRoot = entt;
						NE::ECS::Command::GetEntityMeta(entt).prefabID = AssetManager::GetInstance().RetrieveUUID(dropped);
						break;
					}
				}

				if (prefabRoot != NE::ECS::NO_ENTITY) {
					Editor::EditorScene::s_selectedEntity = nullptr;
					for (auto& ee : Editor::EditorScene::s_entities) {
						if (ee.linkedEntity == prefabRoot) {
							Editor::EditorScene::s_selectedEntity = &ee;
							break;
						}
					}
				}

				// Clear asset selection since we just selected an entity
				Editor::EditorScene::selectedAsset.clear();
			} else if (const ImGuiPayload* materialPayload = ImGui::AcceptDragDropPayload("MATERIAL_PATH")) {
				std::string dropped((const char*)materialPayload->Data, materialPayload->DataSize - 1);
				std::string uuid = AssetManager::GetInstance().RetrieveUUID(dropped);

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
				std::string uuid = AssetManager::GetInstance().RetrieveUUID(dropped);

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

					EditorScene::s_entities.clear();
					NE::EditorEdit();

					auto numEntt = NE::GetNumEntities();
					EditorScene::s_entities.reserve(numEntt.size());
					for (auto e : numEntt) {
						EditorScene::s_entities.push_back(EditorEntity{ e });
					}
				}
			}
			ImGui::End();
			ImGui::PopStyleColor();
			ImGui::PopStyleVar(2);
		}

		// Handle input only when scene panel is focused
		if (ImGui::IsWindowFocused()) {
			ImGuiIO& io = ImGui::GetIO();

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

			if (!ImGuizmo::IsUsingAny() && !s_usingUIGizmo) {
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

						// object picking
						uint32_t id = NE::GetPickedEntity(x, y);

						EditorScene::s_selectedEntity = nullptr;
						EditorScene::selectedAsset = "";

						if (id != NE::ECS::NO_ENTITY) {
							// probably need to change this looks terrible when we have alot of entities
							for (auto& ent : EditorScene::s_entities) {
								if (ent.linkedEntity == id) {
									EditorScene::s_selectedEntity = &ent;
									EditorScene::ForceOpenParents(ent.linkedEntity);
									break;
								}
							}
						}
					}
				}
			}
		}

		// transform gizmos
		if (EditorScene::s_selectedEntity) {
			static ImGuizmo::OPERATION currentOperation = ImGuizmo::TRANSLATE;
			if (ImGui::IsKeyPressed(ImGuiKey_W)) currentOperation = ImGuizmo::TRANSLATE;
			if (ImGui::IsKeyPressed(ImGuiKey_E)) currentOperation = ImGuizmo::ROTATE;
			if (ImGui::IsKeyPressed(ImGuiKey_R)) currentOperation = ImGuizmo::SCALE;

			ImGuizmo::SetOrthographic(false);
			ImGuizmo::SetDrawlist();
			ImGuizmo::SetRect(panelPos.x, panelPos.y, panelSize.x, panelSize.y);

			//using Owner = NE::ECS::Component::Transform;
			//const uint32_t eid = EditorScene::s_selectedEntity->linkedEntity;
			//const auto& tRO = NE::ECS::Query::GetEntityTransform(eid);

			//float matrix[16];
			//memcpy(matrix, tRO.worldMatrix.Data(), sizeof(float) * 16);

			//bool editedThisFrame = ImGuizmo::Manipulate(
			//	m_editorCamera.GetViewMatrix().Data(),
			//	m_editorCamera.GetProjectionMatrix().Data(),
			//	currentOperation, ImGuizmo::LOCAL, matrix
			//);
			//bool isUsing = ImGuizmo::IsUsing();

			using Owner = NE::ECS::Component::Transform;
			const uint32_t eid = EditorScene::s_selectedEntity->linkedEntity;

			// check which type of entity this is
			bool hasTransform = NE::ECS::Query::HasTransform(eid);
			bool hasUIRectTransform = NE::ECS::Query::HasUIRectTransform(eid);

			if (hasTransform) {
				// Get parent world matrix
				NE::Math::Mat4 parentWorld;
				parentWorld.SetToIdentity();

				auto& tRO = NE::ECS::Query::GetEntityTransform(eid); // or Command::GetEntityTransform
				if (tRO.parent != NE::ECS::Component::INVALID_PARENT) {
					const auto& parentT = NE::ECS::Query::GetEntityTransform(tRO.parent);
					parentWorld = parentT.worldMatrix;
				}

				float worldMatrix[16];
				memcpy(worldMatrix, tRO.worldMatrix.Data(), sizeof(float) * 16);

				bool editedThisFrame = ImGuizmo::Manipulate(
					EditorScene::m_editorCamera.GetViewMatrix().Data(),
					EditorScene::m_editorCamera.GetProjectionMatrix().Data(),
					currentOperation, ImGuizmo::LOCAL, worldMatrix
				);
				bool isUsing = ImGuizmo::IsUsing();

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

				if (!s_gizmoActive && isUsing) {
					s_gizmoActive = true;
					s_gizmoMask = opToMask(currentOperation);
					auto before = NE::ECS::Query::GetEntityTransform(eid);
					s_gizmoCmd = std::make_unique<Editor::SetTransformCommand>(
						eid, "Gizmo: Transform", before, before,
						&NE::ECS::Command::GetEntityTransform, s_gizmoMask
					);
				}

				//if (s_gizmoActive && isUsing && editedThisFrame && s_gizmoCmd) {
				//	float tr[3], rotDeg[3], sc[3];
				//	ImGuizmo::DecomposeMatrixToComponents(matrix, tr, rotDeg, sc);

				//	auto current = NE::ECS::Query::GetEntityTransform(eid);
				//	auto after = current;

				//	if (s_gizmoMask & Editor::SetTransformCommand::Pos)
				//		after.localPosition = { tr[0], tr[1], tr[2] };
				//	if (s_gizmoMask & Editor::SetTransformCommand::Rot)
				//		after.localRotationEuler = { Radians(rotDeg[0]), Radians(rotDeg[1]), Radians(rotDeg[2]) };
				//	if (s_gizmoMask & Editor::SetTransformCommand::Scl)
				//		after.localScale = { sc[0], sc[1], sc[2] };

				//	s_gizmoCmd->SetAfter(after);
				//}
				if (s_gizmoActive && isUsing && editedThisFrame && s_gizmoCmd) {
					// Convert worldMatrix[16] back to Mat4
					NE::Math::Mat4 newWorld;
					memcpy(newWorld.Data(), worldMatrix, sizeof(float) * 16);

					// local = parent^-1 * world
					NE::Math::Mat4 invParent = parentWorld.Inverse(); // or your InverseTRS(parentWorld)
					NE::Math::Mat4 newLocal = invParent * newWorld;

					// Now decompose *local* matrix, not world
					float tr[3], rotDeg[3], sc[3];
					float localMatrix[16];
					memcpy(localMatrix, newLocal.Data(), sizeof(float) * 16);

					ImGuizmo::DecomposeMatrixToComponents(localMatrix, tr, rotDeg, sc);

					auto current = NE::ECS::Query::GetEntityTransform(eid);
					auto after = current;

					if (s_gizmoMask & Editor::SetTransformCommand::Pos)
						after.localPosition = { tr[0], tr[1], tr[2] };
					if (s_gizmoMask & Editor::SetTransformCommand::Rot)
						after.localRotationEuler = {
							Radians(rotDeg[0]), Radians(rotDeg[1]), Radians(rotDeg[2])
					};
					if (s_gizmoMask & Editor::SetTransformCommand::Scl)
						after.localScale = { sc[0], sc[1], sc[2] };

					s_gizmoCmd->SetAfter(after);
				}

				if (s_gizmoActive && !isUsing) {
					if (s_gizmoCmd) {
						const auto& B = s_gizmoCmd->Before();
						const auto& A = s_gizmoCmd->After();
						auto eq = [](auto a, auto b) {
							return std::fabs(a.x - b.x) <= 1e-6f && std::fabs(a.y - b.y) <= 1e-6f && std::fabs(a.z - b.z) <= 1e-6f;
							};
						bool changed = false;
						if (s_gizmoMask & Editor::SetTransformCommand::Pos) changed |= !eq(B.localPosition, A.localPosition);
						if (s_gizmoMask & Editor::SetTransformCommand::Rot) changed |= !eq(B.localRotationEuler, A.localRotationEuler);
						if (s_gizmoMask & Editor::SetTransformCommand::Scl) changed |= !eq(B.localScale, A.localScale);

						if (changed) {
							Editor::CommandHistory::GetInstance().ExecuteCommand(std::move(s_gizmoCmd));
						} else {
							s_gizmoCmd.reset();
						}
					}
					s_gizmoActive = false;
				}
			} else if (hasUIRectTransform) {
				auto& rectTransform = NE::ECS::Command::GetUIRectTransform(eid);

				// Get the canvas parent to check render mode
				uint32_t canvasEntityId = std::numeric_limits<uint32_t>::max();
				NE::ECS::Component::UICanvas* canvas = nullptr;

				// First check if this entity itself is a canvas
				if (NE::ECS::Query::HasUICanvas(eid)) {
					canvasEntityId = eid;
					canvas = &NE::ECS::Command::GetUICanvas(eid);
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

					Editor::UIGizmoHandler::Update3DGizmo(eid, view, proj, panelPos, panelSize);
					s_usingUIGizmo = Editor::UIGizmoHandler::IsGizmoActive();
				}
				// Screen space canvas (2D gizmo with corner/edge handles)
				else if (canvas->renderMode == NE::ECS::Component::UICanvas::RenderMode::SCREEN_SPACE_OVERLAY ||
					canvas->renderMode == NE::ECS::Component::UICanvas::RenderMode::SCREEN_SPACE_CAMERA) {
					// Begin 2D gizmo if not already active
					if (!UIGizmoHandler::IsGizmoActive()) {
						UIGizmoHandler::Begin2DGizmo(eid);
						s_usingUIGizmo = true;
					}

					// Update 2D gizmo
					if (UIGizmoHandler::IsGizmoActive()) {
						UIGizmoHandler::Update2DGizmo(eid, panelPos, panelSize, 1920.f, 1080.f);
					}

					// End 2D gizmo on mouse release
					if (!ImGui::IsMouseDown(ImGuiMouseButton_Left) && UIGizmoHandler::IsGizmoActive()) {
						UIGizmoHandler::End2DGizmo(eid);
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
}