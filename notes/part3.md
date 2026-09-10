# Redis Client — Part 3: Interactive REPL & Command Tokenization

[← Previous: Part 2](part2.md) • [📖 Index](README.md) • [Next: Part 4: RESP Serialization →](part4.md)

---

## 1. Overview & Architectural Position

In Part 1, we implemented command-line argument parsing, and in Part 2, we built the raw TCP socket networking layer (`redisClient`). 

In **Part 3**, we implement the **User Interaction Layer**:
1. **The Interactive REPL (Read-Eval-Print Loop) in `CLI`**: Prompts the user, reads commands from standard input, handles built-in control commands (`quit`, `help`), and delegates execution.
2. **Command Tokenizer (`commandHandler::splitArgs`)**: Parses raw user input strings into separate tokens/arguments while correctly handling quoted arguments with spaces (e.g. `SET "user:1 name" "John Doe"`).

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
|   1. Connects to Redis server via redisClient.connectToServer()         |
|   2. Prints connected status & prompt: 127.0.0.1:6379>                  |
|   3. Reads lines with std::getline(std::cin, line)                      |
|   4. Filters empty lines, handles 'quit' & 'help'                       |
|   5. Invokes commandHandler::splitArgs(line)                            |
+------------------------------------+------------------------------------+
                                     |
                                     v
+-------------------------------------------------------------------------+
|                          commandHandler Class                           |
|   - Regex Tokenization: R"(("[^"]*"|\S+))"                              |
|   - Extracts unquoted words and quoted multi-word strings               |
|   - Strips leading/trailing quote characters                            |
|   - Returns: std::vector<std::string> of tokens                         |
+-------------------------------------------------------------------------+
```

---

## 2. Source Code Implementation

### A. Header: `commandHandler.h`
```cpp
#ifndef COMMAND_HANDLER_H
#define COMMAND_HANDLER_H

#include <string>
#include <vector>

class commandHandler {
public:
    // Splits input string into individual arguments, preserving quoted strings
    static std::vector<std::string> splitArgs(const std::string &input);
};

#endif
```

---

### B. Implementation: `commandHandler.cpp`
```cpp
#include "commandHandler.h"
#include <regex>
#include <string>
#include <vector>

std::vector<std::string> commandHandler::splitArgs(const std::string &input) {
    std::vector<std::string> tokens;

    // Regex explanation:
    //  "[^"]*" : Matches double-quoted strings (including spaces)
    //  |       : OR
    //  \S+     : Matches one or more non-whitespace characters
    std::regex rgx(R"(("[^"]*"|\S+))");

    auto words_begin = std::sregex_iterator(input.begin(), input.end(), rgx);
    auto words_end = std::sregex_iterator();

    for (auto it = words_begin; it != words_end; ++it) {
        std::string token = it->str();

        // Strip enclosing double quotes if present
        if (token.size() >= 2 && token.front() == '"' && token.back() == '"') {
            token = token.substr(1, token.size() - 2);
        }

        tokens.push_back(token);
    }

    return tokens;
}
```

---

### C. Header: `CLI.h`
```cpp
#ifndef CLI_H
#define CLI_H

#include <string>
#include "redisClient.h"
#include "commandHandler.h"

class CLI {
public:
    CLI(const std::string &host, int port);
    void run();

private:
    redisClient redisClient;
    std::string host;
    int port;
};

#endif
```

---

### D. Implementation: `cli.cpp`
```cpp
#include "CLI.h"
#include <cstddef>
#include <iostream>
#include <string>
#include <vector>

// Helper to trim leading and trailing whitespace
static std::string trim(const std::string &s) {
    size_t start = s.find_first_not_of(" \t\n\r\f\v");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\n\r\f\v");
    return s.substr(start, end - start + 1);
}

CLI::CLI(const std::string &host, int port)
    : redisClient(host, port), host(host), port(port) {}

