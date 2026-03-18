#include "pch.h"
#include <algorithm>
#include "DrawQueue.hpp"
#include "EditorCamera.hpp"
#include "Core/Profiler.hpp"

namespace NE::Graphics {

	DrawQueue::KeyPair::KeyPair(const PipelineKey& a, const PipelineKey& b) {
		if (b < a) {
			this->a = b;
			this->b = a;
		}
		else {
			this->a = a;
			this->b = b;
		}
	}

	bool DrawQueue::KeyPair::operator==(const KeyPair& other) const {
		return a == other.a && b == other.b;
	}

	std::size_t DrawQueue::KeyPairHash::operator()(const KeyPair& k) const {
		const uint64_t pa = k.a.Pack();
		const uint64_t pb = k.b.Pack();
		size_t h = std::hash<uint64_t>{}(pa);
		h ^= std::hash<uint64_t>{}(pb)+0x9e3779b97f4a7c15ull + (h << 6) + (h >> 2);
		return h;
	}

	int DrawQueue::GetCost(const PipelineSpecification& a, const PipelineSpecification& b) {
		static const int COST_SHADER = 100;
		static const int COST_BLEND = 12;
		static const int COST_DEPTH_TEST = 10;
		static const int COST_DEPTH_WRITE = 8;
		static const int COST_CULL_MODE = 2;
		static const int COST_POLYGON_MODE = 1;

		PipelineKey ka = PipelineKey::MakeKey(a);
		PipelineKey kb = PipelineKey::MakeKey(b);
		KeyPair kp(ka, kb);
		auto it = m_CostCache.find(kp);
		if (it != m_CostCache.end()) {
			return it->second;
		}
		else {
			int cost = 0;
			if (a.shader != b.shader)
				cost += COST_SHADER;
			if (a.EnableBlending != b.EnableBlending)
				cost += COST_BLEND;
			if (a.EnableDepthTest != b.EnableDepthTest)
				cost += COST_DEPTH_TEST;
			if (a.DepthWrite != b.DepthWrite)
				cost += COST_DEPTH_WRITE;
			if (a.CullMode != b.CullMode)
				cost += COST_CULL_MODE;
			if (a.PolygonMode != b.PolygonMode)
				cost += COST_POLYGON_MODE;
			m_CostCache.emplace(kp, cost);
			return cost;
		}
	}

	void DrawQueue::Clear() {
		m_Commands.clear();
		m_ParticleCommands.clear();
	}

	void DrawQueue::Submit(const DrawCommand& cmd) {
		m_Commands.push_back(cmd);
	}

	void DrawQueue::Submit(const ParticleDrawCommand& cmd) {
		m_ParticleCommands.push_back(cmd);
	}

