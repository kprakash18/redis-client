#include "CLI.h"
#include "ResponseParser.h"
#include "commandHandler.h"
#include <cstddef>
#include <iostream>
#include <string>
#include <vector>

// helper to trim whitespace
static std::string trim(const std::string &s){
    size_t start = s.find_first_not_of(" \t\n\r\f\v");
    if(start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\n\r\f\v");
    return s.substr(start,end-start+1);
}

CLI::CLI(const std::string &host, int port) 
    : host(host), port(port), redisClient(host, port) {}

void CLI::run(const std::vector<std::string>& commandArgs) {
    if (!commandArgs.empty()) {
        executeCommand(commandArgs);
        return;
    }

    if (!redisClient.connectToServer()) {
        return;
    }

    std::cout << "Connected to Redis at " << host << ":" << port << "\n";

    while (true) {
        std::cout<<host<<":"<<port<<">";
        std::cout.flush();
        std::string line;
        if(!std::getline(std::cin,line)) break;
        line = trim(line);
        if(line.empty()) continue;
        if(line=="quit" || line=="exit"){
            std::cout<<"Goodbye.\n";
            break;
        }

        if(line == "help"){
            std::cout<<"Displaying help\n";
            continue;
        }

        // split command into tokens
        std::vector<std::string> args = commandHandler::splitArgs(line);
        if (args.empty()) continue;

        executeCommand(args);
    }

    redisClient.disconnect();
} 
void CLI::executeCommand(const std::vector<std::string>& args) {
    if (args.empty()) return;

    if (redisClient.getSocketFD() == -1) {
        if (!redisClient.connectToServer()) {
            return;
        }
    }

    std::string command = commandHandler::buildRESPcommand(args);
    if (!redisClient.sendCommand(command)) {
        std::cerr << "(error) Failed to send command.\n";
        return;
    }

    // Parse and print response
    try {
        std::string response = ResponseParser::parseResponse(redisClient.getSocketFD());
        std::cout << response << "\n";
    } catch (const std::exception &e) {
        std::cerr << "(error) Failed to parse response: " << e.what() << "\n";
        std::cerr << "Redis server might have disconnected.\n";
    } catch (...) {
        std::cerr << "(error) Unknown error during response parsing.\n";
    }
}