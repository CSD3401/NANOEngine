#include "ShadowRenderer.hpp"

#include <algorithm>
#include <cfloat>
#include <cmath>

#include <glad/glad.h> // or whatever you use for GL

#include "ResourceManagement/ResourceManager.hpp"
#include "RenderViewManager.hpp"
#include "DrawCommand.hpp"

#include "ECS/Components/Light.hpp"

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
            // Avoid hard flipping based on thresholds if you can
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



        std::array<NE::Math::Vec3, 4> GetViewSpaceCornerRays(const NE::Math::Mat4& invProj) {
            using NE::Math::Vec3;

            // NDC corners at far plane direction (z = 1 in OpenGL NDC)
            // We use w=1 and then perspective divide in TransformPoint.
            const Vec3 ndc[4] = {
                { -1, -1,  1 },
                {  1, -1,  1 },
                {  1,  1,  1 },
                { -1,  1,  1 }
            };

            std::array<Vec3, 4> raysVS;
            for (int i = 0; i < 4; ++i) {
                Vec3 pVS = TransformPoint(invProj, ndc[i], 1.0f); // this gives a point on the far plane in view space
                raysVS[i] = pVS.Normalized(); // direction from origin in view space
            }
            return raysVS;
        }
	}

	void ShadowRenderer::Init() {
        if (!m_shadowShader) {
            m_shadowShader = Resource::ResourceManager::GetInstance()
                .LoadResource<OpenGL::GLShader>("neshadowdepth");
        }
	}

	void ShadowRenderer::Update(const RenderView& view,
		std::vector<ECS::Component::Light*>& lights,
		const std::vector<DrawCommand>& commands
	) {
        if (lights.empty() || commands.empty()) return;

        for (auto& light : lights) {
            // Skip if no shadows
            if (light->shadowType == ECS::Component::Light::ShadowType::None)
                continue;

            // Update mode gating
            switch (light->shadowUpdateMode) {
            case ECS::Component::Light::ShadowUpdateMode::NoneUpdate:
                continue;

            case ECS::Component::Light::ShadowUpdateMode::StaticBake:
                if (light->shadowBaked)
                    continue;
                break;

            case ECS::Component::Light::ShadowUpdateMode::Realtime:
                break;
            }

            if (light->type != ECS::Component::Light::Type::Directional &&
                light->type != ECS::Component::Light::Type::Spot) {
                continue;
            }

            if (light->type == ECS::Component::Light::Directional) {
                EnsureResources2D(*light);

                ComputeDirectionalSplits(view, *light);
                for (int c = 0; c < ECS::Component::Light::DIR_CASCADES; ++c) {
                    Math::Mat4 vp = BuildDirectionalCascadeVP(view, *light, c);
                    light->dirLightVP[c] = vp;
                    RenderDepthDirectionalCascade(*light, c, vp, commands);
                }

                light->shadowCascadeCount = ECS::Component::Light::DIR_CASCADES;

                if (light->shadowUpdateMode == ECS::Component::Light::ShadowUpdateMode::StaticBake)
                    light->shadowBaked = true;

                continue;
            }


            EnsureResources2D(*light);

            // Build and cache VP on the light (so the lighting pass can use it)
            Math::Mat4 lightVP = BuildLightVP(*light);
            light->lightViewProj = lightVP;
            light->shadowCascadeCount = 1;
            RenderDepth(*light, lightVP, commands);

            if (light->shadowUpdateMode == ECS::Component::Light::ShadowUpdateMode::StaticBake)
                light->shadowBaked = true;
        }
	}

	void  ShadowRenderer::Shutdown() {
        m_shadowShader.reset();
	}

	void ShadowRenderer::EnsureResources2D(ECS::Component::Light& light) {
        if (light.type == ECS::Component::Light::Directional) {
            for (int c = 0; c < ECS::Component::Light::DIR_CASCADES; ++c) {

                // --- Texture: create once, allocate, set params ---
                if (light.dirShadowTex[c] == 0) {
                    glGenTextures(1, &light.dirShadowTex[c]);
                }

                glBindTexture(GL_TEXTURE_2D, light.dirShadowTex[c]);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24,
                    m_shadowRes, m_shadowRes, 0,
                    GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);

                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);

                float border[4] = { 1,1,1,1 };
                glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border);

                // --- FBO: create once, but ALWAYS attach ---
                if (light.dirShadowFBO[c] == 0) {
                    glGenFramebuffers(1, &light.dirShadowFBO[c]);
                }

                glBindFramebuffer(GL_FRAMEBUFFER, light.dirShadowFBO[c]);
                glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                    GL_TEXTURE_2D, light.dirShadowTex[c], 0);

                glDrawBuffer(GL_NONE);
                glReadBuffer(GL_NONE);

