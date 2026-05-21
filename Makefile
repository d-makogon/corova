SDK := $(shell xcrun --sdk macosx --show-sdk-path)

SRCDIR := .
OBJDIR := build

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
	cc -o $(OBJDIR)/corova coroutines.c

$(OBJDIR):
	mkdir -p $(OBJDIR)

clean:
	rm -r $(OBJDIR)
