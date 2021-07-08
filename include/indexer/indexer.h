#ifndef INDEXER_INDEXER_H_
#define INDEXER_INDEXER_H_

#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "indexer/inotify_filesystem_watcher.h"
#include "indexer/path_utils.h"

namespace Indexer
{
class WordTokenizer
{
public:
	void setSource(std::string_view newSource)
	{
		source = newSource;
		cursor = std::find_if(source.begin(), source.end(), [](auto c){ return std::isalnum(c); });
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

	~Indexer()
	{
		doStop = true;
		filesystemWatcherThread.join();
	}

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
		watcher.addDirectory(path);
		for (auto&& p: std::filesystem::directory_iterator(path))
		{
			auto entry = p.path();
			if (std::filesystem::is_directory(entry) && recursively == Recursive::Yes)
			{
				addDirectory(entry, recursively);
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

		std::unique_lock pin{indexMutex};
		watcher.addFile(path);
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

	void watchFilesystem()
	{
		while (not doStop)
		{
			watcher.query();
		}
	}

	Tokenizer tokenizer;

	bool doStop = false;
	FilesystemWatcher watcher;
	std::thread filesystemWatcherThread{&Indexer::watchFilesystem, this};

	std::mutex indexMutex;

	std::vector<std::filesystem::path> indexedFiles;
	std::unordered_map<std::filesystem::path, std::unordered_set<std::string>, PathCanonicalHasher> forwardIndex;  // for updating
	std::unordered_map<std::string, std::unordered_set<std::size_t>> invertedIndex;  // for querying
};
}

#endif // INDEXER_INDEXER_H_
