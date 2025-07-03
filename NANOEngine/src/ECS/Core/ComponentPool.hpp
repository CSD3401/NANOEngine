#pragma once
#include "SparseSet.hpp"
#include <unordered_map>
#include <cassert>

namespace NANOEngine::ECS {

    template<typename T, typename Entity, std::size_t MaxEntities>
    class ComponentPool {
    public:
        void Insert(Entity e, const T& component) {
            if (!m_sparseSet.Contains(e)) {
                m_sparseSet.Insert(e);
                if (e >= m_componentData.size())
                    m_componentData.resize(MaxEntities); // optional for sparse direct-index
            }
            m_componentData[e] = component;
        }

        void Remove(Entity e) {
            //assert(m_sparseSet.Contains(e));
            if (!m_sparseSet.Contains(e)) return;
            m_sparseSet.Remove(e);
            // Optionally reset component data to default or nullptr
        }

        bool Has(Entity e) const {
            return m_sparseSet.Contains(e);
        }

        T& Get(Entity e) {
            assert(Has(e));
            return m_componentData[e];
        }

        const T& Get(Entity e) const {
            assert(Has(e));
            return m_componentData[e];
        }

        const std::vector<Entity>& Entities() const {
            return m_sparseSet.GetDenseContainer();
        }

    private:
        SparseSet<Entity, MaxEntities> m_sparseSet;
        std::vector<T> m_componentData; // Indexed directly by Entity ID (flat)
    };

}