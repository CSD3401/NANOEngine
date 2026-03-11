#include "pch.h"
#include "ShadowRenderer.hpp"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <unordered_set>

#include <glad/glad.h>

#include "ResourceManagement/ResourceManager.hpp"
#include "RenderViewManager.hpp"
#include "DrawCommand.hpp"
#include "Frustum.hpp"

#include "ECS/Components/Light.hpp"
#include "Core/Profiler.hpp"

namespace NE::Graphics {
	namespace {
		float Radians(float deg) {
			return deg * 3.14159265358979323846f / 180.0f;
		}

		inline Math::Vec3 TransformPoint(const Math::Mat4& m, const Math::Vec3& p, float w = 1.0f) {
			const float* a = m.Data();

			float x = a[0] * p.x + a[4] * p.y + a[8] * p.z + a[12] * w;
			float y = a[1] * p.x + a[5] * p.y + a[9] * p.z + a[13] * w;
			float z = a[2] * p.x + a[6] * p.y + a[10] * p.z + a[14] * w;
			float ww = a[3] * p.x + a[7] * p.y + a[11] * p.z + a[15] * w;

			if (std::abs(ww) > 1e-6f) {
				float invW = 1.0f / ww;
				x *= invW; y *= invW; z *= invW;
			}
			return { x, y, z };
		}

		inline Math::Vec3 TransformVector(const Math::Mat4& m, const Math::Vec3& v) {
			return TransformPoint(m, v, 0.0f);
		}

		inline void BuildStableBasisFromDir(const Math::Vec3& dirN, Math::Vec3& outRight, Math::Vec3& outUp) {
			Math::Vec3 worldUp = { 0.f, 1.f, 0.f };

			float d = std::abs(dirN.Dot(worldUp));
			if (d > 0.99f) worldUp = { 0.f, 0.f, 1.f };

			outRight = worldUp.Cross(dirN).Normalized();
			outUp = dirN.Cross(outRight).Normalized();
		}

		Math::Mat4 BuildDirectionalLightVP_FitToView(const RenderView& view, Math::Vec3 lightDirWorld, int shadowRes) {
			using Math::Vec3;
			using Math::Mat4;

			Vec3 dir = lightDirWorld.Normalized();

			Mat4 viewProj = view.projection * view.view;
			Mat4 invViewProj = viewProj.Inverse();

			const Vec3 ndcCorners[8] = {
				{ -1, -1, -1 }, {  1, -1, -1 }, {  1,  1, -1 }, { -1,  1, -1 },
				{ -1, -1,  1 }, {  1, -1,  1 }, {  1,  1,  1 }, { -1,  1,  1 }
			};

			Vec3 frustumWS[8];
			for (int i = 0; i < 8; ++i)
				frustumWS[i] = TransformPoint(invViewProj, ndcCorners[i], 1.0f);

			Vec3 center = { 0, 0, 0 };
			for (auto& p : frustumWS) center += p;
			center *= (1.0f / 8.0f);

			Vec3 right, up;
			BuildStableBasisFromDir(dir, right, up);

			float radius = 0.0f;
			for (auto& p : frustumWS) {
				float dist = (p - center).Length();
				radius = std::max(radius, dist);
			}

			Vec3 eye = center - dir * (radius + 50.0f);
			Mat4 lightView = Mat4::BuildViewMtx(eye, center, up);

			float minX = +FLT_MAX, minY = +FLT_MAX, minZ = +FLT_MAX;
			float maxX = -FLT_MAX, maxY = -FLT_MAX, maxZ = -FLT_MAX;

			for (auto& pWS : frustumWS) {
				Vec3 pLS = TransformPoint(lightView, pWS, 1.0f);
				minX = std::min(minX, pLS.x); maxX = std::max(maxX, pLS.x);
				minY = std::min(minY, pLS.y); maxY = std::max(maxY, pLS.y);
				minZ = std::min(minZ, pLS.z); maxZ = std::max(maxZ, pLS.z);
			}

			float extentX = maxX - minX;
			float extentY = maxY - minY;

			float unitsPerTexelX = extentX / float(shadowRes);
			float unitsPerTexelY = extentY / float(shadowRes);

			extentX = std::ceil(extentX / unitsPerTexelX) * unitsPerTexelX;
			extentY = std::ceil(extentY / unitsPerTexelY) * unitsPerTexelY;

			unitsPerTexelX = extentX / float(shadowRes);
			unitsPerTexelY = extentY / float(shadowRes);

			float cx = 0.5f * (minX + maxX);
			float cy = 0.5f * (minY + maxY);
			cx = std::floor(cx / unitsPerTexelX) * unitsPerTexelX;
			cy = std::floor(cy / unitsPerTexelY) * unitsPerTexelY;

			minX = cx - 0.5f * extentX;
			maxX = cx + 0.5f * extentX;
			minY = cy - 0.5f * extentY;
			maxY = cy + 0.5f * extentY;

			const float padZ = 100.0f;
			float nearP = -maxZ - padZ;
			float farP = -minZ + padZ;

			if (nearP < 0.1f) nearP = 0.1f;
			if (farP <= nearP) farP = nearP + 1.0f;

			Mat4 lightProj = Mat4::BuildOrtho(minX, maxX, minY, maxY, nearP, farP);
			return lightProj * lightView;
		}

