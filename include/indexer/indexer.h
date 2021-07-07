#ifndef INDEXER_INDEXER_H_
#define INDEXER_INDEXER_H_

#include <filesystem>
#include <fstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace Indexer
{
struct PathCanonicalHasher
{
	std::size_t operator()(std::filesystem::path const& path) const
	{
		return std::filesystem::hash_value(path);
	}
};

class WordTokenizer
{
public:
	void setSource(std::string_view newSource)
	{
		source = newSource;
		cursor = source.begin();
		isDone = false;
		findNext();
	}

	[[nodiscard]] std::string_view next()
	{
		auto t = nextToken;
		findNext();
		return t;
	}

	[[nodiscard]] bool done() const { return isDone; }

private:
	void findNext()
	{
		if (isDone)
		{
			return;
		}

		if (cursor == source.end())
		{
			nextToken = "";
			isDone = true;
			return;
		}

		auto end = std::find_if_not(cursor, source.end(), [](auto c){ return std::isalnum(c); });
		nextToken = std::string_view{cursor, end}; 
		cursor = std::find_if(end, source.end(), [](auto c){ return std::isalnum(c); });
	}

	using size_type = std::string_view::size_type;

	std::string_view source;
	std::string_view nextToken;
	std::string_view::iterator cursor;
	bool isDone = true;
};

enum class Recursive
{
	No, Yes
};

using PathSet = std::unordered_set<std::filesystem::path, PathCanonicalHasher>;

template <typename Tokenizer=WordTokenizer>
class Indexer
{
public:
	Indexer() {}
	Indexer(Tokenizer const& tokenizer_): tokenizer{tokenizer_} {}
	Indexer(Tokenizer&& tokenizer_): tokenizer{std::move(tokenizer_)} {}

	void addPath(std::filesystem::path const& path, Recursive recursively = Recursive::Yes)
	{
		if (not std::filesystem::exists(path))
			return;

		auto canonicalPath = std::filesystem::canonical(path);
		
		if (std::filesystem::is_directory(canonicalPath))
		{
			addDirectory(canonicalPath, recursively);
		}
		else
		{
			addFile(canonicalPath);
		}
	}

	[[nodiscard]] PathSet search(std::string const& needle) const
	{
		if (not invertedIndex.contains(needle))
			return PathSet{};

		PathSet haystacks;
		for (auto&& i: invertedIndex.at(needle))
			haystacks.insert(indexedFiles[i]);
		return haystacks;
	}

private:
	void addDirectory(std::filesystem::path const& path, Recursive recursively)
	{
		for (auto&& p: std::filesystem::directory_iterator(path))
		{
			auto entry = p.path();
			if (std::filesystem::is_directory(entry) && recursively == Recursive::Yes)
			{
				addPath(entry, recursively);
			}
			else
			{
				addFile(entry);
			}
		}
	}

	void addFile(std::filesystem::path const& path)
	{
		if (forwardIndex.contains(path))  // No-op
		{
			return;
		}

		auto fileId = indexedFiles.size();
		indexedFiles.push_back(path);

		auto [insertIterator, didInsert] = forwardIndex.insert({path, {}});
		auto& fileTokens = insertIterator->second;

		std::ifstream f{path};
		std::string buf;
		for (std::getline(f, buf); not f.eof(); std::getline(f, buf))
		{
			tokenizer.setSource(buf);
			while (not tokenizer.done())
			{
				auto token = std::string{tokenizer.next()};
				fileTokens.insert(token);

				if (not invertedIndex.contains(token))
				{
					invertedIndex.insert({token, {}});
				}
				invertedIndex.at(token).insert(fileId);
			}
		}
	}

	Tokenizer tokenizer;
	std::vector<std::filesystem::path> indexedFiles;
	std::unordered_map<std::filesystem::path, std::unordered_set<std::string>, PathCanonicalHasher> forwardIndex;  // for updating
	std::unordered_map<std::string, std::unordered_set<std::size_t>> invertedIndex;  // for querying
};
}

#endif // INDEXER_INDEXER_H_
