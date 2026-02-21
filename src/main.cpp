#include <iostream>
#include <string>

int main(int argc, char** argv)
{
	if (argc != 2)
	{
		std::cerr << "Usage: " << (argc > 0 ? argv[0] : "cli_one_arg") << " <value>\n";
		return 1;
	}

	std::string value = argv[1];

	std::cout << value << std::endl;
	return 0;
}
