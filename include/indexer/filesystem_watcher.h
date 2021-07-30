#ifndef INDEXER_FILESYSTEM_WATCHER_H_
#define INDEXER_FILESYSTEM_WATCHER_H_

#include <filesystem>
#include <vector>

namespace Indexer
{
class FilesystemWatcherImpl;

class FilesystemWatcher
{
public:
	FilesystemWatcher();
	FilesystemWatcher(FilesystemWatcher&&);
	FilesystemWatcher& operator=(FilesystemWatcher&&);
	~FilesystemWatcher();

	void addFile(std::filesystem::path const& path);
	void addDirectory(std::filesystem::path const& path);

	void removePath(std::filesystem::path const& path);

	void requestStop();

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
	std::vector<Event> pollEvents();

private:
	std::unique_ptr<FilesystemWatcherImpl> pImpl;
};
}

#endif // INDEXER_FILESYSTEM_WATCHER_H_
