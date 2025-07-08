#include "ScenePanel.hpp"
#include "src/Math/Vec3.hpp"
#include <imgui/imgui.h>
#include "../EditorScene.hpp"
#include "Engine.hpp"
#include <imgui/widgets/imguizmo/ImGuizmo.h>
#include <ECSInternals.hpp>
#include <src/ECS/Components/Transform.hpp>

namespace Editor {
	static uint32_t temp;

	// TEMP TO BE MOVED TO SHARED MATH LIB
	float Radians(float deg) {
		return deg * 3.14159265358979323846f / 180.0f;
	}

	ScenePanel::ScenePanel(uint32_t sceneFrameBuffer) {
		temp = sceneFrameBuffer;

		NANOEngine::Math::Vec3 position = { 0.0f, 0.0f, 10.0f };
		NANOEngine::Math::Vec3 target = { 0.0f, 0.0f, 0.0f };
		NANOEngine::Math::Vec3 up = { 0.0f, 1.0f, 0.0f };


		float fovYRadians = 45.0f * (NANOEngine::Math::PI / 180.0f); // 45 degrees fov
		float aspectRatio = 1920.f / 1080.f;
		float nearPlane = 0.1f;
		float farPlane = 100.0f;

		m_editorCamera.SetPerspective(fovYRadians, aspectRatio, nearPlane, farPlane);
		m_editorCamera.SetPosition(position);
		m_editorCamera.LookAt(target, up);
	}

	void ScenePanel::OnImGuiRender()
	{
		using namespace NANOEngine::Math;

		ImGui::Begin("Scene", nullptr,
			ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse 
			| ImGuiWindowFlags_MenuBar);

		ImVec2 panelPos = ImGui::GetCursorScreenPos();
		ImVec2 panelSize = ImGui::GetContentRegionAvail();

		float deltaTime = ImGui::GetIO().DeltaTime;

		ImGui::Image((ImTextureID)(uintptr_t)temp, panelSize, ImVec2(0, 1), ImVec2(1, 0));

		// Handle input only when scene panel is focused
		if (ImGui::IsWindowFocused()) {
			ImGuiIO& io = ImGui::GetIO();

			// Movement: WASD or arrow keys
			Vec3 move(0.0f);
			if (ImGui::IsKeyDown(ImGuiKey_W) || ImGui::IsKeyDown(ImGuiKey_UpArrow))    move.z += 1.0f;
			if (ImGui::IsKeyDown(ImGuiKey_S) || ImGui::IsKeyDown(ImGuiKey_DownArrow))  move.z -= 1.0f;
			if (ImGui::IsKeyDown(ImGuiKey_A) || ImGui::IsKeyDown(ImGuiKey_LeftArrow))  move.x -= 1.0f;
			if (ImGui::IsKeyDown(ImGuiKey_D) || ImGui::IsKeyDown(ImGuiKey_RightArrow)) move.x += 1.0f;

			if (move.LengthSquared() > 0.0f) {
				move.Normalize();
				// Calculate direction vectors
				Vec3 forward = m_editorCamera.GetForward();
				Vec3 right = forward.Cross(Vec3(0, 1, 0)).Normalized();

				Vec3 offset = (right * move.x + forward * move.z) * m_cameraSpeed * deltaTime;
				m_editorCamera.SetPosition(m_editorCamera.GetPosition() + offset);
			}

			// Mouse look
			if (ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
				if (!m_rightMouseHeld) {
					m_lastMousePos = io.MousePos;
					m_rightMouseHeld = true;
				}

				ImVec2 delta = { io.MousePos.x - m_lastMousePos.x, io.MousePos.y - m_lastMousePos.y };
				m_lastMousePos = io.MousePos;

				m_cameraYaw += delta.x * m_mouseSensitivity;
				m_cameraPitch -= delta.y * m_mouseSensitivity;

				// Clamp pitch
				if (m_cameraPitch > 89.0f) m_cameraPitch = 89.0f;
				if (m_cameraPitch < -89.0f) m_cameraPitch = -89.0f;

				Vec3 dir;
				dir.x = cosf(Radians(m_cameraYaw)) * cosf(Radians(m_cameraPitch));
				dir.y = sinf(Radians(m_cameraPitch));
				dir.z = sinf(Radians(m_cameraYaw)) * cosf(Radians(m_cameraPitch));
				m_editorCamera.LookAt(m_editorCamera.GetPosition() + dir, Vec3(0, 1, 0));
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

						uint32_t id = NANOEngine::GetPickedEntity(x, y);

						EditorScene::s_selectedEntity = nullptr;
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

			auto& t = NANOEngine::GetEntityTransform(EditorScene::s_selectedEntity->linkedEntity);

			float matrix[16];
			memcpy(matrix, t.modelMatrix.Data(), sizeof(float) * 16);

			if (ImGuizmo::Manipulate(m_editorCamera.GetViewMatrix().Data(),
				m_editorCamera.GetProjectionMatrix().Data(),
				currentOperation, ImGuizmo::LOCAL, matrix)) {
				float tr[3], rot[3], sc[3];
				ImGuizmo::DecomposeMatrixToComponents(matrix, tr, rot, sc);
				t.position = { tr[0], tr[1], tr[2] };
				t.rotation = { rot[0], rot[1], rot[2] };
				t.scale = { sc[0], sc[1], sc[2] };
				t.isDirty = true;
			}
		}

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

	NANOEngine::Graphics::Camera* ScenePanel::GetCamera()
	{
		return &m_editorCamera;
	}
}
