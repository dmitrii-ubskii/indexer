#include <catch2/catch_test_macros.hpp>

#include "indexer/indexer.h"

#include "filesystem_utils.h"

TEST_CASE("Basic indexing test")
{
	auto test1 = std::filesystem::current_path() / "__test1";
	auto test2 = std::filesystem::current_path() / "__test2";

	write(test1, "TEST\n");
	write(test2, "TEST\nTWO\n");

	SECTION("Common and different terms")
	{
		Indexer::Indexer indexer;

		indexer.addPath(test1);
		indexer.addPath(test2);

		REQUIRE(indexer.search("TEST").contains(test1));
		REQUIRE(indexer.search("TEST").contains(test2));
		REQUIRE(not indexer.search("TWO").contains(test1));
		REQUIRE(indexer.search("TWO").contains(test2));
	}

	std::filesystem::remove(test1);
	std::filesystem::remove(test2);
}

TEST_CASE("Recursive indexing test")
{
	Indexer::Indexer indexer;

	auto testDir = std::filesystem::current_path() / "__test_dir";
	std::filesystem::create_directory(testDir);

	auto testShallow = testDir / "__shallow";
	write(testShallow, "TEST\n");

	auto testDeep = testDir / "__subdir" / "__deep";
	std::filesystem::create_directory(testDeep.parent_path());
	write(testDeep, "TEST\n");

	SECTION("Non-recursive directory")
	{
		indexer.addPath(testDir, Indexer::Recursive::No);
		REQUIRE(indexer.search("TEST").contains(testShallow));
		REQUIRE_FALSE(indexer.search("TEST").contains(testDeep));
	}
	SECTION("Recursive directory")
	{
		indexer.addPath(testDir, Indexer::Recursive::Yes);
		REQUIRE(indexer.search("TEST").contains(testShallow));
		REQUIRE(indexer.search("TEST").contains(testDeep));
	}

	std::filesystem::remove_all(testDir);
}

TEST_CASE("Path normalization test")
{
	auto test = std::filesystem::current_path() / "__test";
	write(test, "TEST\n");
	SECTION("")
	{
		Indexer::Indexer indexer;
		indexer.addPath("__test");
		indexer.addPath("./__test");
		indexer.addPath("src/../__test");
		REQUIRE(indexer.search("TEST").contains(test));
		REQUIRE(indexer.search("TEST").size() == 1);
	}
	std::filesystem::remove(test);
}
