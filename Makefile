all: vt05 dp3300 fonttest
vt05: vt05.c vt05chars.h args.h
	cc -o vt05 `sdl2-config --libs --cflags` vt05.c
dp3300: dp3300.c dpchars.h args.h
	cc -g -o dp3300 dp3300.c `sdl2-config --libs --cflags` -lm -lpthread
