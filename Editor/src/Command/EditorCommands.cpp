#include "pch.h"
#include "EditorCommands.hpp"

// Include component headers before ECSExports (which has forward declarations)
#include <ECS/Components/EntityMeta.hpp>
#include <ECS/Components/Renderer.hpp>
#include <ECS/Components/Hierarchy.hpp>
#include <ECS/Components/UIRectTransform.hpp>
#include <ECS/Components/UICanvas.hpp>
#include <ECS/Components/UIImage.hpp>
#include <ECS/Components/UIText.hpp>
#include <ECS/Components/UIButton.hpp>
#include <ECS/Components/UISlider.hpp>
#include <ECS/Components/UIToggle.hpp>

#include "EditorInterface/ECSExports.hpp"
#include <EditorInterface/RendererExports.hpp>
#include "../EditorScene.hpp"
#include <ECS/Core/Entity.hpp>
#include <SceneManagement/SceneManager.hpp>
#include <algorithm>
#include <limits>
#include <Core/LUIDGenerator.hpp>

namespace Editor {
	CreateEmptyEntityCommand::CreateEmptyEntityCommand(uint32_t parentEntity) 
		: m_entity(0), m_parentEntity(parentEntity) {}

	void CreateEmptyEntityCommand::Execute() {
		m_entity = NE::ECS::Command::CreateEmptyEntity(m_parentEntity);
		if (m_parentEntity == NE::ECS::NO_ENTITY)
			EditorScene::s_rootOrder.push_back(m_entity);
	}

	void CreateEmptyEntityCommand::Undo() {
		EditorScene::UnregisterRoot(m_entity);
		NE::ECS::Command::DestroyEntity(m_entity);
	}

	// Helper function to find existing canvas or create one
	uint32_t FindOrCreateCanvas() {
		// Search for existing canvas entity
		auto& ecs = NE::GetScene().GetECSCoordinator();
		const auto& entities = ecs.GetEntityManager().GetUsedEntities();

		for (auto e : entities) {
			if (NE::ECS::Query::HasUICanvas(e)) {
				return e;
			}
		}

		// No canvas found, create one
		uint32_t canvasEntity = NE::ECS::Command::CreateUICanvasEntity();
		EditorScene::s_rootOrder.push_back(canvasEntity);
		return canvasEntity;
	}

	// ==================== CreateUICanvasCommand ====================
	CreateUICanvasCommand::CreateUICanvasCommand() {}

	void CreateUICanvasCommand::Execute() {
		m_entity = NE::ECS::Command::CreateUICanvasEntity();
		EditorScene::s_rootOrder.push_back(m_entity);
		EditorScene::s_selection.SetSingle(m_entity);
	}

	void CreateUICanvasCommand::Undo() {
		EditorScene::UnregisterRoot(m_entity);
		NE::ECS::Command::DestroyEntity(m_entity);
	}

	// ==================== CreateUITextCommand ====================
	CreateUITextCommand::CreateUITextCommand(uint32_t parentEntity)
		: m_parentEntity(parentEntity) {}

	void CreateUITextCommand::Execute() {
		// Find or create canvas
		uint32_t canvasEntity = NE::ECS::NO_ENTITY;

		// Check if parent is valid and under a canvas
		if (m_parentEntity != NE::ECS::NO_ENTITY && NE::ECS::Query::HasUIRectTransform(m_parentEntity)) {
			canvasEntity = m_parentEntity;
			// Walk up to find the canvas root
			while (canvasEntity != NE::ECS::NO_ENTITY) {
				if (NE::ECS::Query::HasUICanvas(canvasEntity)) break;
				canvasEntity = NE::ECS::Query::HasHierarchy(canvasEntity) ? NE::ECS::Query::GetEntityHierarchy(canvasEntity).parent : NE::ECS::NO_ENTITY;
			}
		}

		// If no canvas found in hierarchy, find or create one
		if (canvasEntity == NE::ECS::NO_ENTITY || !NE::ECS::Query::HasUICanvas(canvasEntity)) {
			m_canvasEntity = FindOrCreateCanvas();
			m_createdCanvas = (m_canvasEntity != NE::ECS::NO_ENTITY);
			canvasEntity = m_canvasEntity;
		}

		// Determine parent for the text entity
		uint32_t parentForText = (m_parentEntity != NE::ECS::NO_ENTITY && NE::ECS::Query::HasUIRectTransform(m_parentEntity))
			? m_parentEntity : canvasEntity;

		// Create the text entity
		m_entity = NE::ECS::Command::CreateEntityNoComponents();

		NE::ECS::Command::AddEntityMetaComponent(m_entity,
			NE::ECS::Component::EntityMeta{ .name = "Text", .luid = NE::Core::LUIDGenerator::Generate("em") });

		NE::ECS::Component::Hierarchy hierarchy{};
		hierarchy.luid = NE::Core::LUIDGenerator::Generate("hr");
		NE::ECS::Command::AddHierarchyComponent(m_entity, hierarchy);

		// Setup UIRectTransform
		NE::ECS::Component::UIRectTransform rectTransform{};
		rectTransform.width = 160.0f;
		rectTransform.height = 30.0f;
		rectTransform.x = 0.0f;
		rectTransform.y = 0.0f;
		// NOTE: Parent relationship now managed by Hierarchy component via SetParent call below
		NE::ECS::Command::AddUIRectTransformComponent(m_entity, rectTransform);

		// Setup UIText
		NE::ECS::Component::UIText text{};
		text.text = "New Text";
		text.fontSize = 24.0f;
		text.color = NE::Math::Vec4(1.0f, 1.0f, 1.0f, 1.0f);
		text.horizontalAlign = NE::ECS::Component::UIText::Alignment::CENTER;
		text.verticalAlign = NE::ECS::Component::UIText::VerticalAlignment::MIDDLE;
		NE::ECS::Command::AddUITextComponent(m_entity, text);

		// Set hierarchy parent (syncs Hierarchy component)
		NE::ECS::Command::SetParent(m_entity, parentForText, -1, false);

		EditorScene::s_selection.SetSingle(m_entity);
	}

