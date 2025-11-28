#pragma once
#include <vector>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include "EditorEntity.hpp"
#include "Graphics/Core/EditorCamera.hpp"

namespace Editor {

    //const std::string

    struct Node {
        uint32_t id = 0;        // editor id == linkedEntity
        uint32_t parent = 0;    // 0 => root
        float    orderKey = 0;  // sibling order
    };

    struct EditorScene {
    public:
        static std::vector<EditorEntity> s_entities; // to be removed soon
        static EditorEntity* s_selectedEntity;
        static std::string selectedAsset;
        static std::string currentScenePath;
        static std::string selectedPrefab;

        static NE::Graphics::EditorCamera m_editorCamera;

        static std::vector<uint8_t> clipboard;

        // NEW: hierarchy index
        static std::unordered_map<uint32_t, Node> s_nodes;                // id -> node
        static std::unordered_map<uint32_t, std::vector<uint32_t>> s_children; // parent -> children ids
        static std::vector<uint32_t> s_roots;
        static std::unordered_set<uint32_t> s_forceOpen;

        static bool ReorderWithinSiblings(uint32_t parent, uint32_t child, int insertIndex);
        static bool AttachAsChild(uint32_t newParent, uint32_t child, int insertIndex);
        static bool UnparentToRoot(uint32_t child, int insertIndex = -1);
        static void BuildHierarchyFromECS();
        static void GetAllDescendants(uint32_t id, std::vector<uint32_t>& out);
        static void SetAllDescendantsActive(uint32_t id, bool& active);
        // Helpers
        static void RebuildFromActiveScene();
        static const std::vector<uint32_t>& ChildrenOf(uint32_t parent);


        static void DuplicateSelected();
        static void CopySelected();
        static void PasteSelected();
        static void ForceOpenParents(uint32_t child);
    };

}
