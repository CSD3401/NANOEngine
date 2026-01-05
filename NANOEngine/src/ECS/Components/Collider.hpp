#ifndef COLLIDER_HPP
#define COLLIDER_HPP

#include <variant>
#include <cstdint>

#include "Math/Vec3.hpp"
#include "Core/Reflection.hpp"

namespace NE::ECS::Component {
	struct Collider {
		struct NoColliderData {
			NE_REFLECT_BEGIN(NoColliderData)
				NE_REFLECT_END()
		};

		struct BoxColliderData {
			Math::Vec3 halfExtents{ 0.5f, 0.5f, 0.5f };
			NE_REFLECT_BEGIN(BoxColliderData)
				NE_REFLECT_FIELD(halfExtents)
				NE_REFLECT_END()
		};

		struct SphereColliderData {
			float radius{ 0.5f };
			NE_REFLECT_BEGIN(SphereColliderData)
				NE_REFLECT_FIELD(radius)
				NE_REFLECT_END()
		};

		struct CapsuleColliderData {
			float radius{ 0.5f };
			float height{ 1.0f };
			NE_REFLECT_BEGIN(CapsuleColliderData)
				NE_REFLECT_FIELD(radius),
				NE_REFLECT_FIELD(height)
				NE_REFLECT_END()
		};

		struct CylinderColliderData {
			float radius{ 0.5f };
			float height{ 1.0f };
			NE_REFLECT_BEGIN(CylinderColliderData)
				NE_REFLECT_FIELD(radius),
				NE_REFLECT_FIELD(height)
				NE_REFLECT_END()
		};

		struct MeshColliderData {
			NE_REFLECT_BEGIN(MeshColliderData)
				NE_REFLECT_END()
		};

		using ColliderData = std::variant<
			NoColliderData,
			BoxColliderData,
			SphereColliderData,
			CapsuleColliderData,
			CylinderColliderData,
			MeshColliderData
		>;

		enum class ColliderType : uint8_t {
			None,
			Box,
			Sphere,
			Capsule,
			Cylinder,
			Mesh
		};

		ColliderData data;
		Math::Vec3 center{ 0.f, 0.f, 0.f };
		uint64_t luid;
		ColliderType type{ ColliderType::None };
		bool isTrigger = false;

		bool isDirty = false;

		NE_REFLECT_BEGIN(Collider)
			NE_REFLECT_FIELD_HIDDEN(type), // Hidden due to no enum reflection support but for disk serialization
			NE_REFLECT_FIELD(isTrigger),
			NE_REFLECT_FIELD(center),
			NE_REFLECT_FIELD_HIDDEN(data), // Hidden due to no std::variant reflection support but for disk serialization
			NE_REFLECT_FIELD_HIDDEN(luid)
			NE_REFLECT_END()
	};
}

#endif