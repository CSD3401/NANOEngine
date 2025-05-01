#include "ScenePanel.hpp"
#include <imgui/imgui.h>

namespace Editor {
	ScenePanel::ScenePanel() {
	}

	void ScenePanel::OnImGuiRender()
	{
		ImGui::Begin("Scene", nullptr,
			ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_MenuBar);

		ImVec2 panelPos = ImGui::GetCursorScreenPos();
		ImVec2 panelSize = ImGui::GetContentRegionAvail();

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
}
