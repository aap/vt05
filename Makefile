all: vt05 vt52 dp3300
DEPS=`sdl2-config --libs --cflags` -lm -lpthread
vt05: vt05.c terminal.c vt05chars.h args.h
	cc -O3 -o vt05 vt05.c terminal.c $(DEPS)
vt52: vt52.c terminal.c vt52rom.h args.h
	cc -O3 -o vt52 vt52.c terminal.c $(DEPS)
dp3300: dp3300.c terminal.c dpchars.h args.h
	cc -O3 -o dp3300 dp3300.c terminal.c $(DEPS)
