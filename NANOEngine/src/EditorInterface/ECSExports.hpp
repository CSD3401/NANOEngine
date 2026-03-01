#pragma once
#include <cstdint>
#include <unordered_map>
#include <string>
#include <typeindex>
#include <vector>
#include "../NANOEngineAPI.hpp"
#include "../Core/Reflection.hpp"
#include "ECS/Core/Entity.hpp"
#include "SceneManagement/SceneManager.hpp"
#include "Engine.hpp"
#include "Core/Layers.hpp"
#include "Animation/AnimationClip.hpp"

//namespace NE {
//	SceneManagement::Scene& GetScene();
//}

namespace NE::ECS {
	// Forward Decl
	namespace Component {
		struct Transform;
		struct Renderer;
		struct Light;
		struct Rigidbody;
		struct Collider;
		struct EntityMeta;
		struct AudioSource;
		struct NativeScript;
		struct Animator;
		struct Camera;
		struct PhysicsBody;
		struct UIRectTransform;
		struct UIImage;
		struct UICanvas;
		struct UIText;
		struct UIButton;
		struct UISlider;
		struct UIToggle;
		struct Hierarchy;
		struct PrefabLink;
		struct PrefabInstance;
		struct CharacterController;
        struct DecalProjector;
		struct UILayoutGroup;
		struct UIGridLayoutGroup;
		struct UILayoutElement;
		struct UIScrollRect;
		struct UIAutoSize;
		struct UIInputField;
		struct UIDropdown;
	}

	namespace Query {

		// --- Component Handling ---
		NANOENGINE_API uint64_t GetEntitySignature(uint32_t e);

		// --- Editor Component View --- //
		NANOENGINE_API const Component::EntityMeta& GetEntityMeta(uint32_t e);
		NANOENGINE_API const Component::Transform& GetEntityTransform(uint32_t e);
		NANOENGINE_API const Component::Renderer& GetEntityRenderer(uint32_t e);
		NANOENGINE_API const Component::Light& GetEntityLight(uint32_t e);
		NANOENGINE_API const Component::Rigidbody& GetEntityRigidbody(uint32_t e);
		NANOENGINE_API const Component::Collider& GetEntityCollider(uint32_t e);
		NANOENGINE_API const Component::AudioSource& GetEntityAudioSource(uint32_t e);
		NANOENGINE_API const Component::NativeScript& GetEntityScript(uint32_t e);
		NANOENGINE_API const Component::UIRectTransform& GetUIRectTransform(uint32_t e);
		NANOENGINE_API const Component::UIImage& GetUIImage(uint32_t e);
		NANOENGINE_API const Component::UICanvas& GetUICanvas(uint32_t e);
		NANOENGINE_API const Component::UIText& GetUIText(uint32_t e);
		NANOENGINE_API const Component::UIButton& GetUIButton(uint32_t e);
		NANOENGINE_API const Component::UISlider& GetUISlider(uint32_t e);
		NANOENGINE_API const Component::UIToggle& GetUIToggle(uint32_t e);
		NANOENGINE_API const Component::Hierarchy& GetEntityHierarchy(uint32_t e);
		NANOENGINE_API const Component::Animator& GetEntityAnimator(uint32_t e);
		NANOENGINE_API const Component::Camera& GetEntityCamera(uint32_t e);
		NANOENGINE_API const Component::PrefabLink& GetPrefabLink(uint32_t e);
		NANOENGINE_API const Component::PrefabInstance& GetPrefabInstance(uint32_t e);
		NANOENGINE_API const Component::CharacterController& GetCharacterController(uint32_t e);
        NANOENGINE_API const Component::DecalProjector& GetDecalProjector(uint32_t e);
		NANOENGINE_API const Component::UILayoutGroup& GetUILayoutGroup(uint32_t e);
		NANOENGINE_API const Component::UIGridLayoutGroup& GetUIGridLayoutGroup(uint32_t e);
		NANOENGINE_API const Component::UILayoutElement& GetUILayoutElement(uint32_t e);
		NANOENGINE_API const Component::UIScrollRect& GetUIScrollRect(uint32_t e);
		NANOENGINE_API const Component::UIAutoSize& GetUIAutoSize(uint32_t e);
		NANOENGINE_API const Component::UIInputField& GetUIInputField(uint32_t e);
		NANOENGINE_API const Component::UIDropdown& GetUIDropdown(uint32_t e);

		// to move to this in the future
		// also need to find a way to enforce C as component
		template <typename C>
		const C& GetComponent(Entity e);

