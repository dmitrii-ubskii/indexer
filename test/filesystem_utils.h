#include <filesystem>
#include <fstream>

inline void touch(std::filesystem::path const& file)
{
	std::ofstream fout{file, std::ios_base::app};
}

inline void write(std::filesystem::path const& file, std::string const& string)
{
	std::ofstream fout{file};  // creates the file
	fout << string;
}
