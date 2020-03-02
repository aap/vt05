all: vt05 vt52 dp3300
vt05: vt05.c vt05chars.h args.h
	cc -o vt05 `sdl2-config --libs --cflags` vt05.c
vt52: vt52.c vt52rom.h args.h
	cc -g -o vt52 vt52.c `sdl2-config --libs --cflags` -lm -lpthread
dp3300: dp3300.c dpchars.h args.h
	cc -g -o dp3300 dp3300.c `sdl2-config --libs --cflags` -lm -lpthread
