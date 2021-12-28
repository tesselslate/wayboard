# configuration
DESTINATION = /usr/local
CC          = cc
LD_FLAGS    = -lconfig -lSDL2 -lSDL2_ttf -lX11 -lxcb -lm

# configuration (cflags)
#CFLAGS      = -fsanitize=undefined -g -O0 -Wall -Wextra -Werror -pedantic
CFLAGS       = -Os -Wall -Wextra -Werror -pedantic

# tasks
input-display:
	mkdir -p out
	${CC} ${CFLAGS} -o out/input-display input-display.c ${LD_FLAGS}

clean:
	rm -f out/input-display

install: input-display
	mkdir -p ${DESTINATION}/bin
	cp out/input-display ${DESTINATION}/bin
	chmod 755 ${DESTINATION}/bin/input-display

uninstall:
	rm -f ${DESTINATION}/bin/input-display
