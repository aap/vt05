all: vt05 vt52 dp3300 gecon dm2500 vt100/vt100
WARN=-Wall -Wno-unused-parameter -Wno-parentheses -Wno-unused-result
DEPS=`sdl2-config --libs --cflags` -lm -lpthread
vt05: vt05.c terminal.c vt05chars.h args.h
	cc $(WARN) -O3 -o vt05 vt05.c terminal.c $(DEPS)
vt52: vt52.c terminal.c vt52rom.h args.h
	cc $(WARN) -O3 -o vt52 vt52.c terminal.c $(DEPS)
dp3300: dp3300.c terminal.c dpchars.h args.h
	cc $(WARN) -O3 -o dp3300 dp3300.c terminal.c $(DEPS)
gecon: gecon.c terminal.c dpchars.h args.h
	cc $(WARN) -O3 -o $@ gecon.c terminal.c $(DEPS)
dm2500: dm2500.c terminal.c dmchars.h args.h
	cc $(WARN) -O3 -o dm2500 dm2500.c terminal.c $(DEPS)
vt100/vt100: vt100/main.c vt100/cpu.c vt100/sys.c vt100/ddt.c terminal.c \
	vt100/pusart.c vt100/brightness.c \
	vt100/nvr.c vt100/keyboard.c vt100/video.c vt100/event.c \
	vt100/logger.c vt100/time.c vt100/rom.c vt100/sound.c
	cc $(WARN) -g -I. -o $@ $^ $(DEPS)
	#cc $(WARN) -O3 -flto -I. -o $@ $^ $(DEPS)
	#clang $(WARN) -Ofast -I. -o $@ $^ $(DEPS)
