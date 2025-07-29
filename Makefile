LLVM_CONFIG = /opt/homebrew/opt/llvm/bin/llvm-config
CFLAGS = $(shell $(LLVM_CONFIG) --cflags)
LDFLAGS = $(shell $(LLVM_CONFIG) --ldflags)
LIBS = $(shell $(LLVM_CONFIG) --libs)

build:
	mkdir -p dist
	clang $(CFLAGS) $(LDFLAGS) -o dist/conlang src/main.c $(LIBS)	

hello_world:
	$(MAKE) build
	./dist/conlang examples/hello_world.con

