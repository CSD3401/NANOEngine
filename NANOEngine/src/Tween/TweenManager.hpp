#pragma once
#include "Tween.hpp"
#include <vector>
#include <memory>

class TweenManager
{
public:
	TweenManager();
	~TweenManager();
	static TweenManager& Get();

	template <typename V, typename U, typename Object>
	void StartTween(
		Object& obj,
		void (Object::* setter)(V),
		U const& start,
		U const& end,
		float duration,
		TweenType type = TweenType::CUBIC_EASE_IN);

	void Update(float dt);

	void Clean();

private:
	std::vector<std::unique_ptr<TweenBase>> tweens;
};

template <typename V, typename U, typename Object>
void TweenManager::StartTween(
	Object& object,
	void (Object::* setter)(V),
	U const& start,
	U const& end,
	float duration,
	TweenType type)
{
	tweens.push_back(std::make_unique<Tween<V, U, Object>>(object, setter, start, end, duration, type));
}