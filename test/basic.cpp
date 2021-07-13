#include <catch2/catch_test_macros.hpp>

#include "indexer/indexer.h"

TEST_CASE("Basic indexing test")
{
	Indexer::Indexer indexer;

	indexer.addPath("include/indexer/indexer.h");
	indexer.addPath("README.adoc");

	REQUIRE(indexer.search("Indexer").contains(std::filesystem::canonical("README.adoc")));
	REQUIRE(indexer.search("Indexer").contains(std::filesystem::canonical("include/indexer/indexer.h")));
	REQUIRE(not indexer.search("Tokenizer").contains(std::filesystem::canonical("README.adoc")));
	REQUIRE(indexer.search("Tokenizer").contains(std::filesystem::canonical("include/indexer/indexer.h")));
}

TEST_CASE("Recursive indexing test")
{
	Indexer::Indexer indexer;

	SECTION("Non-recursive directory")
	{
		indexer.addPath("include", Indexer::Recursive::No);
		REQUIRE_FALSE(indexer.search("Indexer").contains(std::filesystem::canonical("include/indexer/indexer.h")));
	}
	SECTION("Recursive directory")
	{
		indexer.addPath("include", Indexer::Recursive::Yes);
		REQUIRE(indexer.search("Indexer").contains(std::filesystem::canonical("include/indexer/indexer.h")));
	}
}

TEST_CASE("Path normalization test")
{
	Indexer::Indexer indexer;
	indexer.addPath("README.adoc");
	indexer.addPath("./README.adoc");
	indexer.addPath("src/../README.adoc");
	REQUIRE(indexer.search("Indexer").size() == 1);
}
