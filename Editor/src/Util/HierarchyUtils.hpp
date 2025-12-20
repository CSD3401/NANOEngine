#pragma once
#include <cstdint>
#include <unordered_set>

#include <EditorInterface/ECSExports.hpp>

namespace Editor::Utility {
    inline bool IsDescendantOfSelected(uint32_t e,
        const std::unordered_set<uint32_t>& selected)
    {
        uint32_t p = NE::ECS::Query::GetParent(e);
        while (p != NE::ECS::NO_ENTITY) {
            if (selected.contains(p)) return true;
            p = NE::ECS::Query::GetParent(p);
        }
        return false;
    }
}
