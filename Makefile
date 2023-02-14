# Copyright (c) 2023 licktheroom

CC = clang
CFLAGS = -O2 -march=native -pipe -fomit-frame-pointer -Wall -Wextra -Wshadow \
		-Wdouble-promotion -fno-common -std=c11
CLIBS = -lxcb -lGL -lxcb -lX11 -lX11-xcb

files = main.o

all: ${files}
	mkdir -p build/
	${CC} ${CFLAGS} ${CLIBS} ${files} -o build/xcb-opengl
	rm -f *.o

main.o:
	${CC} ${CFLAGS} -c -o main.o src/main.c