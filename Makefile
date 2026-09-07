CXX = clang++
CXXFLAGS = -std=c++17 -Wall -Wextra -I.

BUILD_DIR = build
TARGET = $(BUILD_DIR)/redis_client

SRCS = $(wildcard *.cpp)
OBJS = $(patsubst %.cpp, $(BUILD_DIR)/%.o, $(SRCS))

.PHONY: all clean rebuild run

all: $(TARGET)

$(TARGET): $(OBJS) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -o $@ $^

$(BUILD_DIR)/%.o: %.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

clean:
	rm -rf $(BUILD_DIR) *.o redis_client

rebuild: clean all

run: $(TARGET)
	./$(TARGET)
