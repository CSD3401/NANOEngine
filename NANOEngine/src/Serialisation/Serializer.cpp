#include "pch.h"
#include "Serializer.hpp"

#include <fstream>

#include "BinaryReflection.hpp"
#include "ECS/Core/ECSCoordinator.hpp"
#include "Graphics/Core/GraphicsManager.hpp"
#include "Core/LUIDGenerator.hpp"

// Components
#include "ECS/Components/EntityMeta.hpp"
#include "ECS/Components/Transform.hpp"
#include "ECS/Components/Renderer.hpp"
#include "ECS/Components/Light.hpp"
#include "ECS/Components/Collider.hpp"
#include "ECS/Components/Rigidbody.hpp"
#include "ECS/Components/NativeScript.hpp"
#include "ECS/Components/Camera.hpp"
#include "ECS/Components/UIRectTransform.hpp"
#include "ECS/Components/UICanvas.hpp"
#include "ECS/Components/UIImage.hpp"
#include "ECS/Components/UIText.hpp"
#include "ECS/Components/UIButton.hpp"
#include "ECS/Components/UISlider.hpp"
#include "ECS/Components/UIToggle.hpp"
#include "ECS/Components/UILayoutGroup.hpp"
#include "ECS/Components/UIGridLayoutGroup.hpp"
#include "ECS/Components/UILayoutElement.hpp"
#include "ECS/Components/UIScrollRect.hpp"
#include "ECS/Components/UIAutoSize.hpp"
#include "ECS/Components/UIInputField.hpp"
#include "ECS/Components/UIDropdown.hpp"
#include "ECS/Components/Hierarchy.hpp"
#include "ECS/Components/PrefabLink.hpp"
#include "ECS/Components/PrefabInstance.hpp"
#include "ECS/Components/CharacterController.hpp"
#include "ECS/Components/Animator.hpp"
#include "ECS/Components/DecalProjector.hpp"

namespace NE {
	namespace {
		using ComponentTypes = std::tuple<
			ECS::Component::EntityMeta,
			ECS::Component::Hierarchy,
			ECS::Component::PrefabInstance,
			ECS::Component::PrefabLink,
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
			ECS::Component::UIText,
			ECS::Component::UIButton,
			ECS::Component::UISlider,
			ECS::Component::UIToggle,
			ECS::Component::UILayoutGroup,
			ECS::Component::UIGridLayoutGroup,
			ECS::Component::UILayoutElement,
			ECS::Component::UIScrollRect,
			ECS::Component::UIAutoSize,
			ECS::Component::UIInputField,
			ECS::Component::UIDropdown,
			ECS::Component::CharacterController,
			ECS::Component::Animator,
            ECS::Component::DecalProjector
		>;

		using ComponentMask = std::uint64_t;

		template <class F>
		void ForEachComponentType(F&& f) {
			std::apply([&](auto&&... t) {
				(f.template operator() < std::decay_t<decltype(t)> > (), ...);
				}, ComponentTypes{});
		}

		inline constexpr uint32_t NSCE_MAGIC = 0x4E534345;
		inline constexpr int CURRENT_NANOSCENE_FORMAT_VERSION = 5;

		inline constexpr uint32_t NFAB_MAGIC = 0x4E464142;
		inline constexpr int CURRENT_NANOPREFAB_FORMAT_VERSION = 4;

