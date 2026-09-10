# Redis Client — Part 6: Full RESP Deserialization, Interactive REPL & Command Pipeline

[← Previous: Part 5](part5.md) • [📖 Index](README.md) • **Part 6 (Final)**

---

## 1. Overview & Architectural Position

In **Part 5**, we initiated response handling by parsing **Simple Strings (`+`)** and observed socket stream desynchronization when encountering unhandled response types like Bulk Strings (`$`).

In **Part 6**, we complete the core system:
1. **Full RESP Response Deserializer (`ResponseParser`)**: Support for all fundamental Redis RESP 2 types:
   - **Simple Errors (`-`)**
   - **Integers (`:`)**
   - **Bulk Strings (`$`)** (including Null `$-1` and Empty `$0` bulk strings)
   - **Arrays / Multi-Bulk (`*`)** (with recursive nested parsing)
2. **Robust Interactive REPL & Execution Pipeline (`CLI`)**:
   - Dynamic prompt formatting (`<host>:<port>> `)
   - Integrated command loop with whitespace trimming and tokenization
   - Built-in commands (`quit`, `exit`, `help`)
   - **One-Shot Mode** vs. **Interactive Mode** handling
   - Socket error & server disconnect recovery with `try / catch` blocks
3. **Automated Testing Suite**:
   - Unit testing with Unix domain `socketpair()`
   - Integration testing with live `redis-server` instances

```
+-----------------------------------------------------------------------------------------+
|                                    CLI Layer (cli.cpp)                                  |
|   1. Parse input line: "LRANGE mylist 0 -1"                                             |
|   2. commandHandler::splitArgs() -> ["LRANGE", "mylist", "0", "-1"]                     |
|   3. commandHandler::buildRESPcommand() -> "*4\r\n$6\r\nLRANGE\r\n$6\r\nmylist\r\n..." |
|   4. redisClient::sendCommand() writes buffer to TCP socket                             |
+--------------------------------------------+--------------------------------------------+
                                             |
                                             v  (TCP Send)
                                  [ Redis Server: 6379 ]
                                             |
                                             v  (TCP Receive)
+-----------------------------------------------------------------------------------------+
|                               ResponseParser (ResponseParser.cpp)                       |
|   1. Read Prefix: '*'                                                                   |
|   2. parseArray() reads count: 2                                                        |
|   3. Recursive call 1: parseBulkString() -> "second"                                    |
|   4. Recursive call 2: parseBulkString() -> "first"                                     |
|   5. Formats response: "second\nfirst"                                                  |
+--------------------------------------------+--------------------------------------------+
                                             |
                                             v
                                  Terminal Output:
                                  second
                                  first
```

---

## 2. Full RESP 2 Deserialization Engine

### A. The RESP Type Dispatch Table

Every Redis response begins with a 1-byte ASCII type marker:

| Prefix | Type | Wire Example | Parsed Output / Meaning | Implementation Method |
| :---: | :---: | :---: | :---: | :---: |
| `+` | **Simple String** | `+OK\r\n` | `OK` | `parseSimpleString()` |
| `-` | **Simple Error** | `-ERR unknown command\r\n` | `(error) ERR unknown command` | `parseSimpleError()` |
| `:` | **Integer** | `:1000\r\n` | `1000` | `parseInteger()` |
| `$` | **Bulk String** | `$5\r\nhello\r\n` | `hello` | `parseBulkString()` |
| `$` | **Null Bulk String** | `$-1\r\n` | `(nil)` | `parseBulkString()` |
| `*` | **Array (Multi-Bulk)**| `*2\r\n$3\r\nfoo\r\n$3\r\nbar\r\n` | `foo\nbar` | `parseArray()` (recursive) |
| `*` | **Null Array** | `*-1\r\n` | `(nil)` | `parseArray()` |

---

### B. Detailed Wire-Level Parsing Mechanics

#### 1. Simple Strings (`+`) & Errors (`-`)
- Read characters until carriage return `\r`, then consume line feed `\n`.
- Simple errors prefix the message with `(error) ` to match standard CLI conventions:
  ```cpp
  std::string ResponseParser::parseSimpleError(int sockfd) {
      return "(error) " + readLine(sockfd);
  }
  ```

#### 2. Integers (`:`)
- Formatted as `:<number>\r\n`.
- Returned as an ASCII string representation of the integer (e.g., `:42\r\n` $\rightarrow$ `"42"`).