		std::array<NE::Math::Vec3, 4> GetViewSpaceCornerDirs(const NE::Math::Mat4& invProj)
		{
			using NE::Math::Vec3;
			using NE::Math::Vec4;

			const Vec3 ndc[4] = {
				{ -1, -1,  1 },
				{  1, -1,  1 },
				{  1,  1,  1 },
				{ -1,  1,  1 }
			};

			std::array<Vec3, 4> dirs;
			for (int i = 0; i < 4; ++i) {
				// point on far plane in view space
				Vec3 pVS = TransformPoint(invProj, ndc[i], 1.0f);

				// ray direction from camera origin
				Vec3 d = pVS.Normalized();

				// ensure it points forward (negative z in view space)
				// (optional assert)
				// if (d.z >= -1e-6f) ...

				dirs[i] = d;
			}
			return dirs;
		}

		inline void DeleteTexture(uint32_t& id) {
			if (id != 0) {
				GLuint glId = id;
				glDeleteTextures(1, &glId);
				id = 0;
			}
		}

		inline void DeleteFramebuffer(uint32_t& id) {
			if (id != 0) {
				GLuint glId = id;
				glDeleteFramebuffers(1, &glId);
				id = 0;
			}
		}
	}

	void ShadowRenderer::Init() {
		if (!m_shadowShader) {
			m_shadowShader = Resource::ResourceManager::GetInstance()
				.LoadResource<OpenGL::GLShader>("neshadowdepth");
		}
	}

	void ShadowRenderer::Update(const RenderView& view,
		const std::vector<RenderLightRef>& lights,
		const std::vector<DrawCommand>& commands
	) {
#ifndef PRODUCTION_BUILD
		NE_PROFILE_FUNCTION();
#endif
		PruneUnusedRuntimes(lights);

		if (lights.empty() || commands.empty()) return;

		for (const auto& lightRef : lights) {
			ECS::Component::Light* light = lightRef.light;
			if (!light) continue;

			LightShadowRuntime& runtime = GetOrCreateRuntime(lightRef.entity);

			if (light->shadowType == ECS::Component::Light::ShadowType::None ||
				light->type == ECS::Component::Light::Type::Point)
				continue;

			switch (light->shadowUpdateMode) {
			case ECS::Component::Light::ShadowUpdateMode::NoneUpdate:
				continue;

			case ECS::Component::Light::ShadowUpdateMode::StaticBake:
				if (runtime.shadowBaked)
					continue;
				break;

			case ECS::Component::Light::ShadowUpdateMode::Realtime:
				runtime.shadowBaked = false;
				break;
			}

			if (light->type == ECS::Component::Light::Directional) {
				EnsureResources2D(*light, runtime);

				ComputeDirectionalSplits(view, runtime);
				for (int c = 0; c < ECS::Component::Light::DIR_CASCADES; ++c) {
					Math::Mat4 vp = BuildDirectionalCascadeVP(view, *light, runtime, c);
					runtime.dirLightVP[c] = vp;
					RenderDepthDirectionalCascade(runtime, c, vp, commands);
				}

				runtime.shadowCascadeCount = ECS::Component::Light::DIR_CASCADES;

				if (light->shadowUpdateMode == ECS::Component::Light::ShadowUpdateMode::StaticBake)
					runtime.shadowBaked = true;

				continue;
			}

			EnsureResources2D(*light, runtime);

			Math::Mat4 lightVP = BuildLightVP(*light);
			runtime.lightViewProj = lightVP;
			runtime.shadowCascadeCount = 1;
			RenderDepth(runtime, lightVP, commands);

			if (light->shadowUpdateMode == ECS::Component::Light::ShadowUpdateMode::StaticBake)
				runtime.shadowBaked = true;
		}
	}

