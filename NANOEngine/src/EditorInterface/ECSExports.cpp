#include "pch.h"
#include "ECSExports.hpp"

#include "../ECS/Components/EntityMeta.hpp"
#include "../ECS/Components/Transform.hpp"
#include "../ECS/Components/Renderer.hpp"
#include "../ECS/Components/LightmapBinding.hpp"
#include "../ECS/Components/Light.hpp"
#include "../ECS/Components/Rigidbody.hpp"
#include "../ECS/Components/Collider.hpp"
#include "../ECS/Components/AudioSource.hpp"
#include "../ECS/Components/NativeScript.hpp"
#include "../ECS/Components/UIRectTransform.hpp"
#include "../ECS/Components/UIImage.hpp"
#include "../ECS/Components/UICanvas.hpp"
#include "../ECS/Components/UIText.hpp"
#include "../ECS/Components/UIButton.hpp"
#include "../ECS/Components/UISlider.hpp"
#include "../ECS/Components/UIToggle.hpp"
#include "../ECS/Components/PrefabLink.hpp"
#include "../ECS/Components/PrefabInstance.hpp"
#include "../ECS/Components/CharacterController.hpp"
#include "../ECS/Components/DecalProjector.hpp"
#include "../ECS/Components/UILayoutGroup.hpp"
#include "../ECS/Components/UIGridLayoutGroup.hpp"
#include "../ECS/Components/UILayoutElement.hpp"
#include "../ECS/Components/UIScrollRect.hpp"
#include "../ECS/Components/UIAutoSize.hpp"
#include "../ECS/Components/UIInputField.hpp"
#include "../ECS/Components/UIDropdown.hpp"
#include "../ECS/Components/Camera.hpp"
#include "../ECS/Systems/ScriptSystem.hpp"
#include "../ECS/Systems/UIRenderSystem.hpp"
#include "../ECS/Systems/UIEventSystem.hpp"
#include "../SceneManagement/Scene.hpp"
#include "../ECS/Components/Animator.hpp"
#include "Scripting/ScriptingEngine.hpp"
#include "Core/LUIDGenerator.hpp"
#include "ECS/Systems/TransformSystem.hpp"

#include "ECS/Components/Hierarchy.hpp"
#include "ECS/Systems/HierarchySystem.hpp"
#include "ResourceManagement/ResourceManager.hpp"
#include "Graphics/OpenGL/GLTexture.hpp"
#include "Graphics/Core/Material.hpp"

namespace NE {
	//SceneManagement::Scene& GetScene();
}

namespace NE::ECS {
	namespace Query {
		uint64_t GetEntitySignature(uint32_t e) {
			return GetScene().GetECSCoordinator().GetSignature(e).to_ullong();
		}

		const Component::EntityMeta& GetEntityMeta(uint32_t e) {
			return NE::GetScene().GetECSCoordinator().GetComponent<NE::ECS::Component::EntityMeta>(e);
		}

		const Component::Transform& GetEntityTransform(uint32_t e) {
			return NE::GetScene().GetECSCoordinator().GetComponent<NE::ECS::Component::Transform>(e);
		}

		const Component::Renderer& GetEntityRenderer(uint32_t e) {
			return NE::GetScene().GetECSCoordinator().GetComponent<NE::ECS::Component::Renderer>(e);
		}

		const Component::LightmapBinding& GetLightmapBinding(uint32_t e) {
			return NE::GetScene().GetECSCoordinator().GetComponent<NE::ECS::Component::LightmapBinding>(e);
		}

		const Component::Light& GetEntityLight(uint32_t e) {
			return NE::GetScene().GetECSCoordinator().GetComponent<NE::ECS::Component::Light>(e);
		}

		const Component::Rigidbody& GetEntityRigidbody(uint32_t e) {
			return NE::GetScene().GetECSCoordinator().GetComponent<NE::ECS::Component::Rigidbody>(e);
		}

		const Component::Collider& GetEntityCollider(uint32_t e) {
			return NE::GetScene().GetECSCoordinator().GetComponent<NE::ECS::Component::Collider>(e);
		}

		const Component::AudioSource& GetEntityAudioSource(uint32_t e) {
			return NE::GetScene().GetECSCoordinator().GetComponent<NE::ECS::Component::AudioSource>(e);
		}

		const Component::NativeScript& GetEntityScript(uint32_t e) {
			return NE::GetScene().GetECSCoordinator().GetComponent<NE::ECS::Component::NativeScript>(e);
		}

		const Component::UIRectTransform& GetUIRectTransform(uint32_t e) {
			return NE::GetScene().GetECSCoordinator().GetComponent<NE::ECS::Component::UIRectTransform>(e);
		}

		const Component::UIImage& GetUIImage(uint32_t e) {
			return NE::GetScene().GetECSCoordinator().GetComponent<NE::ECS::Component::UIImage>(e);
		}

		const Component::UICanvas& GetUICanvas(uint32_t e) {
			return NE::GetScene().GetECSCoordinator().GetComponent<NE::ECS::Component::UICanvas>(e);
		}

		const Component::UIText& GetUIText(uint32_t e) {
			return NE::GetScene().GetECSCoordinator().GetComponent<NE::ECS::Component::UIText>(e);
		}

		const Component::UIButton& GetUIButton(uint32_t e) {
			return NE::GetScene().GetECSCoordinator().GetComponent<NE::ECS::Component::UIButton>(e);
		}

		const Component::UISlider& GetUISlider(uint32_t e) {
			return NE::GetScene().GetECSCoordinator().GetComponent<NE::ECS::Component::UISlider>(e);
		}

		const Component::UIToggle& GetUIToggle(uint32_t e) {
			return NE::GetScene().GetECSCoordinator().GetComponent<NE::ECS::Component::UIToggle>(e);
		}

		const Component::UILayoutGroup& GetUILayoutGroup(uint32_t e) {
			return NE::GetScene().GetECSCoordinator().GetComponent<NE::ECS::Component::UILayoutGroup>(e);
		}

		const Component::UIGridLayoutGroup& GetUIGridLayoutGroup(uint32_t e) {
			return NE::GetScene().GetECSCoordinator().GetComponent<NE::ECS::Component::UIGridLayoutGroup>(e);
		}

		const Component::UILayoutElement& GetUILayoutElement(uint32_t e) {
			return NE::GetScene().GetECSCoordinator().GetComponent<NE::ECS::Component::UILayoutElement>(e);
		}

		const Component::UIScrollRect& GetUIScrollRect(uint32_t e) {
			return NE::GetScene().GetECSCoordinator().GetComponent<NE::ECS::Component::UIScrollRect>(e);
		}

		const Component::UIAutoSize& GetUIAutoSize(uint32_t e) {
			return NE::GetScene().GetECSCoordinator().GetComponent<NE::ECS::Component::UIAutoSize>(e);
		}

		const Component::UIInputField& GetUIInputField(uint32_t e) {
			return NE::GetScene().GetECSCoordinator().GetComponent<NE::ECS::Component::UIInputField>(e);
		}

		const Component::UIDropdown& GetUIDropdown(uint32_t e) {
			return NE::GetScene().GetECSCoordinator().GetComponent<NE::ECS::Component::UIDropdown>(e);
		}

		bool HasEntityMeta(uint32_t e) {
			return NE::GetScene().GetECSCoordinator().HasComponent<NE::ECS::Component::EntityMeta>(e);
		}

		bool HasHierarchy(uint32_t e) {
			return NE::GetScene().GetECSCoordinator().HasComponent<NE::ECS::Component::Hierarchy>(e);
		}

		bool HasTransform(uint32_t e) {
			return NE::GetScene().GetECSCoordinator().HasComponent<NE::ECS::Component::Transform>(e);
		}

		bool HasUIRectTransform(uint32_t e) {
			return NE::GetScene().GetECSCoordinator().HasComponent<NE::ECS::Component::UIRectTransform>(e);
		}

		bool HasUICanvas(uint32_t e) {
			return NE::GetScene().GetECSCoordinator().HasComponent<NE::ECS::Component::UICanvas>(e);
		}

		bool HasUIImage(uint32_t e) {
			return NE::GetScene().GetECSCoordinator().HasComponent<NE::ECS::Component::UIImage>(e);
		}

