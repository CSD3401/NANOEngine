#include "EditorScene.hpp"
#include <algorithm>
#include <ECS/Core/Entity.hpp>
#include <EditorInterface/ECSExports.hpp>

namespace {
    void RemoveFromVec(std::vector<uint32_t>& v, uint32_t id) {
        auto it = std::find(v.begin(), v.end(), id);
        if (it != v.end()) v.erase(it);
    }
}

namespace Editor {

    std::vector<EditorEntity> EditorScene::s_entities;
    EditorEntity* EditorScene::s_selectedEntity;

    //std::string EditorScene::selectedMaterial;
    std::string EditorScene::selectedAsset;

    std::string EditorScene::currentScenePath("Assets/NewScene.scene");

    std::unordered_map<uint32_t, Node> EditorScene::s_nodes;
    std::unordered_map<uint32_t, std::vector<uint32_t>> EditorScene::s_children;
    std::vector<uint32_t> EditorScene::s_roots;

    //void EditorScene::BuildFlatHierarchy() {
    //    s_nodes.clear();
    //    s_children.clear();
    //    s_roots.clear();

    //    s_roots.reserve(s_entities.size());
    //    float k = 0.f;
    //    for (const auto& e : s_entities) {                    // s_entities exists already
    //        uint32_t id = e.linkedEntity;                    // (see your struct) 
    //        s_nodes[id] = Node{ id, NE::ECS::NO_ENTITY, k };                 // everyone root; keys 0..N-1
    //        s_roots.push_back(id);
    //        k += 1.f;
    //    }
    //}

    static void RenormalizeKeys(std::vector<uint32_t>& ids, uint32_t) {
        float k = 0.f;
        for (auto id : ids) {
            auto it = EditorScene::s_nodes.find(id);
            if (it != EditorScene::s_nodes.end()) it->second.orderKey = k, k += 1.f;
        }
    }

    const std::vector<uint32_t>& EditorScene::ChildrenOf(uint32_t parent) {
        if (parent == NE::ECS::NO_ENTITY) return s_roots;
        return s_children[parent];
    }

    // Reorder within siblings by updating the dragged row's orderKey (constant work).
    bool EditorScene::ReorderWithinSiblings(uint32_t parent, uint32_t child, int insertIndex) {
        auto& vec = (parent == NE::ECS::NO_ENTITY) ? s_roots : s_children[parent];
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

    bool EditorScene::AttachAsChild(uint32_t newParent, uint32_t child, int insertIndex) {
        if (child == NE::ECS::NO_ENTITY || newParent == child) return false;
        // Ensure nodes exist
        auto itChild = s_nodes.find(child);
        if (itChild == s_nodes.end()) return false;
        if (newParent != NE::ECS::NO_ENTITY && s_nodes.find(newParent) == s_nodes.end()) return false;

        // Remove from old siblings (roots or old parent's children)
        uint32_t oldParent = itChild->second.parent;
        auto& oldVec = (oldParent == NE::ECS::NO_ENTITY) ? s_roots : s_children[oldParent];
        RemoveFromVec(oldVec, child);

        // Update parent
        itChild->second.parent = newParent;

        // Insert into new parent's vector
        auto& newVec = (newParent == NE::ECS::NO_ENTITY) ? s_roots : s_children[newParent];
        if (insertIndex < 0 || insertIndex >(int)newVec.size()) insertIndex = (int)newVec.size();

        // Compute an orderKey between neighbors (same idea as ReorderWithinSiblings)
        float prevK = (insertIndex - 1 >= 0 && insertIndex - 1 < (int)newVec.size())
            ? s_nodes[newVec[insertIndex - 1]].orderKey
            : std::floor(itChild->second.orderKey) - 1.0f;
        float nextK = (insertIndex < (int)newVec.size())
            ? s_nodes[newVec[insertIndex]].orderKey
            : std::ceil(itChild->second.orderKey) + 1.0f;

        float newK = 0.5f * (prevK + nextK);
        newVec.insert(newVec.begin() + insertIndex, child);

        if (!(newK > prevK && newK < nextK)) {
            // keys collapsed: renormalize this sibling list
            RenormalizeKeys(newVec, newParent);
        } else {
            itChild->second.orderKey = newK;
        }
        NE::ECS::Command::SetParent(child, newParent, /*worldPositionStays=*/true);
        return true;
    }

    bool EditorScene::UnparentToRoot(uint32_t child, int insertIndex) {
        NE::ECS::Command::SetParent(child, NE::ECS::NO_ENTITY);
        return AttachAsChild(NE::ECS::NO_ENTITY, child, insertIndex);
    }

    void EditorScene::GetAllDescendants(uint32_t id, std::vector<uint32_t>& out) {
        out.push_back(id);

        auto it = s_children.find(id);
        if (it != s_children.end()) {
            for (uint32_t child : it->second) {
                GetAllDescendants(child, out);
            }
        }
    }

    void EditorScene::BuildHierarchyFromECS() {
        s_nodes.clear();
        s_children.clear();
        s_roots.clear();

        s_roots.reserve(s_entities.size());

        // --- First pass: create nodes with parent taken from ECS ---
        for (const auto& e : s_entities) {
            uint32_t id = e.linkedEntity;

            // Ask ECS what the parent is (this should reflect Transform.parent)
            uint32_t parent = NE::ECS::Command::GetParent(id); // NO_ENTITY if root

            Node node;
            node.id = id;
            node.parent = parent;  // NE::ECS::NO_ENTITY means root for us too
            node.orderKey = 0.0f;  // we'll fill this in per-sibling

            s_nodes[id] = node;
        }

        // --- Second pass: fill children and roots ---
        for (auto& [id, node] : s_nodes) {
            if (node.parent == NE::ECS::NO_ENTITY) {
                s_roots.push_back(id);
            } else {
                s_children[node.parent].push_back(id);
            }
        }

        // --- Third pass: assign simple orderKey for each sibling list ---
        {
            // roots
            float k = 0.f;
            for (uint32_t id : s_roots) {
                s_nodes[id].orderKey = k;
                k += 1.f;
            }

            // children
            for (auto& [parent, vec] : s_children) {
                float kk = 0.f;
                for (uint32_t id : vec) {
                    s_nodes[id].orderKey = kk;
                    kk += 1.f;
                }
            }
        }
    }
}