# cmdp
An easy command option parser

# usage

## cmdp.add
This function bind a callback function without argument and return to an option.
```cpp
#include <iostream> 
#include "cmdp.h"

int main(int argc, char const* argv[])
{  
    // Create a command parser object cmdp  
    ntl::cmd::cmdp cmdp;   

    // Add a command "-test" to cmdp and bind a lambda function as its handler  
    cmdp.add("-test")  
        .bind([]()
        {   
            // If the command "-test" is called, output "cmdp test"  
            std::cout << "cmdp test" << std::endl;   
        });  

    // Add another command "echo" and bind a lambda function  
    cmdp.add("echo")  
        .bind([&cmdp]()   
        {  
            // If the command "echo" is called, use cmdp.next_str() to get the next string and output it  
            std::cout << cmdp.next() << std::endl;  
        });  

    // Initialize the command parser, passing the command line arguments  
    cmdp.init(argc, argv);  
    
    // Parse the command line arguments and invoke the corresponding handlers  
    cmdp.parse();   
}
```