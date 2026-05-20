SDK := $(shell xcrun --sdk macosx --show-sdk-path)

coroutines: coroutines.o
	ld -o coroutines coroutines.o \
			-lSystem \
			-syslibroot $(SDK) \
			-e _start \
			-arch arm64

coroutines.o: coroutines.s
	as -g -o coroutines.o coroutines.s

clean:
	rm -f coroutines coroutines.o
