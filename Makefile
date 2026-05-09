CXX := g++
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra -pedantic

TARGET := build/static_tree_bench.exe
SRC := src/main.cpp

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(SRC)
	@if not exist build mkdir build
	$(CXX) $(CXXFLAGS) $(SRC) -o $(TARGET)

run: $(TARGET)
	$(TARGET)

clean:
	@if exist build rmdir /s /q build