#### 3. Bulk Strings (`$`)
Bulk strings are binary-safe strings with an explicit byte length prefix:
$$\text{Wire format: } \$<\text{length}>\backslash\text{r}\backslash\text{n}<\text{payload}>\backslash\text{r}\backslash\text{n}$$

Key parsing details:
1. **Null Bulk String**: If length is `-1` (`$-1\r\n`), it represents a non-existent key $\rightarrow$ return `"(nil)"`.
2. **Empty Bulk String**: If length is `0` (`$0\r\n\r\n`), return `""`.
3. **Chunked Socket Read**: Network packets may be fragmented. The parser loops using `recv()` until exactly `length` bytes have been received:
   ```cpp
   std::string bulk;
   bulk.resize(length);
   int totalRead = 0;
   while (totalRead < length) {
       ssize_t r = recv(sockfd, &bulk[totalRead], length - totalRead, 0);
       if (r <= 0) return "(error) Incomplete bulk data.";
       totalRead += r;
   }
   ```
4. **Consume Trailing `\r\n`**: Bulk string payloads are followed by two framing bytes (`\r\n`) which must be consumed to leave the socket buffer in sync for the next command.

#### 4. Arrays / Multi-Bulk (`*`)
Arrays represent list returns, hash key-values, and scan results:
$$\text{Wire format: } *<\text{count}>\backslash\text{r}\backslash\text{n}<\text{element}_1><\text{element}_2>\dots$$

Key parsing details:
1. **Null Array**: If count is `-1` (`*-1\r\n`), return `"(nil)"`.
2. **Recursive Parsing**: Each element within the array is itself a valid RESP type (e.g. bulk strings, integers, or nested arrays). By calling `parseResponse(sockfd)` recursively `count` times, arrays of arbitrary depth and mixed types are parsed cleanly:
   ```cpp
   std::ostringstream oss;
   for (int i = 0; i < count; ++i) {
       oss << parseResponse(sockfd);
       if (i != count - 1) oss << "\n";
   }
   return oss.str();
   ```

---

## 3. CLI Class & REPL Pipeline Architecture

### A. Class Declaration (`CLI.h`)

```cpp
#ifndef CLI_H
#define CLI_H

#include <string>
#include <vector>
#include "redisClient.h"
#include "commandHandler.h"
#include "ResponseParser.h"

class CLI {
public:
    CLI(const std::string &host, int port);
    
    // Runs REPL or one-shot command
    void run(const std::vector<std::string>& args = {});
    
    // Sends a command and prints parsed response
    void executeCommand(const std::vector<std::string>& args);

private:
    std::string host;
    int port;
    redisClient redisClient;
};

#endif // CLI_H
```

---

### B. Pipeline Implementation (`cli.cpp`)

#### 1. Interactive REPL Loop (`CLI::run`)
- Connects to the Redis server on startup.
- Displays prompt `<host>:<port>> `.
- Trims whitespace and parses user input.
- Intercepts built-in commands (`quit`, `exit`, `help`).
- Hands off tokenized commands to `executeCommand()`.
- Disconnects cleanly on exit or EOF (`Ctrl+D`).

```cpp
void CLI::run(const std::vector<std::string>& commandArgs) {
    // If one-shot arguments were supplied, execute and terminate immediately
    if (!commandArgs.empty()) {
        executeCommand(commandArgs);
        return;
    }

    if (!redisClient.connectToServer()) {
        return;
    }

    std::cout << "Connected to Redis at " << host << ":" << port << "\n";

    while (true) {
        std::cout << host << ":" << port << "> ";
        std::cout.flush();
        std::string line;
        if (!std::getline(std::cin, line)) break; // EOF
        line = trim(line);
        if (line.empty()) continue;

        if (line == "quit" || line == "exit") {
            std::cout << "Goodbye.\n";
            break;
        }

        if (line == "help") {
            std::cout << "Displaying help: Enter standard Redis commands (e.g. PING, SET, GET)\n";
            continue;
        }

        std::vector<std::string> args = commandHandler::splitArgs(line);
        if (args.empty()) continue;

        executeCommand(args);
    }

    redisClient.disconnect();
}
```

#### 2. Command Execution & Exception Safety (`CLI::executeCommand`)
- Ensures active connection before transmission (for one-shot CLI execution).
- Builds RESP serialized string via `commandHandler::buildRESPcommand(args)`.
- Sends byte payload over socket via `redisClient.sendCommand(command)`.
- Parses response using `ResponseParser::parseResponse(sockfd)`.
- Protects against network exceptions or unexpected disconnects with `try/catch` boundaries.

```cpp
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
```

