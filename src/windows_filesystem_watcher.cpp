#include "indexer/filesystem_watcher.h"

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>

#include <Windows.h>

#include "indexer/path_utils.h"

using namespace std::chrono_literals;

namespace Indexer
{
template <typename T>
class ThreadSafeQueue
{
public:
	std::vector<T> waitDrain()
	{
		std::unique_lock<std::mutex> pin{contentsMutex};
		if (not sync.wait_for(pin, 5ms, [this]{ return not empty(); }))
		{
			return {};
		}

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
		sync.notify_all();
	}

	bool empty() const
	{
		return queue.empty();
	}

private:
	mutable std::mutex contentsMutex;
	std::condition_variable sync;

	std::vector<T> queue;
};

using EventQueue = ThreadSafeQueue<FilesystemWatcher::Event>;

enum class WatchType
{
	File, DirectoryContents, DirectoryDeletion
};

struct WatchInfo
{
	// Has to be first so that the pointer to overlapped could be reinterpreted to point to the info
	OVERLAPPED overlapped;

	HANDLE handle;
	DWORD flags;
	char buffer[1 << 16];

	std::filesystem::path directoryPath;
	WatchType watchType;
	std::filesystem::path filename{};

	EventQueue* eventQueue;

	std::atomic<bool> doStop{false};
};

void CALLBACK watchCallback(DWORD dwErrorCode, DWORD dwNumberOfBytesTransfered, LPOVERLAPPED lpOverlapped);

class FilesystemWatcherImpl
{
public:
	~FilesystemWatcherImpl()
	{
		doStop = true;
		if (watcherThread.joinable())
		{
			watcherThread.join();
		}
	}

	void addFile(std::filesystem::path const& path)
	{
		// We want to block this thread of execution until the path is being watched
		// to not miss any events in the meanwhile
		std::unique_lock<std::mutex> pin{watchesMutex};
		addedFiles.push(path);
		watchesSync.wait(pin);
	}

	void addDirectory(std::filesystem::path const& path)
	{
		// see addFile()
		std::unique_lock<std::mutex> pin{watchesMutex};
		addedDirectories.push(path);
		watchesSync.wait(pin);
	}

	void removePath(std::filesystem::path const& path)
	{
		std::unique_lock<std::mutex> pin{watchesMutex};
		removedPaths.push(path);
		watchesSync.wait(pin);
	}

	std::vector<FilesystemWatcher::Event> pollEvents()
	{
		return eventQueue.waitDrain();
	}

private:
	std::thread watcherThread{&FilesystemWatcherImpl::watchFilesystem, this};
	std::mutex watchesMutex;
	std::condition_variable watchesSync;

	EventQueue eventQueue;
	ThreadSafeQueue<std::filesystem::path> addedFiles;
	ThreadSafeQueue<std::filesystem::path> addedDirectories;
	ThreadSafeQueue<std::filesystem::path> removedPaths;

	std::atomic<bool> doStop{false};

	void watchFilesystem()
	{
		while (not doStop)
		{
			std::unique_lock<std::mutex> pin{watchesMutex};
			for (auto&& path: addedFiles.drain())
			{
				if (not watches.contains(path))
				{
					watches.insert({path, PathWatcher{
						path, WatchType::File,
						FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_ATTRIBUTES | FILE_NOTIFY_CHANGE_LAST_WRITE,
						&eventQueue
					}});
				}
			}
			for (auto&& path: addedDirectories.drain())
			{
				if (not watches.contains(path))
				{
					if (path.has_relative_path())
					{
						watches.insert({path, PathWatcher{
							path, WatchType::DirectoryDeletion,
							FILE_NOTIFY_CHANGE_DIR_NAME,
							&eventQueue
						}});
					}
					watches.insert({path, PathWatcher{
						path, WatchType::DirectoryContents,
						FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME,
						&eventQueue
					}});
				}
			}
			for (auto&& path: removedPaths.drain())
			{
				if (watches.contains(path))
				{
					watches.erase(path);
				}
			}
			watchesSync.notify_all();
			pin.unlock();
			MsgWaitForMultipleObjectsEx(0, NULL, 5, QS_ALLINPUT, MWMO_ALERTABLE);
		}
	}

