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

namespace Editor {
	static uint32_t temp; // Note: hi i copy pasted this code into game panel also
	static std::unique_ptr<Editor::SetTransformCommand> s_gizmoCmd;
	static bool s_gizmoActive = false;

	// TEMP TO BE MOVED TO SHARED MATH LIB
	float Radians(float deg) {
		return deg * 3.14159265358979323846f / 180.0f;
	}

	ScenePanel::ScenePanel(uint32_t sceneFrameBuffer) {
		temp = sceneFrameBuffer;

		NE::Math::Vec3 position = { 0.0f, 0.0f, 10.0f };
		NE::Math::Vec3 target = { 0.0f, 0.0f, 0.0f };
		NE::Math::Vec3 up = { 0.0f, 1.0f, 0.0f };


		float fovYRadians = 45.0f * (NE::Math::PI / 180.0f); // 45 degrees fov
		float aspectRatio = 1920.f / 1080.f;
		float nearPlane = 0.1f;
		float farPlane = 100.0f;

		m_editorCamera.SetPerspective(fovYRadians, aspectRatio, nearPlane, farPlane);
		m_editorCamera.SetPosition(position);
		m_editorCamera.LookAt(target, up);

		// Give address of the editor camera to the scene camera tweener
		sceneCameraTweener.SetSceneCamera(GetCamera());
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

		ImGui::Image((ImTextureID)(uintptr_t)temp, panelSize, ImVec2(0, 1), ImVec2(1, 0));

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
					Vec3 forward = m_editorCamera.GetForward();
					Vec3 right = forward.Cross(Vec3(0, 1, 0)).Normalized();

					Vec3 offset = (right * move.x + forward * move.z + Vec3(0, 1, 0) * move.y) * m_cameraSpeed * deltaTime;
					m_editorCamera.SetPosition(m_editorCamera.GetPosition() + offset);
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
						EditorScene::selectedMaterial = "";
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

			using Owner = NE::ECS::Component::Transform;
			const uint32_t eid = EditorScene::s_selectedEntity->linkedEntity;
			const auto& tRO = NE::ECS::Query::GetEntityTransform(eid);

			float matrix[16];
			memcpy(matrix, tRO.modelMatrix.Data(), sizeof(float) * 16);

			bool editedThisFrame = ImGuizmo::Manipulate(
				m_editorCamera.GetViewMatrix().Data(),
				m_editorCamera.GetProjectionMatrix().Data(),
				currentOperation, ImGuizmo::LOCAL, matrix
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

			if (s_gizmoActive && isUsing && editedThisFrame && s_gizmoCmd) {
				float tr[3], rotDeg[3], sc[3];
				ImGuizmo::DecomposeMatrixToComponents(matrix, tr, rotDeg, sc);

				auto current = NE::ECS::Query::GetEntityTransform(eid);
				auto after = current;

				if (s_gizmoMask & Editor::SetTransformCommand::Pos)
					after.position = { tr[0], tr[1], tr[2] };
				if (s_gizmoMask & Editor::SetTransformCommand::Rot)
					after.rotation = { Radians(rotDeg[0]), Radians(rotDeg[1]), Radians(rotDeg[2]) };
				if (s_gizmoMask & Editor::SetTransformCommand::Scl)
					after.scale = { sc[0], sc[1], sc[2] };

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
					if (s_gizmoMask & Editor::SetTransformCommand::Pos) changed |= !eq(B.position, A.position);
					if (s_gizmoMask & Editor::SetTransformCommand::Rot) changed |= !eq(B.rotation, A.rotation);
					if (s_gizmoMask & Editor::SetTransformCommand::Scl) changed |= !eq(B.scale, A.scale);

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
		m_editorCamera.LookAt(m_editorCamera.GetPosition() + dir, Vec3(0, 1, 0));

		//if (EditorScene::s_selectedEntity) {
		//	static ImGuizmo::OPERATION currentOperation = ImGuizmo::TRANSLATE;

		//	if (ImGui::IsKeyPressed(ImGuiKey_Q)) currentOperation = ImGuizmo::TRANSLATE;
		//	if (ImGui::IsKeyPressed(ImGuiKey_W)) currentOperation = ImGuizmo::ROTATE;
		//	if (ImGui::IsKeyPressed(ImGuiKey_E)) currentOperation = ImGuizmo::SCALE;

		//	ImGuizmo::SetOrthographic(false);
		//	ImGuizmo::SetDrawlist();
		//	ImGuizmo::SetRect(panelPos.x, panelPos.y, panelSize.x, panelSize.y);

		//	const auto& t = NE::ECS::Query::GetEntityTransform(EditorScene::s_selectedEntity->linkedEntity);

		//	float matrix[16];
		//	memcpy(matrix, t.modelMatrix.Data(), sizeof(float) * 16);

		//	if (ImGuizmo::Manipulate(m_editorCamera.GetViewMatrix().Data(),
		//		m_editorCamera.GetProjectionMatrix().Data(),
		//		currentOperation, ImGuizmo::LOCAL, matrix)) {
		//		float tr[3], rot[3], sc[3];
		//		ImGuizmo::DecomposeMatrixToComponents(matrix, tr, rot, sc);
		//		//t.position = { tr[0], tr[1], tr[2] };
		//		//t.rotation = { rot[0], rot[1], rot[2] };
		//		//t.scale = { sc[0], sc[1], sc[2] };
		//		//t.isDirty = true;
		//	}
		//}

		//ImGuizmo::IsUsingAny()
		//ImVec2 mousePos = ImGui::GetMousePos();
		//float localX = mousePos.x - panelPos.x;
		//float localY = mousePos.y - panelPos.y;
		//float spMouseX = localX / panelSize.x;
		//float spMouseY = localY / panelSize.y;

		//if (localX < 0 || localY < 0 || localX > panelSize.x || localY > panelSize.y) {
		//	//return; // Mouse is outside the panel
		//} else {
		//	float scrollOffset = ImGui::GetIO().MouseWheel;
		//	if (scrollOffset != 0) {
		//		float zoomSpeed = 0.1f; // Zoom sensitivity
		//		camera.SetZoom(scrollOffset * zoomSpeed);
		//	}

		//	// Moving the camera
		//	if (ImGui::IsWindowFocused()) {
		//		float camSpeed = 10.f;
		//		if (ImGui::IsKeyDown(ImGuiKey_UpArrow)) {
		//			camera.MoveUp(camSpeed);
		//		} else if (ImGui::IsKeyDown(ImGuiKey_DownArrow)) {
		//			camera.MoveDown(camSpeed);
		//		}
		//		if (ImGui::IsKeyDown(ImGuiKey_LeftArrow)) {
		//			camera.MoveLeft(camSpeed);
		//		} else if (ImGui::IsKeyDown(ImGuiKey_RightArrow)) {
		//			camera.MoveRight(camSpeed);
		//		}

		//		// Object Picking
		//		if (!ImGuizmo::IsUsingAny()) {
		//			if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
		//				int x = static_cast<int>(spMouseX * Application::GetWindowSize().first);
		//				int y = static_cast<int>(spMouseY * Application::GetWindowSize().second);
		//				Vec4 colour = GraphicsManager::GetInstance().GetPixelColor(
		//					GraphicsManager::GetInstance().frameBuffers[GraphicsManager::FrameBufferIndex::OBJ_PICKING_ENGINE], x, y);

		//				uint32_t clickedEntity = ECSManager::GetInstance().renderSystem->DecodeColor(colour);
		//				//std::cout << "Clicked on entity: " << clickedEntity << std::endl;
		//				if (sceneEntityMap.find(clickedEntity) != sceneEntityMap.end()) {
		//					selectedEntity = &*sceneEntityMap.find(clickedEntity)->second;
		//					//auto r = ECSManager::GetInstance().TryGetComponent<Renderer>(selectedEntity->id);
		//					//if (r.has_value()) {
		//					//	ECSManager::GetInstance().renderSystem->SetVisibility(r->get().currentMeshDebugID, true);
		//					//}
		//				} else {
		//					//if (selectedEntity) {
		//					//	auto r = ECSManager::GetInstance().TryGetComponent<Renderer>(selectedEntity->id);
		//					//	if (r.has_value()) {
		//					//		ECSManager::GetInstance().renderSystem->SetVisibility(r->get().currentMeshDebugID, false);
		//					//	}
		//					//}
		//					selectedEntity = nullptr;
		//				}
		//			}
		//		}
		//	}

		//	// Camera Dragging
		//	static bool isDragging = false;
		//	static ImVec2 lastMousePos;
		//	if (ImGui::IsMouseDragging(ImGuiMouseButton_Right)) {
		//		if (!isDragging) {
		//			isDragging = true;
		//			lastMousePos = ImGui::GetMousePos();
		//		}

		//		ImVec2 currentMousePos = ImGui::GetMousePos();
		//		ImVec2 delta = ImVec2(currentMousePos.x - lastMousePos.x, currentMousePos.y - lastMousePos.y);

		//		float viewportWidth = panelSize.x;
		//		float viewportHeight = panelSize.y;

		//		float ndcX = delta.x / viewportWidth * 2.0f;
		//		float ndcY = delta.y / viewportHeight * 2.0f;

		//		float worldDeltaX = ndcX * (camera.screenWidth / 2.0f) / camera.zoom;
		//		float worldDeltaY = ndcY * (camera.screenHeight / 2.0f) / camera.zoom;

		//		camera.MoveRight(-worldDeltaX);
		//		camera.MoveUp(worldDeltaY);

		//		lastMousePos = currentMousePos;
		//	} else {
		//		isDragging = false;
		//	}

		//	if (ImGui::IsKeyPressed(ImGuiKey_W)) {
		//		currentOperation = ImGuizmo::TRANSLATE;
		//	}
		//	if (ImGui::IsKeyPressed(ImGuiKey_E)) {
		//		currentOperation = ImGuizmo::SCALE;
		//	}
		//	if (ImGui::IsKeyPressed(ImGuiKey_R)) {
		//		currentOperation = ImGuizmo::ROTATE;
		//	}
		//}

		ImGui::End();
	}

	NE::Graphics::EditorCamera* ScenePanel::GetCamera()
	{
		return &m_editorCamera;
	}
}
