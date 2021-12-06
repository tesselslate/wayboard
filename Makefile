# debug build
input-display:
	mkdir -p out
	gcc -fanalyzer -fsanitize=undefined -Wall -Wextra -pedantic -o out/input-display input-display.c -lSDL2 -lX11 -lxcb

# optimized build
build-optimized:
	mkdir -p out
	gcc -Os -Wall -Wextra -pedantic -o out/input-display input-display.c -lSDL2 -lX11 -lxcb

install: build-optimized
	mkdir -p ${DESTDIR}/bin
	cp out/input-display ${DESTDIR}/bin
	chmod 755 ${DESTDIR}/bin/input-display
