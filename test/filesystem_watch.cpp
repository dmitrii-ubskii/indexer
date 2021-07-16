#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <filesystem>
#include <thread>

#include "indexer/indexer.h"

#include "filesystem_utils.h"

using namespace std::chrono_literals;

inline void wait()
{
	std::this_thread::sleep_for(1ms);  // allow the watcher to catch up
}

TEST_CASE("Watching filesystem")
{
	Indexer::Indexer indexer;
	std::filesystem::path testDir = std::filesystem::current_path() / "__test_dir";
	std::filesystem::create_directory(testDir);

	SECTION("Basic modifications are caught")
	{
		auto testFile = testDir / "section_modify";
		write(testFile, "UNMODIFIED\n");

		indexer.addPath(testFile);
		REQUIRE(indexer.search("UNMODIFIED").contains(testFile));
		REQUIRE_FALSE(indexer.search("MODIFY").contains(testFile));

		write(testFile, "MODIFY\n");
		wait();

		REQUIRE(indexer.search("MODIFY").contains(testFile));
		REQUIRE_FALSE(indexer.search("UNMODIFIED").contains(testFile));

		std::filesystem::remove(testFile);
	}

	SECTION("File creation and modification is caught")
	{
		indexer.addPath(testDir, Indexer::Recursive::Yes);
		auto testFile = testDir / "section_create";

		write(testFile, "CREATE\n");
		wait();

		REQUIRE(indexer.search("CREATE").contains(testFile));

		auto subdir = testDir / "section_create_recursive";
		std::filesystem::create_directory(subdir);
		auto subdirFile = subdir / "section_create_inner";

		write(subdirFile, "CREATE\n");
		wait();

		REQUIRE(indexer.search("CREATE").contains(subdirFile));

		std::filesystem::remove_all(subdir);
		std::filesystem::remove(testFile);
	}

	SECTION("File deletion is caught")
	{
		auto testFile = testDir / "section_delete";
		write(testFile, "DELETE\n");

		indexer.addPath(testDir);

		REQUIRE(indexer.search("DELETE").contains(testFile));

		std::filesystem::remove(testFile);
		wait();
		REQUIRE_FALSE(indexer.search("DELETE").contains(testFile));
	}

	std::filesystem::remove_all(testDir);
}

TEST_CASE("Catching deleted files recreation")
{
	Indexer::Indexer indexer;
	std::filesystem::path testDir = std::filesystem::current_path() / "__test_dir";
	std::filesystem::create_directory(testDir);

	SECTION("Deleted and recreated files are caught")
	{
		auto testFile = testDir / "section_recreate";
		write(testFile, "\nDELETE\n");
		indexer.addPath(testFile);

		REQUIRE(indexer.search("DELETE").contains(testFile));

		std::filesystem::remove(testFile);
		wait();

		REQUIRE_FALSE(indexer.search("DELETE").contains(testFile));

		write(testFile, "RECREATE\n");
		wait();

		REQUIRE(indexer.search("RECREATE").contains(testFile));
		REQUIRE_FALSE(indexer.search("DELETE").contains(testFile));

		std::filesystem::remove(testFile);
	}

	SECTION("Deleted and recreated files are caught deeper in the tree")
	{
		auto subdir = testDir / "section_recreate_recursive";
		std::filesystem::create_directory(subdir);

		auto testFile = subdir / "section_recreate_recursive_file";
		write(testFile, "DELETE\n");
		indexer.addPath(testFile);

		REQUIRE(indexer.search("DELETE").contains(testFile));

		std::filesystem::remove_all(subdir);
		wait();

		REQUIRE_FALSE(indexer.search("DELETE").contains(testFile));

		std::filesystem::create_directory(subdir);
		write(testFile, "RECREATE\n");
		wait();

		REQUIRE(indexer.search("RECREATE").contains(testFile));
		REQUIRE_FALSE(indexer.search("DELETE").contains(testFile));

		std::filesystem::remove_all(subdir);
	}

	SECTION("Deleted and recreated files are caught deeper in the tree -- step-by-step deletion")
	{
		auto subdir = testDir / "section_recreate_recursive";
		std::filesystem::create_directory(subdir);

		auto testFile = subdir / "section_recreate_recursive_file";
		write(testFile, "DELETE\n");
		indexer.addPath(testFile);

		REQUIRE(indexer.search("DELETE").contains(testFile));

		std::filesystem::remove(testFile);
		wait();
		REQUIRE_FALSE(indexer.search("DELETE").contains(testFile));

		std::filesystem::remove(subdir);
		wait();

		std::filesystem::create_directory(subdir);
		write(testFile, "RECREATE\n");
		wait();

		REQUIRE(indexer.search("RECREATE").contains(testFile));
		REQUIRE_FALSE(indexer.search("DELETE").contains(testFile));

		std::filesystem::remove_all(subdir);
	}

	SECTION("Adding file before it's created")
	{
		auto testFile = testDir / "section_create";

		indexer.addPath("__nonexistent");
		indexer.addPath(testFile);

		write(testFile, "CREATE\n");
		wait();

		REQUIRE(indexer.search("CREATE").contains(testFile));

		std::filesystem::remove(testFile);
	}

	std::filesystem::remove_all(testDir);
}

