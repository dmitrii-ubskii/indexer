#ifndef INDEXER_INOTIFY_FILESYSTEM_WATCHER_H_
#define INDEXER_INOTIFY_FILESYSTEM_WATCHER_H_

#include <filesystem>
#include <stdexcept>

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
		: inotifyFd{inotify_init()}
	{
		if (inotifyFd < 0)  // error
		{
			switch (inotifyFd)
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
						"inotify_init(): Unexpected error code " + std::to_string(inotifyFd)
					};
			}
		}
	}

	void addFile(std::filesystem::path const& path)
	{
		auto wd = inotify_add_watch(inotifyFd, path.c_str(), IN_MODIFY | IN_DELETE_SELF | IN_MOVE_SELF);
		idToPath.insert({wd, path});
		pathToId.insert({path, wd});
	}

	void addDirectory(std::filesystem::path const& path)
	{
		auto wd = inotify_add_watch(inotifyFd, path.c_str(), IN_CREATE | IN_DELETE | IN_DELETE_SELF | IN_MOVE_SELF | IN_MOVE);
		idToPath.insert({wd, path});
		pathToId.insert({path, wd});
	}

	void query()
	{
		std::size_t buffSize = 0;
		ioctl(inotifyFd, FIONREAD, reinterpret_cast<char*>(&buffSize));
		if (buffSize == 0)
			return;

		auto buffer = std::vector<char>(buffSize);

		auto bytesReadOrError = read(inotifyFd, buffer.data(), buffSize);
		if (bytesReadOrError < 0)  // error
		{
			switch (bytesReadOrError)
			{
				case EIO:
					throw std::runtime_error{
						"inotify read: EIO: I/O error."
					};
				default:
					throw std::runtime_error{
						"inotify read: Unexpected error code " + std::to_string(bytesReadOrError)
					};
			}
		}

		auto* end = buffer.data() + bytesReadOrError;

		for (auto p = buffer.data(); p < end; )
		{
			auto* event = reinterpret_cast<inotify_event const*>(p);
			p += sizeof(inotify_event) + event->len;
			/* Print event type */
	
			if (event->mask & IN_MODIFY)
				printf("IN_MODIFY: ");
			if (event->mask & IN_DELETE_SELF)
				printf("IN_DELETE_SELF: ");
			if (event->mask & IN_MOVE_SELF)
				printf("IN_MOVE_SELF: ");
			if (event->mask & IN_IGNORED)
				printf("IN_IGNORED: ");
			printf("%x ", event->mask);
	
			/* Print the name of the file */
	
			printf("%d", event->wd);
	
			/* Print type of filesystem object */
	
			if (event->mask & IN_ISDIR)
				printf(" [directory]\n");
			else
				printf(" [file]\n");
		}

	}

private:
	int inotifyFd;

	std::unordered_map<int, std::filesystem::path> idToPath;
	std::unordered_map<std::filesystem::path, int, PathCanonicalHasher> pathToId;
};
}

#endif // INDEXER_INOTIFY_FILESYSTEM_WATCHER_H_
