#ifndef CLI_H
#define CLI_H

#include <string>
#include <vector>
#include "redisClient.h"

class CLI{
    public:
        CLI(const std::string &host, int port);
        void run(const std::vector<std::string>& args = {});
        void executeCommand(const std::vector<std::string>& args);
    private:
        std::string host;
        int port;
        redisClient redisClient;
};
#endif