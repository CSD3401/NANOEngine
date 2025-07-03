#ifndef MAT_3_HPP
#define MAT_3_HPP

// Matrix elements stored in memory using column-major order.
namespace NANOEngine::Math {
	struct Mat3 {
		float a[9] = { 0 };

		/*!***********************************************************************
		\brief
			Getter/setter function to get matrix element at specified row and col.
		\param[in] unsigned int row
			Row of matrix to get/set.
		\param[in] unsigned int col
			Column of matrix to get/set.
		\return
			Reference to element of matrix at specified row and column.
		*************************************************************************/
		float& GetElement(unsigned int row, unsigned int col);

		/*!***********************************************************************
		\brief
			Getter function to get const matrix element at specified row and col.
		\param[in] unsigned int row
			Row of matrix to get/set.
		\param[in] unsigned int col
			Column of matrix to get/set.
		\return
			Const reference to element of matrix at specified row and column.
		*************************************************************************/
		const float& GetElement(unsigned int row, unsigned int col) const;

		/*!***********************************************************************
		\brief
			Returns the determinant of the Matrix.
		\return
			Determinant of the Matrix.
		*************************************************************************/
		float Determinant() const;
	};
}

#endif