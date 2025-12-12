#pragma once
#include <vector>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include "Graphics/Core/EditorCamera.hpp"
#include "EditorSelection.hpp"

namespace Editor {
    struct EditorScene {
    public:
        static std::vector<NE::ECS::Entity> s_rootOrder;
        static EditorSelection s_selection;
        static std::string selectedAsset;
        static std::string currentScenePath;
        static std::string selectedPrefab;

        static NE::Graphics::EditorCamera m_editorCamera;

        static std::vector<uint8_t> clipboard;

        static void RegisterRoot(NE::ECS::Entity e);
        static void UnregisterRoot(NE::ECS::Entity e);
        static void ReorderRoot(NE::ECS::Entity e, int newIndex);

        static void SetParent(NE::ECS::Entity child, NE::ECS::Entity newParent, int insertIndex, bool keepWorld = true);
        static void OnParentChanged(NE::ECS::Entity e, NE::ECS::Entity oldParent, NE::ECS::Entity newParent);
    };

}
