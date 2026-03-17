#include "pch.h"
#include "ModelAssetMeshCook.hpp"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstring>

#include <xatlas/xatlas.h>

#include <Core/SpdLogger.hpp>
#include <Engine.hpp>

#include "ModelAssetUvValidation.hpp"

namespace Editor::Assets::ModelAssetInternal {
	namespace {
		constexpr uint8_t kNanoVertexFlag_HasUv1 = (1u << 1);

		struct CookVertex {
			float px, py, pz;
			float nx, ny, nz;
			float u0, v0;

			float tx, ty, tz;
			float tSign;
		};

		struct CookVertexUv1 {
			float px, py, pz;
			float nx, ny, nz;
			float u0, v0;
			float u1, v1;

			float tx, ty, tz;
			float tSign;
		};

		struct Bounds {
			NE::Math::Vec3 minP{ FLT_MAX, FLT_MAX, FLT_MAX };
			NE::Math::Vec3 maxP{ -FLT_MAX, -FLT_MAX, -FLT_MAX };

			void Expand(float x, float y, float z) {
				minP.x = std::min(minP.x, x); minP.y = std::min(minP.y, y); minP.z = std::min(minP.z, z);
				maxP.x = std::max(maxP.x, x); maxP.y = std::max(maxP.y, y); maxP.z = std::max(maxP.z, z);
			}
		};

		struct RawBlob {
			std::vector<uint8_t> vertices;
			std::vector<uint8_t> indices;
			std::vector<uint8_t> collider;
		};

		static bool GenerateLightmapUVsXAtlas(
			const std::vector<CookVertex>& inVertices,
			const std::vector<uint32_t>& inIndices,
			std::vector<CookVertexUv1>& outVertices,
			std::vector<uint32_t>& outIndices,
			uint32_t paddingTexels,
			bool useInputUvHint
		) {
			if (inVertices.empty() || inIndices.empty())
				return false;

			xatlas::Atlas* atlas = xatlas::Create();
			if (!atlas)
				return false;

			xatlas::MeshDecl decl{};
			decl.vertexCount = static_cast<uint32_t>(inVertices.size());
			decl.vertexPositionData = &inVertices[0].px;
			decl.vertexPositionStride = sizeof(CookVertex);

			decl.vertexNormalData = &inVertices[0].nx;
			decl.vertexNormalStride = sizeof(CookVertex);

			// Provide UV0 as a charting hint if the source mesh has UVs.
			if (useInputUvHint) {
				decl.vertexUvData = &inVertices[0].u0;
				decl.vertexUvStride = sizeof(CookVertex);
			}

			decl.indexData = inIndices.data();
			decl.indexCount = static_cast<uint32_t>(inIndices.size());
			decl.indexFormat = xatlas::IndexFormat::UInt32;
			decl.faceCount = decl.indexCount / 3;

			const xatlas::AddMeshError err = xatlas::AddMesh(atlas, decl);
			if (err != xatlas::AddMeshError::Success) {
				SPD_WARNING("xatlas::AddMesh failed (" << xatlas::StringForEnum(err) << "), skipping UV1 generation.");
				xatlas::Destroy(atlas);
				return false;
			}

			xatlas::ChartOptions chartOptions{};
			chartOptions.useInputMeshUvs = useInputUvHint;
			chartOptions.fixWinding = true;

			xatlas::PackOptions packOptions{};
			packOptions.padding = paddingTexels;
			packOptions.bilinear = true;
			packOptions.blockAlign = false;

			xatlas::Generate(atlas, chartOptions, packOptions);

			if (atlas->meshCount != 1 || !atlas->meshes || !atlas->meshes[0].vertexArray || !atlas->meshes[0].indexArray || atlas->width == 0 || atlas->height == 0) {
				SPD_WARNING("xatlas::Generate produced no usable output, skipping UV1 generation.");
				xatlas::Destroy(atlas);
				return false;
			}

			const xatlas::Mesh& om = atlas->meshes[0];
			outVertices.clear();
			outVertices.resize(om.vertexCount);
			outIndices.assign(om.indexArray, om.indexArray + om.indexCount);

			const float invW = 1.0f / static_cast<float>(atlas->width);
			const float invH = 1.0f / static_cast<float>(atlas->height);

			for (uint32_t i = 0; i < om.vertexCount; ++i) {
				const xatlas::Vertex& ov = om.vertexArray[i];
				const uint32_t src = ov.xref;
				if (src >= inVertices.size()) {
					SPD_WARNING("xatlas output xref out of range, skipping UV1 generation.");
					xatlas::Destroy(atlas);
					return false;
				}

				const CookVertex& iv = inVertices[src];
				CookVertexUv1 v{};
				v.px = iv.px; v.py = iv.py; v.pz = iv.pz;
				v.nx = iv.nx; v.ny = iv.ny; v.nz = iv.nz;
				v.u0 = iv.u0; v.v0 = iv.v0;

				v.u1 = ov.uv[0] * invW;
				v.v1 = ov.uv[1] * invH;

				v.tx = iv.tx; v.ty = iv.ty; v.tz = iv.tz;
				v.tSign = iv.tSign;

				outVertices[i] = v;
			}

			xatlas::Destroy(atlas);
			return true;
		}
	}