---

## 4. Main Entry Point: Dual-Mode Execution

In [main.cpp](file:///Users/kp/Desktop/redis-client/main.cpp), the application dynamically routes execution based on arguments:

1. **One-Shot Mode**: If trailing arguments are provided (e.g., `./redis_client -p 6379 SET foo bar`), the command is executed and the program immediately exits.
2. **Interactive Mode**: If no command arguments are given (e.g., `./redis_client -p 6379`), the interactive REPL is launched.

```cpp
CLI cli(host, port);
if (!commandArgs.empty()) {
    cli.executeCommand(commandArgs);
} else {
    cli.run();
}
```

---

## 5. Verification & Testing

### A. Automated Unit Tests (`tests.cpp`)

Using POSIX `socketpair()`, we test the serializer and parser without requiring a running Redis instance:

```cpp
void testResponseParser() {
    int sv[2];
    assert(socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == 0);

    // Test Bulk String
    write(sv[0], "$5\r\nhello\r\n", 11);
    assert(ResponseParser::parseResponse(sv[1]) == "hello");

    // Test Null Bulk String
    write(sv[0], "$-1\r\n", 5);
    assert(ResponseParser::parseResponse(sv[1]) == "(nil)");

    // Test Array
    write(sv[0], "*2\r\n$3\r\nfoo\r\n$3\r\nbar\r\n", 22);
    assert(ResponseParser::parseResponse(sv[1]) == "foo\nbar");

    close(sv[0]); close(sv[1]);
}
```

Run test suite:
```bash
make test
```
```text
=== Running Redis Client Unit Tests ===
[TEST] commandHandler::splitArgs... PASSED
[TEST] commandHandler::buildRESPcommand... PASSED
[TEST] ResponseParser... PASSED
=== All Unit Tests Passed! ===
```

---

### B. Live Redis Server Verification

#### 1. One-Shot Execution
```bash
$ ./build/redis_client -p 6379 PING
PONG

$ ./build/redis_client -p 6379 SET user:101 "Alice Smith"
OK

$ ./build/redis_client -p 6379 GET user:101
Alice Smith

$ ./build/redis_client -p 6379 INCR user:visits
1

$ ./build/redis_client -p 6379 LPUSH tasks "Task A" "Task B"
2

$ ./build/redis_client -p 6379 LRANGE tasks 0 -1
Task B
Task A
```

#### 2. Interactive REPL Session
```text
$ ./build/redis_client -h 127.0.0.1 -p 6379
Connected to Redis at 127.0.0.1:6379
127.0.0.1:6379> PING
PONG
127.0.0.1:6379> SET greeting "Hello Redis"
OK
127.0.0.1:6379> GET greeting
Hello Redis
127.0.0.1:6379> GET nonexistent
(nil)
127.0.0.1:6379> INVALID_COMMAND
(error) ERR unknown command 'INVALID_COMMAND'
127.0.0.1:6379> quit
Goodbye.
```

---

## 6. Summary of What We Built

| Component | Responsibility | Key Mechanics |
| :--- | :--- | :--- |
| **`commandHandler`** | Input parsing & command serialization | Regex word/quote extraction (`splitArgs`), RESP multi-bulk serialization (`buildRESPcommand`). |
| **`redisClient`** | TCP socket transport | `getaddrinfo` IPv4/IPv6 resolution, socket file descriptor lifecycle, `send()`, `disconnect()`. |
| **`ResponseParser`** | Wire response deserialization | Type prefix routing (`+`, `-`, `:`, `$`, `*`), CRLF consumption, recursive array decoding. |
| **`CLI`** | User interaction & REPL orchestrator | REPL prompt loop, one-shot vs. interactive dispatch, signal handling, exception trapping. |

---

## 7. Future Extensions & Exercises

If you wish to extend this project further, consider exploring:
1. **Pipelining**: Buffering multiple commands to send in a single `send()` syscall and reading responses in batch.
2. **Pub/Sub Mode**: Handling asynchronous `message`, `subscribe`, and `unsubscribe` multi-bulk push frames.
3. **TLS / SSL Support**: Wrapping the Berkeley socket with OpenSSL (`SSL_connect`, `SSL_read`, `SSL_write`).
4. **RESP 3 Protocol**: Supporting maps (`%`), sets (`~`), doubles (`,`), booleans (`#`), and big numbers (`(`).

---

[← Previous: Part 5 — RESP Response Deserialization & Socket Parsing](part5.md) • [📖 Index](README.md) • **🎉 Curriculum Complete**

