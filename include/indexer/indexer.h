#ifndef INDEXER_INDEXER_H_
#define INDEXER_INDEXER_H_

#include <filesystem>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace Indexer
{
struct PathCanonicalHasher
{
	std::size_t operator()(std::filesystem::path const& path)
	{
		return std::filesystem::hash_value(path.lexically_normal());
	}
};

enum class Recursive
{
	No, Yes
};

using PathSet = std::unordered_set<std::filesystem::path, PathCanonicalHasher>;

template <typename Tokenizer>
class Indexer
{
public:
	Indexer() {}
	Indexer(Tokenizer const& tokenizer_): tokenizer{tokenizer_} {}
	Indexer(Tokenizer&& tokenizer_): tokenizer{std::move(tokenizer_)} {}

	void addPath(std::filesystem::path fpath, Recursive = Recursive::Yes);

	PathSet const& search(std::string const& needle)
	{
		if (not invertedIndex.contains(needle))
			return empty;

		return invertedIndex.at(needle);
	}

private:
	Tokenizer tokenizer;

	std::unordered_map<std::filesystem::path, std::unordered_set<std::string>, PathCanonicalHasher> forwardIndex;  // for updating
	std::unordered_map<std::string, PathSet> invertedIndex;  // for querying

	static PathSet const empty;
};
}

#endif // INDEXER_INDEXER_H_
