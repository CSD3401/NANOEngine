#pragma once
#include "TweenBase.hpp"
#include "Math/Vec3.hpp"

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
		TweenType type,				// Type of Tween
		bool wrap360 = false);		// For rotation vectors

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

	// Helper function
	NE::Math::Vec3 WrappedDelta(
		NE::Math::Vec3 const& vecStart,
		NE::Math::Vec3 const& vecEnd, 
		float wrapValue = 360.0f);
};

template <typename V, typename U, typename Object>
Tween<V, U, Object>::Tween(
	Object* obj,				// Pointer to object
	void (Object::* setter)(V),	// Function pointer to object's setter
	U const& start,				// Start value
	U const& end,				// End value
	float duration,				// Duration in seconds
	TweenType type,				// Type of Tween
	bool wrap360)				// For rotation vectors
	: obj		{ obj }
	, setter	{ setter }
	, start		{ start }
	, delta		{ }
	, duration	{ duration }
	, elapsed	{ 0.0f }
	, active	{ true }
	, type		{ type }
{
	if (wrap360)
	{
		if constexpr (std::is_same_v<NE::Math::Vec3, U>)
		{
			delta = WrappedDelta(start, end);
		}
		else
		{
			// Log some warning here since we are no longer dealing with a rotation vector...
		}
	}
	else
	{
		delta = end - start;
	}
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

template<typename V, typename U, typename Object>
inline NE::Math::Vec3 Tween<V, U, Object>::WrappedDelta(
	NE::Math::Vec3 const& vecStart, 
	NE::Math::Vec3 const& vecEnd, 
	float wrapValue)
{
	auto ShortestWrap = [wrapValue](float s, float e)
		{
			return fmodf((e - s + wrapValue * 1.5f), wrapValue) - wrapValue * 0.5f;
		};

	return NE::Math::Vec3(
		ShortestWrap(vecStart.x, vecEnd.x),
		ShortestWrap(vecStart.y, vecEnd.y),
		ShortestWrap(vecStart.z, vecEnd.z));
}