#include "pch.h"
#include "Serializer.hpp"

#include <fstream>

#include <rapidjson/document.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/stringbuffer.h>

// Components
#include <ECS/Components/EntityMeta.hpp>
#include <ECS/Components/Transform.hpp>
#include <ECS/Components/Renderer.hpp>
#include <ECS/Components/LightmapBinding.hpp>
#include <ECS/Components/Light.hpp>
#include <ECS/Components/Collider.hpp>
#include <ECS/Components/Rigidbody.hpp>
#include <ECS/Components/NativeScript.hpp>
#include <ECS/Components/Camera.hpp>
#include <ECS/Components/UIRectTransform.hpp>
#include <ECS/Components/UICanvas.hpp>
#include <ECS/Components/UIImage.hpp>
#include <ECS/Components/UIText.hpp>
#include <ECS/Components/UIButton.hpp>
#include <ECS/Components/UISlider.hpp>
#include <ECS/Components/UIToggle.hpp>
#include <ECS/Components/UILayoutGroup.hpp>
#include <ECS/Components/UIGridLayoutGroup.hpp>
#include <ECS/Components/UILayoutElement.hpp>
#include <ECS/Components/UIScrollRect.hpp>
#include <ECS/Components/UIAutoSize.hpp>
#include <ECS/Components/UIInputField.hpp>
#include <ECS/Components/UIDropdown.hpp>
#include <ECS/Components/Hierarchy.hpp>
#include <ECS/Components/PrefabLink.hpp>
#include <ECS/Components/PrefabInstance.hpp>
#include <ECS/Components/CharacterController.hpp>
#include <ECS/Components/Animator.hpp>
#include <ECS/Components/DecalProjector.hpp>

#include <EditorInterface/ECSExports.hpp>
#include <EditorInterface/RendererExports.hpp>
#include <ECS/Components/ComponentKey.hpp>
#include <Core/LUIDGenerator.hpp>

#include <Graphics/Core/RenderSettings.hpp>
#include <Graphics/Core/PostProcessingSettings.hpp>

#include "JSONReflection.hpp"
#include "../EditorScene.hpp"
#include <Scripting/ScriptingEngine.hpp>
#include <Serialisation/BinaryReflection.hpp>

namespace Editor {
	namespace {
		inline constexpr uint32_t NFAB_MAGIC = 0x4E464142;
		inline constexpr int CURRENT_NANOPREFAB_FORMAT_VERSION = 2;

		using SceneComponentTypes = std::tuple<
			NE::ECS::Component::EntityMeta,
			NE::ECS::Component::Hierarchy,
			NE::ECS::Component::PrefabInstance,
			NE::ECS::Component::PrefabLink,
			NE::ECS::Component::Transform,
			NE::ECS::Component::Renderer,
			NE::ECS::Component::LightmapBinding,
			NE::ECS::Component::Light,
			NE::ECS::Component::Collider,
			NE::ECS::Component::Rigidbody,
			NE::ECS::Component::NativeScript,
			NE::ECS::Component::Camera,
			NE::ECS::Component::UIRectTransform,
			NE::ECS::Component::UICanvas,
			NE::ECS::Component::UIImage,
			NE::ECS::Component::UIText,
			NE::ECS::Component::UIButton,
			NE::ECS::Component::UISlider,
			NE::ECS::Component::UIToggle,
			NE::ECS::Component::UILayoutGroup,
			NE::ECS::Component::UIGridLayoutGroup,
			NE::ECS::Component::UILayoutElement,
			NE::ECS::Component::UIScrollRect,
			NE::ECS::Component::UIAutoSize,
			NE::ECS::Component::UIInputField,
			NE::ECS::Component::UIDropdown,
			NE::ECS::Component::CharacterController,
			NE::ECS::Component::Animator,
			NE::ECS::Component::DecalProjector
		>;

		template <class F>
		void ForEachSceneComponentType(F&& f) {
			std::apply([&](auto&&... t) {
				(f.template operator() < std::decay_t<decltype(t)> > (), ...);
				}, SceneComponentTypes{});
		}