		template<> inline const Component::EntityMeta& GetComponent<Component::EntityMeta>(uint32_t e) { return GetEntityMeta(e); }
		template<> inline const Component::Transform& GetComponent<Component::Transform>(uint32_t e) { return GetEntityTransform(e); }
		template<> inline const Component::Renderer& GetComponent<Component::Renderer>(uint32_t e) { return GetEntityRenderer(e); }
		template<> inline const Component::Light& GetComponent<Component::Light>(uint32_t e) { return GetEntityLight(e); }
		template<> inline const Component::Rigidbody& GetComponent<Component::Rigidbody>(uint32_t e) { return GetEntityRigidbody(e); }
		template<> inline const Component::Collider& GetComponent<Component::Collider>(uint32_t e) { return GetEntityCollider(e); }
		template<> inline const Component::NativeScript& GetComponent<Component::NativeScript>(uint32_t e) { return GetEntityScript(e); }
		template<> inline const Component::UIRectTransform& GetComponent<Component::UIRectTransform>(uint32_t e) { return GetUIRectTransform(e); }
		template<> inline const Component::UICanvas& GetComponent<Component::UICanvas>(uint32_t e) { return GetUICanvas(e); }
		template<> inline const Component::UIImage& GetComponent<Component::UIImage>(uint32_t e) { return GetUIImage(e); }
		template<> inline const Component::UIText& GetComponent<Component::UIText>(uint32_t e) { return GetUIText(e); }
		template<> inline const Component::UIButton& GetComponent<Component::UIButton>(uint32_t e) { return GetUIButton(e); }
		template<> inline const Component::UISlider& GetComponent<Component::UISlider>(uint32_t e) { return GetUISlider(e); }
		template<> inline const Component::UIToggle& GetComponent<Component::UIToggle>(uint32_t e) { return GetUIToggle(e); }
		template<> inline const Component::Hierarchy& GetComponent<Component::Hierarchy>(uint32_t e) { return GetEntityHierarchy(e); }
		template<> inline const Component::Camera& GetComponent<Component::Camera>(uint32_t e) { return GetEntityCamera(e); }
		template<> inline const Component::Animator& GetComponent<Component::Animator>(uint32_t e) { return GetEntityAnimator(e); }
		template<> inline const Component::PrefabLink& GetComponent<Component::PrefabLink>(uint32_t e) { return GetPrefabLink(e); }
		template<> inline const Component::PrefabInstance& GetComponent<Component::PrefabInstance>(uint32_t e) { return GetPrefabInstance(e); }
		template<> inline const Component::CharacterController& GetComponent<Component::CharacterController>(uint32_t e) { return GetCharacterController(e); }
        template<> inline const Component::DecalProjector& GetComponent<Component::DecalProjector>(uint32_t e) { return GetDecalProjector(e); }
		template<> inline const Component::UILayoutGroup& GetComponent<Component::UILayoutGroup>(uint32_t e) { return GetUILayoutGroup(e); }
		template<> inline const Component::UIGridLayoutGroup& GetComponent<Component::UIGridLayoutGroup>(uint32_t e) { return GetUIGridLayoutGroup(e); }
		template<> inline const Component::UILayoutElement& GetComponent<Component::UILayoutElement>(uint32_t e) { return GetUILayoutElement(e); }
		template<> inline const Component::UIScrollRect& GetComponent<Component::UIScrollRect>(uint32_t e) { return GetUIScrollRect(e); }
		template<> inline const Component::UIAutoSize& GetComponent<Component::UIAutoSize>(uint32_t e) { return GetUIAutoSize(e); }
		template<> inline const Component::UIInputField& GetComponent<Component::UIInputField>(uint32_t e) { return GetUIInputField(e); }
		template<> inline const Component::UIDropdown& GetComponent<Component::UIDropdown>(uint32_t e) { return GetUIDropdown(e); }

		NANOENGINE_API bool HasEntityMeta(uint32_t e);
		NANOENGINE_API bool HasHierarchy(uint32_t e);
		NANOENGINE_API bool HasTransform(uint32_t e);
		NANOENGINE_API bool HasUIRectTransform(uint32_t e);
		NANOENGINE_API bool HasUICanvas(uint32_t e);
		NANOENGINE_API bool HasUIImage(uint32_t e);
		NANOENGINE_API bool HasUIText(uint32_t e);
		NANOENGINE_API bool HasUIButton(uint32_t e);
		NANOENGINE_API bool HasUISlider(uint32_t e);
		NANOENGINE_API bool HasUIToggle(uint32_t e);
		NANOENGINE_API bool HasPrefabLink(uint32_t e);
		NANOENGINE_API bool HasPrefabInstance(uint32_t e);
		NANOENGINE_API bool HasRenderer(uint32_t e);
		NANOENGINE_API bool HasLight(uint32_t e);
		NANOENGINE_API bool HasRigidbody(uint32_t e);
		NANOENGINE_API bool HasCollider(uint32_t e);
		NANOENGINE_API bool HasAudioSource(uint32_t e);
		NANOENGINE_API bool HasScript(uint32_t e);
		NANOENGINE_API bool HasAnimator(uint32_t e);
		NANOENGINE_API bool HasCamera(uint32_t e);
		NANOENGINE_API bool HasCharacterController(uint32_t e);
        NANOENGINE_API bool HasDecalProjector(uint32_t e);
		NANOENGINE_API bool HasUILayoutGroup(uint32_t e);
		NANOENGINE_API bool HasUIGridLayoutGroup(uint32_t e);
		NANOENGINE_API bool HasUILayoutElement(uint32_t e);
		NANOENGINE_API bool HasUIScrollRect(uint32_t e);
		NANOENGINE_API bool HasUIAutoSize(uint32_t e);
		NANOENGINE_API bool HasUIInputField(uint32_t e);
		NANOENGINE_API bool HasUIDropdown(uint32_t e);

