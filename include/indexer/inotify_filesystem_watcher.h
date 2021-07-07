#ifndef INDEXER_INOTIFY_FILESYSTEM_WATCHER_H_
#define INDEXER_INOTIFY_FILESYSTEM_WATCHER_H_

#include <filesystem>
#include <stdexcept>

#include <sys/inotify.h>
#include <sys/ioctl.h>
#include <unistd.h>

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
		inotify_add_watch(inotifyFd, path.c_str(), IN_MODIFY | IN_DELETE_SELF | IN_MOVE_SELF);
	}

	void addDirectory(std::filesystem::path const& path)
	{
		inotify_add_watch(inotifyFd, path.c_str(), IN_CREATE | IN_DELETE | IN_DELETE_SELF | IN_MOVE_SELF | IN_MOVE);
	}

	void query()
	{
		std::size_t buffSize = 0;
		ioctl(inotifyFd, FIONREAD, reinterpret_cast<char*>(&buffSize));
		if (buffSize == 0)
			return;

		auto buffer = new char[buffSize];
		read(inotifyFd, buffer, buffSize);
		auto* event = reinterpret_cast<inotify_event const*>(buffer);
		auto* end = reinterpret_cast<inotify_event const*>(buffer + buffSize);

		for ( ; event < end; event++)
		{
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

		delete[] buffer;
	}

private:
	int inotifyFd;
};
}

#endif // INDEXER_INOTIFY_FILESYSTEM_WATCHER_H_
