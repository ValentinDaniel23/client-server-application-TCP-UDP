CXX = g++
CXXFLAGS = -Wall -g -Werror -Wno-error=unused-variable

all: server subscriber

common.o: common.cpp common.h
	$(CXX) $(CXXFLAGS) -c common.cpp -o common.o

server: server.cpp common.o
	$(CXX) $(CXXFLAGS) server.cpp common.o -o server

subscriber: subscriber.cpp common.o
	$(CXX) $(CXXFLAGS) subscriber.cpp common.o -o subscriber

clean:
	rm -rf server client *.o