		using PrefabComponentTypes = std::tuple<
			NE::ECS::Component::EntityMeta,
			NE::ECS::Component::Hierarchy,
			NE::ECS::Component::PrefabInstance,
			NE::ECS::Component::PrefabLink,
			NE::ECS::Component::Transform,
			NE::ECS::Component::Renderer,
			NE::ECS::Component::Light,
			NE::ECS::Component::Collider,
			NE::ECS::Component::Rigidbody,
			NE::ECS::Component::NativeScript,
			NE::ECS::Component::Camera,
			NE::ECS::Component::UIRectTransform,
			NE::ECS::Component::UICanvas,
			NE::ECS::Component::UIImage,
			NE::ECS::Component::UIText,
			NE::ECS::Component::UIButton,
			NE::ECS::Component::UISlider,
			NE::ECS::Component::UIToggle,
			NE::ECS::Component::UILayoutGroup,
			NE::ECS::Component::UIGridLayoutGroup,
			NE::ECS::Component::UILayoutElement,
			NE::ECS::Component::UIScrollRect,
			NE::ECS::Component::UIAutoSize,
			NE::ECS::Component::UIInputField,
			NE::ECS::Component::UIDropdown,
			NE::ECS::Component::CharacterController,
			NE::ECS::Component::Animator,
			NE::ECS::Component::DecalProjector
		>;

		template <class F>
		void ForEachPrefabComponentType(F&& f) {
			std::apply([&](auto&&... t) {
				(f.template operator() < std::decay_t<decltype(t)> > (), ...);
				}, PrefabComponentTypes{});
		}

		std::string projectSettingsLoc = "ProjectSettings/";
		std::string layerSettingsLoc = projectSettingsLoc + "LayerSettings.json";
		constexpr uint32_t kLayerSettingsVersion = 1;

		std::string userSettingsFolder = "UserSettings/";
		std::string userSettingsLoc = userSettingsFolder + "UserSettings.json";
		constexpr uint32_t kUserSettingsVersion = 1;

		bool ReadAllText(const std::filesystem::path& p, std::string& out)
		{
			std::ifstream f(p, std::ios::binary);
			if (!f) return false;

			f.seekg(0, std::ios::end);
			const size_t sz = static_cast<size_t>(f.tellg());
			f.seekg(0, std::ios::beg);

			out.resize(sz);
			if (sz) f.read(out.data(), static_cast<std::streamsize>(sz));
			return true;
		}

		bool WriteAllText(const std::filesystem::path& p, const std::string& s)
		{
			std::ofstream f(p, std::ios::binary);
			if (!f) return false;
			f.write(s.data(), static_cast<std::streamsize>(s.size()));
			return true;
		}

		// Make sure collision matrix is symmetric.
		// If file had mismatches, we pick AND ("most restrictive").
		std::array<NE::Core::LayerMask, NE::Core::MAX_LAYERS>
			SymmetrizeAND(const std::array<NE::Core::LayerMask, NE::Core::MAX_LAYERS>& in)
		{
			using namespace NE::Core;

			auto out = in;

			for (LayerID i = 0; i < MAX_LAYERS; ++i)
				out[i] |= LayerBit(i); // always collide with self

			for (LayerID a = 0; a < MAX_LAYERS; ++a) {
				for (LayerID b = static_cast<LayerID>(a + 1); b < MAX_LAYERS; ++b) {
					const LayerMask bitA = LayerBit(a);
					const LayerMask bitB = LayerBit(b);

					const bool ab = (in[a] & bitB) != 0;
					const bool ba = (in[b] & bitA) != 0;
					const bool en = ab && ba;

					if (en) {
						out[a] |= bitB;
						out[b] |= bitA;
					} else {
						out[a] &= ~bitB;
						out[b] &= ~bitA;
					}
				}
			}

			return out;
		}
	}

	namespace Serialization {
		namespace JSON {
			namespace {
				template <typename C>
				void WriteComponentIfPresent(NE::ECS::Entity e,
					rapidjson::Value& ent, rapidjson::Document::AllocatorType& a) {
					if (!NE::ECS::Query::HasComponent<C>(e)) return;
					auto& c = NE::ECS::Query::GetComponent<C>(e);

					ent.AddMember(rapidjson::Value(ComponentKey<C>::value, a), ToJSON(c, a), a);
				}

