#ifndef MODEL_IMPORT_SETTINGS_HPP
#define MODEL_IMPORT_SETTINGS_HPP

#include <cstdint>
#include <Core/Reflection.hpp>

namespace Editor {

    inline constexpr int MODEL_IMPORTER_VERSION = 1;

    // ---------------- Scene / File level ----------------
    struct SceneImportSettings {
        float scaleFactor = 1.0f;
        bool convertUnits = true;
        bool importBlendShapes = true;
        bool importCameras = false;
        bool importLights = false;
        bool preserveHierarchy = true;

        NE_REFLECT_BEGIN(SceneImportSettings)
            NE_REFLECT_FIELD(scaleFactor),
            NE_REFLECT_FIELD(convertUnits),
            NE_REFLECT_FIELD(importCameras),
            NE_REFLECT_FIELD(importLights),
            NE_REFLECT_FIELD(preserveHierarchy)
        NE_REFLECT_END()

    };

    // ---------------- Mesh / Geometry level ----------------
    struct MeshImportSettings {

        enum class NormalMode : uint8_t {
            Import,
            Calculate,
            None
        };

        enum class TangentMode : uint8_t {
            Import,
            CalculateMikktspace,
            None
        };

        enum class IndexFormat : uint8_t {
            Auto,
            UInt16,
            UInt32
        };

        enum class MeshOptimizationMode : uint8_t {
            None,
            Everything,
            PolygonOrder,
            VertexOrder
        };

        // Meshes
        MeshOptimizationMode meshOptimizationMode = MeshOptimizationMode::Everything;
        bool generateColliders = false;

        // Mesh LODS
        bool generateMeshLODs = false;

        // Geometry
        bool keepQuads = false;
        bool weldVertices = false;
        IndexFormat indexFormat = IndexFormat::Auto;
        NormalMode  normalMode = NormalMode::Import;
        float smoothingAngle = 60.f;
        TangentMode tangentMode = TangentMode::Import;
        bool swapUVs;

        NE_REFLECT_BEGIN(MeshImportSettings)
            NE_REFLECT_FIELD(weldVertices),
            NE_REFLECT_FIELD(keepQuads),
            NE_REFLECT_FIELD(swapUVs),
            NE_REFLECT_FIELD(generateColliders),
            NE_REFLECT_FIELD(generateMeshLODs),
            NE_REFLECT_FIELD(smoothingAngle),
            NE_REFLECT_FIELD(normalMode),
            NE_REFLECT_FIELD(tangentMode),
            NE_REFLECT_FIELD(indexFormat),
            NE_REFLECT_FIELD(meshOptimizationMode)
        NE_REFLECT_END()
    };

    // ---------------- Rig / Skeleton level ----------------
    struct RigImportSettings {

        enum class AnimationType : uint8_t {
            None,
            Generic,
            Humanoid
        };

        AnimationType animationType = AnimationType::None;
        bool stripBones;

        NE_REFLECT_BEGIN(RigImportSettings)
            NE_REFLECT_FIELD(stripBones),
            NE_REFLECT_FIELD(animationType)
        NE_REFLECT_END()
    };

    // ---------------- Animation level ----------------
    struct AnimationImportSettings {
        bool importAnimations = true;
        bool importConstraints = false;
        bool importAnimatedCustomProperties = false;
        bool autoSplitClips = true;

        float sampleRate = 0.0f;

        bool importRootMotion = false;
        bool lockRootPositionXZ = false;
        bool lockRootRotationY = false;

        NE_REFLECT_BEGIN(AnimationImportSettings)
            NE_REFLECT_FIELD(importAnimations),
            NE_REFLECT_FIELD(importConstraints),
            NE_REFLECT_FIELD(importAnimatedCustomProperties),
            NE_REFLECT_FIELD(autoSplitClips),
            NE_REFLECT_FIELD(importRootMotion),
            NE_REFLECT_FIELD(lockRootPositionXZ),
            NE_REFLECT_FIELD(lockRootRotationY),
            NE_REFLECT_FIELD(sampleRate)
        NE_REFLECT_END()
    };

    // ---------------- Material level ----------------
    struct MaterialImportSettings {
        bool importMaterials = true;

        bool tryReuseExistingMaterials = true;

        enum class MaterialCreationMode : uint8_t {
            PerSubmesh,
            PerMesh,
            PerFile
        };

        MaterialCreationMode creationMode = MaterialCreationMode::PerSubmesh;

        NE_REFLECT_BEGIN(MaterialImportSettings)
            NE_REFLECT_FIELD(importMaterials),
            NE_REFLECT_FIELD(tryReuseExistingMaterials),
            NE_REFLECT_FIELD(creationMode)
        NE_REFLECT_END()
    };

    // ---------------- Top-level Model (file) settings ----------------
    struct ModelImportSettings {
        SceneImportSettings     scene;
        MeshImportSettings      mesh;
        RigImportSettings       rig;
        AnimationImportSettings animation;
        MaterialImportSettings  material;

        NE_REFLECT_BEGIN(ModelImportSettings)
            NE_REFLECT_FIELD(scene),
            NE_REFLECT_FIELD(mesh),
            NE_REFLECT_FIELD(rig),
            NE_REFLECT_FIELD(animation),
            NE_REFLECT_FIELD(material)
        NE_REFLECT_END()
    };

} // namespace Editor

#endif // MODEL_IMPORT_SETTINGS_HPP