		template <typename C>
		bool HasComponent(Entity e);

		template<> inline bool HasComponent<Component::EntityMeta>(uint32_t e) { return HasEntityMeta(e); }
		template<> inline bool HasComponent<Component::Hierarchy>(uint32_t e) { return HasHierarchy(e); }
		template<> inline bool HasComponent<Component::Transform>(uint32_t e) { return HasTransform(e); }
		template<> inline bool HasComponent<Component::UIRectTransform>(uint32_t e) { return HasUIRectTransform(e); }
		template<> inline bool HasComponent<Component::UICanvas>(uint32_t e) { return HasUICanvas(e); }
		template<> inline bool HasComponent<Component::UIImage>(uint32_t e) { return HasUIImage(e); }
		template<> inline bool HasComponent<Component::UIText>(uint32_t e) { return HasUIText(e); }
		template<> inline bool HasComponent<Component::UIButton>(uint32_t e) { return HasUIButton(e); }
		template<> inline bool HasComponent<Component::UISlider>(uint32_t e) { return HasUISlider(e); }
		template<> inline bool HasComponent<Component::Renderer>(uint32_t e) { return HasRenderer(e); }
		template<> inline bool HasComponent<Component::Light>(uint32_t e) { return HasLight(e); }
		template<> inline bool HasComponent<Component::Rigidbody>(uint32_t e) { return HasRigidbody(e); }
		template<> inline bool HasComponent<Component::Collider>(uint32_t e) { return HasCollider(e); }
		template<> inline bool HasComponent<Component::NativeScript>(uint32_t e) { return HasScript(e); }
		template<> inline bool HasComponent<Component::Animator>(uint32_t e) { return HasAnimator(e); }
		template<> inline bool HasComponent<Component::Camera>(uint32_t e) { return HasCamera(e); }
		template<> inline bool HasComponent<Component::AudioSource>(uint32_t e) { return HasAudioSource(e); }
		template<> inline bool HasComponent<Component::PrefabLink>(uint32_t e) { return HasPrefabLink(e); }
		template<> inline bool HasComponent<Component::PrefabInstance>(uint32_t e) { return HasPrefabInstance(e); }
		template<> inline bool HasComponent<Component::CharacterController>(uint32_t e) { return HasCharacterController(e); }
        template<> inline bool HasComponent<Component::DecalProjector>(uint32_t e) { return HasDecalProjector(e); }
		template<> inline bool HasComponent<Component::UIToggle>(uint32_t e) { return HasUIToggle(e); }
		template<> inline bool HasComponent<Component::UILayoutGroup>(uint32_t e) { return HasUILayoutGroup(e); }
		template<> inline bool HasComponent<Component::UIGridLayoutGroup>(uint32_t e) { return HasUIGridLayoutGroup(e); }
		template<> inline bool HasComponent<Component::UILayoutElement>(uint32_t e) { return HasUILayoutElement(e); }
		template<> inline bool HasComponent<Component::UIScrollRect>(uint32_t e) { return HasUIScrollRect(e); }
		template<> inline bool HasComponent<Component::UIAutoSize>(uint32_t e) { return HasUIAutoSize(e); }
		template<> inline bool HasComponent<Component::UIInputField>(uint32_t e) { return HasUIInputField(e); }
		template<> inline bool HasComponent<Component::UIDropdown>(uint32_t e) { return HasUIDropdown(e); }

		//template <typename C>
		//ComponentType GetComponentType() {
		//	return GetScene().GetECSCoordinator().GetComponentType<C>();
		//}