				rapidjson::Value WriteEntityRecursive(
					NE::ECS::Entity e,
					rapidjson::Value& entities,
					rapidjson::Document::AllocatorType& a,
					bool includeSceneOnlyComponents) {
					rapidjson::Value ent(rapidjson::kObjectType);

					ent.AddMember("Layer", ToJSON(NE::ECS::Query::GetLayer(e), a), a);
					ent.AddMember("Active", ToJSON(NE::ECS::Query::GetActive(e), a), a);
					if (includeSceneOnlyComponents) {
						ForEachSceneComponentType([&]<typename C>() {
							WriteComponentIfPresent<C>(e, ent, a);
						});
					} else {
						ForEachPrefabComponentType([&]<typename C>() {
							WriteComponentIfPresent<C>(e, ent, a);
						});
					}

					entities.PushBack(ent, a);

					auto& h = NE::ECS::Query::GetEntityHierarchy(e);
					for (auto childId : h.children) {
						NE::ECS::Entity child = static_cast<NE::ECS::Entity>(childId);
						WriteEntityRecursive(child, entities, a, includeSceneOnlyComponents);
					}

					return ent;
				}
			}

			void SerializeScene(const std::string& path) {
				using rapidjson::Document;
				using rapidjson::StringBuffer;
				using rapidjson::PrettyWriter;

				// Save all script instance field values to components before serialization
				auto& coordinator = NE::GetScene().GetECSCoordinator();
				auto& componentManager = coordinator.GetComponentManager();
				//auto& allEntities = coordinator.GetUsedEntities();

				//for (NE::ECS::Entity entity : allEntities) {
				//	if (componentManager.HasComponent<NE::ECS::Component::NativeScript>(entity)) {
				//		auto& nsc = componentManager.GetComponent<NE::ECS::Component::NativeScript>(entity);
				//		NE::Scripting::ScriptingEngine::GetInstance().SaveSerializedFields(entity, nsc);
				//	}
				//}

				Document doc;
				doc.SetObject();
				auto& a = doc.GetAllocator();

				auto& renderSettings = NE::Renderer::Query::GetRenderSettings();
				doc.AddMember("RenderSettings", ToJSON(renderSettings, a), a);

				auto& postProcessingSettings = NE::Renderer::Query::GetPostProcessingSettings();
				doc.AddMember("PostProcessingSettings", ToJSON(postProcessingSettings, a), a);

				doc.AddMember("LightingContainer", ToJSON(NE::GetScene().GetLightingContainer(), a), a);

				Value entities(rapidjson::Type::kArrayType);
				const auto& sceneRoots = EditorScene::s_rootOrder;
				for (auto e : sceneRoots) {
					WriteEntityRecursive(e, entities, a, true);
				}
				doc.AddMember("Entities", entities, a);

				StringBuffer sb;
				PrettyWriter<rapidjson::StringBuffer> wr(sb);
				doc.Accept(wr);

				std::ofstream out(path, std::ios::binary);
				if (out) out.write(sb.GetString(), static_cast<std::streamsize>(sb.GetSize()));
			}

			void SerializePrefab(const std::string& path, bool isScene) {
				using rapidjson::Document;
				using rapidjson::StringBuffer;
				using rapidjson::PrettyWriter;

				Document doc;
				doc.SetObject();
				auto& a = doc.GetAllocator();

				Value entities(rapidjson::Type::kArrayType);
				if (!isScene) {
					const auto& prefabRoot = EditorScene::s_selection.GetLastDropped();
					WriteEntityRecursive(prefabRoot, entities, a, false);
				} else {
					WriteEntityRecursive(EditorScene::s_rootOrder[0], entities, a, false);
				}
				doc.AddMember("Entities", entities, a);

				StringBuffer sb;
				PrettyWriter<rapidjson::StringBuffer> wr(sb);
				doc.Accept(wr);

				std::ofstream out(path, std::ios::binary);
				if (out) out.write(sb.GetString(), static_cast<std::streamsize>(sb.GetSize()));
			}

