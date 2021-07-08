#ifndef INDEXER_PATH_UTILS_H_
#define INDEXER_PATH_UTILS_H_

#include <filesystem>
#include <functional>

namespace Indexer
{
struct PathCanonicalHasher
{
	std::size_t operator()(std::filesystem::path const& path) const
	{
		return std::filesystem::hash_value(path);
	}
};
}

#endif // INDEXER_PATH_UTILS_H_
