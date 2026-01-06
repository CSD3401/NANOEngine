#include "Serializer.hpp"

#include <fstream>

#include <rapidjson/document.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/stringbuffer.h>

// Components
#include <ECS/Components/EntityMeta.hpp>
#include <ECS/Components/Transform.hpp>
#include <ECS/Components/Renderer.hpp>
#include <ECS/Components/Light.hpp>
#include <ECS/Components/Collider.hpp>
#include <ECS/Components/Rigidbody.hpp>
#include <ECS/Components/NativeScript.hpp>
#include <ECS/Components/Camera.hpp>
#include <ECS/Components/UIRectTransform.hpp>
#include <ECS/Components/UICanvas.hpp>
#include <ECS/Components/UIImage.hpp>
#include <ECS/Components/Hierarchy.hpp>
#include <EditorInterface/ECSExports.hpp>
#include <EditorInterface/RendererExports.hpp>
#include <ECS/Components/ComponentKey.hpp>
#include <Graphics/Core/RenderSettings.hpp>
#include <Graphics/Core/PostProcessingSettings.hpp>

#include "JSONReflection.hpp"
#include "../EditorScene.hpp"
#include <Scripting/ScriptingEngine.hpp>

namespace Editor {
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

				using ComponentTypes = std::tuple<
					NE::ECS::Component::EntityMeta,
					NE::ECS::Component::Hierarchy,
					NE::ECS::Component::Transform,
					NE::ECS::Component::Renderer,
					NE::ECS::Component::Light,
					NE::ECS::Component::Collider,
					NE::ECS::Component::Rigidbody,
					NE::ECS::Component::NativeScript,
					NE::ECS::Component::Camera,
					NE::ECS::Component::UIRectTransform,
					NE::ECS::Component::UICanvas,
					NE::ECS::Component::UIImage
				>;

				template <class F>
				void ForEachComponentType(F&& f) {
					std::apply([&](auto&&... t) {
						(f.template operator() < std::decay_t<decltype(t)> > (), ...);
						}, ComponentTypes{});
				}

				rapidjson::Value WriteEntityRecursive(
					NE::ECS::Entity e,
					rapidjson::Value& entities,
					rapidjson::Document::AllocatorType& a) {
					rapidjson::Value ent(rapidjson::kObjectType);

					ForEachComponentType([&]<typename C>() {
						WriteComponentIfPresent<C>(e, ent, a);
					});

					entities.PushBack(ent, a);

					auto& h = NE::ECS::Query::GetEntityHierarchy(e);
					for (auto childId : h.children) {
						NE::ECS::Entity child = static_cast<NE::ECS::Entity>(childId);
						WriteEntityRecursive(child, entities, a);
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
				auto& allEntities = coordinator.GetUsedEntities();

				for (NE::ECS::Entity entity : allEntities) {
					if (componentManager.HasComponent<NE::ECS::Component::NativeScript>(entity)) {
						auto& nsc = componentManager.GetComponent<NE::ECS::Component::NativeScript>(entity);
						NE::Scripting::ScriptingEngine::GetInstance().SaveSerializedFields(entity, nsc);
					}
				}

				Document doc;
				doc.SetObject();
				auto& a = doc.GetAllocator();

				auto& renderSettings = NE::Renderer::Query::GetRenderSettings();
				doc.AddMember("RenderSettings", ToJSON(renderSettings, a), a);

				auto& postProcessingSettings = NE::Renderer::Query::GetPostProcessingSettings();
				doc.AddMember("PostProcessingSettings", ToJSON(postProcessingSettings, a), a);

				Value entities(rapidjson::Type::kArrayType);
				const auto& sceneRoots = EditorScene::s_rootOrder;
				for (auto e : sceneRoots) {
					WriteEntityRecursive(e, entities, a);
				}
				doc.AddMember("Entities", entities, a);

				StringBuffer sb;
				PrettyWriter<rapidjson::StringBuffer> wr(sb);
				doc.Accept(wr);

				std::ofstream out(path, std::ios::binary);
				if (out) out.write(sb.GetString(), static_cast<std::streamsize>(sb.GetSize()));
			}

			void SerializePrefab(const std::string& path) {

			}

			void SerializeEditorSettings() {

			}
		}
	}
}