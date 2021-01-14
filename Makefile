coro: coro.cc
	c++ coro.cc -o coro -no-pie -g -fsanitize=address,undefined
