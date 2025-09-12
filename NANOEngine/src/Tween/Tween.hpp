#pragma once
#include "TweenBase.hpp"

template <typename V, typename U, typename Object>
class Tween : public TweenBase
{
public:
	Tween(
		Object* obj,				// Pointer to object
		void (Object::* setter)(V),	// Function pointer to object's setter
		U const& start,				// Start value
		U const& end,				// End value
		float duration,				// Duration in seconds
		TweenType type);			// Type of Tween

	bool IsActive() const;
	void Update(float dt);
	void* GetObject() const override { return static_cast<void*>(obj); }

private:
	Object* obj;
	void (Object::* setter)(V);
	U start;
	U delta;
	float duration;
	float elapsed;
	bool active;
	TweenType type;
};

template <typename V, typename U, typename Object>
Tween<V, U, Object>::Tween(
	Object* obj,				// Pointer to object
	void (Object::* setter)(V),	// Function pointer to object's setter
	U const& start,				// Start value
	U const& end,				// End value
	float duration,				// Duration in seconds
	TweenType type)				// Type of Tween
	: obj		{ obj }
	, setter	{ setter }
	, start		{ start }
	, delta		{ end - start }
	, duration	{ duration }
	, elapsed	{ 0.0f }
	, active	{ true }
	, type		{ type }
{
}

template <typename V, typename U, typename Object>
bool Tween<V, U, Object>::IsActive() const
{
	return active;
}

template <typename V, typename U, typename Object>
void Tween<V, U, Object>::Update(float dt)
{
	// Return if not active
	if (!active)
	{
		return;
	}

	// Count
	elapsed += dt;

	// Once exceed, we update one final time before removing the Tween
	if (elapsed > duration)
	{
		active = false;
		elapsed = duration;
	}

	// Calculate and interpolate
	float t = elapsed / duration;
	Interpolate(t, type);

	// Use object's setter function to update
	(*obj.*setter)(start + delta * t);
}