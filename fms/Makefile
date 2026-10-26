CC       = gcc
CFLAGS   = -std=c11 -Wall -Wextra -Wpedantic -g -Iinclude
LDFLAGS  = -pthread
LIBSRC   = src/util.c src/logger.c src/hashtable.c src/system.c src/auth.c \
           src/flight.c src/booking.c src/report.c src/worker_pool.c
LIBOBJ   = $(patsubst src/%.c,build/%.o,$(LIBSRC))

all: build/fms

build:
	mkdir -p build

build/%.o: src/%.c include/fms.h | build
	$(CC) $(CFLAGS) -c $< -o $@

build/fms: $(LIBOBJ) build/main.o
	$(CC) $^ -o $@ $(LDFLAGS)

build/test_fms: $(LIBOBJ) tests/test_fms.c
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS) -lcunit

build/bench: $(LIBOBJ) tests/bench.c
	$(CC) $(CFLAGS) -O2 $^ -o $@ $(LDFLAGS)

build/pool_demo: $(LIBOBJ) tests/pool_demo.c
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

run: build/fms
	./build/fms

test: build/test_fms
	@mkdir -p reports
	-./build/test_fms 2>&1 | tee reports/gtest-cunit-report.txt

bench: build/bench
	./build/bench

pool_demo: build/pool_demo
	./build/pool_demo

# memory errors and leaks (must report 0 errors)
valgrind: build/test_fms
	@mkdir -p reports
	valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes --error-exitcode=1 ./build/test_fms 2>&1 | tee reports/valgrind-report.txt

# data races in the multithreaded booking test
helgrind: build/test_fms build/pool_demo
	valgrind --tool=helgrind --error-exitcode=1 ./build/test_fms
	valgrind --tool=helgrind --error-exitcode=1 ./build/pool_demo

# static analysis (add --addon=misra if you have the MISRA rule text file)
cppcheck:
	@mkdir -p reports
	-cppcheck --enable=warning,style,performance,portability --inconclusive \
	         --suppress=missingIncludeSystem -Iinclude src 2>&1 | tee reports/cppcheck-report.txt

# code coverage with gcov (function + branch coverage)
coverage:
	rm -rf build/cov && mkdir -p build/cov
	$(CC) $(CFLAGS) -O0 --coverage $(LIBSRC) tests/test_fms.c -o build/cov/test_cov $(LDFLAGS) -lcunit
	cd build/cov && ./test_cov > /dev/null
	cd build/cov && gcov -b -c test_cov-*.gcda | grep -A4 "File 'src/"

clean:
	rm -rf build fms.log *.gcda *.gcno *.gcov
# Google Test (GTest) tests
GTEST_SRC = tests/test_fms_gtest.cpp

build/test_fms_gtest: $(LIBOBJ) $(GTEST_SRC) | build
	g++ -std=c++17 -Wall -Wextra -Iinclude $(GTEST_SRC) $(LIBOBJ) \
		-o $@ -lgtest_main -lgtest -pthread

gtest: build/test_fms_gtest
	@mkdir -p reports
	./build/test_fms_gtest 2>&1 | tee reports/gtest-report.txt

# Google Test code coverage
coverage-gtest:
	rm -rf build/cov
	mkdir -p build/cov reports
	for f in $(LIBSRC); do \
		obj=build/cov/$$(basename $${f%.c}).o; \
		$(CC) $(CFLAGS) -O0 -g --coverage -c $$f -o $$obj || exit 1; \
	done
	g++ -std=c++17 -Wall -Wextra -Iinclude -O0 -g --coverage \
		tests/test_fms_gtest.cpp build/cov/*.o \
		-o build/cov/test_fms_gtest -lgtest_main -lgtest -pthread
	./build/cov/test_fms_gtest
	gcovr -r . --object-directory build/cov \
		--filter 'src/.*\.c$$' \
		--txt-summary \
		--html-details reports/gtest-coverage.html