	class PathWatcher
	{
	public:
		PathWatcher(std::filesystem::path const& path, WatchType watchType, unsigned flags, EventQueue* eventQueue)
			: watch{std::make_unique<WatchInfo>()}
		{
			watch->watchType = watchType;
			if (watch->watchType == WatchType::DirectoryContents)
			{
				watch->directoryPath = path;
			}
			else
			{
				watch->directoryPath = path.parent_path();
				watch->filename = path.filename();
			}

			watch->handle = CreateFileW(watch->directoryPath.c_str(), FILE_LIST_DIRECTORY,
				FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
				nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, nullptr);
			if (watch->handle != INVALID_HANDLE_VALUE)
			{
				watch->overlapped.hEvent = CreateEvent(nullptr, true, false, nullptr);
				
				watch->flags = flags;
				watch->eventQueue = eventQueue;

				if (not ReadDirectoryChangesW(
					watch->handle, watch->buffer, sizeof(watch->buffer), false,
					watch->flags, nullptr, &watch->overlapped, watchCallback
				))
				{
					CloseHandle(watch->overlapped.hEvent);
					CloseHandle(watch->handle);
				}
			}
		}

		PathWatcher(PathWatcher&&) = default;
		PathWatcher& operator=(PathWatcher&&) = default;

		~PathWatcher()
		{
			if (watch)
			{
				watch->doStop = true;
				CancelIo(watch->handle);
				ReadDirectoryChangesW(
					watch->handle, watch->buffer, sizeof(watch->buffer), false,
					watch->flags, nullptr, &watch->overlapped, nullptr
				);
				if (not HasOverlappedIoCompleted(&watch->overlapped))
				{
					SleepEx(5, true);
				}
				CloseHandle(watch->overlapped.hEvent);
				CloseHandle(watch->handle);
			}
		}

	private:
		std::unique_ptr<WatchInfo> watch;
	};

	std::unordered_multimap<std::filesystem::path, PathWatcher, PathHasher> watches;
};

void watchCallback(DWORD dwErrorCode, DWORD dwNumberOfBytesTransfered, LPOVERLAPPED lpOverlapped)
{
	WatchInfo* watch = (WatchInfo*) lpOverlapped;

	if (dwNumberOfBytesTransfered == 0)  // the watch has been deleted
	{
		return;
	}

	if (dwErrorCode == ERROR_SUCCESS)
	{
		FILE_NOTIFY_INFORMATION* notifyInfo;
        size_t offset = 0;
		do
		{
			notifyInfo = (FILE_NOTIFY_INFORMATION*)(watch->buffer + offset);
			offset += notifyInfo->NextEntryOffset;

			TCHAR filename[MAX_PATH];
			int count = WideCharToMultiByte(CP_ACP, 0, notifyInfo->FileName,
				notifyInfo->FileNameLength / sizeof(WCHAR),
				filename, MAX_PATH - 1, NULL, NULL);
			filename[count] = TEXT('\0');

			auto eventPath = watch->directoryPath / filename;
			if (watch->watchType == WatchType::DirectoryContents)
			{
				switch (notifyInfo->Action)
				{
					case FILE_ACTION_RENAMED_NEW_NAME:
					case FILE_ACTION_ADDED:
						watch->eventQueue->push({FilesystemWatcher::EventType::Created, eventPath, std::filesystem::is_directory(eventPath)});
						break;
					case FILE_ACTION_RENAMED_OLD_NAME:
					case FILE_ACTION_REMOVED:
						watch->eventQueue->push({FilesystemWatcher::EventType::Deleted, eventPath, false});
						break;
				}
			}
			else
			{
				if (filename == watch->filename)
				{
					switch (notifyInfo->Action)
					{
						case FILE_ACTION_RENAMED_OLD_NAME:
						case FILE_ACTION_REMOVED:
							watch->eventQueue->push({FilesystemWatcher::EventType::Deleted, eventPath, watch->watchType == WatchType::DirectoryDeletion});
							break;
						case FILE_ACTION_MODIFIED:
							if (watch->watchType == WatchType::File)
							{
								watch->eventQueue->push({FilesystemWatcher::EventType::Modified, eventPath, false});
							}
							break;
					}
				}
			}

		} while (notifyInfo->NextEntryOffset != 0);
	}

	if (!watch->doStop)
	{
		ReadDirectoryChangesW(
			watch->handle, watch->buffer, sizeof(watch->buffer), false,
			watch->flags, nullptr, lpOverlapped, watchCallback
		);
	}
}

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

