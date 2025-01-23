#include <iostream>
#include <sstream>
#include <fstream>

#include "cmdp.h"

using namespace std;

bool is_debug;

void get_str(const char* str)
{
    std::cout << str << std::endl;
}

int main(int argc, char const *argv[])
{
    ntl::cmd::cmdp cmdp;
    cmdp.flag("-fdebug", &is_debug);
    cmdp.gets("-f", get_str);

    cmdp.parse(argc, argv);

    return 0;
}
