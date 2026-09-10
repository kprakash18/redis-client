# ⚡ redis-client

> A lightweight, zero-dependency **Redis CLI client and RESP 2 protocol parser** built from scratch in modern C++ (C++17) using POSIX sockets.

[![C++17](https://img.shields.io/badge/Language-C%2B%2B17-blue.svg)](https://en.cppreference.com/w/cpp/17)
[![Build Status](https://img.shields.io/badge/Build-Passing-brightgreen.svg)](#-quick-start)
[![Tests](https://img.shields.io/badge/Tests-Unit%20Tests%20Passing-success.svg)](#-running-tests)
[![License](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)
[![Notes](https://img.shields.io/badge/Documentation-6--Part%20Curriculum-orange.svg)](notes/README.md)

---

## ✨ Features

- **Zero External Dependencies**: Built entirely with POSIX networking primitives (`<sys/socket.h>`, `<netdb.h>`) and C++ Standard Library.
- **Full RESP 2 Protocol Support**:
  - `+` Simple Strings (`+OK\r\n`)
  - `-` Simple Errors (`-ERR ...\r\n`)
  - `:` 64-bit Signed Integers (`:1000\r\n`)
  - `$` Binary-safe Bulk Strings & Null bulk strings (`$5\r\nhello\r\n`, `$-1\r\n`)
  - `*` Recursive Multi-Bulk Arrays & Null arrays (`*2\r\n...`, `*-1\r\n`)
- **Interactive REPL Engine**:
  - Configurable target host and port via CLI flags (`-h`, `-p`).
  - Quote-aware argument tokenization (supports multi-word strings like `SET user "Ada Lovelace"`).
  - Built-in commands (`quit`, `exit`, `help`).
  - Signal-safe and graceful error handling on disconnects.
- **One-Shot Mode**: Execute standalone commands directly from the shell:
  ```bash
  ./build/redis_client -h 127.0.0.1 -p 6379 PING
  ```
- **Automated Unit Testing Suite**: Protocol serialization and deserialization are verified deterministically with Unix `socketpair()` without requiring a running Redis instance.

---

## 🏗️ Architecture

```
+-----------------------------------------------------------------------------------------+
|                                    CLI Layer (cli.cpp)                                  |
|   1. Tokenizes input line: "LRANGE mylist 0 -1"                                         |
|   2. commandHandler::buildRESPcommand() -> "*4\r\n$6\r\nLRANGE\r\n$6\r\nmylist\r\n..."  |
|   3. redisClient::sendCommand() writes buffer to TCP socket                             |
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
```

---

## 📚 Step-by-Step Curriculum / Notes

Accompanying this project is a detailed **6-part technical engineering curriculum** explaining the system from first principles:

| Part | Guide | Key Topics |
| :---: | :--- | :--- |
| **01** | [Architecture & CLI Entry](notes/part1.md) | Entry point design, `argc`/`argv` parsing, bounds checking, defaults |
| **02** | [Socket Networking & Connection](notes/part2.md) | Berkeley sockets, `getaddrinfo`, IPv4/IPv6 support, connection lifecycle |
| **03** | [Interactive REPL & Tokenization](notes/part3.md) | REPL prompt loop, quote-aware string splitting, whitespace trimming |
| **04** | [RESP Command Serialization](notes/part4.md) | RESP 2 specification, bulk string serialization, `send()` byte dispatch |
| **05** | [Initial Response Deserialization](notes/part5.md) | Delimiter parsing (`\r\n`), Simple Strings (`+`), stream desync pitfalls |
| **06** | [Full RESP Deserializer & REPL Pipeline](notes/part6.md) | Integers, Errors, Bulk Strings, recursive Arrays, unit tests with `socketpair()` |

👉 **Read the full guide index in [`notes/README.md`](notes/README.md).**

---

## 🚀 Quick Start

### Prerequisites
- C++17 compatible compiler (`clang++` or `g++`)
- `make`
- Redis server (optional for client interactive testing, unit tests require no server)

### 1. Build & Run Tests
```bash
# Clone the repository
git clone https://github.com/your-username/redis-client.git
cd redis-client

# Run the test suite
make test

# Build the client executable
make
```

### 2. Interactive CLI Mode
```bash
# Connect to local Redis (127.0.0.1:6379)
./build/redis_client

# Connect to custom host and port
./build/redis_client -h 192.168.1.50 -p 6380
```

### 3. One-Shot Command Mode
```bash
./build/redis_client -h 127.0.0.1 -p 6379 PING
# Output: PONG

./build/redis_client SET greeting "Hello Redis"
# Output: OK

./build/redis_client GET greeting
# Output: Hello Redis
```

---

## 🧪 Running Tests

The test suite exercises tokenization, serialization, and deserialization using Unix domain `socketpair()`:

```bash
make test
```

Expected output:
```text
=== Running Redis Client Unit Tests ===
[TEST] commandHandler::splitArgs... PASSED
[TEST] commandHandler::buildRESPcommand... PASSED
[TEST] ResponseParser... PASSED
=== All Unit Tests Passed! ===
```

---

## 📂 Project Structure

```text
redis-client/
├── headers/                        # Header declarations
│   ├── CLI.h                       # REPL prompt loop & coordinator
│   ├── commandHandler.h            # Tokenizer & RESP command serializer
│   ├── redisClient.h               # Berkeley TCP socket networking layer
│   └── ResponseParser.h            # RESP 2 response deserializer
├── cli.cpp                         # Interactive REPL implementation
├── commandHandler.cpp              # Tokenizer & serializer implementation
├── redisClient.cpp                 # Socket networking implementation
├── ResponseParser.cpp              # Response deserializer implementation
├── main.cpp                        # Entry point & CLI argument parsing (-h, -p)
├── tests.cpp                       # Automated unit tests using socketpair()
├── Makefile                        # Build & test automation
├── notes/                          # 6-Part technical deep-dive curriculum
│   ├── README.md                   # Notes master index
│   └── part1.md ... part6.md       # Chapters 1 through 6
└── LICENSE                         # MIT License
```

---

## 🙏 Acknowledgments

This project and curriculum were built following the excellent tutorial series **"Building a Redis Client from Scratch"** by [**Dev w/Sel**](https://www.youtube.com/@Dev_with_Sel). Special thanks to Sel for providing such clear, high-quality systems programming content!


