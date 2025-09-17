#include "EditorScene.hpp"
#include <algorithm>

namespace Editor {

    std::vector<EditorEntity> EditorScene::s_entities;
    EditorEntity* EditorScene::s_selectedEntity;

    std::string EditorScene::selectedMaterial;

    std::string EditorScene::currentScenePath("Assets/NewScene.scene");

    std::unordered_map<uint32_t, Node> EditorScene::s_nodes;
    std::unordered_map<uint32_t, std::vector<uint32_t>> EditorScene::s_children;
    std::vector<uint32_t> EditorScene::s_roots;

    void EditorScene::BuildFlatHierarchy() {
        s_nodes.clear();
        s_children.clear();
        s_roots.clear();

        s_roots.reserve(s_entities.size());
        float k = 0.f;
        for (const auto& e : s_entities) {                    // s_entities exists already
            uint32_t id = e.linkedEntity;                    // (see your struct) 
            s_nodes[id] = Node{ id, 0u, k };                 // everyone root; keys 0..N-1
            s_roots.push_back(id);
            k += 1.f;
        }
    }

    static void RenormalizeKeys(std::vector<uint32_t>& ids, uint32_t) {
        float k = 0.f;
        for (auto id : ids) {
            auto it = EditorScene::s_nodes.find(id);
            if (it != EditorScene::s_nodes.end()) it->second.orderKey = k, k += 1.f;
        }
    }

    const std::vector<uint32_t>& EditorScene::ChildrenOf(uint32_t parent) {
        if (parent == 0) return s_roots;
        return s_children[parent];
    }

    // Reorder within siblings by updating the dragged row's orderKey (constant work).
    bool EditorScene::ReorderWithinSiblings(uint32_t parent, uint32_t child, int insertIndex) {
        auto& vec = (parent == 0) ? s_roots : s_children[parent];
        if (vec.empty()) return false;

        // remove 'child' from vec if present
        int from = -1;
        for (int i = 0; i < (int)vec.size(); ++i) if (vec[i] == child) { from = i; break; }
        if (from >= 0) {
            vec.erase(vec.begin() + from);
            if (insertIndex > from) insertIndex -= 1; // account for removal
        }

        insertIndex = std::clamp(insertIndex, 0, (int)vec.size());
        // Determine neighbor keys
        float prevK = (insertIndex - 1 >= 0) ? s_nodes[vec[insertIndex - 1]].orderKey : std::floor(s_nodes[child].orderKey) - 1.0f;
        float nextK = (insertIndex < (int)vec.size()) ? s_nodes[vec[insertIndex]].orderKey : std::ceil(s_nodes[child].orderKey) + 1.0f;

        float newK = 0.5f * (prevK + nextK);
        if (!(newK > prevK && newK < nextK)) { // keys collapsed; renormalize
            vec.insert(vec.begin() + insertIndex, child);
            RenormalizeKeys(vec, parent);
            return true;
        }

        s_nodes[child].orderKey = newK;
        // Reinsert by key order (keep vec roughly sorted for stable rendering)
        vec.insert(vec.begin() + insertIndex, child);
        return true;
    }
}