			bool CookPrefabToBinary(const std::string& jsonPath, const std::string& binPath) {
				std::string jsonContent;
				if (!ReadAllText(jsonPath, jsonContent)) return false;

				rapidjson::Document doc;
				doc.Parse(jsonContent.c_str());
				if (doc.HasParseError() || !doc.HasMember("Entities")) return false;

				NE::ByteBuffer outputBuffer;

				NE::Serialization::ToBinary(outputBuffer, static_cast<uint64_t>(NFAB_MAGIC));
				NE::Serialization::ToBinary(outputBuffer, static_cast<uint64_t>(CURRENT_NANOPREFAB_FORMAT_VERSION));

				const auto& entities = doc["Entities"].GetArray();
				uint64_t entityCount = static_cast<uint64_t>(entities.Size());
				NE::Serialization::ToBinary(outputBuffer, entityCount);

				for (const auto& entVal : entities) {
					uint8_t layer = 0;
					if (entVal.HasMember("Layer")) {
						Deserialization::FromJSON(entVal["Layer"], layer);
					}
					NE::Serialization::ToBinary(outputBuffer, layer);

					uint64_t mask = 0;
					uint32_t bitIdx = 0;
					ForEachPrefabComponentType([&]<typename C>() {
						if (entVal.HasMember(ComponentKey<C>::value)) {
							mask |= (uint64_t(1) << bitIdx);
						}
						bitIdx++;
					});

					NE::Serialization::ToBinary(outputBuffer, mask);

					ForEachPrefabComponentType([&]<typename C>() {
						const char* key = ComponentKey<C>::value;
						if (entVal.HasMember(key)) {
							C tempComp{};
							Deserialization::FromJSON(entVal[key], tempComp);
							NE::Serialization::ToBinary(outputBuffer, tempComp);
						}
					});
				}

				std::filesystem::path filePath(binPath);
				std::filesystem::path directory = filePath.parent_path();
				if (!directory.empty() && !std::filesystem::exists(directory)) {
					std::filesystem::create_directories(directory);
				}

				std::ofstream ofs(binPath, std::ios::binary);
				if (!ofs.is_open()) return false;
				ofs.write(reinterpret_cast<const char*>(outputBuffer.data()), outputBuffer.size());

				return true;
			}

			void SerializeProjectSettings() {
				namespace fs = std::filesystem;
				using namespace NE::Core;

				fs::create_directories(projectSettingsLoc);

				Editor::Layers::LayerDatabase db;

				rapidjson::Document d;
				d.SetObject();
				auto& a = d.GetAllocator();

				d.AddMember("version", kLayerSettingsVersion, a);

				rapidjson::Value layers(rapidjson::kArrayType);
				layers.Reserve(static_cast<rapidjson::SizeType>(MAX_LAYERS), a);

				for (LayerID id = 0; id < MAX_LAYERS; ++id) {
					rapidjson::Value obj(rapidjson::kObjectType);

					const bool used = db.IsUsed(id);
					std::string_view nameSV = db.GetName(id);
					const LayerMask mask = db.GetCollisionMask(id);

					obj.AddMember("id", static_cast<uint32_t>(id), a);
					obj.AddMember("used", used, a);

					rapidjson::Value name;
					name.SetString(nameSV.data(), static_cast<rapidjson::SizeType>(nameSV.size()), a);
					obj.AddMember("name", name, a);

					obj.AddMember("collidesWith", static_cast<uint32_t>(mask), a);

					layers.PushBack(obj, a);
				}

				d.AddMember("layers", layers, a);

				rapidjson::StringBuffer sb;
				rapidjson::PrettyWriter<rapidjson::StringBuffer> w(sb);
				d.Accept(w);

				WriteAllText(layerSettingsLoc, sb.GetString());
			}

			void SerializeUserSettings() {
				namespace fs = std::filesystem;

				fs::create_directories(userSettingsFolder);

				rapidjson::Document doc;
				doc.SetObject();
				auto& a = doc.GetAllocator();

				doc.AddMember("version", kUserSettingsVersion, a);
				doc.AddMember("sessionScenePath", ToJSON(EditorScene::s_currentScenePath, a), a);
				doc.AddMember("sessionSceneUUID", ToJSON(EditorScene::s_currentSceneUUID, a), a);

				// Game panel (main camera) resolution preference.
				doc.AddMember("gameViewWidth", NE::GetGameViewWidth(), a);
				doc.AddMember("gameViewHeight", NE::GetGameViewHeight(), a);

				rapidjson::Value obj(rapidjson::kObjectType);
				obj.AddMember("position", ToJSON(EditorScene::m_editorCamera.GetPosition(), a), a);
				obj.AddMember("yaw", ToJSON(EditorScene::m_cameraYaw, a), a);
				obj.AddMember("pitch", ToJSON(EditorScene::m_cameraPitch, a), a);
				obj.AddMember("speed", ToJSON(EditorScene::m_cameraSpeed, a), a);
				obj.AddMember("minSpeed", ToJSON(EditorScene::m_cameraMinSpeed, a), a);
				obj.AddMember("maxSpeed", ToJSON(EditorScene::m_cameraMaxSpeed, a), a);
				obj.AddMember("hasEasing", ToJSON(EditorScene::m_cameraUseEasing, a), a);
				obj.AddMember("hasAcceleration", ToJSON(EditorScene::m_cameraUseAcceleration, a), a);

				doc.AddMember("editorCamera", obj, a);

				rapidjson::StringBuffer sb;
				rapidjson::PrettyWriter<rapidjson::StringBuffer> w(sb);
				doc.Accept(w);

				WriteAllText(userSettingsLoc, sb.GetString());
			}
		}
	}

