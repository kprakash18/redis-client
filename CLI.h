#ifndef CLI_H
#define CLI_H

#include <string>
#include "redisClient.h"
#include "commandHandler.h"
#include "ResponseParser.h"

class CLI{
    public:
        CLI(const std::string &host, int port);
        void run();
    private:
        redisClient redisClient;
};
#endif