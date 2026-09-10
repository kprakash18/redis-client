# Redis Client — Part 2: TCP Socket Networking & Connection Layer

[← Previous: Part 1](part1.md) • [📖 Index](README.md) • [Next: Part 3: Interactive REPL →](part3.md)

---

## 1. Overview & Architecture Position

In Part 1, we established the CLI argument parsing engine and high-level architectural roadmap. In Part 2, we implement the **Networking Layer (`redisClient`)** and wire it into the **`CLI` REPL coordinator** and **`main.cpp`**.

```
+-------------------------------------------------------------------------+
|                               main.cpp                                  |
|   1. Parses -h <host>, -p <port>                                        |
|   2. Instantiates CLI cli(host, port)                                   |
|   3. Calls cli.run()                                                    |
+------------------------------------+------------------------------------+
                                     |
                                     v
+-------------------------------------------------------------------------+
|                                CLI Class                                |
|   1. Owns redisClient instance                                          |
|   2. Calls redisClient.connectToServer()                                |
|   3. Coordinates user REPL loop                                         |
+------------------------------------+------------------------------------+
                                     |
                                     v
+-------------------------------------------------------------------------+
|                           redisClient Class                             |
|   - DNS / IP Resolution: getaddrinfo() (IPv4 + IPv6)                    |
|   - Socket Creation:     socket(AF_INET/AF_INET6, SOCK_STREAM, 0)       |
|   - TCP 3-Way Handshake: connect()                                      |
|   - Resource Management: RAII Destructor + disconnect() (close)         |
+-------------------------------------------------------------------------+
```

---

## 2. Source Code Implementation

### A. Header: `redisClient.h`
```cpp
#ifndef REDIS_CLIENT_H
#define REDIS_CLIENT_H

#include <string>
#include <iostream>
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cstring>

class redisClient {
public:
    redisClient(const std::string &host, int port);
    redisClient();
    ~redisClient();

    bool connectToServer();
    void disconnect();

private:
    std::string host;
    int port;
    int sockfd;
};

#endif
```

---

### B. Implementation: `redisClient.cpp`
```cpp
#include "redisClient.h"
#include <iostream>
#include <netdb.h>
#include <string>
#include <sys/socket.h>

redisClient::redisClient(const std::string &host, int port) 
    : host(host), port(port), sockfd(-1) {}

redisClient::~redisClient() {
    disconnect();
}

bool redisClient::connectToServer() {
    struct addrinfo hints, *res = nullptr;
    std::memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;     // Allow IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM; // TCP Stream Socket

    std::string portStr = std::to_string(port);
    int err = getaddrinfo(host.c_str(), portStr.c_str(), &hints, &res);
    if (err != 0) {
        std::cerr << "getaddrinfo: " << gai_strerror(err) << "\n";
        return false;
    }

    // Iterate through the resolved addresses linked list
    for (auto p = res; p != nullptr; p = p->ai_next) {
        sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (sockfd == -1) continue; // Try next address candidate

        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == 0) {
            break; // Connection established successfully
        }

        close(sockfd); // Failed connection, close socket before next attempt
        sockfd = -1;
    }

    freeaddrinfo(res); // Free heap memory allocated by getaddrinfo

    if (sockfd == -1) {
        std::cerr << "Could not connect to " << host << ":" << port << "\n";
        return false;
    }

    return true;
}

void redisClient::disconnect() {
    if (sockfd != -1) {
        close(sockfd);
        sockfd = -1;
    }
}
```

---

### C. REPL Controller: `CLI.h` & `cli.cpp`
```cpp
// CLI.h
#ifndef CLI_H
#define CLI_H

#include <string>
#include "redisClient.h"

class CLI {
public:
    CLI(const std::string &host, int port);
    void run();

private:
    redisClient redisClient;
};

#endif
```

```cpp
// cli.cpp
#include "CLI.h"
#include <iostream>

CLI::CLI(const std::string &host, int port) 
    : redisClient(host, port) {}

void CLI::run() {
    if (!redisClient.connectToServer()) {
        return;
    }

    std::cout << "Connected to Redis at \n";
}
```

---

## 3. Core Networking Concepts Explained

### A. Berkeley Sockets API & Socket Lifecycle
A **socket** is an operating system abstraction representing an endpoint for two-way network communication. In Unix-like OSes (macOS, Linux), sockets are represented as integer **File Descriptors (`sockfd`)**.

