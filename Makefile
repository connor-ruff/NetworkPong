all: local

local:
	g++ netpong.cpp -lncurses -lpthread -o pong

clean:
	rm -rf pong