void CLI::run() {
    // 1. Establish connection to Redis Server
    if (!redisClient.connectToServer()) {
        return;
    }

    std::cout << "Connected to Redis at " << host << ":" << port << "\n";

    // 2. Interactive REPL Loop
    while (true) {
        // Display prompt (e.g., 127.0.0.1:6379>)
        std::cout << host << ":" << port << "> ";
        std::cout.flush();

        std::string line;
        // Handle EOF (Ctrl+D) gracefully
        if (!std::getline(std::cin, line)) {
            std::cout << "\n";
            break;
        }

        line = trim(line);
        if (line.empty()) continue;

        // Built-in commands
        if (line == "quit" || line == "exit") {
            std::cout << "Goodbye.\n";
            break;
        }

        if (line == "help") {
            std::cout << "Available commands: Redis commands (e.g. GET, SET, PING), quit, help\n";
            continue;
        }

        // 3. Tokenize command arguments
        std::vector<std::string> args = commandHandler::splitArgs(line);
        if (args.empty()) continue;

        // For debugging / display in Part 3
        for (const auto &arg : args) {
            std::cout << arg << "\n";
        }
    }
}
```

---

## 3. Deep Dive & Core Concepts

### A. The REPL Pattern (Read-Eval-Print Loop)
A REPL operates continuously through four distinct stages:
1. **Read**: Capture raw keystrokes/line from `std::cin`.
2. **Eval (Tokenize & Execute)**: Parse tokens, convert to protocol format (RESP), transmit over socket, receive response.
3. **Print**: Format and print the result to `std::cout`.
4. **Loop**: Repeat until the user exits (`quit` / `EOF` / `SIGINT`).

```
+------------+       +------------+       +-------------+       +-------------+
|    READ    | ----> |  TOKENIZE  | ----> |  EVAL / IO  | ----> |    PRINT    |
| std::cin   |       | splitArgs  |       | redisClient |       | std::cout   |
+------------+       +------------+       +-------------+       +-------------+
      ^                                                                |
      +----------------------------------------------------------------+
```

---

### B. String Trimming Algorithm
Before processing, raw input lines often contain leading/trailing spaces or tabs.

```cpp
static std::string trim(const std::string &s) {
    size_t start = s.find_first_not_of(" \t\n\r\f\v");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\n\r\f\v");
    return s.substr(start, end - start + 1);
}
```

- **`s.find_first_not_of(...)`**: Finds the index of the first character that is **not** whitespace.
- **`start == std::string::npos`**: If every character is whitespace, the string is empty.
- **`s.find_last_not_of(...)`**: Finds the index of the last non-whitespace character.
- **`substr(start, length)`**: Slices the clean substring where `length = end - start + 1`.

---

### C. Regex Tokenization & Quoting Engine

In a Redis CLI, users enter commands with arguments:
```text
SET key value
SET "user message" "Hello, Redis World!"
HSET "user:100" name "Ada Lovelace" role "Developer"
```

A naive `stringstream >> word` approach fails because `"Hello, Redis World!"` would be split into three separate pieces (`"Hello,`, `Redis`, `World!"`).

#### The Regular Expression:
```cpp
std::regex rgx(R"(("[^"]*"|\S+))");
```

Breaking down the regex pattern:
1. **`R"( ... )"`**: C++11 Raw String Literal. Avoids backslash escaping hell (`"\\S+"` becomes `R"(\S+)"`).
2. **`"[^"]*"`**:
   - `"` : Starts with a double quote.
   - `[^"]*` : Zero or more characters that are **not** a double quote (allows spaces inside quotes).
   - `"` : Ends with a double quote.
3. **`|`**: Alternation operator (logical OR).
4. **`\S+`**: One or more non-whitespace characters (normal unquoted tokens).

---

### D. Iterating Matches with `std::sregex_iterator`

```cpp
auto words_begin = std::sregex_iterator(input.begin(), input.end(), rgx);
auto words_end = std::sregex_iterator();
```
- `std::sregex_iterator` is a forward iterator that searches the range `[begin, end)` for all successive regex matches.
- The default constructor `std::sregex_iterator()` creates a special **end-of-sequence** sentinel iterator.
- When incremented (`++it`), it automatically advances to the next match position in the string.

---

### E. Quote Stripping Logic

```cpp
if (token.size() >= 2 && token.front() == '"' && token.back() == '"') {
    token = token.substr(1, token.size() - 2);
}
```
- **Safety check**: Ensures length is at least 2 characters (e.g. `""`).
- **`token.front() == '"' && token.back() == '"'`**: Verifies that both the opening and closing characters are double quotes.
- **`substr(1, token.size() - 2)`**: Drops the first character (index 0) and the last character (index `size - 1`).
  - Example: `"hello world"` (size 13) &rarr; index 1 for length 11 &rarr; `hello world`.

---

## 4. Dry Run & Execution Traces

Let's step through exact execution flows for different inputs.

---

### Dry Run 1: Quoted String with Internal Spaces
**Input String**: `SET "user name" "Alex Smith"`

```
Step 1: Input String:
[S][E][T][ ]["][u][s][e][r][ ][n][a][m][e]["][ ]["][A][l][e][x][ ][S][m][i][t][h]["]
 0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24 25 26 27
```