		NANOENGINE_API ComponentType GetEntityMetaComponentType();
		NANOENGINE_API ComponentType GetTransformComponentType();
		NANOENGINE_API ComponentType GetRendererComponentType();
		NANOENGINE_API ComponentType GetLightComponentType();
		NANOENGINE_API ComponentType GetRigidbodyComponentType();
		NANOENGINE_API ComponentType GetColliderComponentType();
		NANOENGINE_API ComponentType GetAudioSourceComponentType();
		NANOENGINE_API ComponentType GetScriptComponentType();
		NANOENGINE_API ComponentType GetUIRectTransformComponentType();
		NANOENGINE_API ComponentType GetUIImageComponentType();
		NANOENGINE_API ComponentType GetUICanvasComponentType();
		NANOENGINE_API ComponentType GetUITextComponentType();
		NANOENGINE_API ComponentType GetUIButtonComponentType();
		NANOENGINE_API ComponentType GetUISliderComponentType();
		NANOENGINE_API ComponentType GetUIToggleComponentType();
		NANOENGINE_API ComponentType GetEntityAnimatorComponentType();
		NANOENGINE_API ComponentType GetEntityCameraComponentType();
		NANOENGINE_API ComponentType GetPrefabInstanceComponentType();
		NANOENGINE_API ComponentType GetCharacterControllerComponentType();
        NANOENGINE_API ComponentType GetDecalProjectorComponentType();
		NANOENGINE_API ComponentType GetUILayoutGroupComponentType();
		NANOENGINE_API ComponentType GetUIGridLayoutGroupComponentType();
		NANOENGINE_API ComponentType GetUILayoutElementComponentType();
		NANOENGINE_API ComponentType GetUIScrollRectComponentType();
		NANOENGINE_API ComponentType GetUIAutoSizeComponentType();
		NANOENGINE_API ComponentType GetUIInputFieldComponentType();
		NANOENGINE_API ComponentType GetUIDropdownComponentType();

		NANOENGINE_API uint32_t GetParent(uint32_t child);

		NANOENGINE_API const Core::LayerID GetLayer(Entity e);
		NANOENGINE_API const Core::LayerMask GetLayerBit(Entity e);

		// Resolve a component LUID to its owning entity
		// Returns INVALID_ENTITY if the LUID is not found
		NANOENGINE_API Entity ResolveComponentLuidToEntity(uint64_t luid);

		// Resolve an EntityMeta LUID to its entity
		// Returns INVALID_ENTITY if the LUID is not found
		NANOENGINE_API Entity ResolveEntityMetaLuidToEntity(uint64_t luid);
	}

	namespace Command {
		NANOENGINE_API uint32_t CreateEntityNoComponents();
		NANOENGINE_API uint32_t CreateEmptyEntity(uint32_t parentEntt);
		NANOENGINE_API uint32_t CreateCubeEntity(uint32_t parentEntt);
		NANOENGINE_API uint32_t CreateSphereEntity(uint32_t parentEntt);
		NANOENGINE_API uint32_t CreateCylinderEntity(uint32_t parentEntt);
		NANOENGINE_API uint32_t CreateCapsuleEntity(uint32_t parentEntt);
		NANOENGINE_API uint32_t CreatePlaneEntity(uint32_t parentEntt);
		NANOENGINE_API uint32_t CreateQuadEntity(uint32_t parentEntt);
		NANOENGINE_API uint32_t CreateDirectionalLightEntity(uint32_t parentEntt);
		NANOENGINE_API uint32_t CreatePointLightEntity(uint32_t parentEntt);
		NANOENGINE_API uint32_t CreateSpotLightEntity(uint32_t parentEntt);
		NANOENGINE_API uint32_t CreateUICanvasEntity();
		NANOENGINE_API uint32_t CreateUIImageEntity(uint32_t parentCanvas);
		NANOENGINE_API void DestroyEntity(uint32_t e);
		NANOENGINE_API void SetParent(Entity _child, Entity _newParent, int _insertIndex, bool _keepWorldPos = true);
		NANOENGINE_API void SetActive(Entity entity, bool isActive);

		//NANOENGINE_API void AddEntityMetaComponent(uint32_t e);
		NANOENGINE_API void AddLightComponent(uint32_t e);
		NANOENGINE_API void AddRendererComponent(uint32_t e);
		NANOENGINE_API void AddRigidbodyComponent(uint32_t e);
		NANOENGINE_API void AddColliderComponent(uint32_t e);
		NANOENGINE_API void AddAudioSourceComponent(uint32_t e);
		NANOENGINE_API void AddScriptComponent(uint32_t e);
		NANOENGINE_API void AddCameraComponent(uint32_t e);
        NANOENGINE_API void AddDecalProjectorComponent(uint32_t e);

		template <typename C>
		void AddComponent(Entity e) {
			NE::GetScene().GetECSCoordinator().AddComponent<C>(e, C{});
		}

