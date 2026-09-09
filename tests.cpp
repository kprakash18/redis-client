#include <iostream>
#include <cassert>
#include <vector>
#include <string>
#include <sys/socket.h>
#include <unistd.h>
#include "commandHandler.h"
#include "ResponseParser.h"

void testSplitArgs() {
    std::cout << "[TEST] commandHandler::splitArgs... ";
    
    auto tokens = commandHandler::splitArgs("SET key value");
    assert(tokens.size() == 3);
    assert(tokens[0] == "SET");
    assert(tokens[1] == "key");
    assert(tokens[2] == "value");

    auto tokens2 = commandHandler::splitArgs("SET msg \"hello world!\"");
    assert(tokens2.size() == 3);
    assert(tokens2[0] == "SET");
    assert(tokens2[1] == "msg");
    assert(tokens2[2] == "hello world!");

    auto tokens3 = commandHandler::splitArgs("SET empty \"\"");
    assert(tokens3.size() == 3);
    assert(tokens3[0] == "SET");
    assert(tokens3[1] == "empty");
    assert(tokens3[2] == "");

    std::cout << "PASSED\n";
}

void testBuildRESP() {
    std::cout << "[TEST] commandHandler::buildRESPcommand... ";
    
    std::string resp1 = commandHandler::buildRESPcommand({"PING"});
    assert(resp1 == "*1\r\n$4\r\nPING\r\n");

    std::string resp2 = commandHandler::buildRESPcommand({"SET", "k", "v"});
    assert(resp2 == "*3\r\n$3\r\nSET\r\n$1\r\nk\r\n$1\r\nv\r\n");

    std::cout << "PASSED\n";
}

void testResponseParser() {
    std::cout << "[TEST] ResponseParser... ";

    int sv[2];
    assert(socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == 0);

    // 1. Simple string
    std::string data = "+PONG\r\n";
    write(sv[0], data.c_str(), data.size());
    assert(ResponseParser::parseResponse(sv[1]) == "PONG");

    // 2. Simple error
    data = "-ERR unknown command\r\n";
    write(sv[0], data.c_str(), data.size());
    assert(ResponseParser::parseResponse(sv[1]) == "(error) ERR unknown command");

    // 3. Integer
    data = ":42\r\n";
    write(sv[0], data.c_str(), data.size());
    assert(ResponseParser::parseResponse(sv[1]) == "42");

    // 4. Bulk string
    data = "$5\r\nhello\r\n";
    write(sv[0], data.c_str(), data.size());
    assert(ResponseParser::parseResponse(sv[1]) == "hello");

    // 5. Null Bulk string
    data = "$-1\r\n";
    write(sv[0], data.c_str(), data.size());
    assert(ResponseParser::parseResponse(sv[1]) == "(nil)");

    // 6. Empty Bulk string
    data = "$0\r\n\r\n";
    write(sv[0], data.c_str(), data.size());
    assert(ResponseParser::parseResponse(sv[1]) == "");

    // 7. Array
    data = "*2\r\n$3\r\nfoo\r\n$3\r\nbar\r\n";
    write(sv[0], data.c_str(), data.size());
    assert(ResponseParser::parseResponse(sv[1]) == "foo\nbar");

    // 8. Null Array
    data = "*-1\r\n";
    write(sv[0], data.c_str(), data.size());
    assert(ResponseParser::parseResponse(sv[1]) == "(nil)");

    close(sv[0]);
    close(sv[1]);
    std::cout << "PASSED\n";
}

int main() {
    std::cout << "=== Running Redis Client Unit Tests ===\n";
    testSplitArgs();
    testBuildRESP();
    testResponseParser();
    std::cout << "=== All Unit Tests Passed! ===\n";
    return 0;
}
