
LLVM_CONFIG = /opt/homebrew/opt/llvm/bin/llvm-config
CFLAGS = $(shell $(LLVM_CONFIG) --cflags)
LDFLAGS = $(shell $(LLVM_CONFIG) --ldflags)
LIBS = $(shell $(LLVM_CONFIG) --libs)

build:
	mkdir dist || true
	clang $(CFLAGS) $(LDFLAGS) -o dist/conlang src/main.c $(LIBS)
	./dist/conlang examples/hello_world.con # This will generate output.ll
	$(shell $(LLVM_CONFIG) --bindir)/llc -filetype=obj output.ll -o output.o
	clang output.o -o dist/conlang $(LDFLAGS) $(LIBS)


hello_world:
	$(MAKE) build
	./dist/conlang examples/hello_world.con