		bool HasUIText(uint32_t e) {
			return NE::GetScene().GetECSCoordinator().HasComponent<NE::ECS::Component::UIText>(e);
		}

		bool HasUIButton(uint32_t e) {
			return NE::GetScene().GetECSCoordinator().HasComponent<NE::ECS::Component::UIButton>(e);
		}

		bool HasUISlider(uint32_t e) {
			return NE::GetScene().GetECSCoordinator().HasComponent<NE::ECS::Component::UISlider>(e);
		}

		bool HasUIToggle(uint32_t e) {
			return NE::GetScene().GetECSCoordinator().HasComponent<NE::ECS::Component::UIToggle>(e);
		}

		bool HasPrefabLink(uint32_t e) {
			return NE::GetScene().GetECSCoordinator().HasComponent<NE::ECS::Component::PrefabLink>(e);
		}

		bool HasPrefabInstance(uint32_t e) {
			return NE::GetScene().GetECSCoordinator().HasComponent<NE::ECS::Component::PrefabInstance>(e);
		}

		bool HasRenderer(uint32_t e) {
			return GetScene().GetECSCoordinator().HasComponent<ECS::Component::Renderer>(e);
		}

		bool HasLightmapBinding(uint32_t e) {
			return GetScene().GetECSCoordinator().HasComponent<ECS::Component::LightmapBinding>(e);
		}

		bool HasLight(uint32_t e) {
			return GetScene().GetECSCoordinator().HasComponent<ECS::Component::Light>(e);
		}

		bool HasRigidbody(uint32_t e) {
			return GetScene().GetECSCoordinator().HasComponent<ECS::Component::Rigidbody>(e);
		}

		bool HasCollider(uint32_t e) {
			return GetScene().GetECSCoordinator().HasComponent<ECS::Component::Collider>(e);
		}

		bool HasAudioSource(uint32_t e) {
			return GetScene().GetECSCoordinator().HasComponent<ECS::Component::AudioSource>(e);
		}

		bool HasScript(uint32_t e) {
			return GetScene().GetECSCoordinator().HasComponent<ECS::Component::NativeScript>(e);
		}

		bool HasAnimator(uint32_t e) {
			return GetScene().GetECSCoordinator().HasComponent<ECS::Component::Animator>(e);
		}

		bool HasCamera(uint32_t e) {
			return GetScene().GetECSCoordinator().HasComponent<ECS::Component::Camera>(e);
		}

		bool HasCharacterController(uint32_t e) {
			return GetScene().GetECSCoordinator().HasComponent<ECS::Component::CharacterController>(e);
		}

        bool HasDecalProjector(uint32_t e) {
            return GetScene().GetECSCoordinator().HasComponent<ECS::Component::DecalProjector>(e);
        }

		bool HasUILayoutGroup(uint32_t e) {
			return NE::GetScene().GetECSCoordinator().HasComponent<NE::ECS::Component::UILayoutGroup>(e);
		}

		bool HasUIGridLayoutGroup(uint32_t e) {
			return NE::GetScene().GetECSCoordinator().HasComponent<NE::ECS::Component::UIGridLayoutGroup>(e);
		}

		bool HasUILayoutElement(uint32_t e) {
			return NE::GetScene().GetECSCoordinator().HasComponent<NE::ECS::Component::UILayoutElement>(e);
		}

		bool HasUIScrollRect(uint32_t e) {
			return NE::GetScene().GetECSCoordinator().HasComponent<NE::ECS::Component::UIScrollRect>(e);
		}

		bool HasUIAutoSize(uint32_t e) {
			return NE::GetScene().GetECSCoordinator().HasComponent<NE::ECS::Component::UIAutoSize>(e);
		}

		bool HasUIInputField(uint32_t e) {
			return NE::GetScene().GetECSCoordinator().HasComponent<NE::ECS::Component::UIInputField>(e);
		}

		bool HasUIDropdown(uint32_t e) {
			return NE::GetScene().GetECSCoordinator().HasComponent<NE::ECS::Component::UIDropdown>(e);
		}

		const Component::Animator& GetEntityAnimator(uint32_t e) {
			return NE::GetScene().GetECSCoordinator().GetComponent<NE::ECS::Component::Animator>(e);
		}

		const Component::Camera& GetEntityCamera(uint32_t e) {
			return NE::GetScene().GetECSCoordinator().GetComponent<NE::ECS::Component::Camera>(e);
		}

		const Component::PrefabLink& GetPrefabLink(uint32_t e) {
			return NE::GetScene().GetECSCoordinator().GetComponent<NE::ECS::Component::PrefabLink>(e);
		}

		const Component::PrefabInstance& GetPrefabInstance(uint32_t e) {
			return NE::GetScene().GetECSCoordinator().GetComponent<NE::ECS::Component::PrefabInstance>(e);
		}

		const Component::CharacterController& GetCharacterController(uint32_t e) {
			return NE::GetScene().GetECSCoordinator().GetComponent<NE::ECS::Component::CharacterController>(e);
		}

        const Component::DecalProjector& GetDecalProjector(uint32_t e) {
            return NE::GetScene().GetECSCoordinator().GetComponent<NE::ECS::Component::DecalProjector>(e);
        }

		const Component::Hierarchy& GetEntityHierarchy(uint32_t e) {
			return NE::GetScene().GetECSCoordinator().GetComponent<NE::ECS::Component::Hierarchy>(e);
		}

		ComponentType GetEntityMetaComponentType() {
			return GetScene().GetECSCoordinator().GetComponentType<Component::EntityMeta>();
		}

		ComponentType GetTransformComponentType() {
			return GetScene().GetECSCoordinator().GetComponentType<Component::Transform>();
		}

		ComponentType GetRendererComponentType() {
			return GetScene().GetECSCoordinator().GetComponentType<Component::Renderer>();
		}

		ComponentType GetLightmapBindingComponentType() {
			return GetScene().GetECSCoordinator().GetComponentType<Component::LightmapBinding>();
		}

		ComponentType GetLightComponentType() {
			return GetScene().GetECSCoordinator().GetComponentType<Component::Light>();
		}

		ComponentType GetRigidbodyComponentType() {
			return GetScene().GetECSCoordinator().GetComponentType<Component::Rigidbody>();
		}

		ComponentType GetColliderComponentType() {
			return GetScene().GetECSCoordinator().GetComponentType<Component::Collider>();
		}

		ComponentType GetAudioSourceComponentType() {
			return GetScene().GetECSCoordinator().GetComponentType<Component::AudioSource>();
		}

		ComponentType GetScriptComponentType() {
			return GetScene().GetECSCoordinator().GetComponentType<Component::NativeScript>();
		}

		ComponentType GetUIRectTransformComponentType() {
			return GetScene().GetECSCoordinator().GetComponentType<Component::UIRectTransform>();
		}

		ComponentType GetUIImageComponentType() {
			return GetScene().GetECSCoordinator().GetComponentType<Component::UIImage>();
		}

		ComponentType GetUICanvasComponentType() {
			return GetScene().GetECSCoordinator().GetComponentType<Component::UICanvas>();
		}

		ComponentType GetUITextComponentType() {
			return GetScene().GetECSCoordinator().GetComponentType<Component::UIText>();
		}

		ComponentType GetUIButtonComponentType() {
			return GetScene().GetECSCoordinator().GetComponentType<Component::UIButton>();
		}

		ComponentType GetUISliderComponentType() {
			return GetScene().GetECSCoordinator().GetComponentType<Component::UISlider>();
		}

		ComponentType GetUIToggleComponentType() {
			return GetScene().GetECSCoordinator().GetComponentType<Component::UIToggle>();
		}

		ComponentType GetEntityAnimatorComponentType() {
			return GetScene().GetECSCoordinator().GetComponentType<Component::Animator>();
		}

		ComponentType GetEntityCameraComponentType() {
			return GetScene().GetECSCoordinator().GetComponentType<Component::Camera>();
		}

		ComponentType GetPrefabInstanceComponentType() {
			return GetScene().GetECSCoordinator().GetComponentType<Component::PrefabInstance>();
		}

		ComponentType GetCharacterControllerComponentType() {
			return GetScene().GetECSCoordinator().GetComponentType<Component::CharacterController>();
		}

        ComponentType GetDecalProjectorComponentType() {
            return GetScene().GetECSCoordinator().GetComponentType<Component::DecalProjector>();
        }

