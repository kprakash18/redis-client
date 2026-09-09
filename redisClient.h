#ifndef REDIS_CLIENT_H
#define REDIS_CLIENT_H

#include <string>
#include <iostream>
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cstring>
class redisClient{
    public:
        redisClient(const std::string &host, int port);
        redisClient();
        ~redisClient();

        bool connectToServer();
        void disconnect();
        int getSocketFD() const ;
    private:
    std::string host;
    int port;
    int sockfd;
};
#endif