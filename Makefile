vt05: vt05.c vt05chars.h args.h
	cc -o vt05 `sdl-config --libs --cflags` vt05.c