		ComponentType GetUILayoutGroupComponentType() {
			return GetScene().GetECSCoordinator().GetComponentType<Component::UILayoutGroup>();
		}

		ComponentType GetUIGridLayoutGroupComponentType() {
			return GetScene().GetECSCoordinator().GetComponentType<Component::UIGridLayoutGroup>();
		}

		ComponentType GetUILayoutElementComponentType() {
			return GetScene().GetECSCoordinator().GetComponentType<Component::UILayoutElement>();
		}

		ComponentType GetUIScrollRectComponentType() {
			return GetScene().GetECSCoordinator().GetComponentType<Component::UIScrollRect>();
		}

		ComponentType GetUIAutoSizeComponentType() {
			return GetScene().GetECSCoordinator().GetComponentType<Component::UIAutoSize>();
		}

		ComponentType GetUIInputFieldComponentType() {
			return GetScene().GetECSCoordinator().GetComponentType<Component::UIInputField>();
		}

		ComponentType GetUIDropdownComponentType() {
			return GetScene().GetECSCoordinator().GetComponentType<Component::UIDropdown>();
		}

		uint32_t GetParent(uint32_t child) {
			auto& ecs = NE::GetScene().GetECSCoordinator();

			// All entities use Hierarchy component for parent-child relationships
			return ecs.GetComponent<NE::ECS::Component::Hierarchy>(child).parent;
		}

		const Core::LayerID GetLayer(Entity e) {
			return GetScene().GetECSCoordinator().GetEntityManager().GetLayer(e);
		}

		const Core::LayerMask GetLayerBit(Entity e) {
			return GetScene().GetECSCoordinator().GetEntityManager().GetLayerBit(e);
		}

		Entity ResolveComponentLuidToEntity(uint64_t luid) {
			if (luid == 0) return 0; //Invalid entity

			auto& luidRegistry = GetScene().GetECSCoordinator().GetLUIDRegistry();
			const Core::LuidRecord* record = luidRegistry.Find(luid);
			if (!record) return 0; //Invalid Entity

			return static_cast<Entity>(record->m_entityOwner);
		}

		Entity ResolveEntityMetaLuidToEntity(uint64_t luid) {
			if (luid == 0) return 0;

			auto& ecs = GetScene().GetECSCoordinator();
			auto& entityManager = ecs.GetEntityManager();
			auto& componentManager = ecs.GetComponentManager();

			// Iterate through all active entities to find matching EntityMeta LUID
			const auto& usedEntities = entityManager.GetUsedEntities();
			for (Entity entity : usedEntities) {
				if (componentManager.HasComponent<ECS::Component::EntityMeta>(entity)) {
					const auto& meta = componentManager.GetComponent<ECS::Component::EntityMeta>(entity);
					if (meta.luid == luid) {
						return entity;
					}
				}
			}

			return 0;
		}

		bool GetActive(Entity e) {
			return GetScene().GetECSCoordinator().GetEntityManager().GetActive(e);
		}
	}

	namespace Command {
		uint32_t CreateEntityNoComponents() {
			return GetScene().GetECSCoordinator().CreateEntity();
		}

		uint32_t CreateEmptyEntity(uint32_t parentEntt) {
			uint32_t newEntity = GetScene().GetECSCoordinator().CreateEntity();
			GetScene().GetECSCoordinator().AddComponent(
				newEntity,
				Component::EntityMeta{ .luid = Core::LUIDGenerator::Generate("em") }
			);

			GetScene().GetECSCoordinator().AddComponent(
				newEntity,
				Component::Transform{}
			);

			GetScene().GetECSCoordinator().AddComponent(
				newEntity,
				Component::Hierarchy{}
			);

			GetScene().GetECSCoordinator().m_hierarchySystem->SetParent(newEntity, parentEntt);

			return newEntity;
		}

		uint32_t CreateCubeEntity(uint32_t parentEntt) {
			uint32_t newEntity = GetScene().GetECSCoordinator().CreateEntity();
			GetScene().GetECSCoordinator().AddComponent(
				newEntity,
				Component::EntityMeta{ .name{"Cube"}, .luid = Core::LUIDGenerator::Generate("em") }
			);

			GetScene().GetECSCoordinator().AddComponent(
				newEntity,
				Component::Transform{}
			);

			GetScene().GetECSCoordinator().AddComponent(
				newEntity,
				Component::Hierarchy{}
			);

			GetScene().GetECSCoordinator().AddComponent(
				newEntity,
				Component::Renderer{
					.modelUUID{"builtin:model/cube"},
					.materialUUID{"neunlitmat"},
					.subMeshIndex = 0
				}
			);

			GetScene().GetECSCoordinator().AddComponent(
				newEntity,
				Component::Collider{
					.data = Component::Collider::BoxColliderData{},
					.type = Component::Collider::ColliderType::Box,
				}
			);

			GetScene().GetECSCoordinator().m_hierarchySystem->SetParent(newEntity, parentEntt);
			return newEntity;
		}

		uint32_t CreateSphereEntity(uint32_t parentEntt) {
			uint32_t newEntity = GetScene().GetECSCoordinator().CreateEntity();
			GetScene().GetECSCoordinator().AddComponent(
				newEntity,
				Component::EntityMeta{ .name{"Sphere"}, .luid = Core::LUIDGenerator::Generate("em") }
			);

			GetScene().GetECSCoordinator().AddComponent(
				newEntity,
				Component::Transform{}
			);

			GetScene().GetECSCoordinator().AddComponent(
				newEntity,
				Component::Hierarchy{}
			);

			GetScene().GetECSCoordinator().AddComponent(
				newEntity,
				Component::Renderer{
					.modelUUID{"builtin:model/sphere"},
					.materialUUID{"neunlitmat"},
					.subMeshIndex = 0
				}
			);

			GetScene().GetECSCoordinator().AddComponent(
				newEntity,
				Component::Collider{
					.data = Component::Collider::SphereColliderData{},
					.type = Component::Collider::ColliderType::Sphere,
				}
			);

			GetScene().GetECSCoordinator().m_hierarchySystem->SetParent(newEntity, parentEntt);
			return newEntity;
		}

		uint32_t CreateCylinderEntity(uint32_t parentEntt) {
			uint32_t newEntity = GetScene().GetECSCoordinator().CreateEntity();
			GetScene().GetECSCoordinator().AddComponent(
				newEntity,
				Component::EntityMeta{ .name{"Cylinder"}, .luid = Core::LUIDGenerator::Generate("em") }
			);

			GetScene().GetECSCoordinator().AddComponent(
				newEntity,
				Component::Transform{}
			);

			GetScene().GetECSCoordinator().AddComponent(
				newEntity,
				Component::Hierarchy{}
			);

			GetScene().GetECSCoordinator().AddComponent(
				newEntity,
				Component::Renderer{
					.modelUUID{"builtin:model/cylinder"},
					.materialUUID{"neunlitmat"},
					.subMeshIndex = 0
				}
			);

			GetScene().GetECSCoordinator().AddComponent(
				newEntity,
				Component::Collider{
					.data = Component::Collider::CylinderColliderData{},
					.type = Component::Collider::ColliderType::Cylinder,
				}
			);

			GetScene().GetECSCoordinator().m_hierarchySystem->SetParent(newEntity, parentEntt);
			return newEntity;
		}

		uint32_t CreateCapsuleEntity(uint32_t parentEntt) {
			uint32_t newEntity = GetScene().GetECSCoordinator().CreateEntity();
			GetScene().GetECSCoordinator().AddComponent(
				newEntity,
				Component::EntityMeta{ .name{"Capsule"}, .luid = Core::LUIDGenerator::Generate("em") }
			);

			GetScene().GetECSCoordinator().AddComponent(
				newEntity,
				Component::Transform{}
			);

			GetScene().GetECSCoordinator().AddComponent(
				newEntity,
				Component::Hierarchy{}
			);

			GetScene().GetECSCoordinator().AddComponent(
				newEntity,
				Component::Renderer{
					.modelUUID{"builtin:model/capsule"},
					.materialUUID{"neunlitmat"},
					.subMeshIndex = 0
				}
			);

			GetScene().GetECSCoordinator().AddComponent(
				newEntity,
				Component::Collider{
					.data = Component::Collider::CapsuleColliderData{},
					.type = Component::Collider::ColliderType::Capsule,
				}
			);

			GetScene().GetECSCoordinator().m_hierarchySystem->SetParent(newEntity, parentEntt);
			return newEntity;
		}