```
[getaddrinfo]  --> Resolves domain/IP into sockaddr structures
      |
      v
  [socket()]   --> Allocates OS socket resources, returns file descriptor
      |
      v
 [connect()]   --> Initiates TCP 3-Way Handshake (SYN -> SYN-ACK -> ACK)
      |
      v
[send/recv]    --> Data transmission (RESP protocol commands & responses)
      |
      v
  [close()]    --> Terminates connection (FIN -> ACK), releases file descriptor
```

---

### B. Understanding `getaddrinfo` & `struct addrinfo`
Older socket programs used `gethostbyname()` or hardcoded `sockaddr_in` structs, which were IPv4-only and not thread-safe. Modern networking uses **`getaddrinfo()`**:

```cpp
int getaddrinfo(const char *node,
                const char *service,
                const struct addrinfo *hints,
                struct addrinfo **res);
```

#### Why we configure `hints`:
- **`hints.ai_family = AF_UNSPEC`**: `AF_UNSPEC` tells the resolver to return both **IPv4** (`AF_INET`) and **IPv6** (`AF_INET6`) records.
- **`hints.ai_socktype = SOCK_STREAM`**: Specifies reliable, sequenced, two-way connection-based byte streams (**TCP**).
- **`std::memset(&hints, 0, sizeof(hints))`**: Crucial to zero out the memory block so uninitialized padding bytes do not trigger unexpected flags.

#### Linked List of Candidates (`res`):
`getaddrinfo` allocates a singly-linked list of `struct addrinfo` nodes on the heap. A single hostname (e.g., `localhost` or `redis.internal`) may resolve to multiple addresses (e.g., IPv6 `::1` and IPv4 `127.0.0.1`). We traverse this list using `for (auto p = res; p != nullptr; p = p->ai_next)`.

---

### C. `socket()` and `connect()`
```cpp
sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
```
- Creates an endpoint for communication with the exact address family and protocol resolved for candidate `p`.
- Returns `-1` on failure (e.g., process out of file descriptors).

```cpp
int result = connect(sockfd, p->ai_addr, p->ai_addrlen);
```
- Initiates the TCP three-way handshake with the server at `p->ai_addr`.
- Returns `0` on success, or `-1` on failure (`ECONNREFUSED`, `ETIMEDOUT`, etc.).

---

### D. Resource Management & RAII (Resource Acquisition Is Initialization)
1. **Memory Cleanup (`freeaddrinfo`)**:
   - `getaddrinfo()` dynamically allocates memory for the linked list.
   - `freeaddrinfo(res)` must always be called to prevent memory leaks.
2. **File Descriptor Cleanup (`close(sockfd)`)**:
   - Open sockets consume system file descriptors.
   - If a candidate connection fails in the loop, we call `close(sockfd)` immediately before trying the next candidate.
   - The class **destructor `~redisClient()`** guarantees that `disconnect()` is invoked when the client object goes out of scope.

---

## 4. Comprehensive Dry Run & Execution Trace

### Test Scenario 1: Successful Connection to Local Redis (`127.0.0.1:6379`)

#### Initial Program State:
- `host = "127.0.0.1"`, `port = 6379`
- `redisClient` initialized with `sockfd = -1`

#### Execution Step Trace Table:

