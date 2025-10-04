namespace NE::ECS::Component {
    struct EntityMeta;
    struct Transform;
    struct Renderer;
    struct Light;
    struct Collider;
    struct Rigidbody;
}


template <typename T> struct ComponentKey;

#define NE_COMPONENT_KEY(Type, KeyLiteral) \
    template <> struct ComponentKey<Type> { static constexpr const char* value = KeyLiteral; };


NE_COMPONENT_KEY(NE::ECS::Component::EntityMeta, "EntityMeta")
NE_COMPONENT_KEY(NE::ECS::Component::Transform, "Transform")
NE_COMPONENT_KEY(NE::ECS::Component::Renderer, "Renderer")
NE_COMPONENT_KEY(NE::ECS::Component::Light, "Light")
NE_COMPONENT_KEY(NE::ECS::Component::Collider, "Collider")
NE_COMPONENT_KEY(NE::ECS::Component::Rigidbody, "Rigidbody")