	void DrawQueue::Sort(const Vec3& camPos) {
#ifndef PRODUCTION_BUILD
		NE_PROFILE_FUNCTION();
#endif

		if (m_Commands.size() < 2) return;

		// Step 1: Sort by Render Queue Order (base + offset)
		// Stable sort to maintain input order for same order items
		std::stable_sort(m_Commands.begin(), m_Commands.end(), [](const DrawCommand& a, const DrawCommand& b) {
			uint16_t orderA = a.material ? a.material->GetQueueOrder() : 0;
			uint16_t orderB = b.material ? b.material->GetQueueOrder() : 0;
			return orderA < orderB;
			});

		size_t i = 0;
		while (i < m_Commands.size()) {
			const RenderQueue currentRQ = m_Commands[i].material->GetQueueBase();
			// Find the half open range [i, j) of commands with the same Render Queue
			size_t j = i + 1;
			while (j < m_Commands.size() && m_Commands[j].material->GetQueueBase() == currentRQ) ++j;

			switch (currentRQ) {
			case RenderQueue::BACKGROUND:
			case RenderQueue::GEOMETRY:
			case RenderQueue::ALPHATEST: {
				// Step 2a: Within this range, sort by PipelineKey to minimize state changes
				std::stable_sort(m_Commands.begin() + i, m_Commands.begin() + j,
					[](const DrawCommand& a, const DrawCommand& b) {
						const auto pa = a.material ? a.material->GetPipeline() : nullptr;
						const auto pb = b.material ? b.material->GetPipeline() : nullptr;
						// First sort by PipelineKey
						if (pa != pb) {
							if (!pa) return false;
							if (!pb) return true;
							return pa->GetKey() < pb->GetKey();
						}
						// Then sort by mesh
						else {
							return a.mesh.get() < b.mesh.get();
						}
					});

				// Further optimize within this range by grouping commands with less state change cost between their pipelines
				if (!greedyOptimizeGroups) break;
				struct Group {
					PipelineKey key{};
					PipelineSpecification spec{};
					std::vector<size_t> indices; // indices into m_Commands
				};
				std::vector<Group> groups;
				size_t largestGroupIdx = 0;
				groups.reserve(16);
				size_t kBegin = i;
				while (kBegin < j) {
					const auto& cmd0 = m_Commands[kBegin];
					if (!cmd0.material || !cmd0.material->GetPipeline()) {
						Group g;
						g.indices.push_back(kBegin);
						groups.push_back(std::move(g));
						++kBegin;
						continue;
					}
					const auto& pipe0 = cmd0.material->GetPipeline();
					const auto& spec0 = pipe0->GetSpecification();
					const auto& key0 = pipe0->GetKey();

					size_t kEnd = kBegin + 1;
					for (; kEnd < j; ++kEnd) {
						const auto& cmd = m_Commands[kEnd];
						if (!cmd.material || !cmd.material->GetPipeline()) break;
						const auto& key1 = cmd.material->GetPipeline()->GetKey();
						if (!(key1 == key0)) break; // end of this equal-key run
					}
					Group g;
					g.key = key0;
					g.spec = spec0;
					g.indices.reserve(kEnd - kBegin);
					for (size_t p = kBegin; p < kEnd; ++p) g.indices.push_back(p);
					groups.push_back(std::move(g));
					kBegin = kEnd;

					if (groups.back().indices.size() > groups[largestGroupIdx].indices.size())
						largestGroupIdx = groups.size() - 1;
				}
				if (groups.size() > 1) {
					std::vector<size_t> order;
					std::vector<bool> added(groups.size(), false);
					order.reserve(groups.size());

					// Start with the largest group
					order.push_back(largestGroupIdx);
					added[largestGroupIdx] = true;

					// Greedily add the next group with the lowest cost
					for (size_t step = 1; step < groups.size(); ++step) {
						int bestCost = INT_MAX;
						size_t bestIdx = static_cast<size_t>(-1);

						for (size_t g = 0; g < groups.size(); ++g) {
							if (added[g]) continue;
							int cost = GetCost(groups[order.back()].spec, groups[g].spec);
							// Choose the group with the lowest cost, break ties by larger group size
							if (cost < bestCost || (cost == bestCost && groups[g].indices.size() > groups[bestIdx].indices.size())) {
								bestCost = cost;
								bestIdx = g;
							}
						}
						added[bestIdx] = true;
						order.push_back(bestIdx);
					}

					// Reorder m_Commands in [i, j) according to 'order'
					std::vector<DrawCommand> sortedCommands;
					sortedCommands.reserve(j - i);
					for (size_t gIdx : order) {
						for (size_t cmdIdx : groups[gIdx].indices) {
							sortedCommands.push_back(std::move(m_Commands[cmdIdx]));
						}
					}
					std::move(sortedCommands.begin(), sortedCommands.end(), m_Commands.begin() + i);
				}
			} break;
			case RenderQueue::TRANSPARENT: {
				// Step 2b: Sort back to front based on distance to camera
				std::stable_sort(m_Commands.begin() + i, m_Commands.begin() + j,
					[&](const DrawCommand& a, const DrawCommand& b) {
						const float da = (a.transform.GetCol3(3) - camPos).LengthSquared();
						const float db = (b.transform.GetCol3(3) - camPos).LengthSquared();
						return da > db; // farther first (back-to-front)
					});
			} break;
			case RenderQueue::OVERLAY: {
				// Keep original submission order (stable sort already done)
			} break;
			} // end switch
			i = j; // move to the next range
		}
	}
}