		uint32_t CreatePlaneEntity(uint32_t parentEntt) {
			uint32_t newEntity = GetScene().GetECSCoordinator().CreateEntity();
			GetScene().GetECSCoordinator().AddComponent(
				newEntity,
				Component::EntityMeta{ .name{"Plane"}, .luid = Core::LUIDGenerator::Generate("em") }
			);

			GetScene().GetECSCoordinator().AddComponent(
				newEntity,
				Component::Transform{}
			);

			GetScene().GetECSCoordinator().AddComponent(
				newEntity,
				Component::Hierarchy{}
			);

			GetScene().GetECSCoordinator().AddComponent(
				newEntity,
				Component::Renderer{
					.modelUUID{"builtin:model/plane"},
					.materialUUID{"neunlitmat"},
					.subMeshIndex = 0
				}
			);

			GetScene().GetECSCoordinator().m_hierarchySystem->SetParent(newEntity, parentEntt);
			return newEntity;
		}

		uint32_t CreateQuadEntity(uint32_t parentEntt) {
			uint32_t newEntity = GetScene().GetECSCoordinator().CreateEntity();
			GetScene().GetECSCoordinator().AddComponent(
				newEntity,
				Component::EntityMeta{ .name{"Quad"}, .luid = Core::LUIDGenerator::Generate("em") }
			);

			GetScene().GetECSCoordinator().AddComponent(
				newEntity,
				Component::Transform{}
			);

			GetScene().GetECSCoordinator().AddComponent(
				newEntity,
				Component::Hierarchy{}
			);

			GetScene().GetECSCoordinator().AddComponent(
				newEntity,
				Component::Renderer{
					.modelUUID{"builtin:model/quad"},
					.materialUUID{"neunlitmat"},
					.subMeshIndex = 0
				}
			);

			GetScene().GetECSCoordinator().m_hierarchySystem->SetParent(newEntity, parentEntt);
			return newEntity;
		}

		uint32_t CreateDirectionalLightEntity(uint32_t parentEntt) {
			uint32_t newEntity = GetScene().GetECSCoordinator().CreateEntity();
			GetScene().GetECSCoordinator().AddComponent(
				newEntity,
				Component::EntityMeta{ .name{"Directional Light"}, .luid = Core::LUIDGenerator::Generate("em") }
			);

			GetScene().GetECSCoordinator().AddComponent(
				newEntity,
				Component::Transform{}
			);

			GetScene().GetECSCoordinator().AddComponent(
				newEntity,
				Component::Hierarchy{}
			);

			GetScene().GetECSCoordinator().AddComponent(
				newEntity,
				Component::Light{
					.type = Component::Light::Type::Directional,
					.data = Component::Light::DirectionalLightData{}
				}
			);

			GetScene().GetECSCoordinator().m_hierarchySystem->SetParent(newEntity, parentEntt);
			return newEntity;
		}

		uint32_t CreatePointLightEntity(uint32_t parentEntt) {
			uint32_t newEntity = GetScene().GetECSCoordinator().CreateEntity();
			GetScene().GetECSCoordinator().AddComponent(
				newEntity,
				Component::EntityMeta{ .name{"Point Light"}, .luid = Core::LUIDGenerator::Generate("em") }
			);

			GetScene().GetECSCoordinator().AddComponent(
				newEntity,
				Component::Transform{}
			);

			GetScene().GetECSCoordinator().AddComponent(
				newEntity,
				Component::Hierarchy{}
			);

			GetScene().GetECSCoordinator().AddComponent(
				newEntity,
				Component::Light{
					.type = Component::Light::Type::Point,
					.data = Component::Light::PointLightData{}
				}
			);

			GetScene().GetECSCoordinator().m_hierarchySystem->SetParent(newEntity, parentEntt);
			return newEntity;
		}

		uint32_t CreateSpotLightEntity(uint32_t parentEntt) {
			uint32_t newEntity = GetScene().GetECSCoordinator().CreateEntity();
			GetScene().GetECSCoordinator().AddComponent(
				newEntity,
				Component::EntityMeta{ .name{"Spot Light"}, .luid = Core::LUIDGenerator::Generate("em") }
			);

			GetScene().GetECSCoordinator().AddComponent(
				newEntity,
				Component::Transform{}
			);

			GetScene().GetECSCoordinator().AddComponent(
				newEntity,
				Component::Hierarchy{}
			);

			GetScene().GetECSCoordinator().AddComponent(
				newEntity,
				Component::Light{
					.type = Component::Light::Type::Spot,
					.data = Component::Light::SpotLightData{}
				}
			);

			GetScene().GetECSCoordinator().m_hierarchySystem->SetParent(newEntity, parentEntt);
			return newEntity;
		}

		uint32_t CreateUICanvasEntity() {
			uint32_t newEntity = GetScene().GetECSCoordinator().CreateEntity();
			GetScene().GetECSCoordinator().AddComponent(
				newEntity,
				Component::EntityMeta{ .name = "Canvas", .luid = Core::LUIDGenerator::Generate("cv") });

			// Add Hierarchy component (required for parent-child relationships with UI children)
			Component::Hierarchy hierarchy;
			hierarchy.luid = Core::LUIDGenerator::Generate("cv");
			GetScene().GetECSCoordinator().AddComponent(newEntity, hierarchy);

			// set up canvas component
			Component::UICanvas canvas;
			canvas.luid = Core::LUIDGenerator::Generate("cv");
			GetScene().GetECSCoordinator().AddComponent(newEntity, canvas);

			// setup RectTransform for canvas (fullscreen by default)
			Component::UIRectTransform rectTransform;
			rectTransform.width = 1920.0f; // temp
			rectTransform.height = 1080.0f;
			rectTransform.x = 0.0f;
			rectTransform.y = 0.0f;
			rectTransform.z = 0.0f;
			GetScene().GetECSCoordinator().AddComponent(newEntity, rectTransform);

			return newEntity;
		}

		uint32_t CreateUIImageEntity(uint32_t parentCanvas) {
			auto& ecs = GetScene().GetECSCoordinator();

			uint32_t newEntity = ecs.CreateEntity();
			ecs.AddComponent(
				newEntity,
				Component::EntityMeta{ .name = "Image", .luid = Core::LUIDGenerator::Generate("im") });

			// Add Hierarchy component (required for parent-child relationships)
			Component::Hierarchy hierarchy;
			hierarchy.luid = Core::LUIDGenerator::Generate("im");
			ecs.AddComponent(newEntity, hierarchy);

			// setup RectTransform with parent linkage
			Component::UIRectTransform rect;
			rect.x = 0.0f;
			rect.y = 0.0f;
			rect.z = 0.0f;
			rect.width = 100.0f;
			rect.height = 100.0f;

			ecs.AddComponent(newEntity, rect);

			// Set parent via HierarchySystem (proper way)
			if (parentCanvas != NE::ECS::NO_ENTITY) {
				auto hierarchySystem = ecs.m_hierarchySystem;
				if (hierarchySystem) {
					hierarchySystem->SetParent(newEntity, parentCanvas);
				}
			}

			// setup UIImage with default white color
			Component::UIImage img;
			img.color = Math::Vec4{ 1.f, 1.f, 1.f, 1.f };
			img.material = nullptr;  // start with solid color
			ecs.AddComponent(newEntity, img);

			return newEntity;
		}

		// internal
		void DestroyRecursive(uint32_t e) {
			auto& hierarchy = GetScene().GetECSCoordinator().GetComponent<Component::Hierarchy>(e);

			for (auto child : hierarchy.children) {
				DestroyRecursive(child);
			}
			GetScene().GetECSCoordinator().DestroyEntity(e);
		}

		void DestroyEntity(uint32_t e) {
			auto& hierarchy = GetScene().GetECSCoordinator().GetComponent<Component::Hierarchy>(e);
			if (hierarchy.parent != Component::INVALID_PARENT) {
				// change this to purely deleting from parent vector
				GetScene().GetECSCoordinator().m_hierarchySystem->SetParent(e, Component::INVALID_PARENT);
			}

			DestroyRecursive(e);
		}

