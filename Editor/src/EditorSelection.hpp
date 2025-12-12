#pragma once

#include <vector>
#include <functional>
#include <ECS/Core/Entity.hpp>

namespace Editor {

    class EditorSelection {
    public:
        explicit EditorSelection() = default;

        const std::vector<NE::ECS::Entity>& GetSelection() const;
        NE::ECS::Entity GetPrimary()    const;
        NE::ECS::Entity GetLastClicked() const;

        bool Contains(NE::ECS::Entity e) const;
        bool Empty() const;

        std::vector<NE::ECS::Entity> GetTopLevelSelection(
            const std::function<NE::ECS::Entity(NE::ECS::Entity)>& getParent) const;

        void Clear();

        void SetSingle(NE::ECS::Entity e);

        void Toggle(NE::ECS::Entity e);

        void Add(NE::ECS::Entity e);

        void RangeSelect(NE::ECS::Entity anchor,
            NE::ECS::Entity e,
            const std::vector<NE::ECS::Entity>& preorder);

        void SetLastPreorder(const std::vector<NE::ECS::Entity>& preorder);
        const std::vector<NE::ECS::Entity>& GetLastPreorder() const;

        void Remove(NE::ECS::Entity e);
        void RemoveIf(const std::function<bool(NE::ECS::Entity)>& pred);
    private:
        std::vector<NE::ECS::Entity>    m_selection;
        NE::ECS::Entity m_primary =     NE::ECS::NO_ENTITY; // the active one (Inspector)
        NE::ECS::Entity m_lastClicked = NE::ECS::NO_ENTITY; // for Shift-range

        std::vector<NE::ECS::Entity> m_lastPreorder;
    };

}
