#include "pch.h"
#include "TweenBase.hpp"

void TweenBase::Interpolate(float& t, TweenType type)
{
	switch (type)
	{
	case TweenType::LINEAR:
	{
		// Don't have to do anything, since value of t is incremented in child function Update().
		break;
	}
	case TweenType::EASE_IN:
	{
		// y = x^2
		t = t * t;
		break;
	}
	case TweenType::EASE_OUT:
	{
		// y = 1 - (1 - x)^2
		t = 1 - (1 - t) * (1 - t);
		break;
	}
	case TweenType::EASE_BOTH:
	{
		// y = x^2 / x^2 + (1 - x)^2
		t = (t * t) / ((t * t) + ((1 - t) * (1 - t)));
		break;
	}
	case TweenType::CUBIC_EASE_IN:
	{
		// y = x^3
		t = t * t * t;
		break;
	}
	case TweenType::CUBIC_EASE_OUT:
	{
		// y = 1 - (1 - x)^3
		t = 1 - (1 - t) * (1 - t) * (1 - t);
		break;
	}
	case TweenType::CUBIC_EASE_BOTH:
	{
		// y = x^3 / x^3 + (1 - x)^3
		t = (t * t * t) / ((t * t * t) + ((1 - t) * (1 - t) * (1 - t)));
		break;
	}
	default:
	{
		break;
	}
	}
}