		NANOENGINE_API void AddEntityMetaComponent(uint32_t e, const Component::EntityMeta& c);
		NANOENGINE_API void AddTransformComponent(uint32_t e, const Component::Transform& c);
		NANOENGINE_API void AddHierarchyComponent(uint32_t e, const Component::Hierarchy& c);
		NANOENGINE_API void AddRendererComponent(uint32_t e, const Component::Renderer& c);
		NANOENGINE_API void AddLightComponent(uint32_t e, const Component::Light& c);
		NANOENGINE_API void AddRigidbodyComponent(uint32_t e, const Component::Rigidbody& c);
		NANOENGINE_API void AddColliderComponent(uint32_t e, const Component::Collider& c);
		NANOENGINE_API void AddAudioSourceComponent(uint32_t e, const Component::AudioSource& c);
		NANOENGINE_API void AddScriptComponent(uint32_t e, const Component::NativeScript& c);
		NANOENGINE_API void AddCameraComponent(uint32_t e, const Component::Camera& c);
		NANOENGINE_API void AddAnimatorComponent(uint32_t e, const Component::Animator& c);
		NANOENGINE_API void AddUIRectTransformComponent(uint32_t e, const Component::UIRectTransform& c);
		NANOENGINE_API void AddUICanvasComponent(uint32_t e, const Component::UICanvas& c);
		NANOENGINE_API void AddUIImageComponent(uint32_t e, const Component::UIImage& c);
		NANOENGINE_API void AddUITextComponent(uint32_t e, const Component::UIText& c);
		NANOENGINE_API void AddUIButtonComponent(uint32_t e, const Component::UIButton& c);
		NANOENGINE_API void AddUISliderComponent(uint32_t e, const Component::UISlider& c);
		NANOENGINE_API void AddUIToggleComponent(uint32_t e, const Component::UIToggle& c);
		NANOENGINE_API void AddPrefabLinkComponent(uint32_t e, const Component::PrefabLink& c);
		NANOENGINE_API void AddPrefabInstanceComponent(uint32_t e, const Component::PrefabInstance& c);
		NANOENGINE_API void AddCharacterControllerComponent(uint32_t e, const Component::CharacterController& c);
        NANOENGINE_API void AddDecalProjectorComponent(uint32_t e, const Component::DecalProjector& c);
		NANOENGINE_API void AddUILayoutGroupComponent(uint32_t e, const Component::UILayoutGroup& c);
		NANOENGINE_API void AddUIGridLayoutGroupComponent(uint32_t e, const Component::UIGridLayoutGroup& c);
		NANOENGINE_API void AddUILayoutElementComponent(uint32_t e, const Component::UILayoutElement& c);
		NANOENGINE_API void AddUIScrollRectComponent(uint32_t e, const Component::UIScrollRect& c);
		NANOENGINE_API void AddUIAutoSizeComponent(uint32_t e, const Component::UIAutoSize& c);
		NANOENGINE_API void AddUIInputFieldComponent(uint32_t e, const Component::UIInputField& c);
		NANOENGINE_API void AddUIDropdownComponent(uint32_t e, const Component::UIDropdown& c);

		template <typename C>
		void AddComponent(Entity e, const C& component);

		template<> inline void AddComponent<Component::EntityMeta>(uint32_t e, const Component::EntityMeta& component) { AddEntityMetaComponent(e, component); }
		template<> inline void AddComponent<Component::Transform>(uint32_t e, const Component::Transform& component) { AddTransformComponent(e, component); }
		template<> inline void AddComponent<Component::Hierarchy>(uint32_t e, const Component::Hierarchy& component) { AddHierarchyComponent(e, component); }
		template<> inline void AddComponent<Component::Renderer>(uint32_t e, const Component::Renderer& component) { AddRendererComponent(e, component); }
		template<> inline void AddComponent<Component::Light>(uint32_t e, const Component::Light& component) { AddLightComponent(e, component); }
		template<> inline void AddComponent<Component::Rigidbody>(uint32_t e, const Component::Rigidbody& component) { AddRigidbodyComponent(e, component); }
		template<> inline void AddComponent<Component::Collider>(uint32_t e, const Component::Collider& component) { AddColliderComponent(e, component); }
		template<> inline void AddComponent<Component::NativeScript>(uint32_t e, const Component::NativeScript& component) { AddScriptComponent(e, component); }
		template<> inline void AddComponent<Component::Animator>(uint32_t e, const Component::Animator& component) { AddAnimatorComponent(e, component); }
		template<> inline void AddComponent<Component::Camera>(uint32_t e, const Component::Camera& component) { AddCameraComponent(e, component); }
		template<> inline void AddComponent<Component::UIRectTransform>(uint32_t e, const Component::UIRectTransform& component) { AddUIRectTransformComponent(e, component); }
		template<> inline void AddComponent<Component::UICanvas>(uint32_t e, const Component::UICanvas& component) { AddUICanvasComponent(e, component); }
		template<> inline void AddComponent<Component::UIImage>(uint32_t e, const Component::UIImage& component) { AddUIImageComponent(e, component); }
		template<> inline void AddComponent<Component::UIText>(uint32_t e, const Component::UIText& component) { AddUITextComponent(e, component); }
		template<> inline void AddComponent<Component::UIButton>(uint32_t e, const Component::UIButton& component) { AddUIButtonComponent(e, component); }
		template<> inline void AddComponent<Component::UISlider>(uint32_t e, const Component::UISlider& component) { AddUISliderComponent(e, component); }
		template<> inline void AddComponent<Component::PrefabLink>(uint32_t e, const Component::PrefabLink& component) { AddPrefabLinkComponent(e, component); }
		template<> inline void AddComponent<Component::PrefabInstance>(uint32_t e, const Component::PrefabInstance& component) { AddPrefabInstanceComponent(e, component); }
		template<> inline void AddComponent<Component::CharacterController>(uint32_t e, const Component::CharacterController& component) { AddCharacterControllerComponent(e, component); }
        template<> inline void AddComponent<Component::DecalProjector>(uint32_t e, const Component::DecalProjector& component) { AddDecalProjectorComponent(e, component); }
		template<> inline void AddComponent<Component::UIToggle>(uint32_t e, const Component::UIToggle& component) { AddUIToggleComponent(e, component); }
		template<> inline void AddComponent<Component::UILayoutGroup>(uint32_t e, const Component::UILayoutGroup& component) { AddUILayoutGroupComponent(e, component); }
		template<> inline void AddComponent<Component::UIGridLayoutGroup>(uint32_t e, const Component::UIGridLayoutGroup& component) { AddUIGridLayoutGroupComponent(e, component); }
		template<> inline void AddComponent<Component::UILayoutElement>(uint32_t e, const Component::UILayoutElement& component) { AddUILayoutElementComponent(e, component); }
		template<> inline void AddComponent<Component::UIScrollRect>(uint32_t e, const Component::UIScrollRect& component) { AddUIScrollRectComponent(e, component); }
		template<> inline void AddComponent<Component::UIAutoSize>(uint32_t e, const Component::UIAutoSize& component) { AddUIAutoSizeComponent(e, component); }
		template<> inline void AddComponent<Component::UIInputField>(uint32_t e, const Component::UIInputField& component) { AddUIInputFieldComponent(e, component); }
		template<> inline void AddComponent<Component::UIDropdown>(uint32_t e, const Component::UIDropdown& component) { AddUIDropdownComponent(e, component); }

