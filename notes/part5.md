# Redis Client — Part 5: RESP Response Deserialization & Socket Parsing

[← Previous: Part 4](part4.md) • [📖 Index](README.md) • [Next: Part 6: Full RESP Parser →](part6.md)

---

## 1. Overview & Architectural Position

In **Part 4**, we implemented the **RESP Command Serializer** and transmitted commands across the TCP socket to Redis using `redisClient::sendCommand`.

In **Part 5**, we begin building the **RESP Response Parser (`ResponseParser`)** to ingest raw byte streams from the socket descriptor and deserialize them into human-readable strings.

```
+-----------------------------------------------------------------------------------------+
|                                    CLI REPL Loop                                        |
|   1. User inputs: "PING"                                                                |
|   2. commandHandler tokenizes & serializes: "*1\r\n$4\r\nPING\r\n"                      |
|   3. redisClient sends payload over TCP socket (FD 3)                                   |
+--------------------------------------------+--------------------------------------------+
                                             |
                                             v  (TCP Send Buffer)
                                  [ Redis Server: 6379 ]
                                             |
                                             v  (TCP Receive Buffer)
+-----------------------------------------------------------------------------------------+
|                                 ResponseParser Class                                    |
|   - Function: parseResponse(int sockfd)                                                 |
|   - Reads first byte: prefix '+'                                                        |
|   - Dispatches to: parseSimpleString(sockfd)                                            |
|   - Reads CRLF-terminated line: "PONG\r\n" -> "PONG"                                    |
|   - Returns: "PONG"                                                                     |
+--------------------------------------------+--------------------------------------------+
                                             |
                                             v
                                  Terminal Output: PONG
```

---

## 2. Core Concepts: Network Stream Parsing & RESP Wire Framing

### A. The Nature of TCP Byte Streams
Unlike UDP (which is datagram/packet-based), **TCP is an unstructured stream of bytes**:
- There are no built-in "packet boundaries" or message markers at the TCP layer.
- Bytes written by the server may arrive all at once, in fragments, or clumped together.
- Application-level protocols like **RESP** define their own **framing**:
  1. **Type Prefix**: The first byte indicates the data type (`+`, `-`, `:`, `$`, `*`).
  2. **CRLF Framing (`\r\n`)**: End of lines / metadata headers are marked with Carriage Return (`\r`, ASCII 13) and Line Feed (`\n`, ASCII 10).
  3. **Length Prefixes**: Binary data types (e.g., Bulk Strings `$5\r\nhello\r\n`) explicitly define the payload length in bytes.

### B. RESP Type Prefix Dispatch Table

| Prefix | Type | Wire Example | Handled In Part 5 |
| :---: | :---: | :---: | :---: |
| `+` | **Simple String** | `+PONG\r\n` / `+OK\r\n` | **Yes (`parseSimpleString`)** |
| `-` | **Simple Error** | `-ERR unknown command\r\n` | *Planned* |
| `:` | **Integer** | `:1000\r\n` | *Planned* |
| `$` | **Bulk String** | `$5\r\nhello\r\n` | *Planned* |
| `*` | **Array** | `*2\r\n$3\r\nfoo\r\n$3\r\nbar\r\n` | *Planned* |

---

## 3. Source Code Breakdown

### A. Header: `ResponseParser.h`

```cpp
#ifndef RESPONSEPARSER_H
#define RESPONSEPARSER_H

#include <string>

class ResponseParser {
public:
    // Read from the given socket and return parsed response as a string
    static std::string parseResponse(int sockfd);

private:
    // Helper to parse RESP Simple String (+<string>\r\n)
    static std::string parseSimpleString(int sockfd);

    // Stubs for future RESP types:
    // static std::string parseSimpleError(int sockfd);
    // static std::string parseInteger(int sockfd);
    // static std::string parseBulkString(int sockfd);
    // static std::string parseArray(int sockfd);
};

#endif // RESPONSEPARSER_H
```

#### Key Design Points:
- **Stateless Static Interface**: `ResponseParser::parseResponse(int sockfd)` requires only the connected file descriptor.
- **Modularity**: Dedicated private parsers for each RESP wire type keep the parser extensible and maintainable.