	void CreateUITextCommand::Undo() {
		NE::ECS::Command::DestroyEntity(m_entity);
		if (m_createdCanvas && m_canvasEntity != NE::ECS::NO_ENTITY) {
			EditorScene::UnregisterRoot(m_canvasEntity);
			NE::ECS::Command::DestroyEntity(m_canvasEntity);
		}
	}

	// ==================== CreateUIImageCommand ====================
	CreateUIImageCommand::CreateUIImageCommand(uint32_t parentEntity)
		: m_parentEntity(parentEntity) {}

	void CreateUIImageCommand::Execute() {
		// Find or create canvas (same logic as text)
		uint32_t canvasEntity = NE::ECS::NO_ENTITY;

		if (m_parentEntity != NE::ECS::NO_ENTITY && NE::ECS::Query::HasUIRectTransform(m_parentEntity)) {
			canvasEntity = m_parentEntity;
			while (canvasEntity != NE::ECS::NO_ENTITY) {
				if (NE::ECS::Query::HasUICanvas(canvasEntity)) break;
				canvasEntity = NE::ECS::Query::HasHierarchy(canvasEntity) ? NE::ECS::Query::GetEntityHierarchy(canvasEntity).parent : NE::ECS::NO_ENTITY;
			}
		}

		if (canvasEntity == NE::ECS::NO_ENTITY || !NE::ECS::Query::HasUICanvas(canvasEntity)) {
			m_canvasEntity = FindOrCreateCanvas();
			m_createdCanvas = (m_canvasEntity != NE::ECS::NO_ENTITY);
			canvasEntity = m_canvasEntity;
		}

		uint32_t parentForImage = (m_parentEntity != NE::ECS::NO_ENTITY && NE::ECS::Query::HasUIRectTransform(m_parentEntity))
			? m_parentEntity : canvasEntity;

		// Create the image entity 
		m_entity = NE::ECS::Command::CreateEntityNoComponents();

		NE::ECS::Command::AddEntityMetaComponent(m_entity,
			NE::ECS::Component::EntityMeta{ .name = "Image", .luid = NE::Core::LUIDGenerator::Generate("em") });

		NE::ECS::Component::Hierarchy hierarchy{};
		hierarchy.luid = NE::Core::LUIDGenerator::Generate("hr");
		NE::ECS::Command::AddHierarchyComponent(m_entity, hierarchy);

		// Setup UIRectTransform
		NE::ECS::Component::UIRectTransform rectTransform{};
		rectTransform.width = 100.0f;
		rectTransform.height = 100.0f;
		rectTransform.x = 0.0f;
		rectTransform.y = 0.0f;
		// NOTE: Parent relationship now managed by Hierarchy component via SetParent call below
		NE::ECS::Command::AddUIRectTransformComponent(m_entity, rectTransform);

		// Setup UIImage
		NE::ECS::Component::UIImage img{};
		img.color = NE::Math::Vec4(1.0f, 1.0f, 1.0f, 1.0f);
		NE::ECS::Command::AddUIImageComponent(m_entity, img);

		// Set hierarchy parent (syncs Hierarchy component)
		NE::ECS::Command::SetParent(m_entity, parentForImage, -1, false);

		EditorScene::s_selection.SetSingle(m_entity);
	}

	void CreateUIImageCommand::Undo() {
		NE::ECS::Command::DestroyEntity(m_entity);
		if (m_createdCanvas && m_canvasEntity != NE::ECS::NO_ENTITY) {
			EditorScene::UnregisterRoot(m_canvasEntity);
			NE::ECS::Command::DestroyEntity(m_canvasEntity);
		}
	}

	// ==================== CreateUIButtonCommand ====================
	CreateUIButtonCommand::CreateUIButtonCommand(uint32_t parentEntity)
		: m_parentEntity(parentEntity) {}

