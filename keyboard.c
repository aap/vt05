#include <SDL.h>
#include <unistd.h>
#include "terminal.h"

char **scancodemap;

/* Map SDL scancodes to ASCII */
char *scancodemap_both[SDL_NUM_SCANCODES] = {
	[SDL_SCANCODE_ESCAPE] = "\033\033",

	[SDL_SCANCODE_GRAVE] = "`~",
	[SDL_SCANCODE_1] = "1!",
	[SDL_SCANCODE_2] = "2@",
	[SDL_SCANCODE_3] = "3#",
	[SDL_SCANCODE_4] = "4$",
	[SDL_SCANCODE_5] = "5%",
	[SDL_SCANCODE_6] = "6^",
	[SDL_SCANCODE_7] = "7&",
	[SDL_SCANCODE_8] = "8*",
	[SDL_SCANCODE_9] = "9(",
	[SDL_SCANCODE_0] = "0)",
	[SDL_SCANCODE_MINUS] = "-_",
	[SDL_SCANCODE_EQUALS] = "=+",
	[SDL_SCANCODE_BACKSPACE] = "\b\b",
	[SDL_SCANCODE_DELETE] = "\177\177",

	[SDL_SCANCODE_TAB] = "\011\011",
	[SDL_SCANCODE_Q] = "qQ",
	[SDL_SCANCODE_W] = "wW",
	[SDL_SCANCODE_E] = "eE",
	[SDL_SCANCODE_R] = "rR",
	[SDL_SCANCODE_T] = "tT",
	[SDL_SCANCODE_Y] = "yY",
	[SDL_SCANCODE_U] = "uU",
	[SDL_SCANCODE_I] = "iI",
	[SDL_SCANCODE_O] = "oO",
	[SDL_SCANCODE_P] = "pP",
	[SDL_SCANCODE_LEFTBRACKET] = "[{",
	[SDL_SCANCODE_RIGHTBRACKET] = "]}",
	[SDL_SCANCODE_BACKSLASH] = "\\|",

	[SDL_SCANCODE_A] = "aA",
	[SDL_SCANCODE_S] = "sS",
	[SDL_SCANCODE_D] = "dD",
	[SDL_SCANCODE_F] = "fF",
	[SDL_SCANCODE_G] = "gG",
	[SDL_SCANCODE_H] = "hH",
	[SDL_SCANCODE_J] = "jJ",
	[SDL_SCANCODE_K] = "kK",
	[SDL_SCANCODE_L] = "lL",
	[SDL_SCANCODE_SEMICOLON] = ";:",
	[SDL_SCANCODE_APOSTROPHE] = "'\"",
	[SDL_SCANCODE_RETURN] = "\015\015",

	[SDL_SCANCODE_Z] = "zZ",
	[SDL_SCANCODE_X] = "xX",
	[SDL_SCANCODE_C] = "cC",
	[SDL_SCANCODE_V] = "vV",
	[SDL_SCANCODE_B] = "bB",
	[SDL_SCANCODE_N] = "nN",
	[SDL_SCANCODE_M] = "mM",
	[SDL_SCANCODE_COMMA] = ",<",
	[SDL_SCANCODE_PERIOD] = ".>",
	[SDL_SCANCODE_SLASH] = "/?",
	[SDL_SCANCODE_SPACE] = "  ",

	[SDL_SCANCODE_LALT] = "\033\033",
	[SDL_SCANCODE_RALT] = "\033\033",
};

