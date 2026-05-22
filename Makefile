SDK := $(shell xcrun --sdk macosx --show-sdk-path)

SRCDIR := .
OBJDIR := build

INC_DIRS := /Users/dmitry/projects/clib/include
LIB_DIRS := /Users/dmitry/projects/clib/build
LIBS := clib

PHONY: $(OBJDIR)/corova $(OBJDIR)/coroutines

$(OBJDIR)/coroutines: $(OBJDIR)/coroutines.o
	ld -o $(OBJDIR)/coroutines $(OBJDIR)/coroutines.o \
			-lSystem \
			-syslibroot $(SDK) \
			-e _start \
			-arch arm64

$(OBJDIR)/coroutines.o: coroutines.s | $(OBJDIR)
	as -g -o $(OBJDIR)/coroutines.o coroutines.s

$(OBJDIR)/corova: coroutines.c | $(OBJDIR)
	cc -g -I$(INC_DIRS) -L$(LIB_DIRS) -o $(OBJDIR)/corova coroutines.c -lclib -Wl,-rpath,$(LIB_DIRS)

$(OBJDIR):
	mkdir -p $(OBJDIR)

clean:
	rm -r $(OBJDIR)
