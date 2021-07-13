#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>

#include "indexer/indexer.h"

using namespace std::chrono_literals;

TEST_CASE("Watching filesystem")
{
	Indexer::Indexer indexer;
	std::filesystem::path testDir = "__test_dir";
	std::filesystem::create_directory(testDir);
	testDir = std::filesystem::canonical(testDir);

	SECTION("Basic modifications are caught")
	{
		auto testFile = testDir / "section_modify";
		std::ofstream testFileOut{testFile};  // creates the file

		indexer.addPath(testFile);
		REQUIRE_FALSE(indexer.search("MODIFY").contains(testFile));

		testFileOut << "MODIFY\n";
		testFileOut.close();

		std::this_thread::sleep_for(100us);  // allow the watcher to catch up
		REQUIRE(indexer.search("MODIFY").contains(testFile));

		std::filesystem::remove(testFile);
	}

	SECTION("File creation and modification is caught")
	{
		indexer.addPath(testDir);
		auto testFile = testDir / "section_create";

		std::ofstream testFileOut{testFile};
		testFileOut << "CREATE\n";
		testFileOut.close();

		std::this_thread::sleep_for(10ms);  // allow the watcher to catch up
		REQUIRE(indexer.search("CREATE").contains(testFile));

		auto subdir = testDir / "section_create_recursive";
		std::filesystem::create_directory(subdir);
		auto subdirFile = subdir / "section_create_inner";

		testFileOut.open(subdirFile);
		testFileOut << "CREATE\n";
		testFileOut.close();

		std::this_thread::sleep_for(100us);  // allow the watcher to catch up
		REQUIRE(indexer.search("CREATE").contains(subdirFile));

		std::filesystem::remove_all(subdir);
		std::filesystem::remove(testFile);
	}

	SECTION("File deletion is caught")
	{
		auto testFile = testDir / "section_delete";

		std::ofstream testFileOut{testFile};
		testFileOut << "DELETE\n";
		testFileOut.close();

		indexer.addPath(testDir);

		REQUIRE(indexer.search("DELETE").contains(testFile));

		std::filesystem::remove(testFile);
		std::this_thread::sleep_for(100us);  // allow the watcher to catch up
		REQUIRE_FALSE(indexer.search("DELETE").contains(testFile));
	}

	std::filesystem::remove_all(testDir);
}

