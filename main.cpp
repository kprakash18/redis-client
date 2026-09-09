/*
1. Command-Line argument parsing
    -h <host>, default:127.0.0.1, -p <port> default:6379
    If no args , launch interactive REPL mode

2. Object-Oriented Programming
   RedisClient, CommandHandler, ResponseParser, CLI
   
3. Establish TCP connection to redis (redis client)   
   Berkeley sockets to open TCP connection
   Ipv4 and Ipv6  `getaddrinfo`

4. Parsing and Command Formatting (CommandHandler)
    Split user input
    Convert commands into RESP format:

    *3\r\n
    $3\r\nSET\r\n
    $3\r\nnkey\r\n
    $5\r\nvalue\r\n  

5. Handling Redis Responses (ResponseParser)
    Read server responses and parses RESP types
    +OK, -Error, :100, $5\r\nhello -> Bulk string, *2\r\n$3\r\nfoo\r\n$3\r\nbar -> Array response

6. Implement Interactive REPL (CLI)
    Run loop: User Input, validate redis commands, send commands to the server, display parsed response
    Support: help, quit

7. main.cpp: parse command-line args, instantiate CLI and launch in REPL mode.

Socket Programming
Protocol Handling
OOP principles
CLI development


*/

#include <string>
#include <vector>
#include "CLI.h"

int main(int argc, char* argv[]){
    std::string host = "127.0.0.1";
    int port = 6379;
    int i = 1;
    std::vector<std::string>commandArgs;

    // parse command-line args for -h and -p
    while(i < argc){
        std::string arg = argv[i];
        if(arg == "-h" && i+1 <argc){ // -h 127.0.0.1
            host = argv[++i];
        }else if(arg == "-p" && i+1<argc){
            port = std::stoi(argv[++i]);
        }else{
            while(i<argc){
                commandArgs.push_back(argv[i]);
                i++;
            }
            break;
        }
        ++i;
    }

    CLI cli(host, port);
    if(!commandArgs.empty()){
        cli.executeCommand(commandArgs);
    }else{
        cli.run(commandArgs);
    }
    return 0;
}