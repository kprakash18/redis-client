#ifndef COMMAND_HANDLER_H
#define COMMAND_HANDLER_H

#include <string>
#include <vector>
class commandHandler {
    public:
        static std::vector<std::string> splitArgs(const std::string &input);



};
#endif