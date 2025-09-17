#pragma once
#include <vector>
#include <string>
#include <unordered_map>
#include "EditorEntity.hpp"

namespace Editor {

    struct Node {
        uint32_t id = 0;        // editor id == linkedEntity
        uint32_t parent = 0;    // 0 => root
        float    orderKey = 0;  // sibling order
    };

    struct EditorScene {
    public:
        static std::vector<EditorEntity> s_entities;
        static EditorEntity* s_selectedEntity;
        static std::string selectedMaterial;
        static std::string currentScenePath;

        // NEW: hierarchy index
        static std::unordered_map<uint32_t, Node> s_nodes;                // id -> node
        static std::unordered_map<uint32_t, std::vector<uint32_t>> s_children; // parent -> children ids
        static std::vector<uint32_t> s_roots;

        // Build a flat tree (everyone root) from s_entities; stable keys 0..N-1
        static void BuildFlatHierarchy();

        // Sibling-order ops -------------------------------------------------------
        // Moves 'child' to appear at 'insertIndex' among children of 'parent'.
        // Returns true if order changed.
        static bool ReorderWithinSiblings(uint32_t parent, uint32_t child, int insertIndex);

        // Helpers
        static const std::vector<uint32_t>& ChildrenOf(uint32_t parent);
    };

}