		NANOENGINE_API void RemoveLightComponent(uint32_t e);
		NANOENGINE_API void RemoveRendererComponent(uint32_t e);
		NANOENGINE_API void RemoveRigidbodyComponent(uint32_t e);
		NANOENGINE_API void RemoveColliderComponent(uint32_t e);
		NANOENGINE_API void RemoveAudioSourceComponent(uint32_t e);
		NANOENGINE_API void RemoveCameraComponent(uint32_t e);
        NANOENGINE_API void RemoveDecalProjectorComponent(uint32_t e);

		// --- Editor Component Mutators --- //
		//NANOENGINE_API Component::EntityMeta& GetEntityMeta(uint32_t e);
		//NANOENGINE_API Component::Transform& GetEntityTransform(uint32_t e);
		//NANOENGINE_API Component::Renderer& GetEntityRenderer(uint32_t e);
		//NANOENGINE_API Component::Light& GetEntityLight(uint32_t e);
		//NANOENGINE_API Component::Rigidbody& GetEntityRigidbody(uint32_t e);
		//NANOENGINE_API Component::Collider& GetEntityCollider(uint32_t e);
		//NANOENGINE_API Component::AudioSource& GetEntityAudioSource(uint32_t e);
		//NANOENGINE_API Component::NativeScript& GetEntityScript(uint32_t e);
		//NANOENGINE_API Component::UIRectTransform& GetUIRectTransform(uint32_t e);
		//NANOENGINE_API Component::UIImage& GetUIImage(uint32_t e);
		//NANOENGINE_API Component::UICanvas& GetUICanvas(uint32_t e);
		//NANOENGINE_API Component::Camera& GetEntityCamera(uint32_t e);
		//NANOENGINE_API Component::Hierarchy& GetEntityHierarchy(uint32_t e);
		//NANOENGINE_API Component::CharacterController& GetCharacterController(uint32_t e);