		void AppendPreorder(ECS::ECSCoordinator& ecs, ECS::Entity e, std::vector<ECS::Entity>& out) {
			out.push_back(e);

			auto& h = ecs.GetComponent<ECS::Component::Hierarchy>(e);
			for (auto childId : h.children)
				AppendPreorder(ecs, childId, out);
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
			//h.parent = NE::ECS::Component::INVALID_PARENT;
			//h.children.clear();
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
				const uint8_t layer = static_cast<uint8_t>(ecs.GetEntityManager().GetLayer(e));
				ToBinary(buf, layer);
				const bool active = ecs.GetEntityManager().GetActive(e);
				ToBinary(buf, active);

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
				const uint8_t layer = static_cast<uint8_t>(ecs.GetEntityManager().GetLayer(e));
				ToBinary(buf, layer);

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

		void SerializePrefab(ECS::ECSCoordinator& ecs, const ECS::Entity rootNodes, const std::string& path) {
			std::filesystem::path filePath(path);
			std::filesystem::path directory = filePath.parent_path();

			if (!directory.empty() && !std::filesystem::exists(directory)) {
				std::filesystem::create_directories(directory);
			}

			NE::ByteBuffer buf;

			ToBinary(buf, static_cast<uint64_t>(NFAB_MAGIC));
			ToBinary(buf, static_cast<uint64_t>(CURRENT_NANOPREFAB_FORMAT_VERSION));

			std::vector<NE::ECS::Entity> flat;
			flat.reserve(32);
			AppendPreorder(ecs, rootNodes, flat);
			
			const auto entityToLocalId = BuildLocalIdMap(flat, rootNodes);

			ToBinary(buf, static_cast<uint64_t>(flat.size()));

			for (auto e : flat)
				WriteOneEntityCopyBlob(ecs, buf, e, entityToLocalId);

			std::ofstream out(path, std::ios::binary);
			if (!out) return;
			out.write(reinterpret_cast<const char*>(buf.data()),
				static_cast<std::streamsize>(buf.size()));
		}

		void SerializeEntitiesToMemory(ECS::ECSCoordinator& ecs, const uint32_t rootEnt, std::vector<uint8_t>& outBuffer) {
			outBuffer.clear();

			std::vector<ECS::Entity> flat;
			flat.reserve(32);
			AppendPreorder(ecs, rootEnt, flat);
			if (flat.empty())
				return;

			const auto entityToLocalId = BuildLocalIdMap(flat, rootEnt);

			ByteBuffer buf;

			ToBinary(buf, static_cast<uint64_t>(flat.size()));

			for (auto e : flat)
				WriteOneEntityCopyBlob(ecs, buf, e, entityToLocalId);

			outBuffer.assign(buf.data(), buf.data() + buf.size());
		}

		//// Custom binary serialization for NativeScript component
		//size_t ToBinary(ByteBuffer& out, const NE::ECS::Component::NativeScript& nsc) {
		//	const size_t before = out.size();

		//	// Serialize ScriptNames using reflection
		//	ToBinary(out, nsc.ScriptNames);

		//	// Serialize SerializedFields (unordered_map<string, string>)
		//	AppendU64LE(out, static_cast<uint64_t>(nsc.SerializedFields.size()));
		//	for (const auto& [key, value] : nsc.SerializedFields) {
		//		ToBinary(out, key);
		//		ToBinary(out, value);
		//	}

		//	// Serialize EntityReferenceFields (unordered_set<string>)
		//	AppendU64LE(out, static_cast<uint64_t>(nsc.EntityReferenceFields.size()));
		//	for (const auto& field : nsc.EntityReferenceFields) {
		//		ToBinary(out, field);
		//	}

		//	return out.size() - before;
		//}
	}

	namespace Deserialization {
		// Custom deserialization for NativeScript component (must be BEFORE DeserializeScene)
		//bool FromBinary(const uint8_t*& it, const uint8_t* end, NE::ECS::Component::NativeScript& nsc) {
		//	// Deserialize ScriptNames using reflection
		//	if (!FromBinary(it, end, nsc.ScriptNames))
		//		return false;

		//	// Deserialize SerializedFields (unordered_map<string, string>)
		//	uint64_t fieldsCount = 0;
		//	if (!ReadU64LE(it, end, fieldsCount))
		//		return false;

		//	nsc.SerializedFields.clear();
		//	for (uint64_t i = 0; i < fieldsCount; ++i) {
		//		std::string key, value;
		//		if (!FromBinary(it, end, key) || !FromBinary(it, end, value))
		//			return false;
		//		nsc.SerializedFields[key] = value;
		//	}

		//	// Deserialize EntityReferenceFields (unordered_set<string>)
		//	uint64_t refFieldsCount = 0;
		//	if (!ReadU64LE(it, end, refFieldsCount))
		//		return false;

		//	nsc.EntityReferenceFields.clear();
		//	for (uint64_t i = 0; i < refFieldsCount; ++i) {
		//		std::string field;
		//		if (!FromBinary(it, end, field))
		//			return false;
		//		nsc.EntityReferenceFields.insert(field);
		//	}

		//	return true;
		//}

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

