#include "indexer/filesystem_watcher.h"
#include "filesystem_watcher_impl.h"

namespace Indexer
{
FilesystemWatcher::FilesystemWatcher(): pImpl{std::make_unique<FilesystemWatcherImpl>()} {}
FilesystemWatcher::FilesystemWatcher(FilesystemWatcher&&) = default;
FilesystemWatcher& FilesystemWatcher::operator=(FilesystemWatcher&&) = default;
FilesystemWatcher::~FilesystemWatcher() = default;

void FilesystemWatcher::requestStop()
{
	pImpl->requestStop();
}

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
