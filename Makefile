CXX       = g++
CXX_STD   = -std=c++17
CXX_WARN  = -Wall -Wextra -Wpedantic
CXX_DEBUG = -ggdb3 -DDEBUG -Og
CXX_SAN   = -fsanitize=address,leak,undefined
LIBS      = -pthread -lboost_program_options
CXX_NODB  = $(CXX_STD) $(CXX_WARN) -O3 $(LIBS)
CXX_NOSAN = $(CXX_STD) $(CXX_WARN) $(CXX_DEBUG) $(LIBS)
CXX_FLAGS = $(CXX_NOSAN) $(CXX_SAN)
LDFLAGS   = $(CXXFLAGS)
OBJ       = $(SRC:.cc=.o)

all:  cache_server test_cache_store test_cache_client benchmark

benchmark: benchmark.o cache_client.o
	$(CXX) $(LDFLAGS) -o $@ $^ $(LIBS)

cache_server: cache_server.o cache_store.o fifo_evictor.o
	$(CXX) $(LDFLAGS) -o $@ $^ $(LIBS)

test_cache_store: test_cache_store.o cache_store.o catch_main.o fifo_evictor.o
	$(CXX) $(LDFLAGS) -o $@ $^ $(LIBS)

test_cache_client: test_cache_client.o cache_client.o catch_main.o fifo_evictor.o
	$(CXX) $(LDFLAGS) -o $@ $^ $(LIBS)

%.o: %.cc %.hh
	$(CXX) $(CXXFLAGS) $(OPTFLAGS) -c -o $@ $<

clean:
	rm -rf *.o benchmark cache_server test_cache_store test_cache_client benchmark_ cache_server_

benchmark_: benchmark.cc cache_client.cc
	$(CXX) $(CXX_NODB) -o $@ $^

cache_server_: cache_server.cc cache_store.cc fifo_evictor.cc
	$(CXX) $(CXX_NODB) -o $@ $^

run: cache_server_ benchmark_
	./cache_server_ &
	./benchmark_

valgrind: all
	valgrind --leak-check=full --show-leak-kinds=all ./test_cache_store
	valgrind --leak-check=full --show-leak-kinds=all ./benchmark
