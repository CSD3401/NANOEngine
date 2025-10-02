#include "SceneManager.hpp"
#include "Serialisation/JsonSceneSerializer.hpp"

namespace NE::SceneManagement {

	void SceneManager::LoadScene(const std::string& path) {
		m_loadedPath = path;
		m_active = std::make_unique<NE::SceneManagement::Scene>();
		NE::Serialization::JsonSceneSerializer::Deserialize(*m_active, path);
		m_active->Init();
	}

	void SceneManager::ReloadScene() {
		if (!m_active || m_loadedPath.empty()) return;
		m_active->Exit();
		m_active = std::make_unique<NE::SceneManagement::Scene>();
		m_active->Init();
		Serialization::JsonSceneSerializer::Deserialize(*m_active, m_loadedPath);
	}

	void SceneManager::SaveScene() {
		//if (!m_active) return;
		//Serialization::JsonSceneSerializer::Serialize(*m_active, path);
	}

	void SceneManager::Update(double dt) {
		if (m_active) m_active->Update(dt);
	}

	void SceneManager::Render(NE::SceneManagement::RenderPass pass) {
		if (m_active) m_active->Render(pass);
	}

	void SceneManager::ExitScene() {
		if (m_active) m_active->Exit();
	}

}
