#include "pch.h"
#include "ModelAssetMeshCook.hpp"

#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstring>

#include <Core/SpdLogger.hpp>
#include <Engine.hpp>

namespace Editor::Assets::ModelAssetInternal {
	namespace {
		struct CookVertex {
			float px, py, pz;
			float nx, ny, nz;
			float u, v;

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
			rb.vertices.resize(mesh->mNumVertices * sizeof(CookVertex));
			auto* vout = reinterpret_cast<CookVertex*>(rb.vertices.data());

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
					v.u = mesh->mTextureCoords[0][i].x;
					v.v = mesh->mTextureCoords[0][i].y;
				} else {
					v.u = v.v = 0.0f;
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

				vout[i] = v;
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

			rb.indices.resize(idx.size() * sizeof(uint32_t));
			std::memcpy(rb.indices.data(), idx.data(), rb.indices.size());

			subdescs[m].vertexCount = mesh->mNumVertices;
			subdescs[m].indexCount = static_cast<uint32_t>(idx.size());

			submeshPivots[m] = NE::Math::Vec3({
				(subB.minP.x + subB.maxP.x) * 0.5f,
				(subB.minP.y + subB.maxP.y) * 0.5f,
				(subB.minP.z + subB.maxP.z) * 0.5f
				});

			for (unsigned i = 0; i < mesh->mNumVertices; ++i) {
				vout[i].px -= submeshPivots[m].x;
				vout[i].py -= submeshPivots[m].y;
				vout[i].pz -= submeshPivots[m].z;
			}

			if (importSettings.mesh.generateColliders) {
				std::vector<NE::Math::Vec3> physVerts;
				physVerts.reserve(mesh->mNumVertices);
				for (uint32_t i = 0; i < mesh->mNumVertices; ++i) {
					physVerts.push_back({ vout[i].px, vout[i].py, vout[i].pz });
				}

				rb.collider.clear();
				const bool ok = NE::CookMeshCollider(physVerts, idx, rb.collider);
				if (!ok) {
					SPD_WARNING("Collider cook failed for submesh " << m << " (" << sourcePath << "), writing without collider.");
					rb.collider.clear();
				}
			}

			subB.minP = subB.minP - submeshPivots[m];
			subB.maxP = subB.maxP - submeshPivots[m];

			subdescs[m].aabbMin[0] = subB.minP.x; subdescs[m].aabbMin[1] = subB.minP.y; subdescs[m].aabbMin[2] = subB.minP.z;
			subdescs[m].aabbMax[0] = subB.maxP.x; subdescs[m].aabbMax[1] = subB.maxP.y; subdescs[m].aabbMax[2] = subB.maxP.z;

			const NE::Math::Vec3 e = (subB.maxP - subB.minP) * 0.5f;
			subdescs[m].sphereRadius = std::sqrt(e.x * e.x + e.y * e.y + e.z * e.z);
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
			subdescs[m].vertexFlags = 0;

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
