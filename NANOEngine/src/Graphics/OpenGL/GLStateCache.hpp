#pragma once
#include "../Interfaces/IStateCache.hpp"

namespace NE::Graphics::OpenGL {

	class GLStateCache final : public IStateCache {
	public:
		GLStateCache();
		GLStateCache(PipelineSpecification const& p);
		~GLStateCache() = default;

		void InvalidateAll() override;
		void Bind(const PipelineSpecification& spec) override;
		void Bind(const std::shared_ptr<IPipeline>&) override;

	private:
		PipelineSpecification m_CurrentState;
		bool m_Valid = false;
	};

}
