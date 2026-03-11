#include "pch.h"
#include "EditorSelection.hpp"

#include <algorithm>

namespace Editor {

	const std::vector<NE::ECS::Entity>& EditorSelection::GetSelection() const {
		return m_selection;
	}

	NE::ECS::Entity EditorSelection::GetPrimary()    const { 
		return m_primary; 
	}

	NE::ECS::Entity EditorSelection::GetLastClicked() const { 
		return m_lastClicked; 
	}

	NE::ECS::Entity EditorSelection::GetLastDropped() const {
		return m_lastDropped;
	}

	bool EditorSelection::Contains(NE::ECS::Entity e) const {
		return std::find(m_selection.begin(), m_selection.end(), e) != m_selection.end();
	}

	bool EditorSelection::Empty() const { 
		return m_selection.empty() && m_primary == NE::ECS::NO_ENTITY && m_lastClicked == NE::ECS::NO_ENTITY;
	}

	std::vector<NE::ECS::Entity> EditorSelection::GetTopLevelSelection(
		const std::function<NE::ECS::Entity(NE::ECS::Entity)>& getParent) const
	{
		std::vector<NE::ECS::Entity> result;
		result.reserve(m_selection.size());

		for (NE::ECS::Entity e : m_selection) {
			NE::ECS::Entity parent = getParent(e);
			if (parent == NE::ECS::NO_ENTITY) {
				result.push_back(e);
				continue;
			}

			// If parent is also selected, skip this one
			if (!Contains(parent)) {
				result.push_back(e);
			}
		}

		return result;
	}

	void EditorSelection::Clear() {
		m_selection.clear();
		m_primary = NE::ECS::NO_ENTITY;
		m_lastClicked = NE::ECS::NO_ENTITY;
		m_lastDropped = NE::ECS::NO_ENTITY;
	}

	void EditorSelection::SetSingle(NE::ECS::Entity e) {
		m_selection.clear();

		if (e != NE::ECS::NO_ENTITY) {
			m_selection.push_back(e);
		}

		m_primary = e;
		m_lastClicked = e;
	}

	void EditorSelection::SetDropped(NE::ECS::Entity e) {
		m_lastDropped = e;
	}

	void EditorSelection::Toggle(NE::ECS::Entity e) {
		if (e == NE::ECS::NO_ENTITY) return;

		auto it = std::find(m_selection.begin(), m_selection.end(), e);
		if (it == m_selection.end()) {
			m_selection.push_back(e);
			m_primary = e;
		} else {
			m_selection.erase(it);
			if (m_primary == e)
				m_primary = m_selection.empty() ? NE::ECS::NO_ENTITY : m_selection.back();
		}

		m_lastClicked = e;
	}

	void EditorSelection::Add(NE::ECS::Entity e) {
		if (e == NE::ECS::NO_ENTITY)
			return;

		if (!Contains(e)) {
			m_selection.push_back(e);
			if (m_primary == NE::ECS::NO_ENTITY)
				m_primary = e;
		}
	}

	void EditorSelection::RangeSelect(NE::ECS::Entity anchor, NE::ECS::Entity e, const std::vector<NE::ECS::Entity>& preorder) {
		if (anchor == NE::ECS::NO_ENTITY || e == NE::ECS::NO_ENTITY) {
			SetSingle(e);
			return;
		}

		int idxAnchor = -1;
		int idxE = -1;

		for (int i = 0; i < (int)preorder.size(); ++i) {
			if (preorder[i] == anchor) idxAnchor = i;
			if (preorder[i] == e)      idxE = i;
		}

		if (idxAnchor == -1 || idxE == -1) {
			// Fallback if one of them isn't in the list (shouldn't normally happen)
			SetSingle(e);
			return;
		}

		if (idxAnchor > idxE) std::swap(idxAnchor, idxE);

		m_selection.clear();
		for (int i = idxAnchor; i <= idxE; ++i) {
			m_selection.push_back(preorder[i]);
		}

		m_primary = e;
		m_lastClicked = e;
	}

	void EditorSelection::SetLastPreorder(const std::vector<NE::ECS::Entity>& preorder) {
		m_lastPreorder = preorder;
	}

	const std::vector<NE::ECS::Entity>& EditorSelection::GetLastPreorder() const {
		return m_lastPreorder;
	}

	void EditorSelection::Remove(NE::ECS::Entity e) {
		auto it = std::remove(m_selection.begin(), m_selection.end(), e);
		if (it != m_selection.end()) {
			m_selection.erase(it, m_selection.end());
		}

		if (m_primary == e)
			m_primary = m_selection.empty() ? NE::ECS::NO_ENTITY : m_selection.front();

		if (m_lastClicked == e)
			m_lastClicked = m_primary;
	}

	void EditorSelection::RemoveIf(const std::function<bool(NE::ECS::Entity)>& pred) {
		bool changedPrimary = false;
		bool changedLast = false;

		m_selection.erase(
			std::remove_if(m_selection.begin(), m_selection.end(),
				[&](NE::ECS::Entity e) {
					if (!pred(e)) return false;
					if (e == m_primary)    changedPrimary = true;
					if (e == m_lastClicked) changedLast = true;
					return true;
				}),
			m_selection.end());

		if (changedPrimary)
			m_primary = m_selection.empty() ? NE::ECS::NO_ENTITY : m_selection.front();
		if (changedLast)
			m_lastClicked = m_primary;
	}
}