---

### B. Implementation: `ResponseParser.cpp`

```cpp
#include "ResponseParser.h"
#include <unistd.h>
#include <cstdlib>
#include <sys/types.h>
#include <sys/socket.h>

// 1. Read a single character from the socket descriptor
static bool readChar(int sockfd, char &c) {
    ssize_t r = recv(sockfd, &c, 1, 0);
    return (r == 1);
}

// 2. Read a CRLF-delimited line (\r\n) from the socket
static std::string readLine(int sockfd) {
    std::string line;
    char c;
    while (readChar(sockfd, c)) {
        if (c == '\r') {
            // Found '\r'; consume the subsequent '\n' and finish
            readChar(sockfd, c);
            break;
        }
        line.push_back(c);
    }
    return line;
}

// 3. Main response entry point
std::string ResponseParser::parseResponse(int sockfd) {
    char prefix;
    if (!readChar(sockfd, prefix)) {
        return "(Error) No response or connection closed.";
    }
    
    switch (prefix) {
        case '+': 
            return parseSimpleString(sockfd);
        // case '-': return parseSimpleError(sockfd);
        // case ':': return parseInteger(sockfd);
        // case '$': return parseBulkString(sockfd);
        // case '*': return parseArray(sockfd);
        default: 
            return "(Error) Unkown reply type.";
    }
}

// 4. Parse simple string (+<data>\r\n)
std::string ResponseParser::parseSimpleString(int sockfd) {
    return readLine(sockfd);
}
```

---

### C. Detailed Function Breakdown

#### 1. `readChar(int sockfd, char &c)`
```cpp
static bool readChar(int sockfd, char &c) {
    ssize_t r = recv(sockfd, &c, 1, 0);
    return (r == 1);
}
```
- **POSIX `recv()` Syscall**: Requests 1 byte from the kernel socket receive buffer.
- **Return Value**:
  - `1`: Exactly 1 byte was read into `c` $\rightarrow$ returns `true`.
  - `0`: The peer (Redis server) performed an orderly shutdown $\rightarrow$ returns `false`.
  - `-1`: A socket error occurred (e.g., connection reset `ECONNRESET`) $\rightarrow$ returns `false`.

#### 2. `readLine(int sockfd)`
```cpp
static std::string readLine(int sockfd) {
    std::string line;
    char c;
    while (readChar(sockfd, c)) {
        if (c == '\r') {
            readChar(sockfd, c); // Consume the expected '\n'
            break;
        }
        line.push_back(c);
    }
    return line;
}
```
- Ingests bytes sequentially until the Carriage Return (`\r`, ASCII 13) is encountered.
- Reads and discards the next character (the Line Feed `\n`, ASCII 10).
- Returns the stripped string content without trailing `\r\n`.

#### 3. `ResponseParser::parseResponse(int sockfd)`
```cpp
char prefix;
if (!readChar(sockfd, prefix)) {
    return "(Error) No response or connection closed.";
}
switch (prefix) {
    case '+': return parseSimpleString(sockfd);
    default:  return "(Error) Unkown reply type.";
}
```
- Reads the very first byte off the socket stream to inspect the RESP type prefix.
- Dispatches control to the type-specific handler (`parseSimpleString` for `+`).

---

### D. CLI REPL Integration (`cli.cpp`)

```cpp
// 1. Split command into tokens
std::vector<std::string> args = commandHandler::splitArgs(line);
if (args.empty()) continue;

// 2. Serialize tokens into RESP format
std::string command = commandHandler::buildRESPcommand(args);
if (!redisClient.sendCommand(command)) {
    std::cerr << "(error) Failed to send command.\n";
    break;
}

// 3. Ingest, parse, and print response from socket
std::string response = ResponseParser::parseResponse(redisClient.getSocketFD());
std::cout << response << "\n";
```

---

## 4. Step-by-Step Dry Run

### Scenario 1: Executing `PING` (Simple String Response)

#### 1. Client Sends Command
```text
User Input: PING
Serialized RESP sent to Redis: *1\r\n$4\r\nPING\r\n
```

#### 2. Redis Server Returns
```text
Raw bytes placed in TCP socket receive buffer:
['+', 'P', 'O', 'N', 'G', '\r', '\n']
```

