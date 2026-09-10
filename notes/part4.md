# Redis Client — Part 4: RESP Command Serialization & Network Dispatch

[← Previous: Part 3](part3.md) • [📖 Index](README.md) • [Next: Part 5: Initial Deserialization →](part5.md)

---

## 1. Overview & Architectural Position

In **Part 3**, we implemented the interactive REPL and tokenized user input into clean vector tokens (`std::vector<std::string> args`).

In **Part 4**, we implement the **RESP (REdis Serialization Protocol) Serializer** and wire it directly into the **Networking Dispatch Layer**:
1. **RESP Serialization (`commandHandler::buildRESPcommand`)**: Converts tokenized command arguments into raw Redis-compliant wire protocol bytes (an array of bulk strings).
2. **Socket Transmission (`redisClient::sendCommand`)**: Dispatches the serialized byte payload across the active TCP socket descriptor via the POSIX `send()` system call.
3. **REPL Integration (`CLI::run`)**: Chains tokenization $\rightarrow$ RESP serialization $\rightarrow$ network dispatch inside the interactive command loop.

```
+-----------------------------------------------------------------------------------------+
|                                    CLI REPL Loop                                        |
|   1. Reads user input: "SET user:1 \"John Doe\""                                        |
|   2. Tokenizes: ["SET", "user:1", "John Doe"]                                           |
+--------------------------------------------+--------------------------------------------+
                                             |
                                             v
+-----------------------------------------------------------------------------------------+
|                                 commandHandler Class                                    |
|   - Function: buildRESPcommand(args)                                                    |
|   - Generates Array header: *3\r\n                                                      |
|   - Serializes each token as Bulk String:                                               |
|       $3\r\nSET\r\n                                                                     |
|       $6\r\nuser:1\r\n                                                                  |
|       $8\r\nJohn Doe\r\n                                                                |
|   - Returns raw wire payload std::string                                                |
+--------------------------------------------+--------------------------------------------+
                                             |
                                             v
+-----------------------------------------------------------------------------------------+
|                                  redisClient Class                                      |
|   - Function: sendCommand(const std::string &command)                                   |
|   - Socket Syscall: send(sockfd, command.c_str(), command.size(), 0)                   |
|   - Verifies bytes transmitted == payload size                                          |
+--------------------------------------------+--------------------------------------------+
                                             |  (TCP Stream)
                                             v
                                  [ Redis Server: 6379 ]
```

---

## 2. Deep Dive: RESP (REdis Serialization Protocol)

Redis uses **RESP** as its client-server communication protocol. RESP is a human-readable, binary-safe, text-framed binary wire protocol.

### A. Core RESP Data Types
Every RESP message begins with a single prefix byte identifying its data type:

| Prefix | Type | Wire Example | Description |
| :---: | :---: | :---: | :---: |
| `+` | Simple String | `+OK\r\n` | Fast status messages without newlines |
| `-` | Error | `-ERR unknown command\r\n` | Error responses |
| `:` | Integer | `:1000\r\n` | 64-bit signed integers |
| `$` | Bulk String | `$5\r\nhello\r\n` | Binary-safe string with explicit byte length |
| `*` | Array | `*2\r\n$3\r\nfoo\r\n$3\r\nbar\r\n` | Ordered list of other RESP elements |

### B. Command Serialization Format
Redis commands are **always sent from client to server as a RESP Array of Bulk Strings**:

$$\text{Command Payload} = \text{*<number-of-args>\r\n} + \sum_{i=1}^{N} \left(\text{\$<arg\_len>\r\n} + \text{<arg\_data>\r\n}\right)$$

#### Example 1: `PING`
- **Tokens**: `["PING"]` (1 argument)
- **Serialized RESP Payload**:
  ```text
  *1\r\n$4\r\nPING\r\n
  ```

#### Example 2: `SET mykey "hello world"`
- **Tokens**: `["SET", "mykey", "hello world"]` (3 arguments)
- **Serialized RESP Payload**:
  ```text
  *3\r\n$3\r\nSET\r\n$5\r\nmykey\r\n$11\r\nhello world\r\n
  ```
- **Byte Breakdown**:
  ```text
  *3\r\n          -> Array of 3 elements
  $3\r\nSET\r\n   -> Element 1: Bulk string of length 3 ("SET")
  $5\r\nmykey\r\n -> Element 2: Bulk string of length 5 ("mykey")
  $11\r\nhello world\r\n -> Element 3: Bulk string of length 11 ("hello world")
  ```

---

## 3. Source Code Implementation

### A. Header: `commandHandler.h`
Declares the static serializer utility alongside the tokenizer:

```cpp
#ifndef COMMAND_HANDLER_H
#define COMMAND_HANDLER_H

#include <cstring>
#include <string>
#include <vector>

class commandHandler {
public:
    // Split input command into tokens
    static std::vector<std::string> splitArgs(const std::string &input);

    // Build RESP command from vector of argument tokens
    static std::string buildRESPcommand(const std::vector<std::string> &args);
};

#endif
```

---

### B. Implementation: `commandHandler.cpp`
Uses `std::ostringstream` to construct the framed CRLF-delimited (`\r\n`) wire format efficiently:

