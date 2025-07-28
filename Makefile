
build:
	mkdir dist || true
	gcc -o dist/conlang src/main.c

hello_world:
	$(MAKE) build
	./dist/conlang examples/hello_world.con

