#ifndef INDEXER_INDEXER_H_
#define INDEXER_INDEXER_H_

#include <filesystem>
#include <fstream>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace Indexer
{
struct PathCanonicalHasher
{
	std::size_t operator()(std::filesystem::path const& path) const
	{
		return std::filesystem::hash_value(path.lexically_normal());
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

	std::string_view next()
	{
		auto t = nextToken;
		findNext();
		return t;
	}

	bool done() { return isDone; }

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

	void addPath(std::filesystem::path path, Recursive = Recursive::Yes)
	{
		if (forwardIndex.contains(path))  // No-op
		{
			return;
		}

		forwardIndex.insert({path, {}});

		std::ifstream f{path};
		std::string buf;
		for (std::getline(f, buf); not f.eof(); std::getline(f, buf))
		{
			tokenizer.setSource(buf);
			while (not tokenizer.done())
			{
				auto token = tokenizer.next();

				forwardIndex.at(path).emplace(token);

				if (not invertedIndex.contains(std::string{token}))
				{
					invertedIndex.insert({std::string{token}, {}});
				}
				invertedIndex.at(std::string{token}).insert(path);
			}
		}
	}

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

template <typename T>
PathSet const Indexer<T>::empty;
}

#endif // INDEXER_INDEXER_H_
