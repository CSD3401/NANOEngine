#pragma once

namespace NE::ECS::Component {
	struct EntityMeta;
	struct Transform;
	struct Renderer;
	struct Light;
	struct Collider;
	struct Rigidbody;
	struct NativeScript;
	struct Camera;
	struct UIRectTransform;
	struct UICanvas;
	struct UIImage;
	struct Hierarchy;
	struct PrefabLink;
	struct PrefabInstance;
	struct CharacterController;
	struct Animator;
}

template <typename T> struct ComponentKey;

#define NE_COMPONENT_KEY(Type, KeyLiteral) \
    template <> struct ComponentKey<Type> { static constexpr const char* value = KeyLiteral; };

NE_COMPONENT_KEY(NE::ECS::Component::EntityMeta, "EntityMeta")
NE_COMPONENT_KEY(NE::ECS::Component::Transform, "Transform")
NE_COMPONENT_KEY(NE::ECS::Component::Hierarchy, "Hierarchy")
NE_COMPONENT_KEY(NE::ECS::Component::Renderer, "Renderer")
NE_COMPONENT_KEY(NE::ECS::Component::Light, "Light")
NE_COMPONENT_KEY(NE::ECS::Component::Collider, "Collider")
NE_COMPONENT_KEY(NE::ECS::Component::Rigidbody, "Rigidbody")
NE_COMPONENT_KEY(NE::ECS::Component::NativeScript, "NativeScript")
NE_COMPONENT_KEY(NE::ECS::Component::Camera, "Camera")
NE_COMPONENT_KEY(NE::ECS::Component::UIRectTransform, "UIRectTransform")
NE_COMPONENT_KEY(NE::ECS::Component::UICanvas, "UICanvas")
NE_COMPONENT_KEY(NE::ECS::Component::UIImage, "UIImage")
NE_COMPONENT_KEY(NE::ECS::Component::PrefabLink, "PrefabLink")
NE_COMPONENT_KEY(NE::ECS::Component::PrefabInstance, "PrefabInstance")
NE_COMPONENT_KEY(NE::ECS::Component::CharacterController, "CharacterController")
NE_COMPONENT_KEY(NE::ECS::Component::Animator, "Animator")