		bool DeserializeScene(ECS::ECSCoordinator& ecs, const std::string& path) {
			NE::ByteBuffer bytes;
			if (!ReadAllBytes(path, bytes))
				return false;

			const uint8_t* it = bytes.data();
			const uint8_t* end = bytes.data() + bytes.size();

			uint64_t magic = 0;
			uint64_t version = 0;

			if (!ReadT(it, end, magic)) return false;
			if (magic != NSCE_MAGIC) return false;

			if (!ReadT(it, end, version)) return false;
			if (version != CURRENT_NANOSCENE_FORMAT_VERSION)
				return false;

			if (!ReadT(it, end, Graphics::GraphicsManager::renderSettings)) return false;
			if (!ReadT(it, end, Graphics::GraphicsManager::postProcessingSettings)) return false;

			std::uint64_t entityCount = 0;
			if (!ReadT(it, end, entityCount)) return false;

			for (std::uint64_t i = 0; i < entityCount; ++i) {
				ECS::Entity e = ecs.CreateEntity();

				uint8_t layer = 0;
				ReadT(it, end, layer);
				ecs.GetEntityManager().SetLayer(e, layer);
				bool entityActive = true;
				ReadT(it, end, entityActive);

				std::uint64_t maskU64 = 0;
				if (!ReadT(it, end, maskU64)) return false;
				const std::uint64_t mask = maskU64;

				std::uint32_t idx = 0;
				ForEachComponentType([&]<typename C>() {
					if (mask & (std::uint64_t(1) << idx)) {
						C c{};
						if (!ReadT(it, end, c)) { ++idx; return; }
						ecs.AddComponent<C>(e, c);
					}
					++idx;
				});

				//if (!hasEntityActiveBit && ecs.HasComponent<ECS::Component::EntityMeta>(e)) {
				//	entityActive = ecs.GetComponent<ECS::Component::EntityMeta>(e).isActive;
				//}

				ecs.GetEntityManager().ToggleActive(e, entityActive);
				if (ecs.HasComponent<ECS::Component::EntityMeta>(e)) {
					ecs.GetComponent<ECS::Component::EntityMeta>(e).isActive = entityActive;
				}
			}
			return true;
		}

		uint32_t DeserializePrefab(ECS::ECSCoordinator& ecs, const std::string& path) {
			NE::ByteBuffer bytes;
			if (!ReadAllBytes(path, bytes))
				return ECS::Component::INVALID_PARENT;

			const uint8_t* it = bytes.data();
			const uint8_t* end = bytes.data() + bytes.size();

			uint64_t magic = 0;
			uint64_t version = 0;

			if (!ReadT(it, end, magic))   return ECS::Component::INVALID_PARENT;
			if (magic != NFAB_MAGIC)      return ECS::Component::INVALID_PARENT;

			if (!ReadT(it, end, version)) return ECS::Component::INVALID_PARENT;
			if (version != CURRENT_NANOPREFAB_FORMAT_VERSION)
				return ECS::Component::INVALID_PARENT;

			std::uint64_t entityCount64 = 0;
			if (!ReadT(it, end, entityCount64)) return ECS::Component::INVALID_PARENT;

			const size_t count = static_cast<size_t>(entityCount64);
			if (count == 0) return ECS::Component::INVALID_PARENT;

			ECS::Entity outNewRoot = ECS::Component::INVALID_PARENT;

			std::vector<ECS::Entity> created;
			created.reserve(count);

			std::unordered_map<uint64_t, ECS::Entity> oldLuidToEntity;
			oldLuidToEntity.reserve(count);

			std::unordered_map<uint64_t, uint64_t> oldLuidToNewLuid;
			oldLuidToNewLuid.reserve(count);

			struct PendingParent {
				ECS::Entity e;
				uint64_t oldMyLuid;
				uint64_t oldParentLuid;
			};
			std::vector<PendingParent> pending;
			pending.reserve(count);

			bool ok = true;

			for (size_t i = 0; i < count && ok; ++i) {
				ECS::Entity e = ecs.CreateEntity();
				created.push_back(e);

				uint8_t layer;
				ReadT(it, end, layer);
				ecs.GetEntityManager().SetLayer(e, layer);

				std::uint64_t maskU64 = 0;
				ok = ReadT(it, end, maskU64);
				if (!ok) break;

				const std::uint64_t mask = maskU64;

				std::uint32_t idx = 0;
				ForEachComponentType([&]<typename C>() {
					if (!ok) { ++idx; return; }

					if (mask & (std::uint64_t(1) << idx)) {
						if constexpr (std::is_same_v<C, ECS::Component::Hierarchy>) {
							ECS::Component::Hierarchy c{};
							if (!ReadT(it, end, c)) { ok = false; ++idx; return; }
							const uint64_t oldMy = c.luid;
							const uint64_t oldParent = c.parentLuid;
							const uint64_t newMy = Core::LUIDGenerator::Generate("hr");
							c.luid = newMy;
							c.parent = ECS::Component::INVALID_PARENT;
							c.parentLuid = 0;
							c.children.clear();
							ecs.AddComponent<ECS::Component::Hierarchy>(e, c);
							oldLuidToEntity[oldMy] = e;
							oldLuidToNewLuid[oldMy] = newMy;
							pending.push_back({ e, oldMy, oldParent });
							if (oldMy == 0)
								outNewRoot = e;
						} else {
							C c{};
							if (!ReadT(it, end, c)) { ok = false; ++idx; return; }
							if constexpr (HasLuid<C>) {
								c.luid = 0;
							}
							ecs.AddComponent<C>(e, c);
						}
					}

					++idx;
				});
			}

			if (!ok) return ECS::Component::INVALID_PARENT;

			if (outNewRoot == ECS::Component::INVALID_PARENT) {
				outNewRoot = created.empty() ? ECS::Component::INVALID_PARENT : created.front();
			}

			for (const auto& p : pending) {
				if (p.oldMyLuid == 0) continue;

				auto parentEntIt = oldLuidToEntity.find(p.oldParentLuid);
				if (parentEntIt == oldLuidToEntity.end()) continue;

				auto newParentLuidIt = oldLuidToNewLuid.find(p.oldParentLuid);
				if (newParentLuidIt == oldLuidToNewLuid.end()) continue;

				ECS::Entity parentE = parentEntIt->second;

				auto& childH = ecs.GetComponent<ECS::Component::Hierarchy>(p.e);
				auto& parentH = ecs.GetComponent<ECS::Component::Hierarchy>(parentE);

				childH.parent = parentE;
				childH.parentLuid = newParentLuidIt->second;
				parentH.children.push_back(p.e);
			}

			return outNewRoot;
		}

