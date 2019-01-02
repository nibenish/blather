CXX = gcc -g
LDFLAGS =
CXXFLAGS = -o
OBJS = bl-client bl-server bl-showlog
FILES = bl-server.c bl-client.c bl-showlog.c blather.h simpio.c
DEPS = blather.h simpio.c util.c -lpthread server.c

all : $(FILES)
	$(CXX) $(CXXFLAGS) bl-server bl-server.c $(DEPS)
	$(CXX) $(CXXFLAGS) bl-client bl-client.c $(DEPS)
	$(CXX) $(CXXFLAGS) bl-showlog bl-showlog.c $(DEPS)

clean :
	rm -f *.o $(OBJS) *.fifo

bl-showlog : bl-showlog.c blather.h
	$(CXX) $(CXXFLAGS) bl-showlog $< $(DEPS)

bl-server : bl-server.c blather.h
	$(CXX) $(LDFLAGS) $(CXXFLAGS) bl-server $< $(DEPS)

bl-client : bl-client.c blather.h simpio.c
	$(CXX) $(LDFLAGS) $(CXXFLAGS) bl-client $< $(DEPS)

shell-tests : shell_tests.sh shell_tests_data.sh cat-sig.sh clean-tests
	chmod u+rx shell_tests.sh shell_tests_data.sh normalize.awk filter-semopen-bug.awk cat-sig.sh
	./shell_tests.sh

clean-tests :
	rm -f test-*.log test-*.out test-*.expect test-*.diff test-*.valgrindout
