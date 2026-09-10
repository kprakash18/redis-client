# Redis Client — Part 1: Architecture & CLI Entry

[📖 Index](README.md) • **Part 1** • [Part 2: Socket Networking & Connection →](part2.md)

---

## 1. Project Overview & Architecture Roadmap

The project aims to build a custom lightweight **Redis CLI Client** in C++ from scratch. The system is broken down into structured components:

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

### Core Components
1. **`main.cpp` Entry Point**: Parses command-line arguments, sets default parameters, and initializes the application.
2. **`RedisClient` (Networking Layer)**: Manages raw TCP connections using Berkeley Sockets (`socket`, `connect`, `getaddrinfo`) supporting both IPv4 and IPv6.
3. **`CommandHandler` (RESP Serializer)**: Converts user strings into RESP (REdis Serialization Protocol) format (e.g., `*3\r\n$3\r\nSET\r\n...`).
4. **`ResponseParser` (RESP Deserializer)**: Reads and deserializes server data types:
   - `+` Simple Strings (e.g., `+OK\r\n`)
   - `-` Errors (e.g., `-ERR unknown command\r\n`)
   - `:` Integers (e.g., `:100\r\n`)
   - `$` Bulk Strings (e.g., `$5\r\nhello\r\n`)
   - `*` Arrays (e.g., `*2\r\n$3\r\nfoo\r\n$3\r\nbar\r\n`)
5. **`CLI` (REPL Loop)**: Interactive terminal prompt for typing commands, executing them via `RedisClient`, and printing colored/formatted outputs.

---

## 2. Deep Dive: `main.cpp` Implementation

### Source Code
```cpp
#include <string>

int main(int argc, char* argv[]){
    std::string host = "127.0.0.1";
    int port = 6379;
    int i = 1;

    // parse command-line args for -h and -p
    while(i < argc){
        std::string arg = argv[i];
        if(arg == "-h" && i+1 < argc){ // -h 127.0.0.1
            host = argv[++i];
        }else if(arg == "-p" && i+1 < argc){
            port = std::stoi(argv[++i]);
        }else{
            break;
        }
        ++i;
    }
}
```

---

## 3. Code Analysis & Breakdown

### A. Program Signature & Parameters
```cpp
int main(int argc, char* argv[])
```
- **`argc` (Argument Count)**: Number of whitespace-separated CLI arguments (including program name).
- **`argv` (Argument Vector)**: Array of pointers to C-strings. `argv[0]` is the program binary name.

### B. Configuration Defaults
```cpp
std::string host = "127.0.0.1";  // Default Redis server IP (localhost)
int port = 6379;                // Standard Redis TCP port
int i = 1;                      // Pointer starts at index 1 to skip argv[0]
```

### C. The Parsing Loop
The `while(i < argc)` loop iterates through tokens dynamically:
1. **Flag Detection**:
   - `-h`: Host parameter.
   - `-p`: Port parameter.
2. **Bounds Checking (`i + 1 < argc`)**:
   - Ensures an argument value exists immediately after the flag to prevent buffer overrun / segmentation fault.
3. **Value Consumption (`argv[++i]`)**:
   - Pre-increments `i` to consume the value immediately, ensuring it is not mistakenly evaluated as a flag on the next iteration.
   - Converts port string to integer using `std::stoi`.
4. **Early Termination (`else { break; }`)**:
   - Gracefully stops parsing on unrecognized flags or incomplete inputs without crashing.

---

## 4. Execution Flow Examples

### Case 1: Default Invocation
```bash
./redis-client
```
- `argc = 1` &rarr; loop is skipped.
- **State**: `host = "127.0.0.1"`, `port = 6379`.

### Case 2: Custom Host & Port
```bash
./redis-client -h 192.168.1.100 -p 6380
```
- `argc = 5`
  - `i = 1`: `-h` detected &rarr; `host` set to `argv[2]` (`"192.168.1.100"`), `i` becomes `2`.
  - `i = 3`: `-p` detected &rarr; `port` set to `std::stoi(argv[4])` (`6380`), `i` becomes `4`.
  - `i = 5`: Exits loop.
- **State**: `host = "192.168.1.100"`, `port = 6380`.

### Case 3: Missing Flag Argument (Safe Exit)
```bash
./redis-client -h
```
- `argc = 2`
  - `i = 1`, `arg = "-h"`.
  - Condition `i + 1 < argc` evaluates to `2 < 2` (`false`).
  - Hits `else { break; }` and safely terminates argument parsing.

## 5. Comprehensive Dry Run & Variable Trace

Let's perform a step-by-step dry run tracking memory states and variable mutations across iterations.

### Dry Run Test Case: `./redis-client -h 192.168.1.1 -p 8000`

#### Initial Memory & Stack Setup
| Variable | Initial Value | Notes |
| :--- | :--- | :--- |
| `argc` | `5` | Total count of arguments |
| `argv[0]` | `"./redis-client"` | Program name |
| `argv[1]` | `"-h"` | First argument |
| `argv[2]` | `"192.168.1.1"` | Second argument |
| `argv[3]` | `"-p"` | Third argument |
| `argv[4]` | `"8000"` | Fourth argument |
| `host` | `"127.0.0.1"` | Default value |
| `port` | `6379` | Default value |
| `i` | `1` | Starts at first argument after binary |

---

#### Step-by-Step Execution Trace Table

| Step | `i` (start) | `i < argc`? | `arg = argv[i]` | Condition Evaluated | Branch Taken | Action Performed | Variable Changes | `++i` (end) | `i` (next) |
|:---|:---|:---|:---|:---|:---|:---|:---|:---|:---|
| **1** | `1` | `1 < 5` (**True**) | `"-h"` | `arg == "-h" && (1+1 < 5)` &rarr; **True** | `if(arg == "-h" ...)` | `host = argv[++i]` | `i` becomes `2`<br>`host = "192.168.1.1"` | `++i` | `3` |
| **2** | `3` | `3 < 5` (**True**) | `"-p"` | `arg == "-p" && (3+1 < 5)` &rarr; **True** | `else if(arg == "-p" ...)` | `port = std::stoi(argv[++i])` | `i` becomes `4`<br>`port = 8000` | `++i` | `5` |
| **3** | `5` | `5 < 5` (**False**) | — | — | **Loop Exits** | — | Final: `host="192.168.1.1"`, `port=8000` | — | `5` |

---

### Edge Case Dry Run: Incomplete Flag `./redis-client -h`

| Step | `i` | `i < argc`? | `arg` | Condition Checked | Evaluation | Branch | Outcome |
|:---|:---|:---|:---|:---|:---|:---|:---|
| **1** | `1` | `1 < 2` (**True**) | `"-h"` | `arg == "-h" && (1+1 < 2)` | `true && false` &rarr; **False** | `else` | `break;` triggered. Exits safely with defaults (`host="127.0.0.1"`, `port=6379`). |

---

## 6. Next Steps for Development
1. **Socket Connection**: Implement `socket()`, `getaddrinfo()`, and `connect()` to establish a TCP stream with `host:port`.
2. **RESP Formatter**: Write helper functions to serialize user inputs into Redis RESP protocol strings.
3. **REPL Loop**: Set up a readline / input loop with `std::getline(std::cin, input)`.

---

[📖 Index](README.md) • [Next: Part 2 — Socket Networking & Connection →](part2.md)


