#pragma once

namespace NE::ECS::Component {
    struct EntityMeta;
    struct Transform;
    struct Renderer;
    struct Light;
}


template <typename T> struct ComponentKey;

#define NE_COMPONENT_KEY(Type, KeyLiteral) \
    template <> struct ComponentKey<Type> { static constexpr const char* value = KeyLiteral; };


NE_COMPONENT_KEY(NE::ECS::Component::EntityMeta, "EntityMeta")
NE_COMPONENT_KEY(NE::ECS::Component::Transform, "Transform")
NE_COMPONENT_KEY(NE::ECS::Component::Renderer, "Renderer")
NE_COMPONENT_KEY(NE::ECS::Component::Light, "Light")

