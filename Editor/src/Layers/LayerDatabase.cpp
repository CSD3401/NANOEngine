#include "LayerDatabase.hpp"

namespace Editor::Layers {

    LayerDatabase::LayerDatabase()
        : m_registry(NE::Core::LayerRegistry::GetInstance()) {
    }

    NE::Core::LayerID LayerDatabase::CreateLayer(std::string_view name) {
        return m_registry.CreateLayer(name);
    }

    bool LayerDatabase::RenameLayer(NE::Core::LayerID id, std::string_view newName) {
        return m_registry.RenameLayer(id, newName);
    }

    bool LayerDatabase::SetCollision(NE::Core::LayerID a, NE::Core::LayerID b, bool enabled) {
        return m_registry.SetCollision(a, b, enabled);
    }

    bool LayerDatabase::GetCollision(NE::Core::LayerID a, NE::Core::LayerID b) const {
        return m_registry.GetCollision(a, b);
    }

    NE::Core::LayerMask LayerDatabase::GetCollisionMask(NE::Core::LayerID a) const {
        return m_registry.GetCollisionMask(a);
    }

    bool LayerDatabase::SetCollisionMask(NE::Core::LayerID a, NE::Core::LayerMask mask) {
        return m_registry.SetCollisionMask(a, mask);
    }

    bool LayerDatabase::IsUsed(NE::Core::LayerID id) const noexcept {
        return m_registry.IsUsed(id);
    }

    std::string_view LayerDatabase::GetName(NE::Core::LayerID id) const {
        return m_registry.GetName(id);
    }

    NE::Core::LayerID LayerDatabase::FindByName(std::string_view name) const {
        return m_registry.FindByName(name);
    }
}
