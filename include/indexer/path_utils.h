#ifndef INDEXER_PATH_UTILS_H_
#define INDEXER_PATH_UTILS_H_

#include <filesystem>
#include <functional>

namespace Indexer
{
struct PathHasher
{
	std::size_t operator()(std::filesystem::path const& path) const
	{
		return std::filesystem::hash_value(path);
	}
};

inline std::filesystem::path head(std::filesystem::path const& path)
{
	return *path.begin();
}

inline std::filesystem::path tail(std::filesystem::path const& path)
{
	return path.lexically_relative(head(path));
}
}

#endif // INDEXER_PATH_UTILS_H_
