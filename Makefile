all: vt05 vt52 dp3300
DEPS=`sdl2-config --libs --cflags` -lm -lpthread
vt05: vt05.c vt05chars.h args.h
	cc -o vt05 vt05.c keyboard.c $(DEPS)
vt52: vt52.c vt52rom.h args.h
	cc -g -o vt52 vt52.c keyboard.c $(DEPS)
dp3300: dp3300.c dpchars.h args.h
	cc -g -o dp3300 dp3300.c keyboard.c $(DEPS)
