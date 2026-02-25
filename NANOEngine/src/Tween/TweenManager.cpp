#include "pch.h"
#include "TweenManager.hpp"

TweenManager& TweenManager::Get()
{
	static TweenManager instance;
	return instance;
}

TweenManager::TweenManager() : tweens{}
{
}

void TweenManager::Update(float dt)
{
	if (tweens.empty())
	{
		return;
	}

	for (auto it = tweens.begin(); it != tweens.end();)
	{
		// Erase inactive Tweenss
		if (!(*it)->IsActive())
		{
			it = tweens.erase(it);
			continue;
		}

		(*it)->Update(dt);
		++it;
	}
}

void TweenManager::Clean()
{
	for (auto it = tweens.begin(); it != tweens.end();)
	{
		it = tweens.erase(it);
	}
}

TweenManager::~TweenManager()
{
	Clean();
}