		void SetParent(Entity _child, Entity _newParent, int _insertIndex, bool _keepWorldPos) {
			GetScene().GetECSCoordinator().m_hierarchySystem->SetParent(_child, _newParent, _insertIndex, _keepWorldPos);
		}

		//void SetActive(Entity entity, bool isActive) {
		//	GetScene().GetECSCoordinator().m_hierarchySystem->SetActive(entity, isActive);
		//}

		void AddLightComponent(uint32_t e) {
			GetScene().GetECSCoordinator().AddComponent(e, ECS::Component::Light{});
		}

		void AddRendererComponent(uint32_t e) {
			GetScene().GetECSCoordinator().AddComponent(e, ECS::Component::Renderer{});
		}

		void AddLightmapBindingComponent(uint32_t e) {
			GetScene().GetECSCoordinator().AddComponent(e, ECS::Component::LightmapBinding{});
		}

		void AddRigidbodyComponent(uint32_t e) {
			GetScene().GetECSCoordinator().AddComponent(e, ECS::Component::Rigidbody{});
		}

		void AddColliderComponent(uint32_t e) {
			GetScene().GetECSCoordinator().AddComponent(e, ECS::Component::Collider{});
		}

		void AddAudioSourceComponent(uint32_t e) {
			GetScene().GetECSCoordinator().AddComponent(e, ECS::Component::AudioSource{});
		}

		void AddScriptComponent(uint32_t e) {
			GetScene().GetECSCoordinator().AddComponent(e, ECS::Component::NativeScript{});
		}

		void AddCameraComponent(uint32_t e) {
			GetScene().GetECSCoordinator().AddComponent(e, ECS::Component::Camera{});
		}

		void AddDecalProjectorComponent(uint32_t e) {
			GetScene().GetECSCoordinator().AddComponent(e, ECS::Component::DecalProjector{});
		}

		void AddEntityMetaComponent(uint32_t e, const Component::EntityMeta& c) {
			GetScene().GetECSCoordinator().AddComponent<Component::EntityMeta>(e, c);
		}

		void AddTransformComponent(uint32_t e, const Component::Transform& c) {
			GetScene().GetECSCoordinator().AddComponent<Component::Transform>(e, c);
		}

		void AddHierarchyComponent(uint32_t e, const Component::Hierarchy& c) {
			GetScene().GetECSCoordinator().AddComponent<Component::Hierarchy>(e, c);
		}

		void AddRendererComponent(uint32_t e, const Component::Renderer& c) {
			GetScene().GetECSCoordinator().AddComponent<Component::Renderer>(e, c);
		}

		void AddLightmapBindingComponent(uint32_t e, const Component::LightmapBinding& c) {
			GetScene().GetECSCoordinator().AddComponent<Component::LightmapBinding>(e, c);
		}

		void AddLightComponent(uint32_t e, const Component::Light& c) {
			GetScene().GetECSCoordinator().AddComponent<Component::Light>(e, c);
		}

		void AddRigidbodyComponent(uint32_t e, const Component::Rigidbody& c) {
			GetScene().GetECSCoordinator().AddComponent<Component::Rigidbody>(e, c);
		}

		void AddColliderComponent(uint32_t e, const Component::Collider& c) {
			GetScene().GetECSCoordinator().AddComponent<Component::Collider>(e, c);
		}

		void AddAudioSourceComponent(uint32_t e, const Component::AudioSource& c) {
			GetScene().GetECSCoordinator().AddComponent<Component::AudioSource>(e, c);
		}

		void AddScriptComponent(uint32_t e, const Component::NativeScript& c) {
			GetScene().GetECSCoordinator().AddComponent<Component::NativeScript>(e, c);
		}

		void AddCameraComponent(uint32_t e, const Component::Camera& c) {
			GetScene().GetECSCoordinator().AddComponent<Component::Camera>(e, c);
		}

		void AddAnimatorComponent(uint32_t e, const Component::Animator& c) {
			GetScene().GetECSCoordinator().AddComponent<Component::Animator>(e, c);
		}

		void AddUIRectTransformComponent(uint32_t e, const Component::UIRectTransform& c) {
			GetScene().GetECSCoordinator().AddComponent<Component::UIRectTransform>(e, c);
		}

		void AddUICanvasComponent(uint32_t e, const Component::UICanvas& c) {
			GetScene().GetECSCoordinator().AddComponent<Component::UICanvas>(e, c);
		}

		void AddUIImageComponent(uint32_t e, const Component::UIImage& c) {
			GetScene().GetECSCoordinator().AddComponent<Component::UIImage>(e, c);
		}

		void AddUITextComponent(uint32_t e, const Component::UIText& c) {
			auto comp = c;
			if (comp.luid == 0) comp.luid = Core::LUIDGenerator::Generate("tx");
			GetScene().GetECSCoordinator().AddComponent<Component::UIText>(e, comp);
		}

		void AddUIButtonComponent(uint32_t e, const Component::UIButton& c) {
			auto comp = c;
			if (comp.luid == 0) comp.luid = Core::LUIDGenerator::Generate("bt");
			GetScene().GetECSCoordinator().AddComponent<Component::UIButton>(e, comp);
		}

		void AddUISliderComponent(uint32_t e, const Component::UISlider& c) {
			GetScene().GetECSCoordinator().AddComponent<Component::UISlider>(e, c);
		}

		void AddUIToggleComponent(uint32_t e, const Component::UIToggle& c) {
			GetScene().GetECSCoordinator().AddComponent<Component::UIToggle>(e, c);
		}

		void AddUILayoutGroupComponent(uint32_t e, const Component::UILayoutGroup& c) {
			GetScene().GetECSCoordinator().AddComponent<Component::UILayoutGroup>(e, c);
		}

		void AddUIGridLayoutGroupComponent(uint32_t e, const Component::UIGridLayoutGroup& c) {
			GetScene().GetECSCoordinator().AddComponent<Component::UIGridLayoutGroup>(e, c);
		}

		void AddUILayoutElementComponent(uint32_t e, const Component::UILayoutElement& c) {
			GetScene().GetECSCoordinator().AddComponent<Component::UILayoutElement>(e, c);
		}

		void AddUIScrollRectComponent(uint32_t e, const Component::UIScrollRect& c) {
			GetScene().GetECSCoordinator().AddComponent<Component::UIScrollRect>(e, c);
		}

		void AddUIAutoSizeComponent(uint32_t e, const Component::UIAutoSize& c) {
			GetScene().GetECSCoordinator().AddComponent<Component::UIAutoSize>(e, c);
		}

		void AddUIInputFieldComponent(uint32_t e, const Component::UIInputField& c) {
			auto comp = c;
			if (comp.luid == 0) comp.luid = Core::LUIDGenerator::Generate("if");
			GetScene().GetECSCoordinator().AddComponent<Component::UIInputField>(e, comp);
		}

		void AddUIDropdownComponent(uint32_t e, const Component::UIDropdown& c) {
			auto comp = c;
			if (comp.luid == 0) comp.luid = Core::LUIDGenerator::Generate("dd");
			GetScene().GetECSCoordinator().AddComponent<Component::UIDropdown>(e, comp);
		}

		void AddPrefabLinkComponent(uint32_t e, const Component::PrefabLink& c) {
			GetScene().GetECSCoordinator().AddComponent<Component::PrefabLink>(e, c);
		}

		void AddPrefabInstanceComponent(uint32_t e, const Component::PrefabInstance& c) {
			GetScene().GetECSCoordinator().AddComponent<Component::PrefabInstance>(e, c);
		}

		void AddCharacterControllerComponent(uint32_t e, const Component::CharacterController& c) {
			GetScene().GetECSCoordinator().AddComponent<Component::CharacterController>(e, c);
		}

        void AddDecalProjectorComponent(uint32_t e, const Component::DecalProjector& c) {
            GetScene().GetECSCoordinator().AddComponent<Component::DecalProjector>(e, c);
        }

		void RemoveLightComponent(uint32_t e) {
			GetScene().GetECSCoordinator().RemoveComponent<Component::Light>(e);
		}

		void RemoveRendererComponent(uint32_t e) {
			GetScene().GetECSCoordinator().RemoveComponent<Component::Renderer>(e);
		}

