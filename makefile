compile-and-run: testable compile
	./main ./some.so

compile:
	$(CC) tinytest.c -std=c99 -o main -g -lelf -flto -ldl

testable:
	$(CC) testable/alwayspass.c testable/zerodiv.c testable/illegal.c testable/abort.c testable/segfault.c -g -shared -o some.so -w -fPIC -I.

.PHONY: testable
