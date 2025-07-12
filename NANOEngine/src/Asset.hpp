/*********************************************************************
 * \file		Asset.hpp
 * \brief		Interface for all assets that can be loaded from a
 *              file.
 *
 * \author		y.ziyangirwen, 2301345 (y.ziyangirwen@digipen.edu)
 * \date		1 September 2024
 *
 * \copyright	Copyright(C) 2024 DigiPen Institute of Technology.
 *				Reproduction or disclosure of this file or its
 *              contents without the prior written consent of DigiPen
 *              Institute of Technology is prohibited.
 *********************************************************************/

#ifndef ASSET_HPP
#define ASSET_HPP

#include <string>
#include "NANOEngineAPI.hpp"

 /**
  * \class IAsset
  * \brief Interface for all assets that can be loaded from a file.
  *
  * The IAsset class provides a virtual interface for assets, requiring a method to load assets from a file.
  * All asset types (e.g., textures, sounds) should inherit from this class.
  */

#pragma warning(push)
#pragma warning(disable: 4251)

namespace NANOEngine::Asset {

	class NANOENGINE_API IAsset {
	public:
		/**
		 * \brief Default constructor.
		 */
		IAsset() = default;

		/**
		 * \brief Virtual destructor.
		 */
		virtual ~IAsset() = default;

		/**
		 * \brief Loads the asset from a file.
		 *
		 * \param fileName The name of the file to load the asset from.
		 * \return True if the asset is loaded successfully, false otherwise.
		 */
		virtual bool LoadFromFile(const std::string& fileName) = 0;

		std::string uuid;
		std::string filePath;
	protected:
		std::string displayName;
	};
}
#pragma warning(pop)
#endif // !ASSET_HPP