	namespace Deserialization {
		namespace JSON {
			namespace {
				template <typename C>
				void ReadComponentIfPresent(NE::ECS::Entity e,
					const rapidjson::Value& ent) {
					if (!ent.HasMember(ComponentKey<C>::value)) return;
					C c{};
					FromJSON(ent[ComponentKey<C>::value], c);
					NE::ECS::Command::AddComponent<C>(e, c);
				}
			}

			void DeserializeScene(const std::string& path) {
				using rapidjson::Document;
				using rapidjson::StringBuffer;
				using rapidjson::PrettyWriter;

				std::ifstream in(path, std::ios::binary);
				if (!in) return;

				std::string data((std::istreambuf_iterator<char>(in)), {});
				Document doc; doc.Parse(data.c_str());

				if (doc.HasMember("RenderSettings")) {
					auto& rs = NE::Renderer::Command::GetRenderSettings();
					FromJSON(doc["RenderSettings"], rs);
				}

				if (doc.HasMember("PostProcessingSettings")) {
					auto& pps = NE::Renderer::Command::GetPostProcessingSettings();
					FromJSON(doc["PostProcessingSettings"], pps);
				}

				if (doc.HasMember("LightingContainer")) {
					FromJSON(doc["LightingContainer"], NE::GetScene().GetLightingContainer());
				} else {
					NE::GetScene().GetLightingContainer() = {};
				}

				if (!doc.IsObject() || !doc.HasMember("Entities")) return;

				for (auto& entVal : doc["Entities"].GetArray()) {
					NE::ECS::Entity e = NE::ECS::Command::CreateEntityNoComponents();

					if (entVal.HasMember("Layer")) {
						uint8_t layer = 0;
						FromJSON(entVal["Layer"], layer);
						NE::ECS::Command::SetLayer(e, layer);
					}

					ForEachSceneComponentType([&]<typename C>() {
						ReadComponentIfPresent<C>(e, entVal);
					});

					bool isActive = true;
					if (entVal.HasMember("Active")) {
						FromJSON(entVal["Active"], isActive);
					} else if (NE::ECS::Query::HasComponent<NE::ECS::Component::EntityMeta>(e)) {
						isActive = NE::ECS::Query::GetComponent<NE::ECS::Component::EntityMeta>(e).isActive;
					}

					NE::ECS::Command::ToggleActive(e, isActive);
					if (NE::ECS::Query::HasComponent<NE::ECS::Component::EntityMeta>(e)) {
						NE::ECS::Command::GetComponent<NE::ECS::Component::EntityMeta>(e).isActive = isActive;
					}
				}
			}

