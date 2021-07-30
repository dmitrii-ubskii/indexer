#ifndef FILESYSTEM_WATCHER_IMPL_H_
#define FILESYSTEM_WATCHER_IMPL_H_

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <unordered_set>

#include <QCoreApplication>
#include <QFileSystemWatcher>

#include "indexer/filesystem_watcher.h"
#include "indexer/path_utils.h"

using namespace std::chrono_literals;

namespace Indexer
{
template <typename T>
class ThreadSafeQueue
{
public:
	~ThreadSafeQueue()
	{
		sync.notify_all();
	}

	std::vector<T> waitDrain()
	{
		std::unique_lock<std::mutex> pin{contentsMutex};
		sync.wait(pin);
		auto events = std::vector<T>{};
		std::swap(events, queue);
		return events;
	}

	std::vector<T> drain()
	{
		std::unique_lock<std::mutex> pin{contentsMutex};
		auto events = std::vector<T>{};
		std::swap(events, queue);
		return events;
	}

	void push(T const& ev)
	{
		std::unique_lock<std::mutex> pin{contentsMutex};
		queue.push_back(ev);
		sync.notify_one();
	}

private:
	bool emptyUnsafe() const
	{
		return queue.empty();
	}

	mutable std::mutex contentsMutex;
	std::condition_variable sync;

	std::vector<T> queue;
};

using EventQueue = ThreadSafeQueue<FilesystemWatcher::Event>;

class FilesystemWatcherImpl
{
public:
	~FilesystemWatcherImpl()
	{
		doStop = true;
		watcherThread.join();
	}

	void addFile(std::filesystem::path const& path)
	{
		addedFiles.push(path);
	}

	void addDirectory(std::filesystem::path const& path)
	{
		addedDirectories.push(path);
	}

	void removePath(std::filesystem::path const& path)
	{
		removedPaths.push(path);
	}

	std::vector<FilesystemWatcher::Event> pollEvents()
	{
		return eventQueue.waitDrain();
	}

	void pushEvent(FilesystemWatcher::Event event)
	{
		eventQueue.push(event);
	}

private:
	EventQueue eventQueue;
	ThreadSafeQueue<std::filesystem::path> addedFiles;
	ThreadSafeQueue<std::filesystem::path> addedDirectories;
	ThreadSafeQueue<std::filesystem::path> removedPaths;

	std::atomic<bool> doStop{false};
	
	void watchFilesystem();
	std::thread watcherThread{&FilesystemWatcherImpl::watchFilesystem, this};
};

class WatchReporter: public QObject
{
	Q_OBJECT

public:
	WatchReporter(FilesystemWatcherImpl* parent_): parent{parent_} {}

	void addDirectory(std::filesystem::path const& path)
	{
		if (not std::filesystem::exists(path))
		{
			parent->pushEvent(FilesystemWatcher::Event{FilesystemWatcher::EventType::Deleted, path, true});
			return;
		}
		std::unordered_set<std::filesystem::path, PathHasher> currentContents;
		for (auto&& entry: std::filesystem::directory_iterator(path))
		{
			currentContents.insert(entry.path());
		}
		directoryContents.insert({path, std::move(currentContents)});
	}

public slots:
	void onFileChanged(QString const& path)
	{
		auto fsPath = std::filesystem::path(path.toUtf8().constData());
		if (std::filesystem::exists(fsPath))
		{
			parent->pushEvent(FilesystemWatcher::Event{FilesystemWatcher::EventType::Modified, fsPath, false});
		}
		else
		{
			parent->pushEvent(FilesystemWatcher::Event{FilesystemWatcher::EventType::Deleted, fsPath, false});
		}
	}

	void onDirectoryChanged(QString const& path)
	{
		auto fsPath = std::filesystem::path(path.toUtf8().constData());
		if (std::filesystem::exists(fsPath))
		{
			std::unordered_set<std::filesystem::path, PathHasher> currentContents;
			for (auto&& entry: std::filesystem::directory_iterator(fsPath))
			{
				auto& entryPath = entry.path();
				currentContents.insert(entryPath);
				if (not directoryContents.at(fsPath).contains(entryPath))
				{
					parent->pushEvent(FilesystemWatcher::Event{FilesystemWatcher::EventType::Created, entryPath, std::filesystem::is_directory(entryPath)});
				}
			}
			directoryContents.at(fsPath) = std::move(currentContents);
		}
		else
		{
			parent->pushEvent(FilesystemWatcher::Event{FilesystemWatcher::EventType::Deleted, fsPath, true});
		}
	}

private:
	FilesystemWatcherImpl* parent;

	std::unordered_map<std::filesystem::path, std::unordered_set<std::filesystem::path, PathHasher>, PathHasher> directoryContents;
};

inline void FilesystemWatcherImpl::watchFilesystem()
{
	int fakeArgc = 1;
	char name[] = "indexer";
	char* fakeArgv[] = {name};
	QCoreApplication app(fakeArgc, fakeArgv);
	QFileSystemWatcher filesystemWatcher;
	WatchReporter reporter(this);
	QObject::connect(&filesystemWatcher, &QFileSystemWatcher::fileChanged, &reporter, &WatchReporter::onFileChanged);
	QObject::connect(&filesystemWatcher, &QFileSystemWatcher::directoryChanged, &reporter, &WatchReporter::onDirectoryChanged);

	while (not doStop)
	{
		app.processEvents();
		for (auto&& path: addedFiles.drain())
		{
			filesystemWatcher.addPath(path.c_str());
		}
		for (auto&& path: addedDirectories.drain())
		{
			reporter.addDirectory(path);
			filesystemWatcher.addPath(path.c_str());
		}
		for (auto&& path: removedPaths.drain())
		{
			filesystemWatcher.removePath(path.c_str());
		}
	}
}
}
#endif  // FILESYSTEM_WATCHER_IMPL_H_