#ifdef NE_DEBUG
                GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
                if (status != GL_FRAMEBUFFER_COMPLETE) {
                    SPD_ERROR("Dir Shadow FBO incomplete! c=" << c << " status=" << status);
                }
#endif
            }

            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glBindTexture(GL_TEXTURE_2D, 0);
            return;
        }

        if (light.shadowMapTex != 0 && light.shadowMapFBO != 0)
            return;

        // Create depth texture
        if (light.shadowMapTex == 0) {
            glGenTextures(1, &light.shadowMapTex);
        }
        glBindTexture(GL_TEXTURE_2D, light.shadowMapTex);

        // Allocate storage (Depth24 is fine)
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24,
            m_shadowRes, m_shadowRes, 0,
            GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);

        // Recommended defaults for shadow maps
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); // PCF-friendly
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);

        float border[4] = { 1.f, 1.f, 1.f, 1.f };
        glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border);

        glBindTexture(GL_TEXTURE_2D, 0);

        // Create FBO
        if (light.shadowMapFBO == 0) {
            glGenFramebuffers(1, &light.shadowMapFBO);
        }
        glBindFramebuffer(GL_FRAMEBUFFER, light.shadowMapFBO);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
            GL_TEXTURE_2D, light.shadowMapTex, 0);
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

	void ShadowRenderer::RenderDepth(ECS::Component::Light& light, const Math::Mat4& lightVP, 
        const std::vector<DrawCommand>& commands
    ) {
        glBindFramebuffer(GL_FRAMEBUFFER, light.shadowMapFBO);

        glViewport(0, 0, m_shadowRes, m_shadowRes);

        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_TRUE);
        glDepthFunc(GL_LESS);

        glDisable(GL_BLEND);
        glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

        glClear(GL_DEPTH_BUFFER_BIT);

        //glEnable(GL_CULL_FACE);
        //glCullFace(GL_BACK);

        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(1.0f, 1.0f);


        m_shadowShader->Bind();
        m_shadowShader->SetUniformMat4("u_LightVP", lightVP);

        for (const auto& cmd : commands) {
            if (!cmd.castsShadow) continue;

            m_shadowShader->SetUniformMat4("u_Model", cmd.transform);
            cmd.mesh->Bind();
            cmd.mesh->Draw();
        }
        glDisable(GL_POLYGON_OFFSET_FILL);
        //glCullFace(GL_BACK);

        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}

    void ShadowRenderer::RenderDepthDirectionalCascade(
        ECS::Component::Light& light,
        int cascadeIdx,
        const NE::Math::Mat4& lightVP,
        const std::vector<DrawCommand>& commands
    ) {
        GLuint fbo = light.dirShadowFBO[cascadeIdx];
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);

        glViewport(0, 0, m_shadowRes, m_shadowRes);

        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_TRUE);
        glDepthFunc(GL_LESS);

        glDisable(GL_BLEND);
        glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

        glClear(GL_DEPTH_BUFFER_BIT);

        //glEnable(GL_CULL_FACE);
        //glCullFace(GL_BACK);

        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(1.0f, 1.0f);

        m_shadowShader->Bind();
        m_shadowShader->SetUniformMat4("u_LightVP", lightVP);

        for (const auto& cmd : commands) {
            if (!cmd.castsShadow) continue;
            m_shadowShader->SetUniformMat4("u_Model", cmd.transform);
            cmd.mesh->Bind();
            cmd.mesh->Draw();
        }

        glDisable(GL_POLYGON_OFFSET_FILL);
        //glCullFace(GL_BACK);

        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    void ShadowRenderer::ComputeDirectionalSplits(const RenderView& view, ECS::Component::Light& light) {
        constexpr int C = ECS::Component::Light::DIR_CASCADES;

        const float n = std::max(0.01f, view.nearPlane);
        const float f = std::max(n + 0.01f, view.farPlane);

        const float lambda = 0.65f;

        for (int i = 0; i < C; ++i) {
            float p = float(i + 1) / float(C);

            float logSplit = n * std::pow(f / n, p);
            float uniSplit = n + (f - n) * p;

            float split = uniSplit * (1.0f - lambda) + logSplit * lambda;

            light.dirCascadeSplitsVS[i] = split;
        }
    }

    NE::Math::Mat4 ShadowRenderer::BuildDirectionalCascadeVP(
        const RenderView& view,
        const ECS::Component::Light& light,
        int cascadeIdx
    ) {
        using NE::Math::Vec3;
        using NE::Math::Mat4;

        const int C = ECS::Component::Light::DIR_CASCADES;

        const float n = std::max(0.01f, view.nearPlane);
        const float f = std::max(n + 0.01f, view.farPlane);

        const float cascadeNear = (cascadeIdx == 0) ? n : light.dirCascadeSplitsVS[cascadeIdx - 1];
        const float cascadeFar = light.dirCascadeSplitsVS[cascadeIdx];

        // Inverse matrices
        Mat4 invProj = view.projection.Inverse();
        Mat4 invView = view.view.Inverse();

        // 4 corner rays in view space
        auto raysVS = GetViewSpaceCornerRays(invProj);

        // Build 8 corners of the cascade slice in view space, then world space
        Vec3 frustumWS[8];
        for (int i = 0; i < 4; ++i) {
            Vec3 nearVS = raysVS[i] * cascadeNear;
            Vec3 farVS = raysVS[i] * cascadeFar;

            frustumWS[i + 0] = TransformPoint(invView, nearVS, 1.0f);
            frustumWS[i + 4] = TransformPoint(invView, farVS, 1.0f);
        }

        // Compute center of slice
        Vec3 center{ 0,0,0 };
        for (auto& p : frustumWS) center += p;
        center *= (1.0f / 8.0f);

        // Build light view (stable basis)
        Vec3 dir = light.direction.Normalized();
        Vec3 right, up;
        BuildStableBasisFromDir(dir, right, up);

        // Compute radius (sphere bound) for the slice
        float radius = 0.0f;
        for (auto& p : frustumWS) {
            radius = std::max(radius, (p - center).Length());
        }

        // Position eye back along dir
        Vec3 eye = center - dir * (radius + 50.0f);
        Mat4 lightView = Mat4::BuildViewMtx(eye, center, up);

        // Fit ortho bounds in light space
        float minX = +FLT_MAX, minY = +FLT_MAX, minZ = +FLT_MAX;
        float maxX = -FLT_MAX, maxY = -FLT_MAX, maxZ = -FLT_MAX;

        for (auto& pWS : frustumWS) {
            Vec3 pLS = TransformPoint(lightView, pWS, 1.0f);
            minX = std::min(minX, pLS.x); maxX = std::max(maxX, pLS.x);
            minY = std::min(minY, pLS.y); maxY = std::max(maxY, pLS.y);
            minZ = std::min(minZ, pLS.z); maxZ = std::max(maxZ, pLS.z);
        }

        // Texel snapping (same as your fit-to-view)
        float extentX = maxX - minX;
        float extentY = maxY - minY;

        float unitsPerTexelX = extentX / float(m_shadowRes);
        float unitsPerTexelY = extentY / float(m_shadowRes);

        extentX = std::ceil(extentX / unitsPerTexelX) * unitsPerTexelX;
        extentY = std::ceil(extentY / unitsPerTexelY) * unitsPerTexelY;

        unitsPerTexelX = extentX / float(m_shadowRes);
        unitsPerTexelY = extentY / float(m_shadowRes);

        float cx = 0.5f * (minX + maxX);
        float cy = 0.5f * (minY + maxY);

        cx = std::floor(cx / unitsPerTexelX) * unitsPerTexelX;
        cy = std::floor(cy / unitsPerTexelY) * unitsPerTexelY;

        minX = cx - 0.5f * extentX;
        maxX = cx + 0.5f * extentX;
        minY = cy - 0.5f * extentY;
        maxY = cy + 0.5f * extentY;

        // Z padding: important because casters can be outside the camera slice
        const float padZ = 150.0f;
        float nearP = -maxZ - padZ;
        float farP = -minZ + padZ;

        if (nearP < 0.1f) nearP = 0.1f;
        if (farP <= nearP) farP = nearP + 1.0f;

        Mat4 lightProj = Mat4::BuildOrtho(minX, maxX, minY, maxY, nearP, farP);
        return lightProj * lightView;
    }

}