			void DeserializeModel(const std::string& metaPath) {
				using rapidjson::Document;

				std::ifstream in(metaPath, std::ios::binary);
				if (!in) return;

				std::string data((std::istreambuf_iterator<char>(in)), {});
				Document doc; doc.Parse(data.c_str());
				if (doc.HasParseError() || !doc.IsObject()) return;

				if (!doc.HasMember("submeshes") || !doc["submeshes"].IsArray()) return;
				auto& sms = doc["submeshes"];
				if (!doc.HasMember("generatedPrefab") || !doc["generatedPrefab"].IsObject()) return;
				auto& gp = doc["generatedPrefab"];
				if (!gp.HasMember("Entities") || !gp["Entities"].IsArray()) return;
				auto& ents = gp["Entities"];

				std::unordered_map<uint64_t, uint64_t> idxToReal;
				idxToReal.reserve(ents.Size());

				auto readTemplateLuid = [](const rapidjson::Value& entVal) -> uint64_t {
					if (entVal.HasMember("Hierarchy") && entVal["Hierarchy"].IsObject()) {
						auto& h = entVal["Hierarchy"];
						if (h.HasMember("luid") && h["luid"].IsUint64())
							return h["luid"].GetUint64();
					}
					return 0;
				};

				for (auto& entVal : ents.GetArray()) {
					if (!entVal.IsObject()) continue;
					uint64_t tpl = readTemplateLuid(entVal);
					if (tpl == 0) continue; 
					if (idxToReal.contains(tpl)) continue;
					idxToReal[tpl] = NE::Core::LUIDGenerator::Generate("em");
				}

				for (auto& entVal : ents.GetArray()) {
					if (!entVal.IsObject()) continue;

					NE::ECS::Entity e = NE::ECS::Command::CreateEntityNoComponents();

					if (entVal.HasMember("Layer")) {
						uint8_t layer = 0;
						FromJSON(entVal["Layer"], layer);
						NE::ECS::Command::SetLayer(e, layer);
					}

					ForEachPrefabComponentType([&]<typename C>() {
						if constexpr (std::is_same_v<C, NE::ECS::Component::Hierarchy>) {
							return;
						} else {
							ReadComponentIfPresent<C>(e, entVal);
						}
					});

					bool isActive = true;
					if (entVal.HasMember("Active")) {
						FromJSON(entVal["Active"], isActive);
					} else if (NE::ECS::Query::HasComponent<NE::ECS::Component::EntityMeta>(e)) {
						isActive = NE::ECS::Query::GetComponent<NE::ECS::Component::EntityMeta>(e).isActive;
					}

					NE::ECS::Command::ToggleActive(e, isActive);
					if (NE::ECS::Query::HasComponent<NE::ECS::Component::EntityMeta>(e)) {
						NE::ECS::Command::GetComponent<NE::ECS::Component::EntityMeta>(e).isActive = isActive;
					}

					if (NE::ECS::Query::HasComponent<NE::ECS::Component::Renderer>(e)) {
						auto& renderer = NE::ECS::Query::GetComponent<NE::ECS::Component::Renderer>(e);
						NE::Math::Vec3 pivotOffset = {};
						FromJSON(sms[renderer.subMeshIndex]["pivotOffset"], pivotOffset);

						auto& transform = NE::ECS::Command::GetComponent<NE::ECS::Component::Transform>(e);
						transform.localPosition = pivotOffset;
					}

					uint64_t tplLuid = readTemplateLuid(entVal);
					uint64_t realLuid = (tplLuid != 0 && idxToReal.contains(tplLuid)) ? idxToReal[tplLuid] : 0;

					if (entVal.HasMember("Hierarchy") && entVal["Hierarchy"].IsObject()) {
						const auto& hj = entVal["Hierarchy"];

						uint64_t tplParent = 0;
						if (hj.HasMember("parentLuid") && hj["parentLuid"].IsUint64())
							tplParent = hj["parentLuid"].GetUint64();

						uint64_t realParent = 0;
						if (tplParent != 0 && idxToReal.contains(tplParent))
							realParent = idxToReal[tplParent];

						NE::ECS::Component::Hierarchy h{};
						h.luid = realLuid;
						h.parentLuid = realParent;

						NE::ECS::Command::AddComponent(e, h);
					}
				}

				EditorScene::BuildRoot();
			}

