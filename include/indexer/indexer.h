#ifndef INDEXER_INDEXER_H_
#define INDEXER_INDEXER_H_

#include <atomic>
#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "indexer/filesystem_watcher.h"
#include "indexer/path_utils.h"

namespace Indexer
{
class Tokenizer
{
public:
	virtual void sendLine(std::string_view newLine) = 0;
	virtual void sendEof() = 0;

	virtual std::unique_ptr<Tokenizer> clone() const = 0;

	[[nodiscard]] virtual std::string_view next() = 0;
	virtual bool done() const = 0;

	virtual ~Tokenizer() = default;
};

template <class T>
concept DerivedTokenizer = std::derived_from<T, Tokenizer>;

class WordTokenizer final: public Tokenizer
{
public:
	virtual void sendLine(std::string_view newLine) override
	{
		source = newLine;
		cursor = std::find_if(source.begin(), source.end(), isWordCharacter);
		isDone = false;
		findNext();
	}

	virtual void sendEof() override {}

	virtual std::unique_ptr<Tokenizer> clone() const override { return std::make_unique<WordTokenizer>(); }

	[[nodiscard]] virtual std::string_view next() override
	{
		auto t = nextToken;
		findNext();
		return t;
	}

	virtual bool done() const override { return isDone; }

private:
	void findNext()
	{
		if (isDone)
		{
			return;
		}

		if (cursor == source.end())
		{
			isDone = true;
			return;
		}

		auto end = std::find_if_not(cursor, source.end(), isWordCharacter);
		nextToken = std::string_view{cursor, end}; 
		cursor = std::find_if(end, source.end(), isWordCharacter);
	}

	static constexpr auto isWordCharacter = [](auto c){ return c >= 0 && c < 256 && std::isalnum(c); };

	std::string_view source;
	std::string_view nextToken;
	std::string_view::iterator cursor;
	bool isDone{true};
};

enum class Recursive
{
	No, Yes
};

using PathSet = std::unordered_set<std::filesystem::path, PathHasher>;

class Indexer
{
public:
	Indexer(): tokenizer{std::make_unique<WordTokenizer>()} {}

	template <DerivedTokenizer T>
	Indexer(T const& tokenizer_): tokenizer{std::make_unique<T>(tokenizer_)} {}

	template <DerivedTokenizer T>
	Indexer(T&& tokenizer_): tokenizer{std::make_unique<T>(std::move(tokenizer_))} {}

	~Indexer()
	{
		doStop = true;
		filesystemWatcherThread.join();
	}

	void addPath(std::filesystem::path const&, Recursive = Recursive::No);

	[[nodiscard]] PathSet search(std::string const& needle) const;

private:
	void addDirectory(std::filesystem::path const&, Recursive);

	void addFile(std::filesystem::path const&);
	void addFileAsync(std::filesystem::path const&, std::thread::id parent);
	void removeFile(std::filesystem::path const&);
	void reindexFile(std::filesystem::path const&);

	void awaitCreation(std::filesystem::path const&);
	void watchFilesystem();

	int getFileId(std::filesystem::path const& path)
	{
		static int next = 0;

		if (fileToId.contains(path))
		{
			return fileToId.at(path);
		}
		else
		{
			auto fileId = next++;
			fileToId.insert({path, fileId});
			idToFile.insert({fileId, path});
			return fileId;
		}
	}

	std::unique_ptr<Tokenizer> tokenizer;

	unsigned maxWorkers{std::thread::hardware_concurrency()};
	unsigned numWorkers{0};
	std::unordered_map<std::thread::id, unsigned> threadWorkers;

	std::mutex workerMutex;
	std::condition_variable workerSync;

	// paths that were *explicitly* added by the user
	PathSet addedPaths;
	std::unordered_map<std::filesystem::path, Recursive, PathHasher> indexedDirectories;

	mutable std::mutex indexMutex;

	std::unordered_map<std::filesystem::path, PathSet, PathHasher> creationWatches;

	std::unordered_map<int, std::filesystem::path> idToFile;
	std::unordered_map<std::filesystem::path, int, PathHasher> fileToId;

	std::unordered_map<int, std::unordered_set<std::string>> forwardIndex;  // for updating
	std::unordered_map<std::string, std::unordered_set<int>> invertedIndex;  // for querying

	std::atomic<bool> doStop{false};
	FilesystemWatcher watcher;
	std::thread filesystemWatcherThread{&Indexer::watchFilesystem, this};
};
}

#endif // INDEXER_INDEXER_H_
