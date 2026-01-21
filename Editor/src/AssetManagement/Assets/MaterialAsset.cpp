#include "MaterialAsset.hpp"

#include <vector>
#include <fstream>
#include <filesystem>

#include <rapidjson/document.h>
#include <ResourceManagement/BinaryHeaders/NanoMatHeader.hpp>
#include <Core/SpdLogger.hpp>

namespace Editor::Assets {
	bool MaterialAsset::Cook(const std::string& sourcePath, const std::string& outPath) const {
        std::filesystem::path out = outPath;
        std::filesystem::create_directories(out.parent_path());

        std::ifstream in(sourcePath);
        if (!in) return false;
        std::string j((std::istreambuf_iterator<char>(in)), {});
        rapidjson::Document doc; doc.Parse(j.c_str());
        if (!doc.IsObject()) return false;

        NE::Resource::NanoMatHeader h{};
        h.depthTest = doc.HasMember("DepthTest") ? (doc["DepthTest"].GetBool() ? 1 : 0) : 1;
        h.blendMode = doc.HasMember("BlendMode") ? (doc["BlendMode"].GetBool() ? 1 : 0) : 0;
        h.cullMode = doc.HasMember("CullMode") ? doc["CullMode"].GetUint() : 0;
        h.polygonMode = doc.HasMember("PolygonMode") ? doc["PolygonMode"].GetUint() : 0;
		h.renderQueueOffset = doc.HasMember("RenderQueueOffset") ? doc["RenderQueueOffset"].GetInt() : 0;

		const char* rqName = doc.HasMember("RenderQueueBase") ? doc["RenderQueueBase"].GetString() : "Geometry";
		const uint32_t rqNameLen = (uint32_t)std::strlen(rqName);

        const char* shaderName = doc.HasMember("Shader") ? doc["Shader"].GetString() : "Basic";
        if (strcmp(shaderName, "50f92895-66cc-459d-ad25-0fd250c91f3c") == 0) {
            shaderName = "nelitpbr";
        }
        const uint32_t shaderNameLen = (uint32_t)std::strlen(shaderName);

        // Tables to build
        std::vector<NE::Resource::MatPropRecord> propRecs;
        std::vector<NE::Resource::MatTexRecord>  texRecs;

        std::string strings; // shared string blob (for BOTH prop names and tex names)
        std::string payload; // only for prop data (ints/floats/matrices)

        const uint32_t rqNameRelOffset = (uint32_t)strings.size();
        strings.append(rqName, rqNameLen);
        h.renderQueueNameLen = rqNameLen;

        if (doc.HasMember("Properties") && doc["Properties"].IsObject()) {
            for (auto it = doc["Properties"].MemberBegin(); it != doc["Properties"].MemberEnd(); ++it) {
                const std::string name = it->name.GetString();
                const auto& v = it->value;

                // case A: numeric / array -> goes to prop table
                if (v.IsInt() || v.IsNumber() ||
                    (v.IsArray() && (v.Size() == 3 || v.Size() == 16))) {

                    NE::Resource::MatPropRecord r{};
                    r.nameLen = (uint32_t)name.size();
                    r.count = 1;

                    size_t dataStart = payload.size();
                    if (v.IsInt()) {
                        int32_t x = v.GetInt();
                        r.type = (uint8_t)NE::Resource::MatPropType::INT;
                        payload.append(reinterpret_cast<const char*>(&x), sizeof(x));
                    } else if (v.IsNumber()) {
                        float f = (float)v.GetDouble();
                        r.type = (uint8_t)NE::Resource::MatPropType::FLOAT;
                        payload.append(reinterpret_cast<const char*>(&f), sizeof(f));
                    } else if (v.IsArray() && v.Size() == 3) {
                        float f[3] = {
                            (float)v[0].GetDouble(),
                            (float)v[1].GetDouble(),
                            (float)v[2].GetDouble()
                        };
                        //SPD_INFO("uniform name: " << name << " value: " << v[0].GetDouble() << ", " << v[1].GetDouble() << ", " << v[2].GetDouble());
                        r.type = (uint8_t)NE::Resource::MatPropType::VEC3;
                        payload.append(reinterpret_cast<const char*>(f), sizeof(f));
                    } else if (v.IsArray() && v.Size() == 16) {
                        float m[16];
                        for (rapidjson::SizeType i = 0; i < 16; ++i)
                            m[i] = (float)v[i].GetDouble();
                        r.type = (uint8_t)NE::Resource::MatPropType::MAT4;
                        payload.append(reinterpret_cast<const char*>(m), sizeof(m));
                    }

                    r.dataOffset = (uint32_t)dataStart;
                    r.dataSize = (uint32_t)(payload.size() - dataStart);

                    // add name to shared strings
                    uint32_t nameOff = (uint32_t)strings.size();
                    strings.append(name.data(), name.size());

                    r.nameOffset = nameOff; // relative for now
                    propRecs.push_back(r);
                }
                //// case B: string -> treat as texture uuid
                //else if (v.IsString()) {
                //    const char* uuidStr = v.GetString();
                //    // we assume editor saved actual uuid or empty string
                //    if (uuidStr[0] != '\0') {
                //        NE::Resource::MatTexRecord tr{};
                //        tr.nameLen = (uint32_t)name.size();

                //        // put name in shared strings
                //        uint32_t nameOff = (uint32_t)strings.size();
                //        strings.append(name.data(), name.size());
                //        tr.nameOffset = nameOff;

                //        // copy up to 36 chars (your struct is exactly 36, no null)
                //        std::memset(tr.uuid, 0, 36);
                //        std::memcpy(tr.uuid, uuidStr, std::min<size_t>(std::strlen(uuidStr), 36));

                //        texRecs.push_back(tr);
                //    }
                //    // if empty string, just skip (no texture bound)
                //}
                // else: unknown type -> ignore
                else if (v.IsString()) {
                    const char* uuidStr = v.GetString();

                    NE::Resource::MatTexRecord tr{};
                    tr.nameLen = (uint32_t)name.size();

                    // put name in shared strings
                    uint32_t nameOff = (uint32_t)strings.size();
                    strings.append(name.data(), name.size());
                    tr.nameOffset = nameOff;

                    // write 36 bytes no matter what
                    std::memset(tr.uuid, 0, 36);
                    if (uuidStr && uuidStr[0] != '\0') {
                        std::memcpy(tr.uuid, uuidStr, std::min<size_t>(std::strlen(uuidStr), 36));
                    }

                    texRecs.push_back(tr);
                }
            }
        }

        // Fill header counts
        h.propCount = (uint16_t)propRecs.size();
        h.texCount = (uint16_t)texRecs.size();
        h.shaderNameLen = shaderNameLen;

        size_t offset = sizeof(NE::Resource::NanoMatHeader);

        // shader name
        h.shaderNameOffset = (uint32_t)offset;
        offset += shaderNameLen;

        // prop table
        const uint32_t propTableBytes = (uint32_t)(propRecs.size() * sizeof(NE::Resource::MatPropRecord));
        h.propsOffset = (uint32_t)offset;
        offset += propTableBytes;

        // shared string blob (for prop names + tex names)
        const uint32_t stringsBase = (uint32_t)offset;
        offset += (uint32_t)strings.size();

		// render queue name info
		h.renderQueueNameOffset = stringsBase + rqNameRelOffset;

        // payload (for prop data)
        const uint32_t payloadBase = (uint32_t)offset;
        offset += (uint32_t)payload.size();

        // texture table (if any)
        if (!texRecs.empty()) {
            h.texTableOffset = (uint32_t)offset;
            offset += (uint32_t)(texRecs.size() * sizeof(NE::Resource::MatTexRecord));
        } else {
            h.texTableOffset = 0;
        }

        // Fix up per-record absolute offsets
        for (auto& r : propRecs) {
            r.nameOffset += stringsBase;
            r.dataOffset += payloadBase;
        }
        for (auto& tr : texRecs) {
            tr.nameOffset += stringsBase;
            // tr.uuid is already inline
        }

        // 7) Write file
        std::ofstream ofs(outPath, std::ios::binary);
        if (!ofs) return false;

        ofs.write((char*)&h, sizeof(h));
        ofs.write(shaderName, shaderNameLen);
        if (!propRecs.empty()) ofs.write((char*)propRecs.data(), propTableBytes);
        if (!strings.empty())  ofs.write(strings.data(), (std::streamsize)strings.size());
        if (!payload.empty())  ofs.write(payload.data(), (std::streamsize)payload.size());
        if (!texRecs.empty())  ofs.write((char*)texRecs.data(), (std::streamsize)(texRecs.size() * sizeof(NE::Resource::MatTexRecord)));

        return ofs.good();
	}

    bool MaterialAsset::LoadImportSettings(const std::string& sourcePath) {
        return false;
    }

    bool MaterialAsset::SaveImportSettings(const std::string& sourcePath) {
        return false;
    }
}