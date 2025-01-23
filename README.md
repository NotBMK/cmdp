# cmdp
An easy command option parser

# usage


Add
```cpp
#include "cmdp.h"
```
to your cpp file.

regist your option and call 'parse(argc, argv)'

## cmdp.add
This function bind a 'void(*)()' function to an option.
```cpp
#include <iostream>
int main(int argc, char const* argv[])
{
    ntl::cmd::cmdp cmdp;
    cmdp.add("-test", [](){ std::cout << "cmdp test"; });

    cmdp.parse(argc, argv);
}
```

## cmdp.flag
This function bind a boolean variant to an option.
When the option is given, the variant would set to the given value.

```cpp
    bool is_flag_given;
    cmdp.flag("-fgiven", &is_flag_given, true);
    cmdp.flag("-ngiven", &is_flag_given, false);

```