		NANOENGINE_API Component::EntityMeta& GetEntityMeta(uint32_t e);
		NANOENGINE_API Component::Transform& GetEntityTransform(uint32_t e);
		NANOENGINE_API Component::Renderer& GetEntityRenderer(uint32_t e);
		NANOENGINE_API Component::Light& GetEntityLight(uint32_t e);
		NANOENGINE_API Component::Rigidbody& GetEntityRigidbody(uint32_t e);
		NANOENGINE_API Component::Collider& GetEntityCollider(uint32_t e);
		NANOENGINE_API Component::AudioSource& GetEntityAudioSource(uint32_t e);
		NANOENGINE_API Component::NativeScript& GetEntityScript(uint32_t e);
		NANOENGINE_API Component::UIRectTransform& GetUIRectTransform(uint32_t e);
		NANOENGINE_API Component::UIImage& GetUIImage(uint32_t e);
		NANOENGINE_API Component::UICanvas& GetUICanvas(uint32_t e);
		NANOENGINE_API Component::UIText& GetUIText(uint32_t e);
		NANOENGINE_API Component::UIButton& GetUIButton(uint32_t e);
		NANOENGINE_API Component::UISlider& GetUISlider(uint32_t e);
		NANOENGINE_API Component::UIToggle& GetUIToggle(uint32_t e);
		NANOENGINE_API Component::Hierarchy& GetEntityHierarchy(uint32_t e);
		NANOENGINE_API Component::Animator& GetEntityAnimator(uint32_t e);
		NANOENGINE_API Component::Camera& GetEntityCamera(uint32_t e);
		NANOENGINE_API Component::PrefabLink& GetPrefabLink(uint32_t e);
		NANOENGINE_API Component::PrefabInstance& GetPrefabInstance(uint32_t e);
		NANOENGINE_API Component::CharacterController& GetCharacterController(uint32_t e);
        NANOENGINE_API Component::DecalProjector& GetDecalProjector(uint32_t e);
		NANOENGINE_API Component::UILayoutGroup& GetUILayoutGroup(uint32_t e);
		NANOENGINE_API Component::UIGridLayoutGroup& GetUIGridLayoutGroup(uint32_t e);
		NANOENGINE_API Component::UILayoutElement& GetUILayoutElement(uint32_t e);
		NANOENGINE_API Component::UIScrollRect& GetUIScrollRect(uint32_t e);
		NANOENGINE_API Component::UIAutoSize& GetUIAutoSize(uint32_t e);
		NANOENGINE_API Component::UIInputField& GetUIInputField(uint32_t e);
		NANOENGINE_API Component::UIDropdown& GetUIDropdown(uint32_t e);

		template <typename C>
		C& GetComponent(Entity e);

		template<> inline Component::EntityMeta& GetComponent<Component::EntityMeta>(uint32_t e) { return GetEntityMeta(e); }
		template<> inline Component::Transform& GetComponent<Component::Transform>(uint32_t e) { return GetEntityTransform(e); }
		template<> inline Component::Renderer& GetComponent<Component::Renderer>(uint32_t e) { return GetEntityRenderer(e); }
		template<> inline Component::Light& GetComponent<Component::Light>(uint32_t e) { return GetEntityLight(e); }
		template<> inline Component::Rigidbody& GetComponent<Component::Rigidbody>(uint32_t e) { return GetEntityRigidbody(e); }
		template<> inline Component::Collider& GetComponent<Component::Collider>(uint32_t e) { return GetEntityCollider(e); }
		template<> inline Component::NativeScript& GetComponent<Component::NativeScript>(uint32_t e) { return GetEntityScript(e); }
		template<> inline Component::UIRectTransform& GetComponent<Component::UIRectTransform>(uint32_t e) { return GetUIRectTransform(e); }
		template<> inline Component::UIImage& GetComponent<Component::UIImage>(uint32_t e) { return GetUIImage(e); }
		template<> inline Component::UICanvas& GetComponent<Component::UICanvas>(uint32_t e) { return GetUICanvas(e); }
		template<> inline Component::Hierarchy& GetComponent<Component::Hierarchy>(uint32_t e) { return GetEntityHierarchy(e); }
		template<> inline Component::Camera& GetComponent<Component::Camera>(uint32_t e) { return GetEntityCamera(e); }
		template<> inline Component::Animator& GetComponent<Component::Animator>(uint32_t e) { return GetEntityAnimator(e); }
		template<> inline Component::PrefabLink& GetComponent<Component::PrefabLink>(uint32_t e) { return GetPrefabLink(e); }
		template<> inline Component::PrefabInstance& GetComponent<Component::PrefabInstance>(uint32_t e) { return GetPrefabInstance(e); }
		template<> inline Component::CharacterController& GetComponent<Component::CharacterController>(uint32_t e) { return GetCharacterController(e); }
        template<> inline Component::DecalProjector& GetComponent<Component::DecalProjector>(uint32_t e) { return GetDecalProjector(e); }
		template<> inline Component::UILayoutGroup& GetComponent<Component::UILayoutGroup>(uint32_t e) { return GetUILayoutGroup(e); }
		template<> inline Component::UIGridLayoutGroup& GetComponent<Component::UIGridLayoutGroup>(uint32_t e) { return GetUIGridLayoutGroup(e); }
		template<> inline Component::UILayoutElement& GetComponent<Component::UILayoutElement>(uint32_t e) { return GetUILayoutElement(e); }
		template<> inline Component::UIScrollRect& GetComponent<Component::UIScrollRect>(uint32_t e) { return GetUIScrollRect(e); }
		template<> inline Component::UIAutoSize& GetComponent<Component::UIAutoSize>(uint32_t e) { return GetUIAutoSize(e); }
		template<> inline Component::UIInputField& GetComponent<Component::UIInputField>(uint32_t e) { return GetUIInputField(e); }
		template<> inline Component::UIDropdown& GetComponent<Component::UIDropdown>(uint32_t e) { return GetUIDropdown(e); }