| Iteration | Matched Substring (`it->str()`) | Matched Regex Branch | Quote Check (`front=='"' && back=='"'`) | Stripped Token | `tokens` Vector State |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **Match #1** | `"SET"` | `\S+` | False | `SET` | `["SET"]` |
| **Match #2** | `"\"user name\""` | `"[^"]*"` | True (size 11) | `user name` | `["SET", "user name"]` |
| **Match #3** | `"\"Alex Smith\""` | `"[^"]*"` | True (size 12) | `Alex Smith` | `["SET", "user name", "Alex Smith"]` |

**Final Return**:
- `tokens[0] = "SET"`
- `tokens[1] = "user name"`
- `tokens[2] = "Alex Smith"`

---

### Dry Run 2: Mixed Multiple Whitespace & Unquoted Args
**Input String**: `  HSET    user:101   age   30  `

1. **`trim(line)`**:
   - `start` index = 2 (first non-space `'H'`)
   - `end` index = 24 (last non-space `'0'`)
   - Trimmed string = `"HSET    user:101   age   30"`

2. **Regex Iteration**:

| Iteration | Matched Substring | Matched Branch | Stripped Token | `tokens` Vector |
| :--- | :--- | :--- | :--- | :--- |
| **#1** | `"HSET"` | `\S+` | `HSET` | `["HSET"]` |
| **#2** | `"user:101"` | `\S+` | `user:101` | `["HSET", "user:101"]` |
| **#3** | `"age"` | `\S+` | `age` | `["HSET", "user:101", "age"]` |
| **#4** | `"30"` | `\S+` | `30` | `["HSET", "user:101", "age", "30"]` |

---

### Dry Run 3: Empty Quoted String
**Input String**: `SET empty ""`

| Iteration | Matched Substring | Matched Branch | Quote Stripping | Resulting Token |
| :--- | :--- | :--- | :--- | :--- |
| **#1** | `"SET"` | `\S+` | No | `SET` |
| **#2** | `"empty"` | `\S+` | No | `empty` |
| **#3** | `""` | `"[^"]*"` | Yes (`substr(1, 0)`) | `""` (empty string) |

---

## 5. Common Bugs & Pitfalls to Avoid

### 1. The `S+` vs `\S+` Bug
```cpp
// ❌ BUG: Matches only the capital letter 'S'
std::regex rgx(R"(("[^"]*"|S+))");

// ✅ FIX: \S matches any non-whitespace character
std::regex rgx(R"(("[^"]*"|\S+))");
```
*Symptom*: Commands like `SET key val` only matched the letter `S`, silently dropping `ET`, `key`, and `val`.

---

### 2. The `+` vs `*` in Quoted Match
```cpp
// ❌ BUG: Fails on empty quotes ""
std::regex rgx(R"(("[^"]+"|\S+))");

// ✅ FIX: Allows empty string between quotes
std::regex rgx(R"(("[^"]*"|\S+))");
```

---

### 3. Makefile Incremental Compilation
When modifying header files (`.h`) or adding new source files (`commandHandler.cpp`), ensure the `Makefile` compiles and links all translation units:
```makefile
SRCS = $(wildcard *.cpp)
OBJS = $(patsubst %.cpp, $(BUILD_DIR)/%.o, $(SRCS))
```
Always run `make` before executing `./build/redis_client` to ensure changes are compiled into the binary.

---

## 6. Verification & Interactive Testing

### Compilation
```bash
make
```

### Running the Client
```bash
./build/redis_client
```

### Example Session
```text
Connected to Redis at 127.0.0.1:6379
127.0.0.1:6379> help
Available commands: Redis commands (e.g. GET, SET, PING), quit, help
127.0.0.1:6379> SET "user name" "Ada Lovelace"
SET
user name
Ada Lovelace
127.0.0.1:6379> HSET myhash field1 "hello world"
HSET
myhash
field1
hello world
127.0.0.1:6379> quit
Goodbye.
```

---

## 7. Next Step: Part 4 Preview

In **Part 4**, we will connect `commandHandler` to the **RESP (REdis Serialization Protocol) Serializer**:
- Converting `std::vector<std::string>` tokens into RESP array commands (e.g., `["SET", "k", "v"]` &rarr; `*3\r\n$3\r\nSET\r\n$1\r\nk\r\n$1\r\nv\r\n`).
- Sending the serialized byte stream across `redisClient`'s socket.

---

[← Previous: Part 2 — TCP Socket Networking & Connection Layer](part2.md) • [📖 Index](README.md) • [Next: Part 4 — RESP Command Serialization & Network Dispatch →](part4.md)