	void CreateUIButtonCommand::Execute() {
		// Find or create canvas
		uint32_t canvasEntity = NE::ECS::NO_ENTITY;

		if (m_parentEntity != NE::ECS::NO_ENTITY && NE::ECS::Query::HasUIRectTransform(m_parentEntity)) {
			canvasEntity = m_parentEntity;
			while (canvasEntity != NE::ECS::NO_ENTITY) {
				if (NE::ECS::Query::HasUICanvas(canvasEntity)) break;
				canvasEntity = NE::ECS::Query::HasHierarchy(canvasEntity) ? NE::ECS::Query::GetEntityHierarchy(canvasEntity).parent : NE::ECS::NO_ENTITY;
			}
		}

		if (canvasEntity == NE::ECS::NO_ENTITY || !NE::ECS::Query::HasUICanvas(canvasEntity)) {
			m_canvasEntity = FindOrCreateCanvas();
			m_createdCanvas = (m_canvasEntity != NE::ECS::NO_ENTITY);
			canvasEntity = m_canvasEntity;
		}

		uint32_t parentForButton = (m_parentEntity != NE::ECS::NO_ENTITY && NE::ECS::Query::HasUIRectTransform(m_parentEntity))
			? m_parentEntity : canvasEntity;

		// Create the button entity
		m_entity = NE::ECS::Command::CreateEntityNoComponents();

		NE::ECS::Command::AddEntityMetaComponent(m_entity,
			NE::ECS::Component::EntityMeta{ .name = "Button", .luid = NE::Core::LUIDGenerator::Generate("em") });

		NE::ECS::Command::AddHierarchyComponent(m_entity, NE::ECS::Component::Hierarchy{});

		// Setup UIRectTransform for button
		NE::ECS::Component::UIRectTransform buttonRect{};
		buttonRect.width = 160.0f;
		buttonRect.height = 30.0f;
		buttonRect.x = 0.0f;
		buttonRect.y = 0.0f;
		// NOTE: Parent relationship now managed by Hierarchy component via SetParent call below
		NE::ECS::Command::AddUIRectTransformComponent(m_entity, buttonRect);

		// Setup UIImage for button background
		NE::ECS::Component::UIImage img{};
		img.color = NE::Math::Vec4(0.8f, 0.8f, 0.8f, 1.0f);
		NE::ECS::Command::AddUIImageComponent(m_entity, img);

		// Setup UIButton
		NE::ECS::Component::UIButton button{};
		button.normalColor = NE::Math::Vec4(0.8f, 0.8f, 0.8f, 1.0f);
		button.hoverColor = NE::Math::Vec4(0.9f, 0.9f, 0.9f, 1.0f);
		button.pressedColor = NE::Math::Vec4(0.6f, 0.6f, 0.6f, 1.0f);
		button.disabledColor = NE::Math::Vec4(0.5f, 0.5f, 0.5f, 0.5f);
		NE::ECS::Command::AddUIButtonComponent(m_entity, button);

		// Set hierarchy parent for button
		NE::ECS::Command::SetParent(m_entity, parentForButton, -1, false);

		// Create child Text entity
		m_textEntity = NE::ECS::Command::CreateEntityNoComponents();

		NE::ECS::Command::AddEntityMetaComponent(m_textEntity,
			NE::ECS::Component::EntityMeta{ .name = "Text", .luid = NE::Core::LUIDGenerator::Generate("em") });

		NE::ECS::Command::AddHierarchyComponent(m_textEntity, NE::ECS::Component::Hierarchy{});

		// Setup UIRectTransform for text (stretch to fill button)
		NE::ECS::Component::UIRectTransform textRect{};
		textRect.anchorMinX = 0.0f; textRect.anchorMinY = 0.0f;
		textRect.anchorMaxX = 1.0f; textRect.anchorMaxY = 1.0f;
		textRect.offsetMinX = 0.0f; textRect.offsetMinY = 0.0f;
		textRect.offsetMaxX = 0.0f; textRect.offsetMaxY = 0.0f;
		textRect.width = 160.0f;
		textRect.height = 30.0f;
		// NOTE: Parent relationship now managed by Hierarchy component via SetParent call below
		NE::ECS::Command::AddUIRectTransformComponent(m_textEntity, textRect);

		// Setup UIText for button label
		NE::ECS::Component::UIText text{};
		text.text = "Button";
		text.fontSize = 18.0f;
		text.color = NE::Math::Vec4(0.2f, 0.2f, 0.2f, 1.0f);
		text.horizontalAlign = NE::ECS::Component::UIText::Alignment::CENTER;
		text.verticalAlign = NE::ECS::Component::UIText::VerticalAlignment::MIDDLE;
		NE::ECS::Command::AddUITextComponent(m_textEntity, text);

		// Set hierarchy parent for text
		NE::ECS::Command::SetParent(m_textEntity, m_entity, -1, false);

		EditorScene::s_selection.SetSingle(m_entity);
	}

	void CreateUIButtonCommand::Undo() {
		NE::ECS::Command::DestroyEntity(m_textEntity);
		NE::ECS::Command::DestroyEntity(m_entity);
		if (m_createdCanvas && m_canvasEntity != NE::ECS::NO_ENTITY) {
			EditorScene::UnregisterRoot(m_canvasEntity);
			NE::ECS::Command::DestroyEntity(m_canvasEntity);
		}
	}

	// ==================== CreateUIPanelCommand ====================
	CreateUIPanelCommand::CreateUIPanelCommand(uint32_t parentEntity)
		: m_parentEntity(parentEntity) {}

	void CreateUIPanelCommand::Execute() {
		// Find or create canvas
		uint32_t canvasEntity = NE::ECS::NO_ENTITY;

		if (m_parentEntity != NE::ECS::NO_ENTITY && NE::ECS::Query::HasUIRectTransform(m_parentEntity)) {
			canvasEntity = m_parentEntity;
			while (canvasEntity != NE::ECS::NO_ENTITY) {
				if (NE::ECS::Query::HasUICanvas(canvasEntity)) break;
				canvasEntity = NE::ECS::Query::HasHierarchy(canvasEntity) ? NE::ECS::Query::GetEntityHierarchy(canvasEntity).parent : NE::ECS::NO_ENTITY;
			}
		}

		if (canvasEntity == NE::ECS::NO_ENTITY || !NE::ECS::Query::HasUICanvas(canvasEntity)) {
			m_canvasEntity = FindOrCreateCanvas();
			m_createdCanvas = (m_canvasEntity != NE::ECS::NO_ENTITY);
			canvasEntity = m_canvasEntity;
		}

		uint32_t parentForPanel = (m_parentEntity != NE::ECS::NO_ENTITY && NE::ECS::Query::HasUIRectTransform(m_parentEntity))
			? m_parentEntity : canvasEntity;

		// Create the panel entity
		m_entity = NE::ECS::Command::CreateEntityNoComponents();

		NE::ECS::Command::AddEntityMetaComponent(m_entity,
			NE::ECS::Component::EntityMeta{ .name = "Panel", .luid = NE::Core::LUIDGenerator::Generate("em") });

		NE::ECS::Command::AddHierarchyComponent(m_entity, NE::ECS::Component::Hierarchy{});

		// Setup UIRectTransform with some default stretch
		NE::ECS::Component::UIRectTransform rect{};
		rect.width = 200.0f;
		rect.height = 200.0f;
		rect.x = 0.0f;
		rect.y = 0.0f;
		// NOTE: Parent relationship now managed by Hierarchy component via SetParent call below
		NE::ECS::Command::AddUIRectTransformComponent(m_entity, rect);

		// Setup UIImage with semi-transparent white
		NE::ECS::Component::UIImage img{};
		img.color = NE::Math::Vec4(1.0f, 1.0f, 1.0f, 0.4f);
		NE::ECS::Command::AddUIImageComponent(m_entity, img);

		// Set hierarchy parent
		NE::ECS::Command::SetParent(m_entity, parentForPanel, -1, false);

		EditorScene::s_selection.SetSingle(m_entity);
	}

