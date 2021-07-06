#include <iostream>
#include <functional>
#include <string>

#include "indexer/indexer.h"

class Repl
{
public:
	void add_command(std::string command, std::function<void()> callback)
	{
		commands.insert({command, callback});
	}

	void add_alias(std::string alias, std::string command)
	{
		aliases.insert({alias, command});
	}

	void call(std::string const& command)
	{
		auto const& cmd = [&](){
			if (aliases.contains(command))
				return aliases.at(command);
			return command;
		}();
		if (commands.contains(cmd))
			commands.at(cmd)();
	}

private:
	std::unordered_map<std::string, std::function<void()>> commands;
	std::unordered_map<std::string, std::string> aliases;
};

int main()
{
	Indexer::Indexer indexer;  // Indexer indexer? Indexer!

	bool doQuit = false;

	Repl repl;
	repl.add_command("help", [](){ std::cout << "Help is on the way!\n"; });
	repl.add_alias("h", "help");
	repl.add_alias("?", "help");

	repl.add_command("quit", [&](){ doQuit = true; });
	repl.add_alias("q", "quit");

	std::string cmd;
	std::cout << "Type \"help\" or \"?\" for help, \"quit\" to quit\n";
	do {
		std::cout << ">>> ";
		std::getline(std::cin, cmd);
		repl.call(cmd);
	} while (not doQuit && not std::cin.eof());

	return 0;
}
