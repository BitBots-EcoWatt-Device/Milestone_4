CXX = g++
CXXFLAGS = -std=c++11 -I.
LDFLAGS = -lcurl

SOURCES = ModbusHandler.cpp ProtocolAdapter.cpp Inverter.cpp Config.cpp PollingConfig.cpp compress.cpp

all: run tests

main: main.cpp $(SOURCES)
	$(CXX) $(CXXFLAGS) -o main main.cpp $(SOURCES) $(LDFLAGS)

tests: tests.cpp $(SOURCES)
	$(CXX) $(CXXFLAGS) -o tests tests.cpp $(SOURCES) $(LDFLAGS)

run: main
	./main

test: tests
	./tests

clean:
	rm -f main tests *.o