	void CreateUIPanelCommand::Undo() {
		NE::ECS::Command::DestroyEntity(m_entity);
		if (m_createdCanvas && m_canvasEntity != NE::ECS::NO_ENTITY) {
			EditorScene::UnregisterRoot(m_canvasEntity);
			NE::ECS::Command::DestroyEntity(m_canvasEntity);
		}
	}

	// ==================== CreateUISliderCommand ====================
	CreateUISliderCommand::CreateUISliderCommand(uint32_t parentEntity)
		: m_parentEntity(parentEntity) {}

	void CreateUISliderCommand::Execute() {
		// Find or create canvas
		uint32_t canvasEntity = NE::ECS::NO_ENTITY;

		if (m_parentEntity != NE::ECS::NO_ENTITY && NE::ECS::Query::HasUIRectTransform(m_parentEntity)) {
			canvasEntity = m_parentEntity;
			while (canvasEntity != NE::ECS::NO_ENTITY) {
				if (NE::ECS::Query::HasUICanvas(canvasEntity)) break;
				canvasEntity = NE::ECS::Query::HasHierarchy(canvasEntity) ? NE::ECS::Query::GetEntityHierarchy(canvasEntity).parent : NE::ECS::NO_ENTITY;
			}
		}

		if (canvasEntity == NE::ECS::NO_ENTITY || !NE::ECS::Query::HasUICanvas(canvasEntity)) {
			m_canvasEntity = FindOrCreateCanvas();
			m_createdCanvas = (m_canvasEntity != NE::ECS::NO_ENTITY);
			canvasEntity = m_canvasEntity;
		}

		uint32_t parentForSlider = (m_parentEntity != NE::ECS::NO_ENTITY && NE::ECS::Query::HasUIRectTransform(m_parentEntity))
			? m_parentEntity : canvasEntity;

		// Create the slider root entity
		m_entity = NE::ECS::Command::CreateEntityNoComponents();

		NE::ECS::Command::AddEntityMetaComponent(m_entity,
			NE::ECS::Component::EntityMeta{ .name = "Slider", .luid = NE::Core::LUIDGenerator::Generate("em") });

		NE::ECS::Command::AddHierarchyComponent(m_entity, NE::ECS::Component::Hierarchy{});

		// Setup UIRectTransform for slider root
		NE::ECS::Component::UIRectTransform sliderRect{};
		sliderRect.width = 160.0f;
		sliderRect.height = 20.0f;
		sliderRect.x = 0.0f;
		sliderRect.y = 0.0f;
		// NOTE: Parent relationship now managed by Hierarchy component via SetParent call below
		NE::ECS::Command::AddUIRectTransformComponent(m_entity, sliderRect);

		// Setup UIImage for background
		NE::ECS::Component::UIImage bgImg{};
		bgImg.color = NE::Math::Vec4(0.3f, 0.3f, 0.3f, 1.0f);
		bgImg.raycastTarget = true;
		NE::ECS::Command::AddUIImageComponent(m_entity, bgImg);

		// Set hierarchy parent for slider root
		NE::ECS::Command::SetParent(m_entity, parentForSlider, -1, false);

		// Create Fill Area entity
		m_fillEntity = NE::ECS::Command::CreateEntityNoComponents();

		NE::ECS::Command::AddEntityMetaComponent(m_fillEntity,
			NE::ECS::Component::EntityMeta{ .name = "Fill", .luid = NE::Core::LUIDGenerator::Generate("em") });

		NE::ECS::Command::AddHierarchyComponent(m_fillEntity, NE::ECS::Component::Hierarchy{});

		// Setup UIRectTransform for fill
		NE::ECS::Component::UIRectTransform fillRect{};
		fillRect.anchorMinX = 0.0f; fillRect.anchorMinY = 0.0f;
		fillRect.anchorMaxX = 0.0f; fillRect.anchorMaxY = 1.0f;  // Fill from left
		fillRect.offsetMinX = 2.0f; fillRect.offsetMinY = 2.0f;
		fillRect.offsetMaxX = 0.0f; fillRect.offsetMaxY = -2.0f;
		fillRect.width = 0.0f;  // Will be controlled by slider value
		fillRect.height = 16.0f;
		// NOTE: Parent relationship now managed by Hierarchy component via SetParent call below
		NE::ECS::Command::AddUIRectTransformComponent(m_fillEntity, fillRect);

		// Setup UIImage for fill
		NE::ECS::Component::UIImage fillImg{};
		fillImg.color = NE::Math::Vec4(0.2f, 0.6f, 1.0f, 1.0f);  // Blue fill
		fillImg.raycastTarget = false;
		NE::ECS::Command::AddUIImageComponent(m_fillEntity, fillImg);

		NE::ECS::Command::SetParent(m_fillEntity, m_entity, -1, false);

		// Create Handle entity
		m_handleEntity = NE::ECS::Command::CreateEntityNoComponents();

		NE::ECS::Command::AddEntityMetaComponent(m_handleEntity,
			NE::ECS::Component::EntityMeta{ .name = "Handle", .luid = NE::Core::LUIDGenerator::Generate("em") });

		NE::ECS::Command::AddHierarchyComponent(m_handleEntity, NE::ECS::Component::Hierarchy{});

		// Setup UIRectTransform for handle
		NE::ECS::Component::UIRectTransform handleRect{};
		handleRect.width = 20.0f;
		handleRect.height = 20.0f;
		handleRect.x = 0.0f;  // Will be controlled by slider value
		handleRect.y = 0.0f;
		// NOTE: Parent relationship now managed by Hierarchy component via SetParent call below
		NE::ECS::Command::AddUIRectTransformComponent(m_handleEntity, handleRect);

		// Setup UIImage for handle
		NE::ECS::Component::UIImage handleImg{};
		handleImg.color = NE::Math::Vec4(1.0f, 1.0f, 1.0f, 1.0f);  // White handle
		handleImg.raycastTarget = true;
		NE::ECS::Command::AddUIImageComponent(m_handleEntity, handleImg);

		NE::ECS::Command::SetParent(m_handleEntity, m_entity, -1, false);

		// Setup UISlider component on root
		NE::ECS::Component::UISlider slider{};
		slider.luid = NE::Core::LUIDGenerator::Generate("sl");
		slider.value = 0.0f;
		slider.minValue = 0.0f;
		slider.maxValue = 1.0f;
		slider.fillRect = m_fillEntity;
		slider.handleRect = m_handleEntity;
		slider.backgroundRect = m_entity;
		NE::ECS::Command::AddUISliderComponent(m_entity, slider);

		EditorScene::s_selection.SetSingle(m_entity);
	}