		void RemoveAnimatorComponent(uint32_t e) {
			GetScene().GetECSCoordinator().RemoveComponent<Component::Animator>(e);
		}

		void RemoveRigidbodyComponent(uint32_t e) {
			GetScene().GetECSCoordinator().RemoveComponent<Component::Rigidbody>(e);
		}

		void RemoveColliderComponent(uint32_t e) {
			GetScene().GetECSCoordinator().RemoveComponent<Component::Collider>(e);
		}

		void RemoveAudioSourceComponent(uint32_t e) {
			GetScene().GetECSCoordinator().RemoveComponent<Component::AudioSource>(e);
		}

		void RemoveCameraComponent(uint32_t e) {
			GetScene().GetECSCoordinator().RemoveComponent<Component::Camera>(e);
		}

        void RemoveDecalProjectorComponent(uint32_t e) {
            GetScene().GetECSCoordinator().RemoveComponent<Component::DecalProjector>(e);
        }

		Component::EntityMeta& GetEntityMeta(uint32_t e) {
			return NE::GetScene().GetECSCoordinator().GetComponent<NE::ECS::Component::EntityMeta>(e);
		}

		Component::Transform& GetEntityTransform(uint32_t e) {
			return NE::GetScene().GetECSCoordinator().GetComponent<NE::ECS::Component::Transform>(e);
		}

		Component::Renderer& GetEntityRenderer(uint32_t e) {
			return NE::GetScene().GetECSCoordinator().GetComponent<NE::ECS::Component::Renderer>(e);
		}

		Component::LightmapBinding& GetLightmapBinding(uint32_t e) {
			return NE::GetScene().GetECSCoordinator().GetComponent<NE::ECS::Component::LightmapBinding>(e);
		}

		Component::Light& GetEntityLight(uint32_t e) {
			return NE::GetScene().GetECSCoordinator().GetComponent<NE::ECS::Component::Light>(e);
		}

		Component::Rigidbody& GetEntityRigidbody(uint32_t e) {
			return NE::GetScene().GetECSCoordinator().GetComponent<NE::ECS::Component::Rigidbody>(e);
		}

		Component::Collider& GetEntityCollider(uint32_t e) {
			return NE::GetScene().GetECSCoordinator().GetComponent<NE::ECS::Component::Collider>(e);
		}

		Component::AudioSource& GetEntityAudioSource(uint32_t e) {
			return NE::GetScene().GetECSCoordinator().GetComponent<NE::ECS::Component::AudioSource>(e);
		}

		Component::NativeScript& GetEntityScript(uint32_t e) {
			return NE::GetScene().GetECSCoordinator().GetComponent<NE::ECS::Component::NativeScript>(e);
		}

		Component::UIRectTransform& GetUIRectTransform(uint32_t e) {
			return NE::GetScene().GetECSCoordinator().GetComponent<NE::ECS::Component::UIRectTransform>(e);
		}

		Component::UIImage& GetUIImage(uint32_t e) {
			return NE::GetScene().GetECSCoordinator().GetComponent<NE::ECS::Component::UIImage>(e);
		}

		Component::UICanvas& GetUICanvas(uint32_t e) {
			return NE::GetScene().GetECSCoordinator().GetComponent<NE::ECS::Component::UICanvas>(e);
		}

		Component::UIText& GetUIText(uint32_t e) {
			return NE::GetScene().GetECSCoordinator().GetComponent<NE::ECS::Component::UIText>(e);
		}

		Component::UIButton& GetUIButton(uint32_t e) {
			return NE::GetScene().GetECSCoordinator().GetComponent<NE::ECS::Component::UIButton>(e);
		}

		Component::UISlider& GetUISlider(uint32_t e) {
			return NE::GetScene().GetECSCoordinator().GetComponent<NE::ECS::Component::UISlider>(e);
		}

		Component::UIToggle& GetUIToggle(uint32_t e) {
			return NE::GetScene().GetECSCoordinator().GetComponent<NE::ECS::Component::UIToggle>(e);
		}

		Component::Hierarchy& GetEntityHierarchy(uint32_t e) {
			return NE::GetScene().GetECSCoordinator().GetComponent<NE::ECS::Component::Hierarchy>(e);
		}

		Component::Animator& GetEntityAnimator(uint32_t e) {
			return NE::GetScene().GetECSCoordinator().GetComponent<NE::ECS::Component::Animator>(e);
		}

		Component::Camera& GetEntityCamera(uint32_t e) {
			return NE::GetScene().GetECSCoordinator().GetComponent<NE::ECS::Component::Camera>(e);
		}

		Component::PrefabLink& GetPrefabLink(uint32_t e) {
			return NE::GetScene().GetECSCoordinator().GetComponent<NE::ECS::Component::PrefabLink>(e);
		}

		Component::PrefabInstance& GetPrefabInstance(uint32_t e) {
			return NE::GetScene().GetECSCoordinator().GetComponent<NE::ECS::Component::PrefabInstance>(e);
		}

		Component::CharacterController& GetCharacterController(uint32_t e) {
			return NE::GetScene().GetECSCoordinator().GetComponent<NE::ECS::Component::CharacterController>(e);
		}

        Component::DecalProjector& GetDecalProjector(uint32_t e) {
            return NE::GetScene().GetECSCoordinator().GetComponent<NE::ECS::Component::DecalProjector>(e);
        }

		Component::UILayoutGroup& GetUILayoutGroup(uint32_t e) {
			return NE::GetScene().GetECSCoordinator().GetComponent<NE::ECS::Component::UILayoutGroup>(e);
		}

		Component::UIGridLayoutGroup& GetUIGridLayoutGroup(uint32_t e) {
			return NE::GetScene().GetECSCoordinator().GetComponent<NE::ECS::Component::UIGridLayoutGroup>(e);
		}

		Component::UILayoutElement& GetUILayoutElement(uint32_t e) {
			return NE::GetScene().GetECSCoordinator().GetComponent<NE::ECS::Component::UILayoutElement>(e);
		}

		Component::UIScrollRect& GetUIScrollRect(uint32_t e) {
			return NE::GetScene().GetECSCoordinator().GetComponent<NE::ECS::Component::UIScrollRect>(e);
		}

		Component::UIAutoSize& GetUIAutoSize(uint32_t e) {
			return NE::GetScene().GetECSCoordinator().GetComponent<NE::ECS::Component::UIAutoSize>(e);
		}

		Component::UIInputField& GetUIInputField(uint32_t e) {
			return NE::GetScene().GetECSCoordinator().GetComponent<NE::ECS::Component::UIInputField>(e);
		}

		Component::UIDropdown& GetUIDropdown(uint32_t e) {
			return NE::GetScene().GetECSCoordinator().GetComponent<NE::ECS::Component::UIDropdown>(e);
		}

		//void SetParent(uint32_t child, uint32_t parent, bool worldPositionStays) {
		//	NE::GetScene().GetECSCoordinator().m_transformSystem->SetParent(child, parent);
		//}

		// === Script Management Implementation ===

		std::vector<std::string> GetRegisteredScriptNames() {
			auto* scriptSystem = GetScene().GetECSCoordinator().m_scriptSystem.get();
			if (scriptSystem) {
				return Scripting::ScriptingEngine::GetInstance().GetRegisteredScriptNames();
			}
			return {};
		}

		bool SetEntityScript(uint32_t e, const std::string& scriptName) {
			if (!GetScene().GetECSCoordinator().HasComponent<ECS::Component::NativeScript>(e)) {
				return false;
			}

			auto& script = GetEntityScript(e);

			// Verify script exists before setting
			if (!Scripting::ScriptingEngine::GetInstance().IsScriptRegistered(scriptName)) {
				return false;
			}

			// Clean up existing instances if any (via ScriptEngine)
			Scripting::ScriptingEngine::GetInstance().DestroyScriptInstances(e);

			// Set new script name (clear existing and add the new one)
			script.ScriptNames.clear();
			script.ScriptNames.push_back(scriptName);
			script.IsDirty = true;

			// ScriptSystem will handle instance creation in its next Update
			return true;
		}

