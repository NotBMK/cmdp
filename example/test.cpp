#include <iostream>
#include <sstream>
#include <fstream>

#include "cmdp.h"

using namespace std;

bool is_set;

int main(int argc, char const *argv[])
{
    ntl::cmd::cmdp cmdp;
    cmdp.ignore_first(true);
    cmdp.add("-test")
        .alias("-t")
        .bind(&is_set, true);
    cmdp.add("echo")
        .bind([&cmdp]()
        {
            if (cmdp.has_next())
            {
                cout << cmdp.next();
            }
            else
            {
                throw std::invalid_argument("");
            }
        });

    cmdp.init(argc, argv);
    cmdp.parse();

    return 0;
}