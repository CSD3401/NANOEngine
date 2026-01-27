#include "AnimationClipAsset.hpp"

#include <filesystem>
#include <fstream>

#include <rapidjson/document.h>
#include <rapidjson/ostreamwrapper.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/istreamwrapper.h>

#include <Serialisation/BinaryReflection.hpp>

#include "../Settings/AnimClipImportSettings.hpp"
#include "../../Serialization/JSONReflection.hpp"

namespace Editor::Assets {

    bool AnimationClipAsset::Cook(const std::string& sourcePath, const std::string& outPath) const {
        namespace fs = std::filesystem;

        fs::path jsonPath = sourcePath;

        std::ifstream ifs(jsonPath);
        if (!ifs.is_open()) return false;

        rapidjson::IStreamWrapper isw(ifs);
        rapidjson::Document d;
        d.ParseStream(isw);
        if (d.HasParseError() || !d.IsObject()) return false;

        NE::Animation::AnimClipBlob blob;

        // If wrapped: { "clip": { ... } }
        if (d.HasMember("clip") && d["clip"].IsObject()) {
            Editor::Deserialization::FromJSON(d["clip"], blob);
        } else {
            // Or raw object
            Editor::Deserialization::FromJSON(d, blob);
        }

        // 2) Serialize payload via ToBinary
        std::vector<uint8_t> payload;
        NE::Serialization::ToBinary(payload, blob);

        // 3) Write header + payload
        NE::Resource::NanoAnimClipHeader hdr{};
        hdr.payloadBytes = (uint64_t)payload.size();

        std::ofstream ofs(outPath, std::ios::binary);
        if (!ofs.is_open()) return false;

        ofs.write(reinterpret_cast<const char*>(&hdr), sizeof(hdr));
        if (!payload.empty())
            ofs.write(reinterpret_cast<const char*>(payload.data()), (std::streamsize)payload.size());

        return true;
    }

    bool AnimationClipAsset::LoadImportSettings(const std::string& sourcePath) {
        std::filesystem::path p = sourcePath;

        std::ifstream ifs(p);
        if (!ifs.is_open()) return false;

        rapidjson::IStreamWrapper isw(ifs);
        rapidjson::Document d;
        d.ParseStream(isw);
        if (d.HasParseError() || !d.IsObject()) return false;

        if (!d.HasMember("version") || !d["version"].IsInt()) return false;
        if (d["version"].GetInt() != 1) return false;

        if (!d.HasMember("clip") || !d["clip"].IsObject()) return false;

        Assets::AnimClipImportSettings settings;
        Editor::Deserialization::FromJSON(d["clip"], settings);

        // TODO: store into your asset's import settings member
        // m_settings = settings;

        return true;
    }

    bool AnimationClipAsset::SaveImportSettings(const std::string& sourcePath) {
        std::filesystem::path p = sourcePath;

        AnimClipImportSettings settings{};

        rapidjson::Document d;
        d.SetObject();
        auto& a = d.GetAllocator();

        d.AddMember("format", "NANO_ANIM_IMPORT", a);
        d.AddMember("version", 1, a);
        d.AddMember("clip", Editor::Serialization::ToJSON(settings, a), a);

        std::ofstream ofs(p);
        if (!ofs.is_open()) return false;

        rapidjson::OStreamWrapper osw(ofs);
        rapidjson::PrettyWriter<rapidjson::OStreamWrapper> writer(osw);
        writer.SetIndent(' ', 2);
        d.Accept(writer);
        return true;
    }

    bool AnimationClipAsset::SaveAnimationClip(const std::string& outPath) const {
        std::filesystem::path p = outPath;

        AnimClipImportSettings settings{};

        rapidjson::Document d;
        d.SetObject();
        auto& a = d.GetAllocator();

        d.AddMember("format", "NANO_ANIM_IMPORT", a);
        d.AddMember("version", 1, a);
        d.AddMember("clip", Editor::Serialization::ToJSON(settings, a), a);

        std::ofstream ofs(p);
        if (!ofs.is_open()) return false;

        rapidjson::OStreamWrapper osw(ofs);
        rapidjson::PrettyWriter<rapidjson::OStreamWrapper> writer(osw);
        writer.SetIndent(' ', 2);
        d.Accept(writer);
        return true;
    }
}