		void RemoveEntityScript(uint32_t e) {
			if (!GetScene().GetECSCoordinator().HasComponent<ECS::Component::NativeScript>(e)) {
				return;
			}

			auto& script = GetEntityScript(e);

			// Clean up existing instances via ScriptEngine
			Scripting::ScriptingEngine::GetInstance().DestroyScriptInstances(e);

			// Reset component data
			script.ScriptNames.clear();
			script.SerializedFields.clear();
			script.EntityReferenceFields.clear();
			script.IsDirty = false; // No script to recreate
		}

		bool IsScriptRegistered(const std::string& scriptName) {
			auto* scriptSystem = GetScene().GetECSCoordinator().m_scriptSystem.get();
			if (scriptSystem) {
				return Scripting::ScriptingEngine::GetInstance().IsScriptRegistered(scriptName);
			}
			return false;
		}

		bool AddEntityScript(uint32_t e, const std::string& scriptName) {
			if (!GetScene().GetECSCoordinator().HasComponent<ECS::Component::NativeScript>(e)) {
				return false;
			}

			auto& script = GetEntityScript(e);

			// Verify script exists before adding
			if (!Scripting::ScriptingEngine::GetInstance().IsScriptRegistered(scriptName)) {
				return false;
			}

			// Check if script already exists in the list
			for (const auto& existingName : script.ScriptNames) {
				if (existingName == scriptName) {
					return false; // Already exists
				}
			}

			// Add to the list
			script.ScriptNames.push_back(scriptName);
			script.IsDirty = true;

			// ScriptSystem will handle instance creation in its next Update
			return true;
		}

		void RemoveEntityScriptByIndex(uint32_t e, size_t index) {
			if (!GetScene().GetECSCoordinator().HasComponent<ECS::Component::NativeScript>(e)) {
				return;
			}

			auto& script = GetEntityScript(e);

			// Validate index
			if (index >= script.ScriptNames.size()) {
				return;
			}

			// Remove the script at the specified index
			std::string removedScriptName = script.ScriptNames[index];
			script.ScriptNames.erase(script.ScriptNames.begin() + index);

			// Remove associated serialized fields for this script
			std::string prefix = removedScriptName + ".";
			auto it = script.SerializedFields.begin();
			while (it != script.SerializedFields.end()) {
				if (it->first.starts_with(prefix)) {
					it = script.SerializedFields.erase(it);
				} else {
					++it;
				}
			}

			// Mark as dirty so ScriptSystem will synchronize instances
			script.IsDirty = true;
		}

		void AddAnimatorComponent(uint32_t e) {
			if (GetScene().GetECSCoordinator().HasComponent<ECS::Component::Animator>(e))
				return;
			GetScene().GetECSCoordinator().AddComponent(e, ECS::Component::Animator{});
		}

		void SetLayer(Entity e, Core::LayerID layer) {
			GetScene().GetECSCoordinator().GetEntityManager().SetLayer(e, layer);
		}

		std::shared_ptr<NE::Animation::AnimationClip> GetAnimationClip(const std::string& uuid) {
			return Resource::ResourceManager::GetInstance().LoadResource<Animation::AnimationClip>(uuid);
		}
		
		void AssignAnimClip(uint32_t e, const std::string& uuid) {
			auto& animator = NE::GetScene().GetECSCoordinator().GetComponent<NE::ECS::Component::Animator>(e);
			animator.animClipUUID = uuid;
			animator.clip = Resource::ResourceManager::GetInstance().LoadResource<Animation::AnimationClip>(uuid);
		}

		//=========================================================================
		// UI CANVAS HELPERS
		//=========================================================================

		float GetUICanvasAlpha(uint32_t e) {
			auto& ecs = GetScene().GetECSCoordinator();
			if (!ecs.HasComponent<Component::UICanvas>(e)) return 1.0f;
			return ecs.GetComponent<Component::UICanvas>(e).alpha;
		}

		void SetUICanvasAlpha(uint32_t e, float alpha) {
			auto& ecs = GetScene().GetECSCoordinator();
			if (!ecs.HasComponent<Component::UICanvas>(e)) return;
			ecs.GetComponent<Component::UICanvas>(e).alpha = alpha < 0.0f ? 0.0f : (alpha > 1.0f ? 1.0f : alpha);
		}

		//=========================================================================
		// UI IMAGE UTILITIES
		//=========================================================================

		bool SetUIImageTexture(uint32_t imageEntity, const char* textureUUID) {
			return SetUIImageTextureAndMaterial(imageEntity, textureUUID, "");
		}

		bool SetUIImageTextureAndMaterial(uint32_t imageEntity, const char* textureUUID, const char* materialUUID) {
			auto& ecs = GetScene().GetECSCoordinator();

			// Safety check - entity must have UIImage component
			if (!ecs.HasComponent<Component::UIImage>(imageEntity)) {
				return false;
			}

			auto& img = ecs.GetComponent<Component::UIImage>(imageEntity);

			// Update texture UUID and load the texture
			if (textureUUID && textureUUID[0] != '\0') {
				img.textureUUID = textureUUID;
				auto texture = Resource::ResourceManager::GetInstance()
					.LoadResource<Graphics::OpenGL::GLTexture>(textureUUID);

				if (texture) {
					img.bindlessHandle = texture->GetBindlessHandle();
				} else {
					img.bindlessHandle = 0;  // Texture load failed, fallback to solid color
					return false;
				}
			} else {
				img.textureUUID.clear();
				img.bindlessHandle = 0;  // No texture, solid color only
			}

			// Update material if provided
			if (materialUUID && materialUUID[0] != '\0') {
				img.materialUUID = materialUUID;
				img.material = Resource::ResourceManager::GetInstance()
					.LoadResource<Graphics::Material>(materialUUID);
			}

			img.isDirty = true;  // Signal refresh
			return true;
		}

		void SetUIImageColor(uint32_t imageEntity, float r, float g, float b, float a) {
			auto& ecs = GetScene().GetECSCoordinator();

			if (!ecs.HasComponent<Component::UIImage>(imageEntity)) {
				return;
			}

			auto& img = ecs.GetComponent<Component::UIImage>(imageEntity);
			img.color = Math::Vec4(r, g, b, a);
			img.isDirty = true;
		}

		void SetUIImageFillAmount(uint32_t imageEntity, float fillAmount) {
			auto& ecs = GetScene().GetECSCoordinator();

			if (!ecs.HasComponent<Component::UIImage>(imageEntity)) {
				return;
			}

			auto& img = ecs.GetComponent<Component::UIImage>(imageEntity);

			// Clamp to valid range
			if (fillAmount < 0.0f) fillAmount = 0.0f;
			if (fillAmount > 1.0f) fillAmount = 1.0f;

			img.fillAmount = fillAmount;
			img.isDirty = true;
		}
	}

	namespace Command {

		//=========================================================================
		// UITEXT HELPERS
		//=========================================================================

		void SetUIText(uint32_t e, const char* text) {
			auto& ecs = GetScene().GetECSCoordinator();
			if (!ecs.HasComponent<Component::UIText>(e)) return;
			auto& comp = ecs.GetComponent<Component::UIText>(e);
			comp.text = text ? text : "";
			comp.isDirty = true;
		}

		void SetUITextColor(uint32_t e, float r, float g, float b, float a) {
			auto& ecs = GetScene().GetECSCoordinator();
			if (!ecs.HasComponent<Component::UIText>(e)) return;
			auto& comp = ecs.GetComponent<Component::UIText>(e);
			comp.color = Math::Vec4(r, g, b, a);
			comp.isDirty = true;
		}

		const char* GetUITextString(uint32_t e) {
			auto& ecs = GetScene().GetECSCoordinator();
			if (!ecs.HasComponent<Component::UIText>(e)) return nullptr;
			return ecs.GetComponent<Component::UIText>(e).text.c_str();
		}

		//=========================================================================
		// UIBUTTON HELPERS
		//=========================================================================

		bool WasButtonClicked(uint32_t e) {
			auto& ecs = GetScene().GetECSCoordinator();
			if (!ecs.HasComponent<Component::UIButton>(e)) return false;
			return ecs.GetComponent<Component::UIButton>(e).wasClicked;
		}

		bool IsButtonHovered(uint32_t e) {
			auto& ecs = GetScene().GetECSCoordinator();
			if (!ecs.HasComponent<Component::UIButton>(e)) return false;
			return ecs.GetComponent<Component::UIButton>(e).currentState == Component::UIButton::State::HOVERED;
		}

