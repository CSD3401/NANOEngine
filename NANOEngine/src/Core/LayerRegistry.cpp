#include "pch.h"
#include "LayerRegistry.hpp"

#include <cctype>

namespace NE::Core {
	namespace {
		bool IsWhitespace(char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r'; }

		std::string_view TrimSV(std::string_view s) {
			while (!s.empty() && IsWhitespace(s.front())) s.remove_prefix(1);
			while (!s.empty() && IsWhitespace(s.back())) s.remove_suffix(1);
			return s;
		}
	}

	LayerRegistry& LayerRegistry::GetInstance() {
		static LayerRegistry instance;
		return instance;
	}

	LayerRegistry::LayerRegistry() {
		m_data.slots[0].used = true;
		m_data.slots[0].name = "Default";

		for (size_t i = 0; i < MAX_LAYERS; ++i) {
			m_data.collideWith[i] = ~LayerMask{ 0 };
		}
		RebuildNameMap();
	}

	LayerID LayerRegistry::CreateLayer(std::string_view name) {
		auto key = Normalize(name);
		if (key.empty()) return INVALID_LAYER;
		if (auto it = m_nameToId.find(key); it != m_nameToId.end()) return it->second;

		auto id = FindFreeSlot();
		if (id == INVALID_LAYER) return id;

		m_data.slots[id].used = true;
		m_data.slots[id].name = std::string(name);

		m_data.collideWith[id] = ~LayerMask{ 0 };

		m_nameToId.emplace(std::move(key), id);
		return id;
	}

	bool LayerRegistry::RenameLayer(LayerID id, std::string_view newName) {
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

	bool LayerRegistry::SetCollision(LayerID a, LayerID b, bool enabled) {
		if (!IsValidLayerId(a) || !IsValidLayerId(b)) return false;
		const auto bitB = LayerBit(b);
		const auto bitA = LayerBit(a);

		if (enabled) {
			m_data.collideWith[a] |= bitB;
			m_data.collideWith[b] |= bitA;
		} else {
			m_data.collideWith[a] &= ~bitB;
			m_data.collideWith[b] &= ~bitA;
		}
		return true;
	}

	bool LayerRegistry::GetCollision(LayerID a, LayerID b) const {
		if (!IsValidLayerId(a) || !IsValidLayerId(b)) return false;
		return (m_data.collideWith[a] & LayerBit(b)) != 0;
	}

	LayerMask LayerRegistry::GetCollisionMask(LayerID a) const {
		if (!IsValidLayerId(a)) return 0;
		return m_data.collideWith[a];
	}

	bool LayerRegistry::SetCollisionMask(LayerID a, LayerMask mask) {
		if (!IsValidLayerId(a)) return false;
		m_data.collideWith[a] = mask;
		return true;
	}

	bool LayerRegistry::IsUsed(LayerID id) const noexcept {
		return IsValidLayerId(id) && m_data.slots[id].used && !m_data.slots[id].name.empty();
	}

	std::string_view LayerRegistry::GetName(LayerID id) const {
		if (!IsValidLayerId(id) || !m_data.slots[id].used) return {};
		return m_data.slots[id].name;
	}

	LayerID LayerRegistry::FindByName(std::string_view name) const {
		auto key = Normalize(name);
		auto it = m_nameToId.find(key);
		return (it == m_nameToId.end()) ? INVALID_LAYER : it->second;
	}

	const std::array<LayerMask, MAX_LAYERS>& LayerRegistry::GetCollisionMatrix() const noexcept {
		return m_data.collideWith;
	}

	bool LayerRegistry::IsValidLayerId(LayerID id) const noexcept {
		return id < MAX_LAYERS;
	}

	std::string LayerRegistry::Normalize(std::string_view s) {
		while (!s.empty() && IsWhitespace(s.front())) s.remove_prefix(1);
		while (!s.empty() && IsWhitespace(s.back())) s.remove_suffix(1);

		std::string out;
		out.reserve(s.size());
		for (char c : s) out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
		return out;
	}

	LayerID LayerRegistry::FindFreeSlot() const {
		for (LayerID i = 0; i < MAX_LAYERS; ++i) {
			if (!m_data.slots[i].used) return i;
		}
		return INVALID_LAYER;
	}

	void LayerRegistry::RebuildNameMap() {
		m_nameToId.clear();
		m_nameToId.reserve(MAX_LAYERS);

		for (LayerID i = 0; i < MAX_LAYERS; ++i) {
			if (!m_data.slots[i].used) continue;
			if (m_data.slots[i].name.empty()) continue;
			m_nameToId.emplace(Normalize(m_data.slots[i].name), i);
		}
	}
}