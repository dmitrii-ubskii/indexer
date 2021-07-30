#include "indexer/indexer.h"

#include <cassert>
#include <iostream>

void Indexer::Indexer::addPath(std::filesystem::path const& path, Recursive recursively)
{
	auto canonicalPath = std::filesystem::weakly_canonical(path);
	if (not canonicalPath.has_root_path())
	{
		canonicalPath = std::filesystem::weakly_canonical(".") / path;
	}
	addedPaths.insert(canonicalPath);

	if (not std::filesystem::exists(canonicalPath))
	{
		awaitCreation(canonicalPath);
		if (recursively == Recursive::Yes)  // assume directory
		{
			indexedDirectories.insert({canonicalPath, recursively});
		}
	}
	else if (std::filesystem::is_directory(canonicalPath))
	{
		addDirectory(canonicalPath, recursively);
	}
	else
	{
		addFile(canonicalPath);
	}
}

[[nodiscard]] Indexer::PathSet Indexer::Indexer::search(std::string const& needle) const
{
	std::unique_lock pin{indexMutex};
	if (not invertedIndex.contains(needle))
		return PathSet{};

	PathSet haystacks;
	for (auto&& i: invertedIndex.at(needle))
	{
		assert(idToFile.contains(i));
		haystacks.insert(idToFile.at(i));
	}
	return haystacks;
}

void Indexer::Indexer::addDirectory(std::filesystem::path const& path, Recursive recursively)
{
	assert(std::filesystem::is_directory(path));

	// only locking for these two operations
	{
		std::unique_lock pin{indexMutex};
		watcher.addDirectory(path);
		indexedDirectories.insert({path, recursively});
	}

	for (auto&& p: std::filesystem::directory_iterator(path))
	{
		auto entry = p.path();
		if (not std::filesystem::is_directory(entry))
		{
			addFile(entry);
		}
		else if (recursively == Recursive::Yes)
		{
			addDirectory(entry, recursively);
		}
	}
}

std::unordered_set<std::string> getFileTokens(std::filesystem::path const& path, Indexer::Tokenizer& tokenizer)
{
	std::unordered_set<std::string> fileTokens;
	std::ifstream f{path};
	std::string buf;
	while (not f.eof())
	{
		std::getline(f, buf);
		tokenizer.sendLine(buf);

		if (f.eof())
		{
			tokenizer.sendEof();
		}

		while (not tokenizer.done())
		{
			auto token = std::string{tokenizer.next()};
			fileTokens.insert(token);
		}
	}

	return fileTokens;
}

void Indexer::Indexer::addFile(std::filesystem::path const& path)
{
	if (not std::filesystem::exists(path))  // deleted while we weren't looking
	{
		return;
	}

	assert(std::filesystem::is_regular_file(path) || std::filesystem::is_symlink(path));

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

	auto [insertIterator, didInsert] = forwardIndex.insert({fileId, getFileTokens(path, *tokenizer)});
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

void Indexer::Indexer::removeFile(std::filesystem::path const& path)
{
	std::unique_lock pin{indexMutex};
	assert(fileToId.contains(path));
	auto fileId = fileToId.at(path);
	if (forwardIndex.contains(fileId))
	{
		auto& fileTokens = forwardIndex.at(fileId);
		for (auto const& token: fileTokens)
		{
			invertedIndex.at(token).erase(fileId);
		}
		forwardIndex.erase(fileId);
	}
}

void Indexer::Indexer::reindexFile(std::filesystem::path const& path)
{
	if (not std::filesystem::exists(path))  // deleted while we weren't looking
	{
		return;
	}

	std::unique_lock pin{indexMutex};
	assert(fileToId.contains(path));
	auto fileId = fileToId.at(path);

	std::unordered_set<std::string> newTokens = getFileTokens(path, *tokenizer);
	for (auto const& token: newTokens)
	{
		if (not invertedIndex.contains(token))
		{
			invertedIndex.insert({token, {}});
		}
		invertedIndex.at(token).insert(fileId);
	}

	assert(forwardIndex.contains(fileId));
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

void Indexer::Indexer::awaitCreation(std::filesystem::path const& path)
{
	try
	{
		if (std::filesystem::exists(path))
		{
			addPath(path);
			return;
		}

		auto existingParent = path.parent_path();
		while (not std::filesystem::exists(existingParent))
		{
			existingParent = existingParent.parent_path();
		}
		if (not creationWatches.contains(existingParent))
		{
			watcher.addDirectory(existingParent);
			creationWatches.insert({existingParent, {}});
		}
		creationWatches.at(existingParent).insert(path.lexically_relative(existingParent));
	}
	catch(const std::filesystem::filesystem_error& e)
	{
		std::cerr << e.what() << '\n';
		std::cerr << e.code() << '\n';
	}
}

void Indexer::Indexer::watchFilesystem()
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
							if (indexedDirectories.contains(parent) || addedPaths.contains(event.path))
							{
								addFile(event.path);
							}
						}
						else if (indexedDirectories.contains(parent) && indexedDirectories.at(parent) == Recursive::Yes)
						{
							addDirectory(event.path, Recursive::Yes);
						}

						if (creationWatches.contains(parent))
						{
							auto& watches = creationWatches.at(parent);
							auto name = event.path.filename();

							if (watches.contains(name))
							{
								addPath(event.path);
								watches.erase(name);
							}

							for (auto it = watches.begin(); it != watches.end(); )
							{
								auto& path = *it;

								if (path.has_parent_path() && head(path) == name)
								{
									auto fullPath = parent / path;
									it = watches.erase(it);  // advances the iterator
									awaitCreation(fullPath);
								}
								else  // advance normally
								{
									++it;
								}
							}

							if (watches.empty())
							{
								watcher.removePath(parent);
								creationWatches.erase(parent);
							}
						}
					}
					break;

				case FilesystemWatcher::EventType::Deleted:
					if (fileToId.contains(event.path))
					{
						removeFile(event.path);
					}
					if (addedPaths.contains(event.path))
					{
						awaitCreation(event.path);
					}
					if (creationWatches.contains(event.path))
					{
						for (auto& path: creationWatches.at(event.path))
						{
							awaitCreation(event.path / path);
						}
						watcher.removePath(event.path);
						creationWatches.erase(event.path);
					}
					break;
			}
		}
	}
}
