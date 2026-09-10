# 🚀 Building a Redis Client from Scratch in C++

A comprehensive, production-grade engineering curriculum documenting how to build a lightweight **Redis CLI client and RESP 2 protocol parser** in modern C++ (C++17) using Berkeley / POSIX sockets from first principles.

---

## 📖 Curriculum Overview

This 6-part series guides you through system-level network programming, binary-safe serialization/deserialization, interactive terminal REPL design, and automated socket testing:

```
                  +-----------------------------------+
                  |            main.cpp               |
                  |  (CLI argument parsing: -h, -p)   |
                  +-----------------+-----------------+
                                    |
                                    v
                  +-----------------------------------+
                  |             CLI Class             |
                  |     (Interactive REPL Engine)     |
                  +--------+-----------------+--------+
                           |                 |
            +--------------+                 +--------------+
            v                                               v
+-----------------------+                       +-----------------------+
|    CommandHandler     |                       |    ResponseParser     |
| (Formats commands to  |                       |  (Parses incoming     |
|     RESP protocol)    |                       |    RESP responses)    |
+-----------+-----------+                       +-----------+-----------+
            |                                               |
            +-----------------------+-----------------------+
                                    |
                                    v
                  +-----------------------------------+
                  |           RedisClient             |
                  |  (TCP Socket Networking / I/O)    |
                  +-----------------------------------+
```

---

## 📚 Parts Breakdown

| Chapter | Title | Key Topics Covered |
| :---: | :--- | :--- |
| **[Part 1](part1.md)** | **Architecture & CLI Entry** | Project architecture, `argc`/`argv` parsing, flag bounds checking, default configs |
| **[Part 2](part2.md)** | **Socket Networking & Connection** | POSIX Sockets, DNS resolution via `getaddrinfo()`, IPv4/IPv6 support, connection lifecycle & RAII |
| **[Part 3](part3.md)** | **Interactive REPL & Tokenization** | Read-Eval-Print Loop, quote-aware string splitting, whitespace trimming, prompt formatting |
| **[Part 4](part4.md)** | **RESP Command Serialization** | Wire protocol specification, array serialization, byte-level packet dispatch with `send()` |
| **[Part 5](part5.md)** | **Initial Deserialization Mechanics** | Stream parsing, delimiter extraction (`\r\n`), simple strings (`+`), stream desync edge cases |
| **[Part 6](part6.md)** | **Full RESP Parser, REPL & Testing** | Integers (`:`), Errors (`-`), Bulk Strings (`$`), recursive Arrays (`*`), `socketpair()` testing |

---

## 🛠️ Prerequisites & Tech Stack

- **Language Standard**: C++17 or newer
- **Compiler**: `clang++` or `g++`
- **Operating System**: macOS or Linux (POSIX Sockets: `<sys/socket.h>`, `<netdb.h>`, `<unistd.h>`)
- **Build System**: `make`
- **Target Server**: Any Redis-compatible server (e.g. Redis 6.x/7.x or KeyDB) running on `127.0.0.1:6379`

---

## 🚀 Quick Start

### 1. Build Binary & Run Tests
```bash
# Clone the repository
git clone https://github.com/your-username/redis-client.git
cd redis-client

# Run automated unit test suite
make test

# Build the client executable
make
```

### 2. Start Interactive CLI
```bash
# Connect to local Redis instance (default: 127.0.0.1:6379)
./build/redis_client

# Connect to a remote host and custom port
./build/redis_client -h 192.168.1.100 -p 6380
```

### 3. Example Session
```text
Connecting to 127.0.0.1:6379...
Connected to Redis!
127.0.0.1:6379> PING
PONG
127.0.0.1:6379> SET user:100 "Alice Smith"
OK
127.0.0.1:6379> GET user:100
Alice Smith
127.0.0.1:6379> RPUSH mylist "first" "second" "third"
3
127.0.0.1:6379> LRANGE mylist 0 -1
first
second
third
127.0.0.1:6379> quit
Goodbye!
```

---

## 🧭 Suggested Learning Path

1. Start with **[Part 1: Architecture & CLI Entry](part1.md)** to grasp the overall system decomposition.
2. Progress sequentially through **Part 2** to **Part 6**.
3. Inspect `tests.cpp` to see how low-level network protocol parsers are verified deterministically without external server dependencies using Unix `socketpair()`.
