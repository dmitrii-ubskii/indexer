#include <iostream>
#include <string>

int main()
{
	std::string cmd;
	std::cout << "Type \"help\" or \"?\" for help, \"quit\" to quit\n";
	do {
		std::cout << ">>> ";
		std::getline(std::cin, cmd);
		std::cout << cmd << "\n";
	} while (cmd != "q" && not std::cin.eof());

	return 0;
}
