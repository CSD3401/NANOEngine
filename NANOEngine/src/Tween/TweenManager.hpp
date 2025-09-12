#pragma once
#include "Tween.hpp"
#include <vector>
#include <memory>
#include "../NANOEngineAPI.hpp"

#pragma warning(push)
#pragma warning(disable: 4251) // suppress STL export warnings

class NANOENGINE_API TweenManager
{
public:
	TweenManager();
	~TweenManager();
	static TweenManager& Get();

	TweenManager(TweenManager const&) = delete;
	TweenManager& operator=(TweenManager const&) = delete;

	template <typename V, typename U, typename Object>
	void StartTween(
		Object* obj,
		void (Object::* setter)(V),
		U const& start,
		U const& end,
		float duration,
		TweenType type = TweenType::CUBIC_EASE_IN);

	template <typename Object>
	bool CheckTween(Object* obj);

	void Update(float dt);

	void Clean();

private:
	std::vector<std::unique_ptr<TweenBase>> tweens;
};

template <typename V, typename U, typename Object>
void TweenManager::StartTween(
	Object* object,
	void (Object::* setter)(V),
	U const& start,
	U const& end,
	float duration,
	TweenType type)
{
	tweens.push_back(std::make_unique<Tween<V, U, Object>>(object, setter, start, end, duration, type));
}

template <typename Object>
bool TweenManager::CheckTween(Object* obj)
{
	for (auto it = tweens.begin(); it != tweens.end(); ++it)
	{
		if ((*it)->GetObject() == static_cast<void*>(obj))
		{
			return true;
		}
	}
	return false;
}

#pragma warning(pop)