	bool CookModelBinary(
		const aiScene* scene,
		const ModelImportSettings& importSettings,
		float sceneScale,
		const std::string& sourcePath,
		const std::filesystem::path& outPath,
		MeshCookResult& outResult
	) {
		outResult = {};

		if (!scene || !scene->HasMeshes()) {
			std::fprintf(stderr, "[CookMesh] Failed to load %s\n", sourcePath.c_str());
			return false;
		}

		std::vector<NE::Resource::NanoSubmeshDesc> subdescs(scene->mNumMeshes);
		std::vector<NE::Math::Vec3> submeshPivots(scene->mNumMeshes);
		std::vector<uint32_t> colliderDataSizes(scene->mNumMeshes, 0);
		std::vector<RawBlob> blobs(scene->mNumMeshes);

		for (unsigned m = 0; m < scene->mNumMeshes; ++m) {
			const aiMesh* mesh = scene->mMeshes[m];
			Bounds subB;

			RawBlob& rb = blobs[m];
			std::vector<CookVertex> vertices;
			vertices.resize(mesh->mNumVertices);

			for (unsigned i = 0; i < mesh->mNumVertices; ++i) {
				CookVertex v{};
				v.px = mesh->mVertices[i].x * sceneScale;
				v.py = mesh->mVertices[i].y * sceneScale;
				v.pz = mesh->mVertices[i].z * sceneScale;

				if (mesh->HasNormals()) {
					v.nx = mesh->mNormals[i].x;
					v.ny = mesh->mNormals[i].y;
					v.nz = mesh->mNormals[i].z;
				} else {
					v.nx = v.ny = v.nz = 0.0f;
				}

				if (mesh->HasTextureCoords(0)) {
					v.u0 = mesh->mTextureCoords[0][i].x;
					v.v0 = mesh->mTextureCoords[0][i].y;
				} else {
					v.u0 = v.v0 = 0.0f;
				}

				if (mesh->HasTangentsAndBitangents()) {
					auto NormalizeSafe = [](aiVector3D value) {
						const float len2 = value.x * value.x + value.y * value.y + value.z * value.z;
						if (len2 > 1e-12f) {
							const float invLen = 1.0f / std::sqrt(len2);
							value.x *= invLen;
							value.y *= invLen;
							value.z *= invLen;
						}
						return value;
						};

					const aiVector3D tangent = NormalizeSafe(mesh->mTangents[i]);
					const aiVector3D bitangent = NormalizeSafe(mesh->mBitangents[i]);
					const aiVector3D normal = NormalizeSafe(mesh->HasNormals() ? mesh->mNormals[i] : aiVector3D(0, 1, 0));

					const float sign = ((normal ^ tangent) * bitangent) < 0.0f ? -1.0f : 1.0f;

					v.tx = tangent.x;
					v.ty = tangent.y;
					v.tz = tangent.z;
					v.tSign = sign;
				} else {
					v.tx = v.ty = v.tz = 0.0f;
					v.tSign = 1.0f;
				}

				vertices[i] = v;
				subB.Expand(v.px, v.py, v.pz);
			}

			std::vector<uint32_t> idx;
			idx.reserve(mesh->mNumFaces * 3);
			for (unsigned f = 0; f < mesh->mNumFaces; ++f) {
				const aiFace& face = mesh->mFaces[f];
				for (unsigned j = 0; j < face.mNumIndices; ++j) {
					idx.push_back(face.mIndices[j]);
				}
			}

			subdescs[m].vertexCount = static_cast<uint32_t>(vertices.size());
			subdescs[m].indexCount = static_cast<uint32_t>(idx.size());

			submeshPivots[m] = NE::Math::Vec3({
				(subB.minP.x + subB.maxP.x) * 0.5f,
				(subB.minP.y + subB.maxP.y) * 0.5f,
				(subB.minP.z + subB.maxP.z) * 0.5f
				});

			for (auto& v : vertices) {
				v.px -= submeshPivots[m].x;
				v.py -= submeshPivots[m].y;
				v.pz -= submeshPivots[m].z;
			}

			const bool generateUv1 = importSettings.mesh.generateLightmapUVs;

			subB.minP = subB.minP - submeshPivots[m];
			subB.maxP = subB.maxP - submeshPivots[m];

			subdescs[m].aabbMin[0] = subB.minP.x; subdescs[m].aabbMin[1] = subB.minP.y; subdescs[m].aabbMin[2] = subB.minP.z;
			subdescs[m].aabbMax[0] = subB.maxP.x; subdescs[m].aabbMax[1] = subB.maxP.y; subdescs[m].aabbMax[2] = subB.maxP.z;

			const NE::Math::Vec3 e = (subB.maxP - subB.minP) * 0.5f;
			subdescs[m].sphereRadius = std::sqrt(e.x * e.x + e.y * e.y + e.z * e.z);

			if (generateUv1) {
				std::vector<CookVertexUv1> verticesUv1;
				std::vector<uint32_t> indicesUv1;

				const uint32_t paddingTexels = std::clamp(importSettings.mesh.lightmapUvPaddingTexels, 4u, 64u);
				const bool useInputUvHint = mesh->HasTextureCoords(0);
				if (GenerateLightmapUVsXAtlas(vertices, idx, verticesUv1, indicesUv1, paddingTexels, useInputUvHint)) {
					// Validate UV1 before writing it out.
					std::vector<NE::Math::Vec2> uv1;
					uv1.reserve(verticesUv1.size());
					for (const auto& v : verticesUv1) uv1.push_back({ v.u1, v.v1 });

					UvValidationConfig config{};
					std::vector<UvValidationIssue> issues;
					const bool ok = ValidateLightmapUv1(
						sourcePath,
						static_cast<uint32_t>(m),
						uv1.data(),
						static_cast<uint32_t>(uv1.size()),
						static_cast<uint32_t>(verticesUv1.size()),
						indicesUv1.data(),
						static_cast<uint32_t>(indicesUv1.size()),
						config,
						issues
					);

					bool hasError = !ok;
					for (const auto& issue : issues) {
						if (issue.severity == UvValidationSeverity::Error) {
							hasError = true;
							SPD_ERROR(issue.message);
						} else {
							SPD_WARNING(issue.message);
						}
					}
					if (hasError) {
						SPD_ERROR("UV1 validation failed for submesh " << m << " (" << sourcePath << "). Fix the mesh or disable Generate Lightmap UVs (UV1).");
						return false;
					}

					subdescs[m].vertexCount = static_cast<uint32_t>(verticesUv1.size());
					subdescs[m].indexCount = static_cast<uint32_t>(indicesUv1.size());
					subdescs[m].vertexFlags = static_cast<uint8_t>(subdescs[m].vertexFlags | kNanoVertexFlag_HasUv1);

					rb.vertices.resize(verticesUv1.size() * sizeof(CookVertexUv1));
					std::memcpy(rb.vertices.data(), verticesUv1.data(), rb.vertices.size());

					rb.indices.resize(indicesUv1.size() * sizeof(uint32_t));
					std::memcpy(rb.indices.data(), indicesUv1.data(), rb.indices.size());

					// Collider can be cooked from either the original or duplicated mesh. Use the final indices for best alignment.
					if (importSettings.mesh.generateColliders) {
						std::vector<NE::Math::Vec3> physVerts;
						physVerts.reserve(verticesUv1.size());
						for (const auto& v : verticesUv1)
							physVerts.push_back({ v.px, v.py, v.pz });

						rb.collider.clear();
						const bool ok = NE::CookMeshCollider(physVerts, indicesUv1, rb.collider);
						if (!ok) {
							SPD_WARNING("Collider cook failed for submesh " << m << " (" << sourcePath << "), writing without collider.");
							rb.collider.clear();
						}
					}

					continue;
				}

				SPD_ERROR("UV1 generation failed for submesh " << m << " (" << sourcePath << "). Fix the mesh or disable Generate Lightmap UVs (UV1).");
				return false;
			}

			// Default path: write legacy vertex/index buffers unchanged.
			rb.vertices.resize(vertices.size() * sizeof(CookVertex));
			std::memcpy(rb.vertices.data(), vertices.data(), rb.vertices.size());

			rb.indices.resize(idx.size() * sizeof(uint32_t));
			std::memcpy(rb.indices.data(), idx.data(), rb.indices.size());

			if (importSettings.mesh.generateColliders) {
				std::vector<NE::Math::Vec3> physVerts;
				physVerts.reserve(vertices.size());
				for (const auto& v : vertices)
					physVerts.push_back({ v.px, v.py, v.pz });

				rb.collider.clear();
				const bool ok = NE::CookMeshCollider(physVerts, idx, rb.collider);
				if (!ok) {
					SPD_WARNING("Collider cook failed for submesh " << m << " (" << sourcePath << "), writing without collider.");
					rb.collider.clear();
				}
			}
		}

		std::ofstream ofs(outPath, std::ios::binary);
		if (!ofs) return false;

		NE::Resource::NanoMeshHeader header{};
		header.submeshCount = static_cast<uint16_t>(scene->mNumMeshes);

		ofs.write(reinterpret_cast<const char*>(&header), sizeof(header));

		const std::streamoff subTablePos = ofs.tellp();
		ofs.write(reinterpret_cast<const char*>(subdescs.data()),
			subdescs.size() * sizeof(NE::Resource::NanoSubmeshDesc));

		for (size_t m = 0; m < blobs.size(); ++m) {
			auto& rb = blobs[m];

			const uint32_t vertexOffset = static_cast<uint32_t>(ofs.tellp());
			ofs.write(reinterpret_cast<const char*>(rb.vertices.data()), rb.vertices.size());

			const uint32_t indexOffset = static_cast<uint32_t>(ofs.tellp());
			ofs.write(reinterpret_cast<const char*>(rb.indices.data()), rb.indices.size());

			uint32_t colliderOffset = 0;
			uint32_t colliderSize = 0;
			if (!rb.collider.empty()) {
				colliderOffset = static_cast<uint32_t>(ofs.tellp());
				colliderSize = static_cast<uint32_t>(rb.collider.size());
				ofs.write(reinterpret_cast<const char*>(rb.collider.data()), rb.collider.size());
			}

			subdescs[m].vertexDataOffset = vertexOffset;
			subdescs[m].indexDataOffset = indexOffset;
			subdescs[m].colliderDataOffset = colliderOffset;
			subdescs[m].colliderDataSize = colliderSize;
			subdescs[m].colliderType = 1;

			colliderDataSizes[m] = colliderSize;
		}

		ofs.seekp(subTablePos, std::ios::beg);
		ofs.write(reinterpret_cast<const char*>(subdescs.data()),
			subdescs.size() * sizeof(NE::Resource::NanoSubmeshDesc));

		std::printf("[CookMesh] wrote %s\n", outPath.string().c_str());

		outResult.subdescs = std::move(subdescs);
		outResult.submeshPivots = std::move(submeshPivots);
		outResult.colliderDataSizes = std::move(colliderDataSizes);
		return true;
	}
}