	LightShadowRuntime* ShadowRenderer::FindRuntime(ECS::Entity entity) {
		auto it = m_lightShadowRuntime.find(entity);
		return it != m_lightShadowRuntime.end() ? &it->second : nullptr;
	}

	const LightShadowRuntime* ShadowRenderer::FindRuntime(ECS::Entity entity) const {
		auto it = m_lightShadowRuntime.find(entity);
		return it != m_lightShadowRuntime.end() ? &it->second : nullptr;
	}

	void  ShadowRenderer::Shutdown() {
		for (auto& [_, runtime] : m_lightShadowRuntime) {
			ReleaseRuntimeResources(runtime);
		}
		m_lightShadowRuntime.clear();
		m_shadowShader.reset();
	}

	LightShadowRuntime& ShadowRenderer::GetOrCreateRuntime(ECS::Entity entity) {
		return m_lightShadowRuntime[entity];
	}

	void ShadowRenderer::PruneUnusedRuntimes(const std::vector<RenderLightRef>& lights) {
		std::unordered_set<ECS::Entity> activeLights;
		activeLights.reserve(lights.size());

		for (const auto& lightRef : lights) {
			if (lightRef.light) {
				activeLights.insert(lightRef.entity);
			}
		}

		for (auto it = m_lightShadowRuntime.begin(); it != m_lightShadowRuntime.end();) {
			if (activeLights.find(it->first) != activeLights.end()) {
				++it;
				continue;
			}

			ReleaseRuntimeResources(it->second);
			it = m_lightShadowRuntime.erase(it);
		}
	}

	void ShadowRenderer::ReleaseRuntimeResources(LightShadowRuntime& runtime) {
		DeleteTexture(runtime.shadowMapTex);
		DeleteFramebuffer(runtime.shadowMapFBO);

		for (int c = 0; c < LightShadowRuntime::DIR_CASCADES; ++c) {
			DeleteTexture(runtime.dirShadowTex[c]);
			DeleteFramebuffer(runtime.dirShadowFBO[c]);
		}

		runtime = LightShadowRuntime{};
	}

	void ShadowRenderer::EnsureResources2D(const ECS::Component::Light& light, LightShadowRuntime& runtime) {
		if (light.type == ECS::Component::Light::Directional) {
			for (int c = 0; c < ECS::Component::Light::DIR_CASCADES; ++c) {
				if (runtime.dirShadowTex[c] == 0)
					glGenTextures(1, &runtime.dirShadowTex[c]);

				if (runtime.dirShadowFBO[c] == 0)
					glGenFramebuffers(1, &runtime.dirShadowFBO[c]);

				bool needsAlloc = (runtime.dirShadowRes[c] != m_shadowRes);

				glBindTexture(GL_TEXTURE_2D, runtime.dirShadowTex[c]);

				if (needsAlloc) {
					glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24,
						m_shadowRes, m_shadowRes, 0,
						GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);

					runtime.dirShadowRes[c] = m_shadowRes;

					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);

					const float border[4] = { 1.f, 1.f, 1.f, 1.f };
					glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border);
				}

				glBindTexture(GL_TEXTURE_2D, 0);

				glBindFramebuffer(GL_FRAMEBUFFER, runtime.dirShadowFBO[c]);
				glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
					GL_TEXTURE_2D, runtime.dirShadowTex[c], 0);

				glDrawBuffer(GL_NONE);
				glReadBuffer(GL_NONE);

#ifdef NE_DEBUG
				GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
				if (status != GL_FRAMEBUFFER_COMPLETE)
					SPD_ERROR("Dir Shadow FBO incomplete! c=" << c << " status=" << status);
#endif
				glBindFramebuffer(GL_FRAMEBUFFER, 0);
			}

			return;
		}

		if (runtime.shadowMapTex == 0)
			glGenTextures(1, &runtime.shadowMapTex);

		if (runtime.shadowMapFBO == 0)
			glGenFramebuffers(1, &runtime.shadowMapFBO);

		const bool needsAlloc = (runtime.shadowMapRes != m_shadowRes);

		glBindTexture(GL_TEXTURE_2D, runtime.shadowMapTex);

		if (needsAlloc) {
			glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24,
				m_shadowRes, m_shadowRes, 0,
				GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);

			runtime.shadowMapRes = m_shadowRes;

			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);

			const float border[4] = { 1.f, 1.f, 1.f, 1.f };
			glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border);
		}

		glBindTexture(GL_TEXTURE_2D, 0);

		glBindFramebuffer(GL_FRAMEBUFFER, runtime.shadowMapFBO);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
			GL_TEXTURE_2D, runtime.shadowMapTex, 0);
		glDrawBuffer(GL_NONE);
		glReadBuffer(GL_NONE);

