#pragma once

#include "IPanel.hpp"
#include "../Lighting/DirectLightmapBaker.hpp"
#include <string>

namespace Editor {
	class LightingPanel : public IPanel {
	public:
		LightingPanel() = default;
		virtual ~LightingPanel() override;

		virtual void OnImGuiRender() override;

	private:
		float m_texelsPerUnit = 16.0f;
		int m_bvhLeafSize = 8;
		float m_bvhTraversalEpsilon = 1e-5f;
		int m_directBakeWorkerCount = 0;
		float m_directBakeRayBias = 1e-3f;
		float m_directBakeRayMinDistance = 1e-4f;
		float m_directBakeFiniteLightEpsilon = 1e-3f;
		float m_directBakePreviewExposure = 1.0f;
		bool m_rasterSelfCheckPassed = false;
		std::string m_rasterSelfCheckMessage;
		bool m_outputSelfCheckPassed = false;
		std::string m_outputSelfCheckMessage;
	};
}