		bool DeserializePrefab(ECS::ECSCoordinator& ecs, const std::string& path, uint32_t root) {
			NE::ByteBuffer bytes;
			if (!ReadAllBytes(path, bytes))
				return false;

			const uint8_t* it = bytes.data();
			const uint8_t* end = bytes.data() + bytes.size();

			uint64_t magic = 0;
			uint64_t version = 0;

			if (!ReadT(it, end, magic)) return false;
			if (magic != NFAB_MAGIC)    return false;

			if (!ReadT(it, end, version)) return false;
			if (version != CURRENT_NANOPREFAB_FORMAT_VERSION)
				return false;

			std::uint64_t entityCount64 = 0;
			if (!ReadT(it, end, entityCount64)) return false;

			const size_t count = static_cast<size_t>(entityCount64);
			if (count == 0) return false;

			const ECS::Entity attachRoot = root;
			if (attachRoot == ECS::Component::INVALID_PARENT)
				return false;

			std::vector<ECS::Entity> created;
			created.reserve(count > 0 ? count - 1 : 0);

			std::unordered_map<uint64_t, ECS::Entity> oldLuidToEntity;
			oldLuidToEntity.reserve(count);

			std::unordered_map<uint64_t, uint64_t> oldLuidToNewLuid;
			oldLuidToNewLuid.reserve(count);

			struct PendingParent {
				ECS::Entity e;
				uint64_t oldMyLuid;
				uint64_t oldParentLuid;
			};
			std::vector<PendingParent> pending;
			pending.reserve(count);

			bool ok = true;

			uint64_t oldRootLuid = 0;
			bool haveOldRootLuid = false;

			// Helper: read component using V3 legacy struct and copy to current layout
			for (size_t i = 0; i < count && ok; ++i) {
				const bool skipFirst = (i == 0);

				ECS::Entity e = ECS::Component::INVALID_PARENT;
				uint8_t layer;
				ReadT(it, end, layer);

				if (!skipFirst) {
					e = ecs.CreateEntity();
					ecs.GetEntityManager().SetLayer(e, layer);
					created.push_back(e);
				}

				std::uint64_t maskU64 = 0;
				ok = ReadT(it, end, maskU64);
				if (!ok) break;

				const std::uint64_t mask = maskU64;

				std::uint32_t idx = 0;
				ForEachComponentType([&]<typename C>() {
					if (!ok) { ++idx; return; }

					if (mask & (std::uint64_t(1) << idx)) {
						if constexpr (std::is_same_v<C, ECS::Component::Hierarchy>) {
							ECS::Component::Hierarchy c{};
							if (!ReadT(it, end, c)) { ok = false; ++idx; return; }
							const uint64_t oldMy = c.luid;
							const uint64_t oldParent = c.parentLuid;
							if (skipFirst) {
								oldRootLuid = oldMy;
								haveOldRootLuid = true;
								auto& attachH = ecs.GetComponent<ECS::Component::Hierarchy>(attachRoot);
								oldLuidToEntity[oldMy] = attachRoot;
								oldLuidToNewLuid[oldMy] = attachH.luid;
							} else {
								const uint64_t newMy = Core::LUIDGenerator::Generate("hr");
								c.luid = newMy;
								c.parent = ECS::Component::INVALID_PARENT;
								c.parentLuid = 0;
								c.children.clear();
								ecs.AddComponent<ECS::Component::Hierarchy>(e, c);
								oldLuidToEntity[oldMy] = e;
								oldLuidToNewLuid[oldMy] = newMy;
								pending.push_back({ e, oldMy, oldParent });
							}
						} else {
							C c{};
							if (!ReadT(it, end, c)) { ok = false; ++idx; return; }
							if (!skipFirst) {
								if constexpr (HasLuid<C>) {
									c.luid = 0;
								}
								ecs.AddComponent<C>(e, c);
							}
						}
					}

					++idx;
				});
			}

			if (!ok) return false;

			for (const auto& p : pending) {
				auto parentEntIt = oldLuidToEntity.find(p.oldParentLuid);
				if (parentEntIt == oldLuidToEntity.end()) continue;

				auto newParentLuidIt = oldLuidToNewLuid.find(p.oldParentLuid);
				if (newParentLuidIt == oldLuidToNewLuid.end()) continue;

				ECS::Entity parentE = parentEntIt->second;

				auto& childH = ecs.GetComponent<ECS::Component::Hierarchy>(p.e);
				auto& parentH = ecs.GetComponent<ECS::Component::Hierarchy>(parentE);

				childH.parent = parentE;
				childH.parentLuid = newParentLuidIt->second;
				parentH.children.push_back(p.e);
			}

			return true;
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

			std::unordered_map<uint64_t, ECS::Entity> oldLuidToEntity;
			oldLuidToEntity.reserve(count);

			std::unordered_map<uint64_t, uint64_t> oldLuidToNewLuid;
			oldLuidToNewLuid.reserve(count);

			struct PendingParent {
				ECS::Entity e;
				uint64_t oldMyLuid;
				uint64_t oldParentLuid;
			};
			std::vector<PendingParent> pending;
			pending.reserve(count);

			bool ok = true;

			for (size_t i = 0; i < count && ok; ++i) {
				ECS::Entity e = ecs.CreateEntity();
				created.push_back(e);

				uint8_t layer;
				ReadT(it, end, layer);
				ecs.GetEntityManager().SetLayer(e, layer);

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

						if constexpr (std::is_same_v<C, ECS::Component::Hierarchy>) {
							const uint64_t oldMy = c.luid;
							const uint64_t oldParent = c.parentLuid;

							const uint64_t newMy = Core::LUIDGenerator::Generate("hr");
							c.luid = newMy;

							c.parent = ECS::Component::INVALID_PARENT;
							c.parentLuid = 0;

							ecs.AddComponent<ECS::Component::Hierarchy>(e, c);

							oldLuidToEntity[oldMy] = e;
							oldLuidToNewLuid[oldMy] = newMy;
							pending.push_back({ e, oldMy, oldParent });

							if (oldMy == 0)
								outNewRoot = e;
						} else {
							ecs.AddComponent<C>(e, c);
						}
					}

					++idx;
				});
			}

			if (!ok) return ECS::Component::INVALID_PARENT;

			if (outNewRoot == ECS::Component::INVALID_PARENT) {
				outNewRoot = created.empty() ? ECS::Component::INVALID_PARENT : created.front();
			}

			for (const auto& p : pending) {
				if (p.oldMyLuid == 0) continue;

				auto parentEntIt = oldLuidToEntity.find(p.oldParentLuid);
				if (parentEntIt == oldLuidToEntity.end()) continue;

				ECS::Entity parentE = parentEntIt->second;

				auto newParentLuidIt = oldLuidToNewLuid.find(p.oldParentLuid);
				if (newParentLuidIt == oldLuidToNewLuid.end()) continue;

				auto& childH = ecs.GetComponent<ECS::Component::Hierarchy>(p.e);
				auto& parentH = ecs.GetComponent<ECS::Component::Hierarchy>(parentE);

				childH.parent = parentE;
				childH.parentLuid = newParentLuidIt->second;
				parentH.children.push_back(p.e);
			}

			return outNewRoot;
		}
	}
}
