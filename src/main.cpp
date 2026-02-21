#include <iostream>
#include <string>

static void print_help(const char* prog)
{
    std::cerr << "Usage: " << (prog ? prog : "cli_one_arg")
              << " <meshFile> <boneWeightFile> <inverseBindPoseFile> <newPoseFile> <resultFile>\n";
    std::cerr << "Provide all 5 file paths in order. Pass --help or -h to show this message.\n";
}

int main(int argc, char** argv)
{
    if (argc == 2 && (std::string(argv[1]) == "--help" || std::string(argv[1]) == "-h"))
    {
        print_help(argc > 0 ? argv[0] : nullptr);
        return 0;
    }

    if (argc != 6)
    {
        print_help(argc > 0 ? argv[0] : nullptr);
        return 1;
    }

    std::string meshFile = argv[1];
    std::string boneWeightFile = argv[2];
    std::string inverseBindPoseFile = argv[3];
    std::string newPoseFile = argv[4];
    std::string resultFile = argv[5];

    std::cout << "meshFile: " << meshFile << std::endl;
    std::cout << "boneWeightFile: " << boneWeightFile << std::endl;
    std::cout << "inverseBindPoseFile: " << inverseBindPoseFile << std::endl;
    std::cout << "newPoseFile: " << newPoseFile << std::endl;
    std::cout << "resultFile: " << resultFile << std::endl;

    return 0;
}
