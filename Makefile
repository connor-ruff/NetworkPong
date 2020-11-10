all: local

local:
	g++ pong3.cpp -lncurses -lpthread -o pong

clean:
	rm -rf pong
