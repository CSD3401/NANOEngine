#include "Serializer.hpp"

#include <fstream>

#include "BinaryReflection.hpp"
#include "ECS/Core/ECSCoordinator.hpp"
#include "Graphics/Core/GraphicsManager.hpp"
// Components
#include "../ECS/Components/EntityMeta.hpp"
#include "../ECS/Components/Transform.hpp"
#include "../ECS/Components/Renderer.hpp"
#include "../ECS/Components/Light.hpp"
#include "../ECS/Components/Collider.hpp"
#include "../ECS/Components/Rigidbody.hpp"
#include "../ECS/Components/NativeScript.hpp"
#include "ECS/Components/Camera.hpp"
#include "../ECS/Components/UIRectTransform.hpp"
#include "../ECS/Components/UICanvas.hpp"
#include "../ECS/Components/UIImage.hpp"
#include "../ECS/Components/Hierarchy.hpp"

namespace NE {

    namespace {
        using ComponentTypes = std::tuple<
            ECS::Component::EntityMeta,
            ECS::Component::Transform,
            ECS::Component::Renderer,
            ECS::Component::Light,
            ECS::Component::Collider,
            ECS::Component::Rigidbody,
            ECS::Component::NativeScript,
            ECS::Component::Camera,
            ECS::Component::UIRectTransform,
            ECS::Component::UICanvas,
            ECS::Component::UIImage,
            ECS::Component::Hierarchy
        >;

        template <class F>
        void ForEachComponentType(F&& f) {
            std::apply([&](auto&&... t) {
                (f.template operator() < std::decay_t<decltype(t)> > (), ...);
                }, ComponentTypes{});
        }

        inline constexpr uint32_t NSCE_MAGIC = 0x4E534345;
        inline constexpr int CURRENT_NANOSCENE_FORMAT_VERSION = 1;
    }

	namespace Serialization {

		namespace {
            //constexpr uint32_t kComponentCount = (uint32_t)std::tuple_size_v<ComponentTypes>;

            using ComponentMask = std::uint64_t;

            void AppendPreorder(ECS::ECSCoordinator& ecs, ECS::Entity e, std::vector<ECS::Entity>& out) {
                out.push_back(e);
                auto& h = ecs.GetComponent<ECS::Component::Hierarchy>(e);
                for (auto childId : h.children)
                    AppendPreorder(ecs, childId, out);
            }

            void WriteOneEntity(ECS::ECSCoordinator& ecs, ByteBuffer& buf, ECS::Entity e) {
                ComponentMask mask = 0;
                uint32_t idx = 0;
                ForEachComponentType([&]<typename C>() {
                    if (ecs.HasComponent<C>(e))
                        mask |= (ComponentMask(1) << idx);
                    ++idx;
                });

                ToBinary(buf, (std::uint64_t)mask);

                idx = 0;
                ForEachComponentType([&]<typename C>() {
                    if (mask & (ComponentMask(1) << idx)) {
                        const auto& c = ecs.GetComponent<C>(e);
                        ToBinary(buf, c);
                    }
                    ++idx;
                });
            }
		}

		void SerializeScene(NE::ECS::ECSCoordinator& ecs, const std::vector<ECS::Entity>& rootNodes, const std::string& path) {
            std::filesystem::path filePath(path);
            std::filesystem::path directory = filePath.parent_path();

            if (!directory.empty() && !std::filesystem::exists(directory)) {
                std::filesystem::create_directories(directory);
            }

            NE::ByteBuffer buf;

            ToBinary(buf, static_cast<uint64_t>(NSCE_MAGIC));
            ToBinary(buf, static_cast<uint64_t>(CURRENT_NANOSCENE_FORMAT_VERSION));

            auto& renderSettings = Graphics::GraphicsManager::renderSettings;
            ToBinary(buf, renderSettings);

            auto& pp = Graphics::GraphicsManager::postProcessingSettings;
            ToBinary(buf, pp);

            std::vector<NE::ECS::Entity> flat;
            for (auto root : rootNodes)
                AppendPreorder(ecs, root, flat);

            ToBinary(buf, static_cast<uint64_t>(flat.size()));

            for (auto e : flat)
                WriteOneEntity(ecs, buf, e);

            std::ofstream out(path, std::ios::binary);
            if (!out) return;
            out.write(reinterpret_cast<const char*>(buf.data()),
                static_cast<std::streamsize>(buf.size()));
		}
	}

	namespace Deserialization {
        namespace {
            bool ReadAllBytes(const std::string& path, NE::ByteBuffer& out) {
                std::ifstream in(path, std::ios::binary | std::ios::ate);
                if (!in) return false;

                const std::streamsize size = in.tellg();
                if (size <= 0) return false;

                out.resize(static_cast<size_t>(size));
                in.seekg(0, std::ios::beg);
                return (bool)in.read(reinterpret_cast<char*>(out.data()), size);
            }

            template <typename T>
            bool ReadT(const uint8_t*& it, const uint8_t* end, T& v) {
                return Deserialization::FromBinary(it, end, v);
            }
        }

		void DeserializeScene(ECS::ECSCoordinator& ecs, const std::string& path) {
            NE::ByteBuffer bytes;
            if (!ReadAllBytes(path, bytes))
                return;

            const uint8_t* it = bytes.data();
            const uint8_t* end = bytes.data() + bytes.size();

            uint64_t magic = 0;
            uint64_t version = 0;

            if (!ReadT(it, end, magic)) return;
            if (magic != NSCE_MAGIC) return;

            if (!ReadT(it, end, version)) return;
            if (version != CURRENT_NANOSCENE_FORMAT_VERSION) return;

            if (!ReadT(it, end, Graphics::GraphicsManager::renderSettings)) return;
            if (!ReadT(it, end, Graphics::GraphicsManager::postProcessingSettings)) return;
            
            std::uint64_t entityCount = 0;
            if (!ReadT(it, end, entityCount)) return;

            for (std::uint64_t i = 0; i < entityCount; ++i) {
                ECS::Entity e = ecs.CreateEntity();

                std::uint64_t maskU64 = 0;
                if (!ReadT(it, end, maskU64)) return;
                const std::uint64_t mask = maskU64;

                std::uint32_t idx = 0;
                ForEachComponentType([&]<typename C>() {
                    if (mask & (std::uint64_t(1) << idx)) {
                        C c{};
                        if (!ReadT(it, end, c)) throw 1;  // will be caught below
                        ecs.AddComponent<C>(e, c);
                    }
                    ++idx;
                });
            }
		}
	}
}
