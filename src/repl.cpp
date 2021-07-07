#include <chrono>
#include <functional>
#include <iostream>
#include <string>

#include "indexer/indexer.h"

class Repl
{
public:
	void add_command(std::string command, std::function<void(std::string_view)> callback)
	{
		commands.insert({command, callback});
	}

	void add_alias(std::string alias, std::string command)
	{
		aliases.insert({alias, command});
	}

	void call(std::string const& command, std::string_view args)
	{
		auto const& cmd = [&](){
			if (aliases.contains(command))
				return aliases.at(command);
			return command;
		}();
		if (commands.contains(cmd))
			commands.at(cmd)(args);
	}

private:
	std::unordered_map<std::string, std::function<void(std::string_view)>> commands;
	std::unordered_map<std::string, std::string> aliases;
};

std::string formatDuration(std::chrono::steady_clock::duration duration)
{
	auto units = duration.count();  // ns

	constexpr char names[7][4] = {"ns", "μs", "ms", "s", "min", "hrs"};
	constexpr int sizes[] = {1000, 1000, 1000, 60, 60};

	std::size_t i = 0;
	for (; i < std::size(sizes); i++)
	{
		if (units < sizes[i])
			break;

		units = (units + sizes[i] / 2) / sizes[i];
	}
	return std::to_string(units) + " " + names[i];
}

int main()
{
	Indexer::Indexer indexer;  // Indexer indexer? Indexer!

	bool doQuit = false;

	Repl repl;
	repl.add_command("help", [](auto rest){ std::cout << "Fetching help on `" << rest << "`\n"; });
	repl.add_alias("h", "help");
	repl.add_alias("?", "help");

	repl.add_command("quit", [&](auto){ doQuit = true; });
	repl.add_alias("q", "quit");

	repl.add_command("add", [&](auto path){
		auto start = std::chrono::steady_clock::now();
		indexer.addPath(path);
		auto duration = std::chrono::steady_clock::now() - start;
		std::cerr << "Took ~" << formatDuration(duration) << " to index\n";
	});

	repl.add_command("search", [&](auto token){
		for (auto&& f: indexer.search(std::string{token}))
			std::cout << f << "\n";
	});

	std::string cmd;
	std::cout << "Type \"help\" or \"?\" for help, \"quit\" to quit\n";
	do {
		std::cout << ">>> ";
		std::getline(std::cin, cmd);
		auto commandEnd = cmd.find_first_of(' ');
		if (commandEnd == std::string::npos)
		{
			repl.call(cmd, "");
		}
		else
		{
			repl.call(cmd.substr(0, commandEnd), cmd.substr(cmd.find_first_not_of(' ', commandEnd)));
		}
	} while (not doQuit && not std::cin.eof());

	return 0;
}
