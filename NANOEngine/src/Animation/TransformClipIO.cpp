// NANOEngine/src/Animation/TransformClipIO.cpp
#include "TransformClipIO.hpp"
#include <fstream>
#include <sstream>
#include <filesystem>

namespace NE::Animation {

    static void WriteKeys(std::ofstream& out, const char* tag,
        const std::vector<KeyframeVec3>& v) {
        out << tag << " " << v.size() << "\n";
        for (auto& k : v) out << k.t << " " << k.v.x << " " << k.v.y << " " << k.v.z << "\n";
    }

    bool SaveTransformClip(const TransformClip& c, const std::string& path) {
        std::filesystem::create_directories(std::filesystem::path(path).parent_path());
        std::ofstream out(path, std::ios::trunc);
        if (!out) return false;
        out << "NECLIP 1\n";
        out << "name " << c.name << "\n";
        out << "length " << c.length << "\n";
        WriteKeys(out, "pos", c.pos);
        WriteKeys(out, "rot", c.rot);
        WriteKeys(out, "scl", c.scl);
        return true;
    }

    static bool ReadKeys(std::istringstream& iss, std::vector<KeyframeVec3>& v, size_t n) {
        v.clear(); v.reserve(n);
        for (size_t i = 0;i < n;i++) {
            KeyframeVec3 k; if (!(iss >> k.t >> k.v.x >> k.v.y >> k.v.z)) return false;
            v.push_back(k);
        }
        return true;
    }

    bool LoadTransformClip(TransformClip& c, const std::string& path) {
        std::ifstream in(path);
        if (!in) return false;

        std::string line, tag; int version = 0;
        if (!std::getline(in, line)) return false;
        { std::istringstream h(line); h >> tag >> version; if (tag != "NECLIP") return false; }

        c = TransformClip{};
        while (std::getline(in, line)) {
            if (line.empty()) continue;
            std::istringstream iss(line);
            iss >> tag;
            if (tag == "name") { iss >> c.name; }
            else if (tag == "length") { iss >> c.length; }
            else if (tag == "pos" || tag == "rot" || tag == "scl") {
                size_t n = 0; iss >> n;
                std::string block; for (size_t i = 0;i < n;i++) { std::string l; std::getline(in, l); block += l + "\n"; }
                std::istringstream keys(block);
                if (tag == "pos") { if (!ReadKeys(keys, c.pos, n)) return false; }
                if (tag == "rot") { if (!ReadKeys(keys, c.rot, n)) return false; }
                if (tag == "scl") { if (!ReadKeys(keys, c.scl, n)) return false; }
            }
        }
        return true;
    }
}
