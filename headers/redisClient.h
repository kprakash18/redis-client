#ifndef REDIS_CLIENT_H
#define REDIS_CLIENT_H

#include <string>

class redisClient{
    public:
        redisClient(const std::string &host, int port);
        redisClient();
        ~redisClient();

        bool connectToServer();
        void disconnect();
        int getSocketFD() const ;
        bool sendCommand(const std::string &command);
    private:
    std::string host;
    int port;
    int sockfd;
};
#endif