char *scancodemap_upper[SDL_NUM_SCANCODES] = {
	[SDL_SCANCODE_ESCAPE] = "\033\033",

	[SDL_SCANCODE_GRAVE] = "\033\033",
	[SDL_SCANCODE_1] = "1!",
	[SDL_SCANCODE_2] = "2@",
	[SDL_SCANCODE_3] = "3#",
	[SDL_SCANCODE_4] = "4$",
	[SDL_SCANCODE_5] = "5%",
	[SDL_SCANCODE_6] = "6^",
	[SDL_SCANCODE_7] = "7&",
	[SDL_SCANCODE_8] = "8*",
	[SDL_SCANCODE_9] = "9(",
	[SDL_SCANCODE_0] = "0)",
	[SDL_SCANCODE_MINUS] = "-_",
	[SDL_SCANCODE_EQUALS] = "=+",
	[SDL_SCANCODE_BACKSPACE] = "\b\b",
	[SDL_SCANCODE_DELETE] = "\177\177",

	[SDL_SCANCODE_TAB] = "\011\011",
	[SDL_SCANCODE_Q] = "QQ",
	[SDL_SCANCODE_W] = "WW",
	[SDL_SCANCODE_E] = "EE",
	[SDL_SCANCODE_R] = "RR",
	[SDL_SCANCODE_T] = "TT",
	[SDL_SCANCODE_Y] = "YY",
	[SDL_SCANCODE_U] = "UU",
	[SDL_SCANCODE_I] = "II",
	[SDL_SCANCODE_O] = "OO",
	[SDL_SCANCODE_P] = "PP",
	[SDL_SCANCODE_LEFTBRACKET] = "[[",
	[SDL_SCANCODE_RIGHTBRACKET] = "]]",
	[SDL_SCANCODE_BACKSLASH] = "\\\\",

	[SDL_SCANCODE_A] = "AA",
	[SDL_SCANCODE_S] = "SS",
	[SDL_SCANCODE_D] = "DD",
	[SDL_SCANCODE_F] = "FF",
	[SDL_SCANCODE_G] = "GG",
	[SDL_SCANCODE_H] = "HH",
	[SDL_SCANCODE_J] = "JJ",
	[SDL_SCANCODE_K] = "KK",
	[SDL_SCANCODE_L] = "LL",
	[SDL_SCANCODE_SEMICOLON] = ";:",
	[SDL_SCANCODE_APOSTROPHE] = "'\"",
	[SDL_SCANCODE_RETURN] = "\015\015",

	[SDL_SCANCODE_Z] = "ZZ",
	[SDL_SCANCODE_X] = "XX",
	[SDL_SCANCODE_C] = "CC",
	[SDL_SCANCODE_V] = "VV",
	[SDL_SCANCODE_B] = "BB",
	[SDL_SCANCODE_N] = "NN",
	[SDL_SCANCODE_M] = "MM",
	[SDL_SCANCODE_COMMA] = ",<",
	[SDL_SCANCODE_PERIOD] = ".>",
	[SDL_SCANCODE_SLASH] = "/?",
	[SDL_SCANCODE_SPACE] = "  ",

	[SDL_SCANCODE_LALT] = "\033\033",
	[SDL_SCANCODE_RALT] = "\033\033",
};

int ctrl;
int shift;

void
keydown(SDL_Keysym keysym, int repeat)
{
	char *keys;
	int key;

	switch(keysym.scancode){
	case SDL_SCANCODE_LSHIFT:
	case SDL_SCANCODE_RSHIFT: shift = 1; return;
	case SDL_SCANCODE_CAPSLOCK:
	case SDL_SCANCODE_LCTRL:
	case SDL_SCANCODE_RCTRL: ctrl = 1; return;
	}
	if(keystate[SDL_SCANCODE_LGUI] || keystate[SDL_SCANCODE_RGUI])
		return;

	if(keysym.scancode == SDL_SCANCODE_F11 && !repeat){
		u32 f = SDL_GetWindowFlags(window) &
			SDL_WINDOW_FULLSCREEN_DESKTOP;
		SDL_SetWindowFullscreen(window,
			f ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP);
	}

	keys = scancodemap[keysym.scancode];
	/* TODO: implement keypad */
	if(keys == nil)
		return;
	key = keys[shift];
	if(ctrl)
		key &= 037;
//	printf("%o(%d %d) %c\n", key, shift, ctrl, key);

	char c = key;
	write(pty, &c, 1);
/*
	recvchar(c);
	updatebuf = 1;
	draw();
*/
}

void
keyup(SDL_Keysym keysym)
{
	switch(keysym.scancode){
	case SDL_SCANCODE_LSHIFT:
	case SDL_SCANCODE_RSHIFT: shift = 0; return;
	case SDL_SCANCODE_CAPSLOCK:
	case SDL_SCANCODE_LCTRL:
	case SDL_SCANCODE_RCTRL: ctrl = 0; return;
	}
}

void*
readthread(void *p)
{
	char c;
	SDL_Event ev;
	static struct timespec slp;

	SDL_memset(&ev, 0, sizeof(SDL_Event));
	ev.type = userevent;

	while(1){
		if(read(pty, &c, 1) < 0)
			exit(0);
		recvchar(c);

		/* simulate baudrate, TODO: get from tty */
//		slp.tv_nsec = 1000*1000*1000 / (baud/11);
//		nanosleep(&slp, NULL);

		SDL_PushEvent(&ev);
	}
}
