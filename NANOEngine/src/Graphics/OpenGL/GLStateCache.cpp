#include "pch.h"
#include "GLStateCache.hpp"
#include <glad/glad.h>

namespace NE::Graphics::OpenGL {

	GLStateCache::GLStateCache()
		: m_CurrentState({}), m_Valid(false)
	{
	}

	GLStateCache::GLStateCache(PipelineSpecification const& p)
		: m_CurrentState(p), m_Valid(false)
	{
	}

	void GLStateCache::InvalidateAll()
	{
		m_Valid = false;
	}

	void GLStateCache::Reset() 
	{
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
		glDisable(GL_CULL_FACE);
		glDisable(GL_DEPTH_TEST);
		glDepthMask(GL_FALSE);
		glDisable(GL_BLEND);
		m_Valid = false;
	}

	void GLStateCache::Bind(const PipelineSpecification& spec)
	{
		if (!spec.shader) return;

		if (!m_Valid) {
			// Apply all states
			// Blending
			if (spec.EnableBlending) {
				glEnable(GL_BLEND);
				glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			}
			else {
				glDisable(GL_BLEND);
			}
			// Depth test
			if (spec.EnableDepthTest) {
				glEnable(GL_DEPTH_TEST);
			}
			else {
				glDisable(GL_DEPTH_TEST);
			}
			// Depth write
			if (spec.DepthWrite) {
				glDepthMask(GL_TRUE);
			}
			else {
				glDepthMask(GL_FALSE);
			}
			// Culling
			if (spec.CullMode != GL_NONE) {
				glEnable(GL_CULL_FACE);
				glCullFace(spec.CullMode);
			}
			else {
				glDisable(GL_CULL_FACE);
			}
			// Polygon mode
			glPolygonMode(GL_FRONT_AND_BACK, spec.PolygonMode);
			// Shader
			spec.shader->Bind();

			// Update current state
			m_CurrentState = spec;
			m_Valid = true;
		}
		else {
			// Apply only changed states
			// Blending
			if (m_CurrentState.EnableBlending != spec.EnableBlending) {
				if (spec.EnableBlending) {
					glEnable(GL_BLEND);
					glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
				}
				else {
					glDisable(GL_BLEND);
				}
				m_CurrentState.EnableBlending = spec.EnableBlending;
			}
			// Depth test
			if (m_CurrentState.EnableDepthTest != spec.EnableDepthTest) {
				if (spec.EnableDepthTest) {
					glEnable(GL_DEPTH_TEST);
				}
				else {
					glDisable(GL_DEPTH_TEST);
				}
				m_CurrentState.EnableDepthTest = spec.EnableDepthTest;
			}
			// Depth write
			if (m_CurrentState.DepthWrite != spec.DepthWrite) {
				if (spec.DepthWrite) {
					glDepthMask(GL_TRUE);
				}
				else {
					glDepthMask(GL_FALSE);
				}
				m_CurrentState.DepthWrite = spec.DepthWrite;
			}
			// Culling
			if (m_CurrentState.CullMode != spec.CullMode) {
				if (spec.CullMode != GL_NONE) {
					glEnable(GL_CULL_FACE);
					glCullFace(spec.CullMode);
				}
				else {
					glDisable(GL_CULL_FACE);
				}
				m_CurrentState.CullMode = spec.CullMode;
			}
			// Polygon mode
			if (m_CurrentState.PolygonMode != spec.PolygonMode) {
				glPolygonMode(GL_FRONT_AND_BACK, spec.PolygonMode);
				m_CurrentState.PolygonMode = spec.PolygonMode;
			}
			// Shader
			if (m_CurrentState.shader->GetProgramID() != spec.shader->GetProgramID()) {
				spec.shader->Bind();
				m_CurrentState.shader = spec.shader;
			}
		}
	}

	void GLStateCache::Bind(const std::shared_ptr<IPipeline>& pipeline) {
		if (pipeline) {
			Bind(pipeline->GetSpecification());
		}
	}
}