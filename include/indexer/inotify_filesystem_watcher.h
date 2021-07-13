#ifndef INDEXER_INOTIFY_FILESYSTEM_WATCHER_H_
#define INDEXER_INOTIFY_FILESYSTEM_WATCHER_H_

#include <cassert>
#include <filesystem>
#include <stdexcept>
#include <unordered_map>

#include <poll.h>
#include <sys/inotify.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "indexer/path_utils.h"

namespace Indexer
{
class FilesystemWatcher
{
public:
	FilesystemWatcher()
		: inotifyFileDescriptor{inotify_init()}
	{
		if (inotifyFileDescriptor < 0)  // error
		{
			auto errorCode = inotifyFileDescriptor;
			switch (errorCode)
			{
				case EMFILE:
					throw std::runtime_error{
						"inotify_init(): EMFILE: The user limit on the total number of inotify instances"
						" OR the per-process limit on the number of open file descriptors has been reached."
					};
				case ENFILE:
					throw std::runtime_error{
						"inotify_init(): ENFILE: The system-wide limit on the total number of open files has been reached."
					};
				case ENOMEM:
					throw std::runtime_error{
						"inotify_init(): ENOMEM: Insufficient kernel memory is available."
					};
				default:
					throw std::runtime_error{
						"inotify_init(): Unexpected error code " + std::to_string(errorCode)
					};
			}
		}
	}

	void addFile(std::filesystem::path const& path)
	{
		auto watchDescriptor = inotify_add_watch(inotifyFileDescriptor, path.c_str(), IN_MODIFY | IN_MOVE_SELF);
		registerWatchDescriptor(watchDescriptor, path);
	}

	void addDirectory(std::filesystem::path const& path)
	{
		auto watchDescriptor = inotify_add_watch(inotifyFileDescriptor, path.c_str(), IN_CREATE | IN_MOVED_TO | IN_MOVE_SELF);
		registerWatchDescriptor(watchDescriptor, path);
	}

	void removePath(std::filesystem::path const& path)
	{
		if (pathToDescriptor.contains(path))
		{
			auto watchDescriptor = pathToDescriptor.at(path);
			inotify_rm_watch(inotifyFileDescriptor, watchDescriptor);
			unregisterWatchDescriptor(watchDescriptor);
		}
		// or do nothing
	}

	enum class EventType
	{
		Created, Modified, Deleted
	};

	struct Event
	{
		EventType type;
		std::filesystem::path path;
		bool isDirectory;
	};

	std::vector<Event> pollEvents()
	{
		pollfd pollDescriptor{inotifyFileDescriptor, POLLIN, 0};
		poll(&pollDescriptor, 1, 5);
		if (not (pollDescriptor.revents & POLLIN))  // no data available to read
		{
			return {};  // so no events generated
		}

		std::size_t buffSize = 0;
		ioctl(inotifyFileDescriptor, FIONREAD, reinterpret_cast<char*>(&buffSize));
		assert(buffSize > 0);  // we know for a fact there are events in the pipeline

		auto buffer = std::vector<char>(buffSize);

		auto bytesRead = read(inotifyFileDescriptor, buffer.data(), buffSize);
		if (bytesRead < 0)  // error
		{
			auto errorCode = bytesRead;
			switch (errorCode)
			{
				case EIO:
					throw std::runtime_error{
						"inotify read: EIO: I/O error."
					};
				default:
					throw std::runtime_error{
						"inotify read: Unexpected error code " + std::to_string(errorCode)
					};
			}
		}

		auto* end = buffer.data() + bytesRead;

		std::vector<Event> events;

		for (auto p = buffer.data(); p < end; )
		{
			auto* event = reinterpret_cast<inotify_event const*>(p);
			p += sizeof(inotify_event) + event->len;

			// queued event for an already unregistered descriptor
			if (not descriptorToPath.contains(event->wd))
			{
				continue;
			}

			auto path = descriptorToPath.at(event->wd);

			// modified
			if (event->mask & IN_MODIFY)
			{
				events.emplace_back(EventType::Modified, path, false);
			}

			// created
			if (event->mask & (IN_CREATE | IN_MOVED_TO))
			{
				events.emplace_back(EventType::Created, path / event->name, event->mask & IN_ISDIR);
			}

			// deleted
			if (event->mask & (IN_IGNORED | IN_MOVE_SELF))
			{
				unregisterWatchDescriptor(event->wd);
				events.emplace_back(EventType::Deleted, path, event->mask & IN_ISDIR);
			}
		}

		return events;
	}

private:
	int inotifyFileDescriptor;

	std::unordered_map<int, std::filesystem::path> descriptorToPath;
	std::unordered_map<std::filesystem::path, int, PathHasher> pathToDescriptor;

	void registerWatchDescriptor(int watchDescriptor, std::filesystem::path const& path)
	{
		descriptorToPath.insert({watchDescriptor, path});
		pathToDescriptor.insert({path, watchDescriptor});
	}

	void unregisterWatchDescriptor(int watchDescriptor)
	{
		auto& path = descriptorToPath.at(watchDescriptor);
		descriptorToPath.erase(watchDescriptor);
		pathToDescriptor.erase(path);
	}
};
}

#endif // INDEXER_INOTIFY_FILESYSTEM_WATCHER_H_
