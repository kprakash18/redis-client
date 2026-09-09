#ifndef COMMAND_HANDLER_H
#define COMMAND_HANDLER_H

#include <cstring>
#include <string>
#include <vector>
class commandHandler {
    public:
        // split commad into tokens
        static std::vector<std::string> splitArgs(const std::string &input);

    // build RESP command from the vector arguments
    static std::string buildRESPcommand(const std::vector<std::string> &args);

};
#endif