#### 3. Execution Trace in `ResponseParser`

| Step | Function | Socket Read | `prefix` / `c` | `line` | Notes |
| :---: | :---: | :---: | :---: | :---: | :--- |
| **1** | `parseResponse` | `readChar(sockfd, prefix)` | `prefix = '+'` | `""` | First byte is `+`. Dispatches to `parseSimpleString`. |
| **2** | `parseSimpleString` | Calls `readLine(sockfd)` | — | `""` | Begins line parsing loop. |
| **3** | `readLine` | `readChar(sockfd, c)` | `c = 'P'` | `"P"` | Not `\r`, append to `line`. |
| **4** | `readLine` | `readChar(sockfd, c)` | `c = 'O'` | `"PO"` | Not `\r`, append to `line`. |
| **5** | `readLine` | `readChar(sockfd, c)` | `c = 'N'` | `"PON"` | Not `\r`, append to `line`. |
| **6** | `readLine` | `readChar(sockfd, c)` | `c = 'G'` | `"PONG"` | Not `\r`, append to `line`. |
| **7** | `readLine` | `readChar(sockfd, c)` | `c = '\r'` | `"PONG"` | Detected `\r`. Enters `if (c == '\r')`. |
| **8** | `readLine` | `readChar(sockfd, c)` | `c = '\n'` | `"PONG"` | Consumes `\n` delimiter and breaks loop. |
| **9** | `cli.cpp` | `std::cout << response` | — | — | **Prints: `PONG`** |

---

### Scenario 2: What Happens with Unimplemented Types (e.g., `echo HI`)?

Why did `echo HI` followed by `PING` produce `(Error) Unkown reply type.`?

```
User Input: echo HI
Redis Server Responds with a Bulk String:
$2\r\nHI\r\n
```

#### Step-by-step Socket Stream Desynchronization:
1. `parseResponse()` reads byte 1: `'$'`.
2. `switch (prefix)` has no handler for `'$'`, so it hits `default: return "(Error) Unkown reply type."`.
3. **Crucial Issue**: The rest of the message (`2\r\nHI\r\n`) **remains unread inside the TCP socket receive buffer**!
4. The user enters `PING` next:
   - Client sends `*1\r\n$4\r\nPING\r\n`.
   - Redis server replies with `+PONG\r\n`.
   - But `parseResponse()` reads the next unread byte left in the socket buffer from the *previous* command: `'2'`!
   - `'2'` is not a valid prefix $\rightarrow$ hits `default: return "(Error) Unkown reply type."`.
   - The stream is now out of sync until all leftover bytes are consumed.

> [!IMPORTANT]
> **Stream Synchronization Rule**: A protocol parser must consume **every byte** of a response before the next command's response can be parsed cleanly. Implementing all RESP types ($ for Bulk String, - for Error, : for Integer, * for Array) prevents stream desynchronization.

---

## 5. Verification & Testing

### 1. Build the Binary
```bash
make clean && make
```

### 2. Run the Client
```bash
./build/redis_client
```

### 3. Interactive Execution
```text
Connected to Redis at 3
127.0.0.1:6379> PING
PONG
127.0.0.1:6379> quit
Goodbye.
```

---

## 6. Next Steps: Expanding ResponseParser (Part 6)

In **Part 6**, we will complete the remaining RESP data types to support all standard Redis commands:
1. **Simple Errors (`-`)**: Parse error messages like `-ERR unknown command 'foobar'`.
2. **Integers (`:`)**: Parse integer replies like `:1` from `INCR`, `DEL`, or `EXISTS`.
3. **Bulk Strings (`$`)**: Parse length-prefixed strings like `$5\r\nhello\r\n` (and null bulk strings `$-1\r\n`) for commands like `GET`, `ECHO`.
4. **Arrays (`*`)**: Recursively parse nested multi-bulk replies for commands like `MGET`, `KEYS`, `LRANGE`.

---

[← Previous: Part 4 — RESP Command Serialization & Network Dispatch](part4.md) • [📖 Index](README.md) • [Next: Part 6 — Full RESP Deserialization, Interactive REPL & Command Pipeline →](part6.md)

