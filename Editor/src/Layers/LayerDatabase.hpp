#pragma once

#include "LayerDefs.hpp"
#include <unordered_map>
#include <string>

namespace Editor::Layers {
	class LayerDatabase {
	public:
		static constexpr size_t MaxLayers = NE::Core::MAX_LAYERS;

		LayerDatabase();

		NE::Core::LayerID CreateLayer(std::string_view name);
		bool RenameLayer(NE::Core::LayerID id, std::string_view newName);

		bool SetCollision(NE::Core::LayerID a, NE::Core::LayerID b, bool enabled);
		bool GetCollision(NE::Core::LayerID a, NE::Core::LayerID b) const;
		NE::Core::LayerMask GetCollisionMask(NE::Core::LayerID a) const;
		bool SetCollisionMask(NE::Core::LayerID a, NE::Core::LayerMask mask);

		bool IsUsed(NE::Core::LayerID id) const noexcept;
		std::string_view GetName(NE::Core::LayerID id) const;
		NE::Core::LayerID FindByName(std::string_view name) const;

		template<typename Fn>
		void ForEachUsed(Fn&& fn) const {
			for (NE::Core::LayerID i = 0; i < NE::Core::MAX_LAYERS; ++i) {
				if (!m_data.slots[i].used) continue;
				if (m_data.slots[i].name.empty()) continue;
				fn(i, std::string_view(m_data.slots[i].name));
			}
		}
	private:
		LayerDatabaseData m_data{};
		std::unordered_map<std::string, NE::Core::LayerID> m_nameToId;

		void RebuildNameMap();
		static std::string Normalize(std::string_view s);
		NE::Core::LayerID FindFreeSlot() const;
		bool IsValidLayerId(NE::Core::LayerID id) const noexcept;
	};
}


