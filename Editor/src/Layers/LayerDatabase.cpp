#include "LayerDatabase.hpp"

namespace Editor::Layers {
    namespace {
        bool IsWhitespace(char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r'; }

        std::string_view TrimSV(std::string_view s) {
            while (!s.empty() && IsWhitespace(s.front())) s.remove_prefix(1);
            while (!s.empty() && IsWhitespace(s.back()))  s.remove_suffix(1);
            return s;
        }
    }

    LayerDatabase::LayerDatabase() {
        m_data.slots[0].used = true;
        m_data.slots[0].name = "Default";

        for (size_t i = 0; i < NE::Core::MAX_USER_LAYERS; ++i) {
            m_data.collideWith[i] = ~NE::Core::LayerMask{ 0 };
        }
        RebuildNameMap();
    }

    NE::Core::LayerID LayerDatabase::CreateLayer(std::string_view name) {
        auto key = Normalize(name);
        if (key.empty()) return NE::Core::INVALID_LAYER;
        if (m_nameToId.find(key) != m_nameToId.end()) return m_nameToId[key];

        auto id = FindFreeSlot();
        if (id == NE::Core::INVALID_LAYER) return id;

        m_data.slots[id].used = true;
        m_data.slots[id].name = std::string(name);

        m_data.collideWith[id] = ~NE::Core::LayerMask{ 0 };

        m_nameToId.emplace(std::move(key), id);
        return id;
    }

    bool LayerDatabase::RenameLayer(NE::Core::LayerID id, std::string_view newName) {
        if (!IsValidLayerId(id)) return false;
        if (id == 0) return false;

        newName = std::string_view(newName.data(), newName.size());
        auto key = Normalize(newName);

        if (key.empty()) {
            m_data.slots[id].used = false;
            m_data.slots[id].name.clear();
            RebuildNameMap();
            return true;
        }

        auto itTaken = m_nameToId.find(key);
        if (itTaken != m_nameToId.end() && itTaken->second != id) {
            return false;
        }

        if (m_data.slots[id].used && !m_data.slots[id].name.empty()) {
            m_nameToId.erase(Normalize(m_data.slots[id].name));
        }

        m_data.slots[id].used = true;
        m_data.slots[id].name = std::string(TrimSV(newName));

        m_nameToId[key] = id;

        return true;
    }

    bool LayerDatabase::SetCollision(NE::Core::LayerID a, NE::Core::LayerID b, bool enabled) {
        if (!IsValidLayerId(a) || !IsValidLayerId(b)) return false;
        const auto bitB = NE::Core::LayerBit(b);
        const auto bitA = NE::Core::LayerBit(a);

        if (enabled) {
            m_data.collideWith[a] |= bitB;
            m_data.collideWith[b] |= bitA;
        } else {
            m_data.collideWith[a] &= ~bitB;
            m_data.collideWith[b] &= ~bitA;
        }
        return true;
    }

    bool LayerDatabase::GetCollision(NE::Core::LayerID a, NE::Core::LayerID b) const {
        if (!IsValidLayerId(a) || !IsValidLayerId(b)) return false;
        return (m_data.collideWith[a] & NE::Core::LayerBit(b)) != 0;
    }

    NE::Core::LayerMask LayerDatabase::GetCollisionMask(NE::Core::LayerID a) const {
        if (!IsValidLayerId(a)) return 0;
        return m_data.collideWith[a];
    }

    bool LayerDatabase::SetCollisionMask(NE::Core::LayerID a, NE::Core::LayerMask mask) {
        if (!IsValidLayerId(a)) return false;
        m_data.collideWith[a] = mask;
        return true;
    }

    bool LayerDatabase::IsUsed(NE::Core::LayerID id) const noexcept {
        return IsValidLayerId(id) && m_data.slots[id].used && !m_data.slots[id].name.empty();
    }

    std::string_view LayerDatabase::GetName(NE::Core::LayerID id) const {
        if (!IsValidLayerId(id) || !m_data.slots[id].used) return {};
        return m_data.slots[id].name;
    }

    NE::Core::LayerID LayerDatabase::FindByName(std::string_view name) const {
        auto key = Normalize(name);
        auto it = m_nameToId.find(key);
        return (it == m_nameToId.end()) ? NE::Core::INVALID_LAYER : it->second;
    }

    bool LayerDatabase::IsValidLayerId(NE::Core::LayerID id) const noexcept {
        return id < NE::Core::MAX_USER_LAYERS;
    }

    std::string LayerDatabase::Normalize(std::string_view s) {
        while (!s.empty() && IsWhitespace(s.front())) s.remove_prefix(1);
        while (!s.empty() && IsWhitespace(s.back()))  s.remove_suffix(1);

        std::string out;
        out.reserve(s.size());
        for (char c : s) out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
        return out;
    }

    NE::Core::LayerID LayerDatabase::FindFreeSlot() const {
        for (NE::Core::LayerID i = 0; i < NE::Core::MAX_USER_LAYERS; ++i)
            if (!m_data.slots[i].used) return i;
        return NE::Core::INVALID_LAYER;
    }

    void LayerDatabase::RebuildNameMap() {
        m_nameToId.clear();
        m_nameToId.reserve(NE::Core::MAX_USER_LAYERS);

        for (NE::Core::LayerID i = 0; i < NE::Core::MAX_USER_LAYERS; ++i) {
            if (!m_data.slots[i].used) continue;
            if (m_data.slots[i].name.empty()) continue;
            m_nameToId.emplace(Normalize(m_data.slots[i].name), i);
        }
    }
}