			void DeserializeProjectSettings() {
				namespace fs = std::filesystem;
				using namespace NE::Core;

				//fs::create_directories(projectSettingsLoc);

				Editor::Layers::LayerDatabase db;

				if (!fs::exists(layerSettingsLoc)) {
					Serialization::JSON::SerializeProjectSettings();
					return;
				}

				std::string text;
				if (!ReadAllText(layerSettingsLoc, text))
					return;

				rapidjson::Document d;
				d.Parse(text.c_str());
				if (d.HasParseError() || !d.IsObject())
					return;

				if (!d.HasMember("layers") || !d["layers"].IsArray())
					return;

				// 1) Apply names/used flags first
				//    (we treat "used=false" or empty name as delete/clear)
				for (auto& it : d["layers"].GetArray()) {
					if (!it.IsObject()) continue;
					if (!it.HasMember("id") || !it["id"].IsUint()) continue;

					const uint32_t idU = it["id"].GetUint();
					if (idU >= MAX_LAYERS) continue;

					const LayerID id = static_cast<LayerID>(idU);

					// Layer 0 is always Default; ignore file attempts to rename/disable it.
					if (id == 0) continue;

					bool used = true;
					if (it.HasMember("used") && it["used"].IsBool())
						used = it["used"].GetBool();

					std::string_view nameSV{};
					if (it.HasMember("name") && it["name"].IsString())
						nameSV = it["name"].GetString();

					if (!used || nameSV.empty()) {
						// Clear layer slot (your RenameLayer supports empty -> used=false)
						db.RenameLayer(id, "");
					} else {
						// Set/overwrite (RenameLayer works even if slot was unused)
						db.RenameLayer(id, nameSV);
					}
				}

				// 2) Read collision masks (full matrix), then symmetrize, then apply
				std::array<LayerMask, MAX_LAYERS> loadedMasks{};
				for (LayerID i = 0; i < MAX_LAYERS; ++i)
					loadedMasks[i] = db.GetCollisionMask(i); // fallback to current if not present in file

				for (auto& it : d["layers"].GetArray()) {
					if (!it.IsObject()) continue;
					if (!it.HasMember("id") || !it["id"].IsUint()) continue;

					const uint32_t idU = it["id"].GetUint();
					if (idU >= MAX_LAYERS) continue;

					const LayerID id = static_cast<LayerID>(idU);

					if (it.HasMember("collidesWith") && it["collidesWith"].IsUint())
						loadedMasks[id] = static_cast<LayerMask>(it["collidesWith"].GetUint());
				}

				const auto symMasks = SymmetrizeAND(loadedMasks);

				for (LayerID i = 0; i < MAX_LAYERS; ++i)
					db.SetCollisionMask(i, symMasks[i]);
			}

			void DeserializeUserSettings() {
				namespace fs = std::filesystem;

				if (!fs::exists(userSettingsLoc)) {
					Serialization::JSON::SerializeUserSettings();
					return;
				}
				std::string text;
				if (!ReadAllText(userSettingsLoc, text))
					return;

				rapidjson::Document doc;
				doc.Parse(text.c_str());
				if (doc.HasParseError() || !doc.IsObject())
					return;

				if (doc.HasMember("sessionScenePath") && doc["sessionScenePath"].IsString())
					EditorScene::s_currentScenePath = doc["sessionScenePath"].GetString();

				if (doc.HasMember("sessionSceneUUID") && doc["sessionSceneUUID"].IsString())
					EditorScene::s_currentSceneUUID = doc["sessionSceneUUID"].GetString();

				if (doc.HasMember("gameViewWidth") && doc["gameViewWidth"].IsUint() &&
					doc.HasMember("gameViewHeight") && doc["gameViewHeight"].IsUint())
				{
					const uint32_t w = doc["gameViewWidth"].GetUint();
					const uint32_t h = doc["gameViewHeight"].GetUint();
					NE::SetGameViewResolution(w, h);
				}

				if (doc.HasMember("editorCamera") && doc["editorCamera"].IsObject()) {
					auto& camObj = doc["editorCamera"];
					if (camObj.HasMember("position")) {
						NE::Math::Vec3 position{ 0.f, 0.f, 0.f };
						FromJSON(camObj["position"], position);
						EditorScene::m_editorCamera.SetPosition(position);
					}
					if (camObj.HasMember("yaw"))
						FromJSON(camObj["yaw"], EditorScene::m_cameraYaw);
					if (camObj.HasMember("pitch"))
						FromJSON(camObj["pitch"], EditorScene::m_cameraPitch);
					if (camObj.HasMember("speed"))
						FromJSON(camObj["speed"], EditorScene::m_cameraSpeed);
					if (camObj.HasMember("minSpeed"))
						FromJSON(camObj["minSpeed"], EditorScene::m_cameraMinSpeed);
					if (camObj.HasMember("maxSpeed"))
						FromJSON(camObj["maxSpeed"], EditorScene::m_cameraMaxSpeed);
					if (camObj.HasMember("hasEasing"))
						FromJSON(camObj["hasEasing"], EditorScene::m_cameraUseEasing);
					if (camObj.HasMember("hasAcceleration"))
						FromJSON(camObj["hasAcceleration"], EditorScene::m_cameraUseAcceleration);
				}
			}
		}
	}
}


