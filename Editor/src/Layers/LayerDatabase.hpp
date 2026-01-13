#pragma once
#include <Core/LayerRegistry.hpp>

namespace Editor::Layers {
	class LayerDatabase {
	public:
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
			m_registry.ForEachUsed(std::forward<Fn>(fn));
		}
	private:
		NE::Core::LayerRegistry& m_registry;
	};
}