		// --- Script Management ---
		NANOENGINE_API std::vector<std::string> GetRegisteredScriptNames();
		NANOENGINE_API bool SetEntityScript(uint32_t e, const std::string& scriptName);  // Replaces all scripts
		NANOENGINE_API void RemoveEntityScript(uint32_t e);  // Removes all scripts
		NANOENGINE_API bool AddEntityScript(uint32_t e, const std::string& scriptName);  // Appends to list
		NANOENGINE_API void RemoveEntityScriptByIndex(uint32_t e, size_t index);  // Removes specific script
		NANOENGINE_API bool IsScriptRegistered(const std::string& scriptName);

		NANOENGINE_API void AddAnimatorComponent(uint32_t e);              // <-- ADD
		NANOENGINE_API Component::Animator& GetEntityAnimator(uint32_t e);

		NANOENGINE_API void SetLayer(Entity e, Core::LayerID layer);

		NANOENGINE_API std::shared_ptr<NE::Animation::AnimationClip> GetAnimationClip(const std::string& uuid);
		NANOENGINE_API void AssignAnimClip(uint32_t e, const std::string& uuid);

		// --- UI Image Utilities ---
		/// Swap the texture on a UIImage component (handles GPU resource loading)
		NANOENGINE_API bool SetUIImageTexture(uint32_t imageEntity, const char* textureUUID);

		/// Swap texture and material on a UIImage component
		NANOENGINE_API bool SetUIImageTextureAndMaterial(uint32_t imageEntity, const char* textureUUID, const char* materialUUID);

		/// Set the color tint on a UIImage component
		NANOENGINE_API void SetUIImageColor(uint32_t imageEntity, float r, float g, float b, float a);

		/// Set the fill amount on a UIImage component (for FILLED image type)
		NANOENGINE_API void SetUIImageFillAmount(uint32_t imageEntity, float fillAmount);

		// --- UIText Helpers ---
		NANOENGINE_API void SetUIText(uint32_t e, const char* text);
		NANOENGINE_API void SetUITextColor(uint32_t e, float r, float g, float b, float a);
		NANOENGINE_API const char* GetUITextString(uint32_t e);

		// --- UIButton Helpers ---
		NANOENGINE_API bool WasButtonClicked(uint32_t e);
		NANOENGINE_API bool IsButtonHovered(uint32_t e);
		NANOENGINE_API bool IsButtonPressed(uint32_t e);
		NANOENGINE_API void SetButtonInteractable(uint32_t e, bool interactable);
		NANOENGINE_API bool IsButtonInteractable(uint32_t e);

		// --- UIToggle Helpers ---
		NANOENGINE_API bool IsToggleOn(uint32_t e);
		NANOENGINE_API void SetToggleOn(uint32_t e, bool value);
		NANOENGINE_API bool ToggleValueChanged(uint32_t e);
		NANOENGINE_API void SetToggleInteractable(uint32_t e, bool interactable);

		// --- UISlider Helpers ---
		NANOENGINE_API float GetSliderValue(uint32_t e);
		NANOENGINE_API void SetSliderValue(uint32_t e, float value);
		NANOENGINE_API float GetSliderNormalizedValue(uint32_t e);
		NANOENGINE_API void SetSliderNormalizedValue(uint32_t e, float normalized);
		NANOENGINE_API void SetSliderMinMax(uint32_t e, float minVal, float maxVal);
		NANOENGINE_API bool SliderValueChanged(uint32_t e);
		NANOENGINE_API void SetSliderInteractable(uint32_t e, bool interactable);

		// --- UIInputField Helpers ---
		NANOENGINE_API const char* GetInputFieldText(uint32_t e);
		NANOENGINE_API void SetInputFieldText(uint32_t e, const char* text);
		NANOENGINE_API bool IsInputFieldFocused(uint32_t e);
		NANOENGINE_API void SetInputFieldInteractable(uint32_t e, bool interactable);

		// --- UIDropdown Helpers ---
		NANOENGINE_API int GetDropdownSelectedIndex(uint32_t e);
		NANOENGINE_API void SetDropdownSelectedIndex(uint32_t e, int index);
		NANOENGINE_API int GetDropdownOptionCount(uint32_t e);
		NANOENGINE_API void SetDropdownInteractable(uint32_t e, bool interactable);

		// --- UI Viewport Bounds (for Editor) ---
		/// Set viewport bounds to transform mouse coordinates from window space to UI space
		NANOENGINE_API void SetUIViewportBounds(float offsetX, float offsetY, float width, float height, float uiWidth, float uiHeight);

		/// Clear viewport bounds (use raw mouse coordinates)
		NANOENGINE_API void ClearUIViewportBounds();
	}

}
