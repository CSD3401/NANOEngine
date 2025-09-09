#pragma once

enum TweenType
{
	LINEAR = 0,
	EASE_IN,
	EASE_OUT,
	EASE_BOTH,
	CUBIC_EASE_IN,
	CUBIC_EASE_OUT,
	CUBIC_EASE_BOTH,
	END
};

class TweenBase
{
public:
	virtual ~TweenBase() = default;
	virtual void Update(float dt) = 0;
	virtual bool IsActive() const = 0;
	void Interpolate(float& t, TweenType type);
};