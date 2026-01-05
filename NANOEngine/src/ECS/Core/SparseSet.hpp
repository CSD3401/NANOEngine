#pragma once
#include <vector>
#include <array>
#include <cassert>

namespace NE::ECS {

    template<typename Entity, std::size_t MaxEntities>
    class SparseSet {
    public:
        void Insert(Entity e) {
            assert(e < MaxEntities && "Entity out of range");
            if (Contains(e)) return;
            m_sparseContainer[e] = static_cast<Entity>(m_denseContainer.size());
            m_denseContainer.push_back(e);
        }

        void Remove(Entity e) {
            // Safety checks
            if (e >= MaxEntities) return;  // Entity out of range
            if (!Contains(e)) return;      // Entity not in set
            
            // Additional safety: check if dense container is empty
            if (m_denseContainer.empty()) return;
            
            Entity index = m_sparseContainer[e];
            
            // Validate index is within bounds
            if (index >= m_denseContainer.size()) {
                // Invalid index - clear the sparse entry and return
                m_sparseContainer[e] = static_cast<Entity>(MaxEntities);  // Mark as invalid
                return;
            }
            
            Entity last = m_denseContainer.back();
            
            // Validate last entity is within range before using as index
            if (last >= MaxEntities) {
                // Invalid entity ID - just remove from dense container
                m_denseContainer.pop_back();
                m_sparseContainer[e] = static_cast<Entity>(MaxEntities);  // Mark as invalid
                return;
            }
            
            // Swap and remove
            m_denseContainer[index] = last;
            m_sparseContainer[last] = index;
            m_denseContainer.pop_back();
            
            // Invalidate the removed entity's sparse entry
            m_sparseContainer[e] = static_cast<Entity>(MaxEntities);
        }

        bool Contains(Entity e) const {
            if (e >= MaxEntities) return false;
            return m_sparseContainer[e] < m_denseContainer.size() && m_denseContainer[m_sparseContainer[e]] == e;
        }

        std::vector<Entity>& GetDenseContainer() { return m_denseContainer; }
        const std::vector<Entity>& GetDenseContainer() const { return m_denseContainer; }

    private:
        std::vector<Entity> m_denseContainer;
        std::array<Entity, MaxEntities> m_sparseContainer{};
    };

}
