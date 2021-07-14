#include "indexer/filesystem_watcher.h"

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <queue>

#include <Windows.h>

#include "indexer/path_utils.h"

using namespace std::chrono_literals;

namespace Indexer
{
class FilesystemWatcherImpl
{
public:
	FilesystemWatcherImpl()
	{
	}

	void addFile(std::filesystem::path const& path)
	{
		constexpr auto flags = FILE_NOTIFY_CHANGE_DIR_NAME | FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_ATTRIBUTES |
			FILE_NOTIFY_CHANGE_SIZE | FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_SECURITY;
		watchThreads.insert({path, PathWatchThread{path, flags, &eventQueue}});
	}

	void addDirectory(std::filesystem::path const& path)
	{
		constexpr auto flags = FILE_NOTIFY_CHANGE_DIR_NAME | FILE_NOTIFY_CHANGE_ATTRIBUTES | FILE_NOTIFY_CHANGE_FILE_NAME;
		watchThreads.insert({path, PathWatchThread{path, flags, &eventQueue}});
	}

	void removePath(std::filesystem::path const& path)
	{
		if (watchThreads.contains(path))
		{
			watchThreads.erase(path);
		}
	}

	std::vector<FilesystemWatcher::Event> pollEvents()
	{
		return eventQueue.drain();
	}

private:
	class EventQueue
	{
	public:
		std::vector<FilesystemWatcher::Event> drain()
		{
			std::unique_lock<std::mutex> pin{m};
			if (not cv.wait_for(pin, 5ms, [this]{ return not empty(); }))
			{
				return {};
			}

			auto events = std::vector<FilesystemWatcher::Event>{};
			std::swap(events, queue);
			return events;
		}

		void push(FilesystemWatcher::Event ev)
		{
			std::unique_lock<std::mutex> pin{m};
			queue.push_back(ev);
			cv.notify_all();
		}

		bool empty() const
		{
			return queue.empty();
		}

	private:
		mutable std::mutex m;
		std::condition_variable cv;

		std::vector<FilesystemWatcher::Event> queue;
	} eventQueue;

	class PathWatchThread
	{
	public:
		PathWatchThread(std::filesystem::path const& path, unsigned flags, EventQueue* eventQueue)
			: thread{&PathWatchThread::watchLoop, this, path, flags, eventQueue}
		{}

		PathWatchThread(PathWatchThread&&) = default;
		PathWatchThread& operator=(PathWatchThread&&) = default;

		~PathWatchThread()
		{
			doStop = true;
			thread.join();
		}

	private:
		void watchLoop(std::filesystem::path path, unsigned flags, EventQueue* eventQueue)
		{
			while (not doStop)
			{

			}
		}

		std::thread thread;
		bool doStop{false};
	};

	std::unordered_map<std::filesystem::path, PathWatchThread, PathHasher> watchThreads;
};

FilesystemWatcher::FilesystemWatcher(): pImpl{std::make_unique<FilesystemWatcherImpl>()} {}
FilesystemWatcher::FilesystemWatcher(FilesystemWatcher&&) = default;
FilesystemWatcher& FilesystemWatcher::operator=(FilesystemWatcher&&) = default;
FilesystemWatcher::~FilesystemWatcher() = default;

void FilesystemWatcher::addFile(const std::filesystem::path &path)
{
	pImpl->addFile(path);
}

void FilesystemWatcher::addDirectory(const std::filesystem::path &path)
{
	pImpl->addDirectory(path);
}

void FilesystemWatcher::removePath(std::filesystem::path const& path)
{
	pImpl->removePath(path);
}

std::vector<FilesystemWatcher::Event> FilesystemWatcher::pollEvents()
{
	return pImpl->pollEvents();
}
}

