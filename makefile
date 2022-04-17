compile:
	$(CXX) tinytest.c -ansi -std=c99 -o main -g -lelf -flto -ldl

testable:
	$(CC) testable/alwayspass.c testable/zerodiv.c testable/segfault.c -g -shared -o some.so -w -fPIC -I.

.PHONY: testable
