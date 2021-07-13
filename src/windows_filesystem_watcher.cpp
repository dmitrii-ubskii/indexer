#include "indexer/filesystem_watcher.h"

#include <cassert>
#include <stdexcept>
#include <unordered_map>

#include <poll.h>
#include <sys/inotify.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "indexer/path_utils.h"

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
	}

	void addDirectory(std::filesystem::path const& path)
	{
	}

	void removePath(std::filesystem::path const& path)
	{
	}

	std::vector<FilesystemWatcher::Event> pollEvents()
	{
	}

private:
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