#ifdef NE_DEBUG
		GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
		if (status != GL_FRAMEBUFFER_COMPLETE) {
			SPD_ERROR("Shadow FBO incomplete! status=" << status);
		}
#endif

		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}

	Math::Mat4 ShadowRenderer::BuildLightVP(const ECS::Component::Light& light) {
		using Light = ECS::Component::Light;
		using Math::Vec3;
		using Math::Mat4;

		Vec3 dir = light.direction.Normalized();
		Vec3 up = (std::abs(dir.y) > 0.99f) ? Vec3{ 0.f, 0.f, 1.f } : Vec3{ 0.f, 1.f, 0.f };

		switch (light.type) {
			//case Light::Type::Directional:
			//    // Drive directional shadow by camera view (stable fit-to-view)
			//    return BuildDirectionalLightVP_FitToView(view, dir, m_shadowRes);

		case Light::Type::Spot: {
			Mat4 lightView = Mat4::BuildViewMtx(light.position, light.position + dir, up);

			const auto* spot = std::get_if<Light::SpotLightData>(&light.data);

			float outerDeg = spot ? spot->outerConeAngleDeg : 30.0f;
			outerDeg = std::clamp(outerDeg, 0.5f, 89.0f);

			float fov = Radians(outerDeg) * 2.0f;
			float nearP = 0.1f;
			float farP = std::max(spot ? spot->range : 10.0f, nearP + 0.01f);

			Mat4 lightProj = Mat4::BuildSymPerspective(fov, 1.0f, nearP, farP);
			return lightProj * lightView;
		}
		default:
			// Not supported in 2D path
			return Mat4();
		}
	}

	void ShadowRenderer::RenderDepth(const LightShadowRuntime& runtime, const Math::Mat4& lightVP,
		const std::vector<DrawCommand>& commands
	) {
		glBindFramebuffer(GL_FRAMEBUFFER, runtime.shadowMapFBO);

		glViewport(0, 0, m_shadowRes, m_shadowRes);

		glEnable(GL_DEPTH_TEST);
		glDepthMask(GL_TRUE);
		glDepthFunc(GL_LESS);

		glDisable(GL_BLEND);
		glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

		glClear(GL_DEPTH_BUFFER_BIT);

		glEnable(GL_POLYGON_OFFSET_FILL);
		glPolygonOffset(0.5f, 1.0f);

		m_shadowShader->Bind();
		m_shadowShader->SetUniformMat4("u_LightVP", lightVP);
		const Frustum frustum = Frustum::ExtractPlanesFromVP(lightVP);
		for (const auto& cmd : commands) {
			if (!frustum.IntersectsSphere(cmd.boundsCenterWS, cmd.boundsRadiusWs))
				continue;

			if (!cmd.castsShadow) continue;

			m_shadowShader->SetUniformMat4("u_Model", cmd.transform);
			cmd.mesh->Bind();
			cmd.mesh->Draw();
		}
		glDisable(GL_POLYGON_OFFSET_FILL);

		glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}

	void ShadowRenderer::RenderDepthDirectionalCascade(
		const LightShadowRuntime& runtime,
		int cascadeIdx,
		const NE::Math::Mat4& lightVP,
		const std::vector<DrawCommand>& commands
	) {
		GLuint fbo = runtime.dirShadowFBO[cascadeIdx];
		glBindFramebuffer(GL_FRAMEBUFFER, fbo);

		glViewport(0, 0, m_shadowRes, m_shadowRes);

		glEnable(GL_DEPTH_TEST);
		glDepthMask(GL_TRUE);
		glDepthFunc(GL_LESS);

		glDisable(GL_BLEND);
		glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

		glClear(GL_DEPTH_BUFFER_BIT);

		glEnable(GL_POLYGON_OFFSET_FILL);
		glPolygonOffset(0.5f, 1.0f);

		m_shadowShader->Bind();
		m_shadowShader->SetUniformMat4("u_LightVP", lightVP);

		const Frustum frustum = Frustum::ExtractPlanesFromVP(lightVP);
		for (const auto& cmd : commands) {
			if (!frustum.IntersectsSphere(cmd.boundsCenterWS, cmd.boundsRadiusWs))
				continue;

			if (!cmd.castsShadow) continue;
			m_shadowShader->SetUniformMat4("u_Model", cmd.transform);
			cmd.mesh->Bind();
			cmd.mesh->Draw();
		}

		glDisable(GL_POLYGON_OFFSET_FILL);

		glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}

	void ShadowRenderer::ComputeDirectionalSplits(const RenderView& view, LightShadowRuntime& runtime) {
		constexpr int C = ECS::Component::Light::DIR_CASCADES;

		const float n = std::max(0.01f, view.nearPlane);
		const float shadowFar = std::min(view.farPlane, m_directionalShadowDistance);
		const float f = std::max(n + 0.01f, shadowFar);

		const float lambda = 0.65f;

		for (int i = 0; i < C; ++i) {
			float p = float(i + 1) / float(C);

			float logSplit = n * std::pow(f / n, p);
			float uniSplit = n + (f - n) * p;

			float split = uniSplit * (1.0f - lambda) + logSplit * lambda;

			runtime.dirCascadeSplitsVS[i] = split;
		}
	}

	NE::Math::Mat4 ShadowRenderer::BuildDirectionalCascadeVP(
		const RenderView& view,
		const ECS::Component::Light& light,
		const LightShadowRuntime& runtime,
		int cascadeIdx
	) {
		using NE::Math::Vec3;
		using NE::Math::Mat4;

		const float n = std::max(0.01f, view.nearPlane);
		const float cascadeNear = (cascadeIdx == 0) ? n : runtime.dirCascadeSplitsVS[cascadeIdx - 1];
		const float cascadeFar = runtime.dirCascadeSplitsVS[cascadeIdx];

		Mat4 invProj = view.projection.Inverse();
		Mat4 invView = view.view.Inverse();

		auto dirsVS = GetViewSpaceCornerDirs(invProj);

		Vec3 frustumWS[8];
		for (int i = 0; i < 4; ++i) {
			Vec3 d = dirsVS[i];

			float tNear = cascadeNear / std::max(1e-6f, -d.z);
			float tFar = cascadeFar / std::max(1e-6f, -d.z);

			Vec3 nearVS = d * tNear;
			Vec3 farVS = d * tFar;

			frustumWS[i + 0] = TransformPoint(invView, nearVS, 1.0f);
			frustumWS[i + 4] = TransformPoint(invView, farVS, 1.0f);
		}

		// Calculate frustum center
		Vec3 center{ 0, 0, 0 };
		for (auto& p : frustumWS) center += p;
		center *= (1.0f / 8.0f);

		Vec3 dir = light.direction.Normalized();
		Vec3 right, up;
		BuildStableBasisFromDir(dir, right, up);

		float radius = 0.0f;
		for (auto& p : frustumWS) {
			float dist = (p - center).Length();
			radius = std::max(radius, dist);
		}

		const float cascadeDepth = std::max(cascadeFar - cascadeNear, 1.0f);
		const float pullBack = radius + cascadeDepth;
		Vec3 eye = center - dir * pullBack;
		Mat4 lightView = Mat4::BuildViewMtx(eye, center, up);

		float minX = +FLT_MAX, minY = +FLT_MAX, minZ = +FLT_MAX;
		float maxX = -FLT_MAX, maxY = -FLT_MAX, maxZ = -FLT_MAX;
		for (auto& pWS : frustumWS) {
			Vec3 pLS = TransformPoint(lightView, pWS, 1.0f);
			minX = std::min(minX, pLS.x);
			maxX = std::max(maxX, pLS.x);
			minY = std::min(minY, pLS.y);
			maxY = std::max(maxY, pLS.y);
			minZ = std::min(minZ, pLS.z);
			maxZ = std::max(maxZ, pLS.z);
		}

		float extentX = std::max(maxX - minX, 1e-3f);
		float extentY = std::max(maxY - minY, 1e-3f);
		float unitsPerTexelX = extentX / float(m_shadowRes);
		float unitsPerTexelY = extentY / float(m_shadowRes);
		float centerX = 0.5f * (minX + maxX);
		float centerY = 0.5f * (minY + maxY);

		centerX = std::floor(centerX / unitsPerTexelX) * unitsPerTexelX;
		centerY = std::floor(centerY / unitsPerTexelY) * unitsPerTexelY;

		minX = centerX - 0.5f * extentX;
		maxX = centerX + 0.5f * extentX;
		minY = centerY - 0.5f * extentY;
		maxY = centerY + 0.5f * extentY;

		const float padZ = std::max(cascadeDepth * 0.5f, 10.0f);
		float nearP = -maxZ - padZ;
		float farP = -minZ + padZ;

		nearP = std::max(nearP, 0.1f);
		if (farP <= nearP) farP = nearP + 1.0f;

		Mat4 lightProj = Mat4::BuildOrtho(minX, maxX, minY, maxY, nearP, farP);
		return lightProj * lightView;
	}
}
