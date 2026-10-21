CXX ?= clang++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -I/opt/homebrew/include
LDFLAGS = -lpthread

MODULES = modules/Models.hpp modules/Storage.hpp modules/ActivityLog.hpp modules/TaskManager.hpp modules/ProjectManager.hpp modules/SmartPlanner.hpp modules/WorkloadAnalyzer.hpp modules/Rescheduler.hpp
TARGET = taskflow

all: $(TARGET)

$(TARGET): main.cpp $(MODULES)
	$(CXX) $(CXXFLAGS) -o $(TARGET) main.cpp $(LDFLAGS)

clean:
	rm -f $(TARGET)
	rm -rf *.dSYM

run: $(TARGET)
	./$(TARGET)

.PHONY: all clean run
