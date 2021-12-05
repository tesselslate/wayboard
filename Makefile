input-display:
	mkdir -p out
	gcc -Wall -Wextra -pedantic -o out/input-display input-display.c -lSDL2 -lX11 -lxcb

run: input-display
	./out/input-display
