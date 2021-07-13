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
	bool isDone{true};
};

enum class Recursive
{
	No, Yes
};

using PathSet = std::unordered_set<std::filesystem::path, PathHasher>;

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
			haystacks.insert(idToFile.at(i));
		return haystacks;
	}

private:
	void addDirectory(std::filesystem::path const& path, Recursive recursively)
	{
		// only locking for these two operations
		{
			std::unique_lock pin{indexMutex};
			watcher.addDirectory(path);
			indexedDirectories.insert({path, recursively});
		}

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

	std::unordered_set<std::string> getFileTokens(std::filesystem::path const& path)
	{
		std::unordered_set<std::string> fileTokens;
		std::ifstream f{path};
		std::string buf;
		for (std::getline(f, buf); not f.eof(); std::getline(f, buf))
		{
			tokenizer.setSource(buf);
			while (not tokenizer.done())
			{
				auto token = std::string{tokenizer.next()};
				fileTokens.insert(token);
			}
		}
		return fileTokens;
	}

	void addFile(std::filesystem::path const& path)
	{
		std::unique_lock pin{indexMutex};
		watcher.addFile(path);

		int fileId;
		if (fileToId.contains(path))
		{
			fileId = fileToId.at(path);
		}
		else
		{
			fileId = nextId();
			fileToId.insert({path, fileId});
			idToFile.insert({fileId, path});
		}

		auto [insertIterator, didInsert] = forwardIndex.insert({fileId, getFileTokens(path)});
		auto& fileTokens = insertIterator->second;
		for (auto const& token: fileTokens)
		{
			if (not invertedIndex.contains(token))
			{
				invertedIndex.insert({token, {}});
			}
			invertedIndex.at(token).insert(fileId);
		}
	}

	void removeFile(std::filesystem::path const& path)
	{
		std::unique_lock pin{indexMutex};
		auto fileId = fileToId.at(path);

		auto& fileTokens = forwardIndex.at(fileId);
		for (auto const& token: fileTokens)
		{
			invertedIndex.at(token).erase(fileId);
		}
		forwardIndex.erase(fileId);
	}

	void reindexFile(std::filesystem::path const& path)
	{
		std::unique_lock pin{indexMutex};
		auto fileId = fileToId.at(path);

		std::unordered_set<std::string> newTokens = getFileTokens(path);
		for (auto const& token: newTokens)
		{
			if (not invertedIndex.contains(token))
			{
				invertedIndex.insert({token, {}});
			}
			invertedIndex.at(token).insert(fileId);
		}

		auto& fileTokens = forwardIndex.at(fileId);
		for (auto const& token: fileTokens)
		{
			if (not newTokens.contains(token))
			{
				invertedIndex.at(token).erase(fileId);
			}
		}
		fileTokens = std::move(newTokens);
	}

	void watchFilesystem()
	{
		while (not doStop)
		{
			for (auto const& event: watcher.pollEvents())
			{
				switch (event.type)
				{
					case FilesystemWatcher::EventType::Modified:
						reindexFile(event.path);
						break;

					case FilesystemWatcher::EventType::Created:
						{
							auto parent = event.path.parent_path();
							if (not event.isDirectory)
							{
								addFile(event.path);
							}
							else if (indexedDirectories.at(parent) == Recursive::Yes)
							{
								addDirectory(event.path, Recursive::Yes);
							}
							// TODO recreation
						}
						break;

					case FilesystemWatcher::EventType::Deleted:
						if (fileToId.contains(event.path))
						{
							removeFile(event.path);
						}
						// TODO add recreation watch
						break;
				}
			}
		}
	}

	int nextId()
	{
		static int next = 0;
		return next++;
	}

	Tokenizer tokenizer;

	bool doStop{false};
	FilesystemWatcher watcher;
	std::thread filesystemWatcherThread{&Indexer::watchFilesystem, this};

	std::unordered_map<std::filesystem::path, Recursive, PathHasher> indexedDirectories;

	std::mutex indexMutex;

	std::unordered_map<int, std::filesystem::path> idToFile;
	std::unordered_map<std::filesystem::path, int, PathHasher> fileToId;

	std::unordered_map<int, std::unordered_set<std::string>> forwardIndex;  // for updating
	std::unordered_map<std::string, std::unordered_set<int>> invertedIndex;  // for querying
};
}

#endif // INDEXER_INDEXER_H_