	void CreateUISliderCommand::Undo() {
		NE::ECS::Command::DestroyEntity(m_handleEntity);
		NE::ECS::Command::DestroyEntity(m_fillEntity);
		NE::ECS::Command::DestroyEntity(m_entity);
		if (m_createdCanvas && m_canvasEntity != NE::ECS::NO_ENTITY) {
			EditorScene::UnregisterRoot(m_canvasEntity);
			NE::ECS::Command::DestroyEntity(m_canvasEntity);
		}
	}

	// ==================== CreateUIToggleCommand ====================
	CreateUIToggleCommand::CreateUIToggleCommand(uint32_t parentEntity)
		: m_parentEntity(parentEntity) {}

	void CreateUIToggleCommand::Execute() {
		// Find or create canvas
		uint32_t canvasEntity = NE::ECS::NO_ENTITY;

		if (m_parentEntity != NE::ECS::NO_ENTITY && NE::ECS::Query::HasUIRectTransform(m_parentEntity)) {
			canvasEntity = m_parentEntity;
			while (canvasEntity != NE::ECS::NO_ENTITY) {
				if (NE::ECS::Query::HasUICanvas(canvasEntity)) break;
				canvasEntity = NE::ECS::Query::HasHierarchy(canvasEntity) ? NE::ECS::Query::GetEntityHierarchy(canvasEntity).parent : NE::ECS::NO_ENTITY;
			}
		}

		if (canvasEntity == NE::ECS::NO_ENTITY || !NE::ECS::Query::HasUICanvas(canvasEntity)) {
			m_canvasEntity = FindOrCreateCanvas();
			m_createdCanvas = (m_canvasEntity != NE::ECS::NO_ENTITY);
			canvasEntity = m_canvasEntity;
		}

		uint32_t parentForToggle = (m_parentEntity != NE::ECS::NO_ENTITY && NE::ECS::Query::HasUIRectTransform(m_parentEntity))
			? m_parentEntity : canvasEntity;

		// Create the toggle root entity
		m_entity = NE::ECS::Command::CreateEntityNoComponents();

		NE::ECS::Command::AddEntityMetaComponent(m_entity,
			NE::ECS::Component::EntityMeta{ .name = "Toggle", .luid = NE::Core::LUIDGenerator::Generate("em") });

		NE::ECS::Command::AddHierarchyComponent(m_entity, NE::ECS::Component::Hierarchy{});

		// Setup UIRectTransform for toggle root
		NE::ECS::Component::UIRectTransform toggleRect{};
		toggleRect.width = 160.0f;
		toggleRect.height = 20.0f;
		toggleRect.x = 0.0f;
		toggleRect.y = 0.0f;
		// NOTE: Parent relationship now managed by Hierarchy component via SetParent call below
		NE::ECS::Command::AddUIRectTransformComponent(m_entity, toggleRect);

		// Set hierarchy parent for toggle root
		NE::ECS::Command::SetParent(m_entity, parentForToggle, -1, false);

		// Create Background entity (the checkbox box)
		m_backgroundEntity = NE::ECS::Command::CreateEntityNoComponents();

		NE::ECS::Command::AddEntityMetaComponent(m_backgroundEntity,
			NE::ECS::Component::EntityMeta{ .name = "Background", .luid = NE::Core::LUIDGenerator::Generate("em") });

		NE::ECS::Command::AddHierarchyComponent(m_backgroundEntity, NE::ECS::Component::Hierarchy{});

		// Setup UIRectTransform for background
		NE::ECS::Component::UIRectTransform bgRect{};
		bgRect.width = 20.0f;
		bgRect.height = 20.0f;
		bgRect.x = 0.0f;
		bgRect.y = 0.0f;
		bgRect.anchorMinX = 0.0f; bgRect.anchorMinY = 0.5f;
		bgRect.anchorMaxX = 0.0f; bgRect.anchorMaxY = 0.5f;
		bgRect.pivotX = 0.0f; bgRect.pivotY = 0.5f;
		// NOTE: Parent relationship now managed by Hierarchy component via SetParent call below
		NE::ECS::Command::AddUIRectTransformComponent(m_backgroundEntity, bgRect);

		// Setup UIImage for background
		NE::ECS::Component::UIImage bgImg{};
		bgImg.color = NE::Math::Vec4(1.0f, 1.0f, 1.0f, 1.0f);
		bgImg.raycastTarget = true;
		NE::ECS::Command::AddUIImageComponent(m_backgroundEntity, bgImg);

		NE::ECS::Command::SetParent(m_backgroundEntity, m_entity, -1, false);

		// Create Checkmark entity
		m_checkmarkEntity = NE::ECS::Command::CreateEntityNoComponents();

		NE::ECS::Command::AddEntityMetaComponent(m_checkmarkEntity,
			NE::ECS::Component::EntityMeta{ .name = "Checkmark", .luid = NE::Core::LUIDGenerator::Generate("em") });

		NE::ECS::Command::AddHierarchyComponent(m_checkmarkEntity, NE::ECS::Component::Hierarchy{});

		// Setup UIRectTransform for checkmark (centered in background)
		NE::ECS::Component::UIRectTransform checkRect{};
		checkRect.width = 14.0f;
		checkRect.height = 14.0f;
		checkRect.x = 0.0f;
		checkRect.y = 0.0f;
		checkRect.anchorMinX = 0.5f; checkRect.anchorMinY = 0.5f;
		checkRect.anchorMaxX = 0.5f; checkRect.anchorMaxY = 0.5f;
		// NOTE: Parent relationship now managed by Hierarchy component via SetParent call below
		NE::ECS::Command::AddUIRectTransformComponent(m_checkmarkEntity, checkRect);

		// Setup UIImage for checkmark
		NE::ECS::Component::UIImage checkImg{};
		checkImg.color = NE::Math::Vec4(0.2f, 0.2f, 0.2f, 1.0f);  // Dark checkmark
		checkImg.raycastTarget = false;
		NE::ECS::Command::AddUIImageComponent(m_checkmarkEntity, checkImg);

		NE::ECS::Command::SetParent(m_checkmarkEntity, m_backgroundEntity, -1, false);

		// Create Label entity
		m_labelEntity = NE::ECS::Command::CreateEntityNoComponents();

		NE::ECS::Command::AddEntityMetaComponent(m_labelEntity,
			NE::ECS::Component::EntityMeta{ .name = "Label", .luid = NE::Core::LUIDGenerator::Generate("em") });

		NE::ECS::Command::AddHierarchyComponent(m_labelEntity, NE::ECS::Component::Hierarchy{});

		// Setup UIRectTransform for label
		NE::ECS::Component::UIRectTransform labelRect{};
		labelRect.width = 130.0f;
		labelRect.height = 20.0f;
		labelRect.x = 25.0f;  // Offset to the right of the checkbox
		labelRect.y = 0.0f;
		labelRect.anchorMinX = 0.0f; labelRect.anchorMinY = 0.5f;
		labelRect.anchorMaxX = 0.0f; labelRect.anchorMaxY = 0.5f;
		labelRect.pivotX = 0.0f; labelRect.pivotY = 0.5f;
		// NOTE: Parent relationship now managed by Hierarchy component via SetParent call below
		NE::ECS::Command::AddUIRectTransformComponent(m_labelEntity, labelRect);

		// Setup UIText for label
		NE::ECS::Component::UIText text{};
		text.text = "Toggle";
		text.fontSize = 14.0f;
		text.color = NE::Math::Vec4(1.0f, 1.0f, 1.0f, 1.0f);
		text.horizontalAlign = NE::ECS::Component::UIText::Alignment::LEFT;
		text.verticalAlign = NE::ECS::Component::UIText::VerticalAlignment::MIDDLE;
		NE::ECS::Command::AddUITextComponent(m_labelEntity, text);

		NE::ECS::Command::SetParent(m_labelEntity, m_entity, -1, false);

		// Setup UIToggle component on root
		NE::ECS::Component::UIToggle toggle{};
		toggle.luid = NE::Core::LUIDGenerator::Generate("tg");
		toggle.isOn = true;
		toggle.graphic = m_checkmarkEntity;
		toggle.background = m_backgroundEntity;
		NE::ECS::Command::AddUIToggleComponent(m_entity, toggle);

		EditorScene::s_selection.SetSingle(m_entity);
	}

