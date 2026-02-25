#include "pch.h"
#include "Mat3.hpp"

namespace NE::Math {
	float& Mat3::GetElement(unsigned int row, unsigned int col)
	{
		return a[col * 3 + row];
	}

	const float& Mat3::GetElement(unsigned int row, unsigned int col) const
	{
		return a[col * 3 + row];
	}

	float Mat3::Determinant() const {
		return GetElement(0, 0) * (GetElement(1, 1) * GetElement(2, 2) - GetElement(1, 2) * GetElement(2, 1))
			- GetElement(0, 1) * (GetElement(1, 0) * GetElement(2, 2) - GetElement(1, 2) * GetElement(2, 0))
			+ GetElement(0, 2) * (GetElement(1, 0) * GetElement(2, 1) - GetElement(1, 1) * GetElement(2, 0));
	}
}