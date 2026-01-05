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

        using ComponentMask = std::uint64_t;

        template <class F>
        void ForEachComponentType(F&& f) {
            std::apply([&](auto&&... t) {
                (f.template operator() < std::decay_t<decltype(t)> > (), ...);
                }, ComponentTypes{});
        }

        inline constexpr uint32_t NSCE_MAGIC = 0x4E534345;
        inline constexpr int CURRENT_NANOSCENE_FORMAT_VERSION = 1;

        void AppendPreorderSafe(ECS::ECSCoordinator& ecs, ECS::Entity e, std::vector<ECS::Entity>& out) {
            out.push_back(e);

            if (!ecs.HasComponent<ECS::Component::Hierarchy>(e))
                return;

            auto& h = ecs.GetComponent<ECS::Component::Hierarchy>(e);
            for (auto childId : h.children)
                AppendPreorderSafe(ecs, childId, out);
        }

        std::unordered_map<NE::ECS::Entity, uint64_t>
            BuildLocalIdMap(const std::vector<NE::ECS::Entity>& flat, NE::ECS::Entity root)
        {
            std::unordered_map<NE::ECS::Entity, uint64_t> map;
            map.reserve(flat.size());

            map[root] = 0;

            uint64_t next = 1;
            for (auto e : flat) {
                if (e == root) continue;
                map.emplace(e, next++);
            }
            return map;
        }

        template <class T>
        concept HasLuid = requires(T & t) { t.luid; };

        template <typename C>
        void PatchForCopy(C& c, ECS::Entity,
            const std::unordered_map<ECS::Entity, uint64_t>&) {

            if constexpr (HasLuid<C>) {
                c.luid = 0;
            }
        }

        //template <>
        //inline void PatchForCopy<NE::ECS::Component::EntityMeta>(
        //    NE::ECS::Component::EntityMeta& meta,
        //    NE::ECS::Entity /*e*/,
        //    const std::unordered_map<NE::ECS::Entity, uint64_t>& /*entityToLocalId*/)
        //{
        //    meta.prefabLocalID = 0;
        //}

        template <>
        inline void PatchForCopy<NE::ECS::Component::Hierarchy>(
            NE::ECS::Component::Hierarchy& h,
            NE::ECS::Entity e,
            const std::unordered_map<NE::ECS::Entity, uint64_t>& entityToLocalId)
        {
            // entityToLocalId must already contain root->0, others->1..N-1
            const uint64_t myId = entityToLocalId.at(e);

            uint64_t parentId = 0;
            if (h.parent != NE::ECS::Component::INVALID_PARENT) {
                auto it = entityToLocalId.find(h.parent);
                if (it != entityToLocalId.end())
                    parentId = it->second; // only if parent is within the copied set
                else
                    parentId = 0; // parent outside selection => treat as root on paste (or special-case)
            }

            // Temporary IDs used only during paste
            h.luid = myId;
            h.parentLuid = parentId;

            // World handles are invalid when pasted
            h.parent = NE::ECS::Component::INVALID_PARENT;
            h.children.clear();
        }
    }

	namespace Serialization {

		namespace {
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

            void WriteOneEntityCopyBlob(ECS::ECSCoordinator& ecs,
                ByteBuffer& buf,
                ECS::Entity e,
                const std::unordered_map<ECS::Entity, uint64_t>& entityToLocalId)
            {
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
                        C copy = ecs.GetComponent<C>(e);                 // copy
                        PatchForCopy<C>(copy, e, entityToLocalId);       // patch
                        ToBinary(buf, copy);                             // write
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

        void SerializeEntitiesToMemory(ECS::ECSCoordinator& ecs, const uint32_t rootEnt, std::vector<uint8_t>& outBuffer) {
            outBuffer.clear();

            if (!ecs.HasComponent<ECS::Component::Hierarchy>(rootEnt))
                return;

            std::vector<ECS::Entity> flat;
            flat.reserve(32);
            AppendPreorderSafe(ecs, rootEnt, flat);
            if (flat.empty())
                return;

            const auto entityToLocalId = BuildLocalIdMap(flat, rootEnt);

            ByteBuffer buf;

            ToBinary(buf, (uint64_t)flat.size());

            for (auto e : flat)
                WriteOneEntityCopyBlob(ecs, buf, e, entityToLocalId);

            outBuffer.assign(buf.data(), buf.data() + buf.size());
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

        uint32_t DeserializeEntitiesFromMemory(ECS::ECSCoordinator& ecs, std::vector<uint8_t>& buffer) {
            uint32_t outNewRoot = ECS::Component::INVALID_PARENT;

            const uint8_t* it = buffer.data();
            const uint8_t* end = buffer.data() + buffer.size();

            uint64_t count64 = 0;
            if (!FromBinary(it, end, count64)) return ECS::Component::INVALID_PARENT;

            const size_t count = static_cast<size_t>(count64);
            if (count == 0) return ECS::Component::INVALID_PARENT;

            std::vector<ECS::Entity> created;
            created.reserve(count);

            std::unordered_map<uint64_t, ECS::Entity> localIdToEntity;
            localIdToEntity.reserve(count);

            struct PendingParent { ECS::Entity e; uint64_t my; uint64_t parent; };
            std::vector<PendingParent> pending;
            pending.reserve(count);

            bool ok = true;

            for (size_t i = 0; i < count && ok; ++i) {
                ECS::Entity e = ecs.CreateEmptyEntity();
                created.push_back(e);

                uint64_t mask64 = 0;
                ok = FromBinary(it, end, mask64);
                if (!ok) break;

                ComponentMask mask = static_cast<ComponentMask>(mask64);

                uint32_t idx = 0;
                ForEachComponentType([&]<typename C>() {
                    if (!ok) { ++idx; return; }

                    if (mask & (ComponentMask(1) << idx)) {
                        C c{};
                        if (!FromBinary(it, end, c)) { ok = false; ++idx; return; }

                        ecs.AddComponent<C>(e, c);

                        if constexpr (std::is_same_v<C, ECS::Component::Hierarchy>) {
                            auto& h = ecs.GetComponent<C>(e);
                            localIdToEntity[h.luid] = e;
                            pending.push_back({ e, h.luid, h.parentLuid });

                            if (h.luid == 0)
                                outNewRoot = e;
                        }
                    }
                    ++idx;
                });
            }

            if (!ok) return ECS::Component::INVALID_PARENT;
            if (outNewRoot == ECS::Component::INVALID_PARENT) {
                // Fallback: if for some reason no hierarchy luid==0 was found,
                // treat the first created entity as root.
                outNewRoot = created.empty() ? ECS::Component::INVALID_PARENT : created.front();
            }

            // Rebuild parent/children
            for (auto& p : pending) {
                if (p.my == 0) continue;

                auto parentIt = localIdToEntity.find(p.parent);
                if (parentIt == localIdToEntity.end()) continue;

                ECS::Entity parentE = parentIt->second;

                auto& childH = ecs.GetComponent<ECS::Component::Hierarchy>(p.e);
                auto& parentH = ecs.GetComponent<ECS::Component::Hierarchy>(parentE);

                childH.parent = parentE;
                parentH.children.push_back(p.e);
            }

            // Clear temp IDs (you said you’ll regenerate after)
            for (auto e : created) {
                if (ecs.HasComponent<ECS::Component::Hierarchy>(e)) {
                    auto& h = ecs.GetComponent<ECS::Component::Hierarchy>(e);
                    h.luid = 0;
                    h.parentLuid = 0;
                }
            }

            return outNewRoot;
        }
	}
}
