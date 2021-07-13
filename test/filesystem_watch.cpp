#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>

#include "indexer/indexer.h"

using namespace std::chrono_literals;

void touch(std::filesystem::path const& file)
{
	std::ofstream fout{file, std::ios_base::app};
}

void write(std::filesystem::path const& file, std::string const& string)
{
	std::ofstream fout{file};  // creates the file
	fout << string;
}

void wait()
{
	std::this_thread::sleep_for(1ms);  // allow the watcher to catch up
}

TEST_CASE("Watching filesystem")
{
	Indexer::Indexer indexer;
	std::filesystem::path testDir = std::filesystem::weakly_canonical("./__test_dir");
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
		indexer.addPath(testDir);
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

		std::ofstream testFileOut{testFile};
		testFileOut << "DELETE\n";
		testFileOut.close();

		indexer.addPath(testDir);

		REQUIRE(indexer.search("DELETE").contains(testFile));

		std::filesystem::remove(testFile);
		wait();
		REQUIRE_FALSE(indexer.search("DELETE").contains(testFile));
	}

	std::filesystem::remove_all(testDir);
}

