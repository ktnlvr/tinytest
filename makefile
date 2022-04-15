compile:
	$(CXX) tinytest.c -ansi -std=c99 -o main -g -lelf -flto

testable:
	$(CC) testable/alwayspass.c testable/zerodiv.c -g -shared -o some.so -w -fPIC -I.

.PHONY: testable
