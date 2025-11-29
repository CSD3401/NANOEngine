#pragma once

namespace Editor::Layers {
	static const char* kLayerNames[] = {
		"Default",      // 0
		"Environment",  // 1
		"Player",       // 2
		"Enemy",        // 3
		"UI",           // 4
		"Layer5",
		"Layer6",
		"Layer7",
		"Layer8",
		"Layer9",
		"Layer10",
		"Layer11",
		"Layer12",
		"Layer13",
		"Layer14"
	};

	inline int GetLayerCount() {
		return (int)(sizeof(kLayerNames) / sizeof(kLayerNames[0]));
	}

	inline const char* GetLayerName(int idx) {
		if (idx >= 0 && idx < GetLayerCount())
			return kLayerNames[idx];
		return "Unknown";
	}
}