| Step | Function / Line | Operation | Variables & State | Notes |
|:---|:---|:---|:---|:---|
| **1** | `main.cpp:61` | `CLI cli("127.0.0.1", 6379)` | `cli.redisClient.host = "127.0.0.1"`<br>`cli.redisClient.port = 6379`<br>`cli.redisClient.sockfd = -1` | Object constructed |
| **2** | `cli.cpp:8` | `redisClient.connectToServer()` | — | Invokes connection routine |
| **3** | `redisClient.cpp:31-33` | Setup `hints` | `hints.ai_family = AF_UNSPEC`<br>`hints.ai_socktype = SOCK_STREAM` | Configures TCP protocol filter |
| **4** | `redisClient.cpp:35-36` | `getaddrinfo("127.0.0.1", "6379", ...)` | `err = 0`<br>`res` points to node `[IPv4 127.0.0.1:6379, ai_next=null]` | DNS/IP resolution succeeds |
| **5** | `redisClient.cpp:43` | `p = res` (Loop 1st iteration) | `p->ai_family = AF_INET (2)`<br>`p->ai_socktype = SOCK_STREAM (1)` | First address candidate |
| **6** | `redisClient.cpp:44` | `socket(AF_INET, SOCK_STREAM, 0)` | `sockfd = 3` | OS creates socket file descriptor `3` |
| **7** | `redisClient.cpp:46` | `connect(3, 127.0.0.1:6379)` | Returns `0` (Success) | 3-Way Handshake completes |
| **8** | `redisClient.cpp:47` | `break;` | Loop terminates early | Active connection kept in `sockfd = 3` |
| **9** | `redisClient.cpp:53` | `freeaddrinfo(res)` | `res` heap memory freed | Prevents memory leak |
| **10** | `redisClient.cpp:55-58` | Check `sockfd == -1` | `sockfd = 3 != -1` &rarr; Condition False | Skips error block, returns `true` |
| **11** | `cli.cpp:12` | Output success message | Prints: `"Connected to Redis at \n"` | REPL ready for commands |
| **12** | `main.cpp:64` | `return 0` / Program exit | `~redisClient()` &rarr; `disconnect()` &rarr; `close(3)` | Socket closed, resources released |

---

### Test Scenario 2: Connection Failure (Redis Server Not Running)

#### Execution Step Trace Table:

| Step | Function / Line | Operation | Variables & State | Notes |
|:---|:---|:---|:---|:---|
| **1** | `redisClient.cpp:36` | `getaddrinfo("127.0.0.1", "6379", ...)` | `err = 0`, `res` populated | IP resolves successfully |
| **2** | `redisClient.cpp:44` | `socket(AF_INET, SOCK_STREAM, 0)` | `sockfd = 3` | Socket allocated |
| **3** | `redisClient.cpp:46` | `connect(3, 127.0.0.1:6379)` | Returns `-1` (`ECONNREFUSED`) | No server listening on port 6379 |
| **4** | `redisClient.cpp:49-50` | `close(3)`, `sockfd = -1` | `sockfd = -1` | Closed socket, reset descriptor |
| **5** | `redisClient.cpp:43` | `p = p->ai_next` | `p = nullptr` | No more candidates in list |
| **6** | `redisClient.cpp:53` | `freeaddrinfo(res)` | Heap memory freed | Cleanup |
| **7** | `redisClient.cpp:55-57` | `if (sockfd == -1)` | Evaluates to `true` | Prints `"Could not connect to 127.0.0.1:6379"` |
| **8** | `redisClient.cpp:58` | `return false;` | Returns failure | `cli.run()` exits gracefully |

---

## 5. Build Automation & Project Layout

### Build Architecture:
```
redis-client/
├── Makefile                # Compiles sources into build/ directory
├── compile_flags.txt       # Clangd language server include configurations
├── .vscode/
│   └── c_cpp_properties.json
├── build/                  # Isolated build output folder
│   ├── cli.o
│   ├── main.o
│   ├── redisClient.o
│   └── redis_client        # Executable binary
├── CLI.h / cli.cpp         # REPL coordinator
├── redisClient.h / .cpp    # Networking layer
└── main.cpp                # Entry point
```

### Makefile Directives:
- **`make`**: Compiles all `.cpp` files with `-std=c++17 -Wall -Wextra` into `build/*.o` and links `build/redis_client`.
- **`make clean`**: Removes the `build/` directory completely.
- **`make run`**: Compiles and executes `./build/redis_client`.

---

## 6. Next Steps (Part 3 Roadmap)

1. **RESP Serializer (`CommandHandler`)**:
   - Tokenize command strings (e.g., `"SET key value"`).
   - Format into Redis RESP Array of Bulk Strings:
     ```text
     *3\r\n$3\r\nSET\r\n$3\r\nkey\r\n$5\r\nvalue\r\n
     ```
2. **Socket Send & Receive**:
   - Implement `sendCommand(const std::string &cmd)` using `send()` and `recv()`.
3. **RESP Response Deserializer (`ResponseParser`)**:
   - Parse type prefixes (`+`, `-`, `:`, `$`, `*`).

---

[← Previous: Part 1 — Architecture & CLI Entry](part1.md) • [📖 Index](README.md) • [Next: Part 3 — Interactive REPL & Command Tokenization →](part3.md)

