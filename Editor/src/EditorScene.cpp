#include "EditorScene.hpp"
#include <algorithm>
#include <EditorInterface/ECSExports.hpp>
#include <ECS/Components/Hierarchy.hpp>

namespace Editor {
    std::vector<NE::ECS::Entity> EditorScene::s_rootOrder;
    EditorSelection EditorScene::s_selection;
    std::string EditorScene::s_currentSceneUUID("Assets/NewScene.scene");
    std::string EditorScene::s_currentScenePath; // temp
    Layers::LayerDatabase EditorScene::layerDatabase;

    std::string EditorScene::selectedAsset;
    std::string EditorScene::selectedPrefab;
    std::vector<uint8_t> EditorScene::clipboard;

    NE::Graphics::EditorCamera EditorScene::m_editorCamera;
    float EditorScene::m_cameraSpeed = 1.0f;
    bool EditorScene::m_cameraUseEasing = false;
    bool EditorScene::m_cameraUseAcceleration = false;
    float EditorScene::m_cameraMinSpeed = 0.01f;
    float EditorScene::m_cameraMaxSpeed = 2.f;
    float EditorScene::m_cameraYaw = -90.0f;  // looking along -Z
    float EditorScene::m_cameraPitch = 0.0f;

    bool EditorScene::isDirty = false;

    void EditorScene::BuildRoot() {
        s_rootOrder.clear();

        s_rootOrder.reserve(64);
        auto& numEntities = NE::GetNumEntities();
        for (auto e : numEntities) {
            auto& h = NE::ECS::Query::GetEntityHierarchy(e);
            if (h.parent == NE::ECS::Component::INVALID_PARENT) {
                s_rootOrder.push_back(e);
            }
        }
    }

    void EditorScene::RegisterRoot(NE::ECS::Entity e) {
        if (e == NE::ECS::NO_ENTITY) return;

        if (std::find(s_rootOrder.begin(), s_rootOrder.end(), e) == s_rootOrder.end()) {
            s_rootOrder.push_back(e);
        }
    }

    void EditorScene::UnregisterRoot(NE::ECS::Entity e) {
        auto it = std::find(s_rootOrder.begin(), s_rootOrder.end(), e);
        if (it != s_rootOrder.end()) {
            s_rootOrder.erase(it);
        }
    }

    void EditorScene::ReorderRoot(NE::ECS::Entity e, int newIndex) {
        auto it = std::find(s_rootOrder.begin(), s_rootOrder.end(), e);
        if (it == s_rootOrder.end()) {
            s_rootOrder.push_back(e);
            it = s_rootOrder.end() - 1;
        }

        newIndex = std::max(0, std::min(newIndex, static_cast<int>(s_rootOrder.size()) - 1));

        int currentIndex = static_cast<int>(std::distance(s_rootOrder.begin(), it));

        if (currentIndex == newIndex) return;

        NE::ECS::Entity entity = *it;
        s_rootOrder.erase(it);

        s_rootOrder.insert(s_rootOrder.begin() + newIndex, entity);
    }

    void EditorScene::SetParent(NE::ECS::Entity child, NE::ECS::Entity newParent, int insertIndex, bool keepWorld) {
        auto& h = NE::ECS::Query::GetEntityHierarchy(child);
        NE::ECS::Entity oldParent =
            (h.parent == NE::ECS::Component::INVALID_PARENT)
            ? NE::ECS::NO_ENTITY
            : static_cast<NE::ECS::Entity>(h.parent);

        NE::ECS::Command::SetParent(child, newParent, insertIndex, keepWorld);

        EditorScene::OnParentChanged(child, oldParent, newParent);
    }

    void EditorScene::OnParentChanged(NE::ECS::Entity e, NE::ECS::Entity oldParent, NE::ECS::Entity newParent) {
        bool wasRoot = oldParent == NE::ECS::NO_ENTITY;
        bool isNowRoot = newParent == NE::ECS::NO_ENTITY;

        if (wasRoot && !isNowRoot) {
            UnregisterRoot(e);
        } else if (!wasRoot && isNowRoot) {
            RegisterRoot(e);
        }
    }

    uint32_t EditorScene::GetParent(NE::ECS::Entity e) {
        auto& h = NE::ECS::Query::GetEntityHierarchy(e);
        return h.parent;
    }

    uint32_t EditorScene::GetIndexInParentOrRoot(NE::ECS::Entity e) {
        auto& h = NE::ECS::Query::GetEntityHierarchy(e);
        NE::ECS::Entity parent = (h.parent == NE::ECS::Component::INVALID_PARENT)
            ? NE::ECS::NO_ENTITY
            : static_cast<NE::ECS::Entity>(h.parent);
        if (parent == NE::ECS::NO_ENTITY) {
            auto it = std::find(s_rootOrder.begin(), s_rootOrder.end(), e);
            if (it != s_rootOrder.end()) {
                return static_cast<uint32_t>(std::distance(s_rootOrder.begin(), it));
            }
        } else {
            auto& parentHierarchy = NE::ECS::Query::GetEntityHierarchy(parent);
            auto& siblings = parentHierarchy.children;
            auto it = std::find(siblings.begin(), siblings.end(), e);
            if (it != siblings.end()) {
                return static_cast<uint32_t>(std::distance(siblings.begin(), it));
            }
        }
		return NE::ECS::NO_ENTITY;
    }

    void EditorScene::CopySelected() {
        if (s_selection.GetLastClicked() == NE::ECS::NO_ENTITY) return;

        clipboard = NE::CopyEntity(EditorScene::s_selection.GetLastClicked());
    }

    void EditorScene::PasteSelected() {
        if (clipboard.empty()) return;

        auto rootEntt = NE::PasteEntity(clipboard);
        RegisterRoot(rootEntt);
        s_selection.SetSingle(rootEntt);

        EditorScene::isDirty = true;
    }

    void EditorScene::DuplicateSelected() {
		auto rootEntt = NE::DuplicateEntity(s_selection.GetLastClicked());
        RegisterRoot(rootEntt);
        s_selection.SetSingle(rootEntt);

        EditorScene::isDirty = true;
    }
}