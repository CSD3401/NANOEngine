#ifndef IASSET_HPP
#define IASSET_HPP

#include <string>

namespace Editor::Assets {
	struct IAsset {
	public:
		virtual ~IAsset() = default;
		
		virtual bool Cook(const std::string& sourcePath,
			const std::string& outPath) const = 0;

		virtual bool LoadImportSettings(const std::string& sourcePath) = 0;
		virtual bool SaveImportSettings(const std::string& sourcePath) = 0;
	};

}

#endif // !IASSET_HPP
