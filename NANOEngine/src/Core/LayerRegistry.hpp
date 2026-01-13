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

	class NANOENGINE_API LayerRegistry {
	public:
		static LayerRegistry& GetInstance();

		LayerID CreateLayer(std::string_view name);
		bool RenameLayer(LayerID id, std::string_view newName);

		bool SetCollision(LayerID a, LayerID b, bool enabled);
		bool GetCollision(LayerID a, LayerID b) const;
		LayerMask GetCollisionMask(LayerID a) const;
		bool SetCollisionMask(LayerID a, LayerMask mask);

		bool IsUsed(LayerID id) const noexcept;
		std::string_view GetName(LayerID id) const;
		LayerID FindByName(std::string_view name) const;

		template<typename Fn>
		void ForEachUsed(Fn&& fn) const {
			for (LayerID i = 0; i < MAX_LAYERS; ++i) {
				if (!m_data.slots[i].used) continue;
				if (m_data.slots[i].name.empty()) continue;
				fn(i, std::string_view(m_data.slots[i].name));
			}
		}

		const std::array<LayerMask, MAX_LAYERS>& GetCollisionMatrix() const noexcept;

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