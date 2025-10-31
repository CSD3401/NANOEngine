#pragma once
#include <vector>
#include "DrawCommand.hpp"
#include "PipelineData.hpp" // for PipelineKey, PipelineSpecification

namespace NE::Graphics {

	class Camera; // Forward declaration

	class DrawQueue {
	public:
		struct KeyPair {
			PipelineKey a;
			PipelineKey b;
			KeyPair(const PipelineKey& a, const PipelineKey& b);
			bool operator==(const KeyPair& other) const;
		};
		struct KeyPairHash {
			std::size_t operator()(const KeyPair& k) const;
		};

		int GetCost(const PipelineSpecification& a, const PipelineSpecification& b);

		void Clear();
		void Submit(const DrawCommand& cmd);
		void Sort(const Camera* camera);

		const std::vector<DrawCommand>& GetCommands() const { return m_Commands; }
	private:
		std::vector<DrawCommand> m_Commands;
		std::unordered_map<KeyPair, int, KeyPairHash> m_CostCache;

		// Settings
		bool greedyOptimizeGroups = true; // whether to do the greedy group optimization in opaque sorting
	};

}