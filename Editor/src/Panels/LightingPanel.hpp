#pragma once

#include "IPanel.hpp"
#include "../Lighting/DirectLightmapBaker.hpp"
#include <unordered_map>
#include <vector>
#include <string>

namespace Editor {
	class LightingPanel : public IPanel {
	public:
		LightingPanel() = default;
		virtual ~LightingPanel() override;

		virtual void OnImGuiRender() override;

	private:
		struct PreviewTextureSet {
			unsigned int lightingTexture = 0;
			unsigned int validityTexture = 0;
		};

		void ReleasePreviewTextures();
		void SyncPreviewTextures(const Editor::Lightmapping::DirectLightBakeSessionState& sessionState);

		float m_texelsPerUnit = 16.0f;
		int m_bvhLeafSize = 8;
		float m_bvhTraversalEpsilon = 1e-5f;
		int m_directBakeWorkerCount = 0;
		float m_directBakeRayBias = 1e-3f;
		float m_directBakeRayMinDistance = 1e-4f;
		float m_directBakeFiniteLightEpsilon = 1e-3f;
		float m_directBakePreviewExposure = 1.0f;
		uint64_t m_cachedBakeRevision = 0;
		std::unordered_map<std::string, PreviewTextureSet> m_previewTextures;
	};
}