	void CreateUIToggleCommand::Undo() {
		NE::ECS::Command::DestroyEntity(m_labelEntity);
		NE::ECS::Command::DestroyEntity(m_checkmarkEntity);
		NE::ECS::Command::DestroyEntity(m_backgroundEntity);
		NE::ECS::Command::DestroyEntity(m_entity);
		if (m_createdCanvas && m_canvasEntity != NE::ECS::NO_ENTITY) {
			EditorScene::UnregisterRoot(m_canvasEntity);
			NE::ECS::Command::DestroyEntity(m_canvasEntity);
		}
	}

	DeleteEntityCommand::DeleteEntityCommand(std::vector<uint32_t> rootEntitiesToDelete)
		: m_initialRootEntities(std::move(rootEntitiesToDelete)) {}

	void DeleteEntityCommand::Execute() {
		constexpr int kAppendIndex = std::numeric_limits<int>::max();

		auto isEntityAlive = [](uint32_t entity) {
			if (entity == NE::ECS::NO_ENTITY)
				return false;

			const auto& usedEntities = NE::GetNumEntities();
			return std::find(usedEntities.begin(), usedEntities.end(), entity) != usedEntities.end();
		};

		// First Execute() captures deletion snapshots. Later Execute() calls are redo.
		if (m_snapshots.empty()) {
			m_snapshots.reserve(m_initialRootEntities.size());

			for (uint32_t rootEntity : m_initialRootEntities) {
				if (!isEntityAlive(rootEntity))
					continue;

				const auto& hierarchy = NE::ECS::Query::GetEntityHierarchy(rootEntity);

				DeletedRootSnapshot snapshot{};
				snapshot.wasRoot = hierarchy.parent == NE::ECS::Component::INVALID_PARENT;
				snapshot.oldParent = snapshot.wasRoot
					? NE::ECS::NO_ENTITY
					: static_cast<uint32_t>(hierarchy.parent);

				uint32_t oldIndex = EditorScene::GetIndexInParentOrRoot(rootEntity);
				snapshot.oldIndex = (oldIndex == NE::ECS::NO_ENTITY)
					? kAppendIndex
					: static_cast<int>(oldIndex);

				snapshot.blob = NE::CopyEntity(rootEntity);
				if (snapshot.blob.empty())
					continue;

				snapshot.liveEntityId = rootEntity;
				m_snapshots.push_back(std::move(snapshot));
			}
		}

		for (auto& snapshot : m_snapshots) {
			if (!isEntityAlive(snapshot.liveEntityId))
				continue;

			NE::ECS::Command::DestroyEntity(snapshot.liveEntityId);
			EditorScene::UnregisterRoot(snapshot.liveEntityId);
			snapshot.liveEntityId = NE::ECS::NO_ENTITY;
		}

		EditorScene::s_selection.Clear();
	}

