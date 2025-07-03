#pragma once
#include <vector>
#include <array>
#include <cassert>

namespace NANOEngine::ECS {

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
            assert(Contains(e));
            Entity index = m_sparseContainer[e];
            Entity last = m_denseContainer.back();
            m_denseContainer[index] = last;
            m_sparseContainer[last] = index;
            m_denseContainer.pop_back();
        }

        bool Contains(Entity e) const {
            return m_sparseContainer[e] < m_denseContainer.size() && m_denseContainer[m_sparseContainer[e]] == e;
        }

        std::vector<Entity>& GetDenseContainer() { return m_denseContainer; }
        const std::vector<Entity>& GetDenseContainer() const { return m_denseContainer; }

    private:
        std::vector<Entity> m_denseContainer;
        std::array<Entity, MaxEntities> m_sparseContainer{};
    };

}
