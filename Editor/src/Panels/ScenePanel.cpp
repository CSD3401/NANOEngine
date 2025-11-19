#include "ScenePanel.hpp"
#include "Math/Vec3.hpp"
#include <imgui/imgui.h>
#include "../EditorScene.hpp"
#include "Engine.hpp"
#include <imgui/widgets/imguizmo/ImGuizmo.h>
#include <EditorInterface/ECSExports.hpp>
#include <ECS/Components/Transform.hpp>
#include <ECS/Components/UIRectTransform.hpp>
#include "../Command/EditorSetTransformCommand.hpp"
#include "../Command/CommandHistory.hpp"
#include "Graphics/Core/UIRenderer.hpp"
#include <iostream>

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
		float farPlane = 1000.0f;

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

		// display the rendered game world
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

					EditorScene::s_entities.clear();
					NE::EditorEdit();

					auto numEntt = NE::GetNumEntities();
					for (unsigned int i = 0; i < numEntt; ++i) {
						EditorScene::s_entities.push_back(EditorEntity{ i });
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

						// convert mouse position to framebuffer coordinates
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

		// transform gizmos
		if (EditorScene::s_selectedEntity) {
			const uint32_t eid = EditorScene::s_selectedEntity->linkedEntity;

			// check which type of entity this is
			bool hasTransform = NE::ECS::Query::HasTransform(eid);
			bool hasUIRectTransform = NE::ECS::Query::HasUIRectTransform(eid);

			if (hasTransform) {
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
					m_editorCamera.GetViewMatrix().Data(),
					m_editorCamera.GetProjectionMatrix().Data(),
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

				// draw in pixel space
				float fbWidth = 1920.f;  // temp hardcoded
				float fbHeight = 1080.f; // temp hardcoded

				// Convert UI rect from framebuffer coords to panel coords
				float scaleX = panelSize.x / fbWidth;
				float scaleY = panelSize.y / fbHeight;

				ImVec2 topLeft(
					panelPos.x + rectTransform.x * scaleX,
					panelPos.y + rectTransform.y * scaleY
				);
				ImVec2 bottomRight(
					panelPos.x + (rectTransform.x + rectTransform.width) * scaleX,
					panelPos.y + (rectTransform.y + rectTransform.height) * scaleY
				);
				ImVec2 center(
					(topLeft.x + bottomRight.x) * 0.5f,
					(topLeft.y + bottomRight.y) * 0.5f
				);

				// outline (white)
				ImDrawList* drawList = ImGui::GetWindowDrawList();
				drawList->AddRect(topLeft, bottomRight, IM_COL32(255, 255, 255, 255), 0.0f, 0, 2.0f);

				// corner handles (blue)
				const float handleSize = 8.0f;
				ImVec2 corners[4] = {
					topLeft,
					ImVec2(bottomRight.x, topLeft.y),
					bottomRight,
					ImVec2(topLeft.x, bottomRight.y)
				};
				for (int i = 0; i < 4; i++) {
					drawList->AddRectFilled(
						ImVec2(corners[i].x - handleSize * 0.5f, corners[i].y - handleSize * 0.5f),
						ImVec2(corners[i].x + handleSize * 0.5f, corners[i].y + handleSize * 0.5f),
						IM_COL32(0, 0, 255, 255)
					);
				}

				// center handle (blue)
				drawList->AddCircleFilled(center, handleSize * 0.5f, IM_COL32(0, 0, 255, 255));

				// --- Interaction state (static so it persists across frames) ---
				static bool isDraggingUI = false;
				static int  draggingCorner = -1; // -1 = none, 0..3 = which corner
				static ImVec2 dragStart;         // mouse start (pixels)
				static NE::ECS::Component::UIRectTransform originalTransform;

				// helper
				auto mouseInPanel = [&](ImVec2 p) {
					return p.x >= panelPos.x && p.x <= panelPos.x + panelSize.x &&
						p.y >= panelPos.y && p.y <= panelPos.y + panelSize.y;
					};

				ImVec2 mousePos = ImGui::GetMousePos();
				bool mouseInThisPanel = mouseInPanel(mousePos);

				// handle move by center handle
				if (!isDraggingUI && draggingCorner < 0 && mouseInThisPanel && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
				{
					float dx = mousePos.x - center.x;
					float dy = mousePos.y - center.y;
					float dist2 = dx * dx + dy * dy;
					float radius = handleSize * 0.5f;
					if (dist2 <= radius * radius) {
						isDraggingUI = true;
						dragStart = mousePos;
						originalTransform = rectTransform;
						// Optional: visual feedback
						ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
					}
				}

				// handle by corner handles
				if (!isDraggingUI && draggingCorner < 0 && mouseInThisPanel && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
				{
					for (int i = 0; i < 4; ++i) {
						float dx = mousePos.x - corners[i].x;
						float dy = mousePos.y - corners[i].y;
						if (fabsf(dx) <= handleSize && fabsf(dy) <= handleSize) {
							draggingCorner = i;
							dragStart = mousePos;
							originalTransform = rectTransform;
							ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
							break;
						}
					}
				}

				// perform move
				if (isDraggingUI) {
					if (ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
						ImVec2 deltaPixels(mousePos.x - dragStart.x, mousePos.y - dragStart.y);
						// Convert panel deltas back to framebuffer deltas
						rectTransform.x = originalTransform.x + (deltaPixels.x / scaleX);
						rectTransform.y = originalTransform.y + (deltaPixels.y / scaleY);
					}
					if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
						isDraggingUI = false;
						// TODO: push command for undo/redo
					}
				}

				// perform resize
				if (draggingCorner >= 0) {
					if (ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
						ImVec2 deltaPixels(mousePos.x - dragStart.x, mousePos.y - dragStart.y);
						float deltaFBX = deltaPixels.x / scaleX;
						float deltaFBY = deltaPixels.y / scaleY;

						// Anchor logic: adjust position to keep the dragged corner under the mouse
						switch (draggingCorner) {
						case 0: // Top-left
							rectTransform.x = originalTransform.x + deltaFBX;
							rectTransform.y = originalTransform.y + deltaFBY;
							rectTransform.width = originalTransform.width - deltaFBX;
							rectTransform.height = originalTransform.height - deltaFBY;
							break;
						case 1: // Top-right
							rectTransform.y = originalTransform.y + deltaFBY;
							rectTransform.width = originalTransform.width + deltaFBX;
							rectTransform.height = originalTransform.height - deltaFBY;
							break;
						case 2: // Bottom-right
							rectTransform.width = originalTransform.width + deltaFBX;
							rectTransform.height = originalTransform.height + deltaFBY;
							break;
						case 3: // Bottom-left
							rectTransform.x = originalTransform.x + deltaFBX;
							rectTransform.width = originalTransform.width - deltaFBX;
							rectTransform.height = originalTransform.height + deltaFBY;
							break;
						}

						// Clamp size (>= 1px)
						rectTransform.width = std::max(1.0f, rectTransform.width);
						rectTransform.height = std::max(1.0f, rectTransform.height);
					}
					if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
						draggingCorner = -1;
						// TODO: push command for undo/redo
					}
				}
			}
		}

		// Calculate camera's look direction regardless of input
		Vec3 dir;
		dir.x = cosf(Radians(m_cameraYaw)) * cosf(Radians(m_cameraPitch));
		dir.y = sinf(Radians(m_cameraPitch));
		dir.z = sinf(Radians(m_cameraYaw)) * cosf(Radians(m_cameraPitch));
		m_editorCamera.LookAt(m_editorCamera.GetPosition() + dir, Vec3(0, 1, 0));

		ImGui::End();
	}

	NE::Graphics::EditorCamera* ScenePanel::GetCamera()
	{
		return &m_editorCamera;
	}
}
