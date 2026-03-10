#pragma once
#include <array>
#include <string>
#include <string_view>
#include <unordered_map>

#include "NANOEngineAPI.hpp"
#include "Layers.hpp"

namespace NE::Core {
	struct LayerSlot {
		bool used = false;
		std::string name;
	};

	struct LayerRegistryData {
		std::array<LayerSlot, MAX_LAYERS> slots{};
		std::array<LayerMask, MAX_LAYERS> collideWith{};
	};

	class LayerRegistry {
	public:
		NANOENGINE_API static LayerRegistry& GetInstance();

		NANOENGINE_API LayerID CreateLayer(std::string_view name);
		NANOENGINE_API bool RenameLayer(LayerID id, std::string_view newName);

		NANOENGINE_API bool SetCollision(LayerID a, LayerID b, bool enabled);
		NANOENGINE_API bool GetCollision(LayerID a, LayerID b) const;
		NANOENGINE_API LayerMask GetCollisionMask(LayerID a) const;
		NANOENGINE_API bool SetCollisionMask(LayerID a, LayerMask mask);

		NANOENGINE_API bool IsUsed(LayerID id) const noexcept;
		NANOENGINE_API std::string_view GetName(LayerID id) const;
		NANOENGINE_API LayerID FindByName(std::string_view name) const;

		template<typename Fn>
		void ForEachUsed(Fn&& fn) const {
			for (LayerID i = 0; i < MAX_LAYERS; ++i) {
				if (!m_data.slots[i].used) continue;
				if (m_data.slots[i].name.empty()) continue;
				fn(i, std::string_view(m_data.slots[i].name));
			}
		}

		NANOENGINE_API const std::array<LayerMask, MAX_LAYERS>& GetCollisionMatrix() const noexcept;

	private:
		LayerRegistry();

		LayerRegistryData m_data{};
		std::unordered_map<std::string, LayerID> m_nameToId;

		void RebuildNameMap();
		static std::string Normalize(std::string_view s);
		LayerID FindFreeSlot() const;
		bool IsValidLayerId(LayerID id) const noexcept;
	};
}