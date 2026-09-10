CXX = clang++
CXXFLAGS = -std=c++17 -Wall -Wextra -Iheaders -I.

BUILD_DIR = build
TARGET = $(BUILD_DIR)/redis_client
TEST_TARGET = $(BUILD_DIR)/test_runner

APP_SRCS = cli.cpp commandHandler.cpp redisClient.cpp ResponseParser.cpp main.cpp
APP_OBJS = $(patsubst %.cpp, $(BUILD_DIR)/%.o, $(APP_SRCS))

TEST_SRCS = cli.cpp commandHandler.cpp redisClient.cpp ResponseParser.cpp tests.cpp
TEST_OBJS = $(patsubst %.cpp, $(BUILD_DIR)/%.o, $(TEST_SRCS))

.PHONY: all clean rebuild run test

all: $(TARGET)

$(TARGET): $(APP_OBJS) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -o $@ $^

$(TEST_TARGET): $(TEST_OBJS) | $(BUILD_DIR)
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

test: $(TEST_TARGET)
	./$(TEST_TARGET)