```cpp
#include "commandHandler.h"
#include <regex>
#include <sstream>
#include <string>

std::vector<std::string> commandHandler::splitArgs(const std::string &input) {
    std::vector<std::string> tokens;
    // Regex to match quoted strings or non-whitespace tokens
    std::regex rgx(R"((\"[^\"]*\"|\S+))"); 
    auto words_begin = std::sregex_iterator(input.begin(), input.end(), rgx);
    auto words_end = std::sregex_iterator();

    for (auto it = words_begin; it != words_end; ++it) {
        std::string token = it->str();
        // Strip outer quotes
        if (token.size() >= 2 && token.front() == '\"' && token.back() == '\"') {
            token = token.substr(1, token.size() - 2);
        }
        tokens.push_back(token);
    }
    return tokens;
}

/*
 * RESP Array of Bulk Strings Builder:
 * *<arg_count>\r\n
 * $<arg_len>\r\n<arg_value>\r\n
 */
std::string commandHandler::buildRESPcommand(const std::vector<std::string> &args) {
    std::ostringstream oss;
    oss << "*" << args.size() << "\r\n"; // Number of arguments in array

    for (const auto &arg : args) {
        oss << "$" << arg.size() << "\r\n" << arg << "\r\n"; // Length and payload of argument
    }

    return oss.str();
}
```

---

### C. Socket Dispatcher: `redisClient.cpp`
Dispatches the raw string payload across the connected Berkeley TCP socket:

```cpp
bool redisClient::sendCommand(const std::string &command) {
    if (sockfd == -1) return false;
    
    // Transmit buffer over connected socket
    ssize_t sent = send(sockfd, command.c_str(), command.size(), 0);
    
    // Verify complete transmission
    return (sent == static_cast<ssize_t>(command.size()));
}
```

---

### D. REPL Wiring: `cli.cpp`
Connects the user input, serializer, and socket transmission in the main loop:

```cpp
void CLI::run() {
    if (!redisClient.connectToServer()) {
        return;
    }

    std::cout << "Connected to Redis at " << redisClient.getSocketFD() << "\n";
    std::string host = "127.0.0.1"; 
    int port = 6379;

    while (true) {
        std::cout << host << ":" << port << "> ";
        std::cout.flush();
        std::string line;
        if (!std::getline(std::cin, line)) break;
        line = trim(line);
        if (line.empty()) continue;
        if (line == "quit") {
            std::cout << "Goodbye.\n";
            break;
        }

        if (line == "help") {
            std::cout << "Displaying help\n";
            continue;
        }

        // 1. Split command into tokens
        std::vector<std::string> args = commandHandler::splitArgs(line);
        if (args.empty()) continue;

        // 2. Serialize tokens into RESP format
        std::string command = commandHandler::buildRESPcommand(args);

        // 3. Send over TCP socket
        if (!redisClient.sendCommand(command)) {
            std::cerr << "(error) Failed to send command.\n";
            break;
        }

        // Next step: Read, parse, and display Redis response
    }
}
```

---

## 4. Key Systems Programming Concepts

### 1. CRLF Framing (`\r\n`)
In network protocols (RESP, HTTP, SMTP), line terminators must strictly be **CRLF (`\r\n`, ASCII `0x0D 0x0A`)**, never just `\n` (`LF`, ASCII `0x0A`). 
If a client sends single `\n`, Redis protocol parsers may stall waiting for the carriage return or reject the packet as a protocol error.

### 2. Binary Safety via Length Prefixes
Because RESP precedes every bulk string with its exact byte length (`$<len>\r\n`), payloads can contain:
- Arbitrary whitespace
- Newline characters (`\n`, `\r\n`)
- Null bytes (`\0`)
- Binary serialized objects (e.g. images, Protobuf buffers)

The Redis server reads exactly `<len>` bytes, eliminating delimiter injection or truncation bugs.

### 3. The `send()` System Call
```c
ssize_t send(int sockfd, const void *buf, size_t len, int flags);
```
- `sockfd`: The active file descriptor returned by `socket()` and `connect()`.
- `buf`: Pointer to the raw byte buffer (`command.c_str()`).
- `len`: Total number of bytes to transmit (`command.size()`).
- `flags`: Socket options (default `0`).
- **Return Value**:
  - Positive integer: Number of bytes actually written to the socket transmit buffer.
  - `-1`: Error occurred (e.g., connection reset `ECONNRESET`, broken pipe `EPIPE`).

---

## 5. Verification & Interactive Testing

### Compilation
```bash
make
```

### Running with Active Redis Server
Ensure `redis-server` is running on port 6379, then run:
```bash
./build/redis_client
```

### Testing Wire Transmission
Sending commands like `PING` or `SET key value`:
```text
Connected to Redis at 3
127.0.0.1:6379> PING
127.0.0.1:6379> SET name Alice
127.0.0.1:6379> GET name
127.0.0.1:6379> quit
Goodbye.
```
*Note: Commands are successfully received and processed by `redis-server` without error.*

---

## 6. Next Step: Part 5 Preview

In **Part 5**, we will implement the **RESP Response Deserializer / Parser**:
- Reading raw bytes from the socket via `read()` / `recv()`.
- Parsing RESP data types (`+OK`, `-ERR`, `:integer`, `$bulk`, `*array`).
- Printing human-readable output to the terminal interface.

---

[← Previous: Part 3 — Interactive REPL & Command Tokenization](part3.md) • [📖 Index](README.md) • [Next: Part 5 — RESP Response Deserialization & Socket Parsing →](part5.md)