		bool IsButtonPressed(uint32_t e) {
			auto& ecs = GetScene().GetECSCoordinator();
			if (!ecs.HasComponent<Component::UIButton>(e)) return false;
			return ecs.GetComponent<Component::UIButton>(e).currentState == Component::UIButton::State::PRESSED;
		}

		void SetButtonInteractable(uint32_t e, bool interactable) {
			auto& ecs = GetScene().GetECSCoordinator();
			if (!ecs.HasComponent<Component::UIButton>(e)) return;
			ecs.GetComponent<Component::UIButton>(e).interactable = interactable;
		}

		bool IsButtonInteractable(uint32_t e) {
			auto& ecs = GetScene().GetECSCoordinator();
			if (!ecs.HasComponent<Component::UIButton>(e)) return false;
			return ecs.GetComponent<Component::UIButton>(e).interactable;
		}

		//=========================================================================
		// UITOGGLE HELPERS
		//=========================================================================

		bool IsToggleOn(uint32_t e) {
			auto& ecs = GetScene().GetECSCoordinator();
			if (!ecs.HasComponent<Component::UIToggle>(e)) return false;
			return ecs.GetComponent<Component::UIToggle>(e).isOn;
		}

		void SetToggleOn(uint32_t e, bool value) {
			auto& ecs = GetScene().GetECSCoordinator();
			if (!ecs.HasComponent<Component::UIToggle>(e)) return;
			auto& comp = ecs.GetComponent<Component::UIToggle>(e);
			if (comp.isOn != value) {
				comp.isOn = value;
				comp.valueChanged = true;
			}
		}

		bool ToggleValueChanged(uint32_t e) {
			auto& ecs = GetScene().GetECSCoordinator();
			if (!ecs.HasComponent<Component::UIToggle>(e)) return false;
			return ecs.GetComponent<Component::UIToggle>(e).valueChanged;
		}

		void SetToggleInteractable(uint32_t e, bool interactable) {
			auto& ecs = GetScene().GetECSCoordinator();
			if (!ecs.HasComponent<Component::UIToggle>(e)) return;
			ecs.GetComponent<Component::UIToggle>(e).interactable = interactable;
		}

		//=========================================================================
		// UISLIDER HELPERS
		//=========================================================================

		float GetSliderValue(uint32_t e) {
			auto& ecs = GetScene().GetECSCoordinator();
			if (!ecs.HasComponent<Component::UISlider>(e)) return 0.0f;
			return ecs.GetComponent<Component::UISlider>(e).value;
		}

		void SetSliderValue(uint32_t e, float value) {
			auto& ecs = GetScene().GetECSCoordinator();
			if (!ecs.HasComponent<Component::UISlider>(e)) return;
			auto& comp = ecs.GetComponent<Component::UISlider>(e);
			if (value < comp.minValue) value = comp.minValue;
			if (value > comp.maxValue) value = comp.maxValue;
			if (comp.value != value) {
				comp.value = value;
				comp.valueChanged = true;
			}
		}

		float GetSliderNormalizedValue(uint32_t e) {
			auto& ecs = GetScene().GetECSCoordinator();
			if (!ecs.HasComponent<Component::UISlider>(e)) return 0.0f;
			auto& comp = ecs.GetComponent<Component::UISlider>(e);
			if (comp.maxValue <= comp.minValue) return 0.0f;
			return (comp.value - comp.minValue) / (comp.maxValue - comp.minValue);
		}

		void SetSliderNormalizedValue(uint32_t e, float normalized) {
			auto& ecs = GetScene().GetECSCoordinator();
			if (!ecs.HasComponent<Component::UISlider>(e)) return;
			auto& comp = ecs.GetComponent<Component::UISlider>(e);
			if (normalized < 0.0f) normalized = 0.0f;
			if (normalized > 1.0f) normalized = 1.0f;
			float newVal = comp.minValue + normalized * (comp.maxValue - comp.minValue);
			if (comp.value != newVal) {
				comp.value = newVal;
				comp.valueChanged = true;
			}
		}

		void SetSliderMinMax(uint32_t e, float minVal, float maxVal) {
			auto& ecs = GetScene().GetECSCoordinator();
			if (!ecs.HasComponent<Component::UISlider>(e)) return;
			auto& comp = ecs.GetComponent<Component::UISlider>(e);
			comp.minValue = minVal;
			comp.maxValue = maxVal;
			// Re-clamp value
			if (comp.value < comp.minValue) comp.value = comp.minValue;
			if (comp.value > comp.maxValue) comp.value = comp.maxValue;
		}

		bool SliderValueChanged(uint32_t e) {
			auto& ecs = GetScene().GetECSCoordinator();
			if (!ecs.HasComponent<Component::UISlider>(e)) return false;
			return ecs.GetComponent<Component::UISlider>(e).valueChanged;
		}

		void SetSliderInteractable(uint32_t e, bool interactable) {
			auto& ecs = GetScene().GetECSCoordinator();
			if (!ecs.HasComponent<Component::UISlider>(e)) return;
			ecs.GetComponent<Component::UISlider>(e).interactable = interactable;
		}

		//=========================================================================
		// UIINPUTFIELD HELPERS
		//=========================================================================

		const char* GetInputFieldText(uint32_t e) {
			auto& ecs = GetScene().GetECSCoordinator();
			if (!ecs.HasComponent<Component::UIInputField>(e)) return nullptr;
			return ecs.GetComponent<Component::UIInputField>(e).text.c_str();
		}

		void SetInputFieldText(uint32_t e, const char* text) {
			auto& ecs = GetScene().GetECSCoordinator();
			if (!ecs.HasComponent<Component::UIInputField>(e)) return;
			ecs.GetComponent<Component::UIInputField>(e).text = text ? text : "";
		}

		bool IsInputFieldFocused(uint32_t e) {
			auto& ecs = GetScene().GetECSCoordinator();
			if (!ecs.HasComponent<Component::UIInputField>(e)) return false;
			return ecs.GetComponent<Component::UIInputField>(e).isFocused;
		}

		void SetInputFieldInteractable(uint32_t e, bool interactable) {
			auto& ecs = GetScene().GetECSCoordinator();
			if (!ecs.HasComponent<Component::UIInputField>(e)) return;
			ecs.GetComponent<Component::UIInputField>(e).interactable = interactable;
		}

		//=========================================================================
		// UIDROPDOWN HELPERS
		//=========================================================================

		int GetDropdownSelectedIndex(uint32_t e) {
			auto& ecs = GetScene().GetECSCoordinator();
			if (!ecs.HasComponent<Component::UIDropdown>(e)) return -1;
			return ecs.GetComponent<Component::UIDropdown>(e).selectedIndex;
		}

		void SetDropdownSelectedIndex(uint32_t e, int index) {
			auto& ecs = GetScene().GetECSCoordinator();
			if (!ecs.HasComponent<Component::UIDropdown>(e)) return;
			auto& comp = ecs.GetComponent<Component::UIDropdown>(e);
			int count = static_cast<int>(comp.options.size());
			if (index < 0) index = 0;
			if (index >= count) index = count > 0 ? count - 1 : 0;
			comp.selectedIndex = index;
		}

		int GetDropdownOptionCount(uint32_t e) {
			auto& ecs = GetScene().GetECSCoordinator();
			if (!ecs.HasComponent<Component::UIDropdown>(e)) return 0;
			return static_cast<int>(ecs.GetComponent<Component::UIDropdown>(e).options.size());
		}

		void SetDropdownInteractable(uint32_t e, bool interactable) {
			auto& ecs = GetScene().GetECSCoordinator();
			if (!ecs.HasComponent<Component::UIDropdown>(e)) return;
			ecs.GetComponent<Component::UIDropdown>(e).interactable = interactable;
		}

		void SetUIViewportBounds(float offsetX, float offsetY, float width, float height, float uiWidth, float uiHeight) {
			Systems::UIEventSystem::SetViewportBounds(offsetX, offsetY, width, height, uiWidth, uiHeight);
		}

		void ClearUIViewportBounds() {
			Systems::UIEventSystem::ClearViewportBounds();
		}

		void ToggleActive(Entity e, bool isActive) {
			 GetScene().GetECSCoordinator().ToggleEntityActive(e, isActive);
		}
	}
}