	void DeleteEntityCommand::Undo() {
		if (m_snapshots.empty())
			return;

		auto isEntityAlive = [](uint32_t entity) {
			if (entity == NE::ECS::NO_ENTITY)
				return false;

			const auto& usedEntities = NE::GetNumEntities();
			return std::find(usedEntities.begin(), usedEntities.end(), entity) != usedEntities.end();
		};

		auto restoreSnapshot = [&](DeletedRootSnapshot& snapshot) {
			uint32_t recreatedEntity = NE::PasteEntity(snapshot.blob);
			if (recreatedEntity == NE::ECS::NO_ENTITY) {
				snapshot.liveEntityId = NE::ECS::NO_ENTITY;
				return;
			}

			const bool canRestoreToOriginalParent =
				!snapshot.wasRoot &&
				snapshot.oldParent != NE::ECS::NO_ENTITY &&
				isEntityAlive(snapshot.oldParent);

			if (canRestoreToOriginalParent) {
				EditorScene::SetParent(recreatedEntity, snapshot.oldParent, snapshot.oldIndex, true);
			} else {
				EditorScene::RegisterRoot(recreatedEntity);
				EditorScene::ReorderRoot(recreatedEntity, snapshot.oldIndex);
			}

			snapshot.liveEntityId = recreatedEntity;
		};

		std::vector<DeletedRootSnapshot*> nonRootSnapshots;
		std::vector<DeletedRootSnapshot*> rootSnapshots;
		nonRootSnapshots.reserve(m_snapshots.size());
		rootSnapshots.reserve(m_snapshots.size());

		for (auto& snapshot : m_snapshots) {
			if (snapshot.wasRoot) {
				rootSnapshots.push_back(&snapshot);
			} else {
				nonRootSnapshots.push_back(&snapshot);
			}
		}

		std::sort(nonRootSnapshots.begin(), nonRootSnapshots.end(),
			[](const DeletedRootSnapshot* lhs, const DeletedRootSnapshot* rhs) {
				if (lhs->oldParent != rhs->oldParent)
					return lhs->oldParent < rhs->oldParent;
				return lhs->oldIndex < rhs->oldIndex;
			});

		std::sort(rootSnapshots.begin(), rootSnapshots.end(),
			[](const DeletedRootSnapshot* lhs, const DeletedRootSnapshot* rhs) {
				return lhs->oldIndex < rhs->oldIndex;
			});

		for (DeletedRootSnapshot* snapshot : nonRootSnapshots)
			restoreSnapshot(*snapshot);

		for (DeletedRootSnapshot* snapshot : rootSnapshots)
			restoreSnapshot(*snapshot);

		std::vector<uint32_t> restoredSelection;
		restoredSelection.reserve(m_snapshots.size());
		for (const auto& snapshot : m_snapshots) {
			if (snapshot.liveEntityId != NE::ECS::NO_ENTITY)
				restoredSelection.push_back(snapshot.liveEntityId);
		}

		if (restoredSelection.empty()) {
			EditorScene::s_selection.Clear();
			return;
		}

		EditorScene::s_selection.SetSingle(restoredSelection.front());
		for (size_t i = 1; i < restoredSelection.size(); ++i)
			EditorScene::s_selection.Add(restoredSelection[i]);
	}

	CreateCubeEntityCommand::CreateCubeEntityCommand(uint32_t parentEntity) 
		: m_entity(0), m_parentEntity(parentEntity) {}

	void CreateCubeEntityCommand::Execute() {
		m_entity = NE::ECS::Command::CreateCubeEntity(m_parentEntity);
		if (m_parentEntity == NE::ECS::NO_ENTITY)
			EditorScene::s_rootOrder.push_back(m_entity);
	}

	void CreateCubeEntityCommand::Undo() {
		EditorScene::UnregisterRoot(m_entity);
		NE::ECS::Command::DestroyEntity(m_entity);
	}

	CreateSphereEntityCommand::CreateSphereEntityCommand(uint32_t parentEntity) 
		: m_entity(0), m_parentEntity(parentEntity) {}

	void CreateSphereEntityCommand::Execute() {
		m_entity = NE::ECS::Command::CreateSphereEntity(m_parentEntity);
		if (m_parentEntity == NE::ECS::NO_ENTITY)
			EditorScene::s_rootOrder.push_back(m_entity);
	}

