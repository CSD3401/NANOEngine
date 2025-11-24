#include "ScenePanel.hpp"
#include "Math/Vec3.hpp"
#include <imgui/imgui.h>
#include "../EditorScene.hpp"
#include "Engine.hpp"
#include <imgui/widgets/imguizmo/ImGuizmo.h>
#include <EditorInterface/ECSExports.hpp>
#include <ECS/Components/Transform.hpp>
#include "../Command/EditorSetTransformCommand.hpp"
#include "../Command/CommandHistory.hpp"
#include <unordered_set>
#include <ECS/Core/Entity.hpp>
#include "../AssetManagement/AssetManager.hpp"
#include <ECS/Components/EntityMeta.hpp>

namespace Editor {
	static std::unique_ptr<Editor::SetTransformCommand> s_gizmoCmd;
	static bool s_gizmoActive = false;

	// TEMP TO BE MOVED TO SHARED MATH LIB
	float Radians(float deg) {
		return deg * 3.14159265358979323846f / 180.0f;
	}

	ScenePanel::ScenePanel() {

		NE::Math::Vec3 position = { 0.0f, 0.0f, 10.0f };
		NE::Math::Vec3 target = { 0.0f, 0.0f, 0.0f };
		NE::Math::Vec3 up = { 0.0f, 1.0f, 0.0f };


		float fovYRadians = 45.0f * (NE::Math::PI / 180.0f); // 45 degrees fov
		float aspectRatio = 1920.f / 1080.f;
		float nearPlane = 0.1f;
		float farPlane = 1000.0f;

		EditorScene::m_editorCamera.SetPerspective(fovYRadians, aspectRatio, nearPlane, farPlane);
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
		ImGui::Image(
			(ImTextureID)(uintptr_t)NE::GetSceneColorAttachment(), 
			panelSize, 
			ImVec2(0, 1), 
			ImVec2(1, 0)
		);

		if (ImGui::BeginDragDropTarget()) {
			if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload("PREFAB_ASSET_PATH")) {
				std::string dropped((const char*)p->Data, p->DataSize - 1);
				std::string uuid = AssetManager::GetInstance().RetrieveUUID(dropped);
				Vec3 camForwardPos = EditorScene::m_editorCamera.GetPosition() + EditorScene::m_editorCamera.GetForward() * 6.0f;
				std::vector<uint32_t> newEntities = NE::DeserializePrefab(dropped, uuid, camForwardPos);

				if (newEntities.empty()) {
					ImGui::EndDragDropTarget();
					return;
				}

				// For convenience in root-detection
				std::unordered_set<uint32_t> newSet(newEntities.begin(), newEntities.end());

				// 3) Register new entities into editor structures (like CreateEntity, but with parents)
				for (uint32_t entt : newEntities) {
					// EditorEntity list
					Editor::EditorScene::s_entities.push_back(Editor::EditorEntity{ entt });

					// Build node from ECS parent info
					Editor::Node node{};
					node.id = entt;

					uint32_t parent = NE::ECS::Command::GetParent(entt); // NO_ENTITY if root
					node.parent = parent;

					if (parent == NE::ECS::NO_ENTITY) {
						// New root in hierarchy
						node.orderKey = static_cast<float>(Editor::EditorScene::s_roots.size());
						Editor::EditorScene::s_roots.push_back(entt);
					} else {
						// Child of existing or newly created entity
						auto& childrenVec = Editor::EditorScene::s_children[parent];
						node.orderKey = static_cast<float>(childrenVec.size());
						childrenVec.push_back(entt);
					}

					Editor::EditorScene::s_nodes[entt] = node;
				}

				// 4) Choose a prefab root among the new entities and select it
				uint32_t prefabRoot = NE::ECS::NO_ENTITY;
				for (uint32_t entt : newEntities) {
					uint32_t parent = NE::ECS::Command::GetParent(entt);
					// Root of this prefab instance = parent is NO_ENTITY OR parent is not in this batch
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

			// Mouse look
			if (ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
				if (!m_rightMouseHeld) {
					m_lastMousePos = io.MousePos;
					m_rightMouseHeld = true;
				}

				Vec3 move(0.0f);
				if (ImGui::IsKeyDown(ImGuiKey_W) || ImGui::IsKeyDown(ImGuiKey_UpArrow))    move.z += 1.0f;
				if (ImGui::IsKeyDown(ImGuiKey_S) || ImGui::IsKeyDown(ImGuiKey_DownArrow))  move.z -= 1.0f;
				if (ImGui::IsKeyDown(ImGuiKey_A) || ImGui::IsKeyDown(ImGuiKey_LeftArrow))  move.x -= 1.0f;
				if (ImGui::IsKeyDown(ImGuiKey_D) || ImGui::IsKeyDown(ImGuiKey_RightArrow)) move.x += 1.0f;
				if (ImGui::IsKeyDown(ImGuiKey_Q) || ImGui::IsKeyDown(ImGuiKey_LeftBracket)) move.y += 1.0f;
				if (ImGui::IsKeyDown(ImGuiKey_E) || ImGui::IsKeyDown(ImGuiKey_RightBracket)) move.y -= 1.0f;

				if (move.LengthSquared() > 0.0f) {
					move.Normalize();
					// Calculate direction vectors
					Vec3 forward = EditorScene::m_editorCamera.GetForward();
					Vec3 right = forward.Cross(Vec3(0, 1, 0)).Normalized();

					Vec3 offset = (right * move.x + forward * move.z + Vec3(0, 1, 0) * move.y) * m_cameraSpeed * deltaTime;
					EditorScene::m_editorCamera.SetPosition(EditorScene::m_editorCamera.GetPosition() + offset);
				}

				ImVec2 delta = { io.MousePos.x - m_lastMousePos.x, io.MousePos.y - m_lastMousePos.y };
				m_lastMousePos = io.MousePos;

				m_cameraYaw += delta.x * m_mouseSensitivity;
				m_cameraPitch -= delta.y * m_mouseSensitivity;

				// Clamp pitch
				if (m_cameraPitch > 89.0f) m_cameraPitch = 89.0f;
				if (m_cameraPitch < -89.0f) m_cameraPitch = -89.0f;
			} else {
				m_rightMouseHeld = false;
			}

			if (!ImGuizmo::IsUsingAny()) {
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

						EditorScene::s_selectedEntity = nullptr;
						EditorScene::selectedAsset = "";
						for (auto& ent : EditorScene::s_entities) {
							if (ent.linkedEntity == id) {
								EditorScene::s_selectedEntity = &ent;
								break;
							}
						}
					}
				}
			}
		}

		if (EditorScene::s_selectedEntity) {
			static ImGuizmo::OPERATION currentOperation = ImGuizmo::TRANSLATE;
			if (ImGui::IsKeyPressed(ImGuiKey_Q)) currentOperation = ImGuizmo::TRANSLATE;
			if (ImGui::IsKeyPressed(ImGuiKey_W)) currentOperation = ImGuizmo::ROTATE;
			if (ImGui::IsKeyPressed(ImGuiKey_E)) currentOperation = ImGuizmo::SCALE;

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
			const auto& tRO = NE::ECS::Query::GetEntityTransform(eid);

			// Get parent world matrix
			NE::Math::Mat4 parentWorld;
			parentWorld.SetToIdentity();

			{
				// However you get parent; adjust to your API
				// Example: if Transform has a 'parent' entity id:
				auto& t = NE::ECS::Query::GetEntityTransform(eid); // or Command::GetEntityTransform
				if (t.parent != NE::ECS::Component::INVALID_PARENT) {
					const auto& parentT = NE::ECS::Query::GetEntityTransform(t.parent);
					parentWorld = parentT.worldMatrix;
				}
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