	void CreateSphereEntityCommand::Undo() {
		EditorScene::UnregisterRoot(m_entity);
		NE::ECS::Command::DestroyEntity(m_entity);
	}

	CreateCylinderEntityCommand::CreateCylinderEntityCommand(uint32_t parentEntity) 
		: m_entity(0), m_parentEntity(parentEntity) {}

	void CreateCylinderEntityCommand::Execute() {
		m_entity = NE::ECS::Command::CreateCylinderEntity(m_parentEntity);
		if (m_parentEntity == NE::ECS::NO_ENTITY)
			EditorScene::s_rootOrder.push_back(m_entity);
	}

	void CreateCylinderEntityCommand::Undo() {
		EditorScene::UnregisterRoot(m_entity);
		NE::ECS::Command::DestroyEntity(m_entity);
	}

	CreateCapsuleEntityCommand::CreateCapsuleEntityCommand(uint32_t parentEntity) 
		: m_entity(0), m_parentEntity(parentEntity) {}

	void CreateCapsuleEntityCommand::Execute() {
		m_entity = NE::ECS::Command::CreateCapsuleEntity(m_parentEntity);
		if (m_parentEntity == NE::ECS::NO_ENTITY)
			EditorScene::s_rootOrder.push_back(m_entity);
	}

	void CreateCapsuleEntityCommand::Undo() {
		EditorScene::UnregisterRoot(m_entity);
		NE::ECS::Command::DestroyEntity(m_entity);
	}

	CreatePlaneEntityCommand::CreatePlaneEntityCommand(uint32_t parentEntity) 
		: m_entity(0), m_parentEntity(parentEntity) {}

	void CreatePlaneEntityCommand::Execute() {
		m_entity = NE::ECS::Command::CreatePlaneEntity(m_parentEntity);
		if (m_parentEntity == NE::ECS::NO_ENTITY)
			EditorScene::s_rootOrder.push_back(m_entity);
	}

	void CreatePlaneEntityCommand::Undo() {
		EditorScene::UnregisterRoot(m_entity);
		NE::ECS::Command::DestroyEntity(m_entity);
	}

	SetEntityLayerCommand::SetEntityLayerCommand(
		uint32_t entity, uint8_t before, uint8_t after)
		: m_entity(entity), m_before(before), m_after(after) {
	}

	void SetEntityLayerCommand::Execute() {
		NE::ECS::Command::SetLayer(m_entity, static_cast<NE::Core::LayerID>(m_after));
	}

	void SetEntityLayerCommand::Undo() {
		NE::ECS::Command::SetLayer(m_entity, static_cast<NE::Core::LayerID>(m_before));
	}

	CreateDirectionalLightCommand::CreateDirectionalLightCommand(uint32_t parentEntity)
		: m_entity(0), m_parentEntity(parentEntity) {
	}

	void CreateDirectionalLightCommand::Execute() {
		m_entity = NE::ECS::Command::CreateDirectionalLightEntity(m_parentEntity);
		if (m_parentEntity == NE::ECS::NO_ENTITY)
			EditorScene::s_rootOrder.push_back(m_entity);
	}

	void CreateDirectionalLightCommand::Undo() {
		EditorScene::UnregisterRoot(m_entity);
		NE::ECS::Command::DestroyEntity(m_entity);
	}

	CreatePointLightCommand::CreatePointLightCommand(uint32_t parentEntity)
		: m_entity(0), m_parentEntity(parentEntity) {
	}

	void CreatePointLightCommand::Execute() {
		m_entity = NE::ECS::Command::CreatePointLightEntity(m_parentEntity);
		if (m_parentEntity == NE::ECS::NO_ENTITY)
			EditorScene::s_rootOrder.push_back(m_entity);
	}

	void CreatePointLightCommand::Undo() {
		EditorScene::UnregisterRoot(m_entity);
		NE::ECS::Command::DestroyEntity(m_entity);
	}

	CreateSpotLightCommand::CreateSpotLightCommand(uint32_t parentEntity)
		: m_entity(0), m_parentEntity(parentEntity) {
	}

	void CreateSpotLightCommand::Execute() {
		m_entity = NE::ECS::Command::CreateSpotLightEntity(m_parentEntity);
		if (m_parentEntity == NE::ECS::NO_ENTITY)
			EditorScene::s_rootOrder.push_back(m_entity);
	}


	void CreateSpotLightCommand::Undo() {
		EditorScene::UnregisterRoot(m_entity);
		NE::ECS::Command::DestroyEntity(m_entity);
	}

	CreateQuadEntityCommand::CreateQuadEntityCommand(uint32_t parentEntity)
		: m_entity(0), m_parentEntity(parentEntity) {
	}

	void CreateQuadEntityCommand::Execute() {
		m_entity = NE::ECS::Command::CreateQuadEntity(m_parentEntity);
		if (m_parentEntity == NE::ECS::NO_ENTITY)
			EditorScene::s_rootOrder.push_back(m_entity);
	}

	void CreateQuadEntityCommand::Undo() {
		EditorScene::UnregisterRoot(m_entity);
		NE::ECS::Command::DestroyEntity(m_entity);
	}

	HierarchyChangeCommand::HierarchyChangeCommand(uint32_t child, uint32_t newParent, int newInsertIndex) 
		: childEntity(child), newParentEntity(newParent), newInsertIndex(newInsertIndex) 
	{
		oldParentEntity = EditorScene::GetParent(childEntity);
		oldInsertIndex = EditorScene::GetIndexInParentOrRoot(childEntity);
	}

	void HierarchyChangeCommand::Execute() {
		EditorScene::SetParent(childEntity, newParentEntity, newInsertIndex, true);
	}

	void HierarchyChangeCommand::Undo() {
		EditorScene::SetParent(childEntity, oldParentEntity, oldInsertIndex, true);
	}

}
