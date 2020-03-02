#define _XOPEN_SOURCE 600
#define _DEFAULT_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <ctype.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <pwd.h>
#include <termios.h>
#include <poll.h>
#include <errno.h>
#include <pthread.h>
#include <SDL.h>
#include <assert.h>

#include "args.h"

typedef uint32_t u32;
typedef uint8_t u8;
#define nil NULL

SDL_Surface *screen;

/* pixel fmt: RGBA */
//u32 fg = 0x94FF00FF;
u32 fg = 0x00FF00FF;
//u32 fg = 0xFFD300FF;	// amber
u32 bg = 0x000000FF;

#include "dpchars.h"

#define TERMWIDTH 72
#define TERMHEIGHT 25

#define VSPACE 5

#define FBWIDTH (TERMWIDTH*(CWIDTH+2)+2*2)
#define FBHEIGHT (TERMHEIGHT*(CHEIGHT+VSPACE)+2*2)

int sclx = 2;
int scly = 2;
//int sclx = 1;
//int scly = 1;

#define WIDTH  (sclx*FBWIDTH)
#define HEIGHT (scly*FBHEIGHT)

SDL_Renderer *renderer;
SDL_Texture *screentex;
u8 *keystate;
char fb[TERMHEIGHT][TERMWIDTH];
u32 *finalfb;
int curx, cury;
int baud = 330;
u32 userevent;
int updatebuf = 1;
int updatescreen = 1;

int pty;

void
panic(char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	fprintf(stderr, "\n");
	va_end(ap);
	exit(1);
}

void
putpixel(u32 *p, int x, int y, u32 col)
{
	int i, j;
	x *= sclx;
	y *= scly;
	for(i = 0; i < sclx; i++)
		for(j = 0; j < scly; j++)
			p[(y+j)*WIDTH + x+i] = col;
}

void
putpixel_(u32 *p, int x, int y, u32 col)
{
	p[y*WIDTH + x] = col;
}

u32 cols[4] = {
	0x2e2a25FF,	// bg
	0xae843eFF,	// leading edge
	0x9f7940FF,	// trailing edge
	0xebaf29FF	// full intensity
};

u32 bgs[2] = {
	0x2e2a25FF,
	0x211f1bFF
};

void
drawchar_(u32 *p, int x, int y, char *c)
{
	int i, j;
	x = 2 + x*(CWIDTH+2);
	y = 2 + y*(CHEIGHT+VSPACE);
	assert(x >= 0);
	assert(x < FBWIDTH);
	assert(y >= 0);
	assert(y < FBHEIGHT);
	for(j = 0; j < CHEIGHT; j++)
		for(i = 0; i < CWIDTH; i++)
			putpixel(p, x+i, y+j, c[j*CWIDTH+i] == '*' ? fg : bg);
}

void
drawchar(u32 *p, int x, int y, char *c)
{
	int i, j;
	x = 2 + x*(CWIDTH);
	y = 2 + y*(CHEIGHT+VSPACE);
	assert(x >= 0);
	assert(x < FBWIDTH);
	assert(y >= 0);
	assert(y < FBHEIGHT);

	x *= sclx;
	y *= scly;

	int bits[CWIDTH+1];

	int this;
	int last;
	u32 col;
	int xx;
	for(j = 0; j < CHEIGHT; j++){
		last = 0;
		memset(bits, 0, sizeof(bits));
		for(i = 0; i < CWIDTH; i++){
			if(c[j*CWIDTH+i] == '*'){
				bits[i] = 1;
				bits[i+1] = 1;
			}
		}

		xx = x;
		for(i = 0; i < CWIDTH+1; i++){
			this = bits[i];
			col = cols[last<<1 | this];
			putpixel_(p, xx, y, col);
			xx++;
			last = this;
		}
		this = 0;
		col = cols[last<<1 | this];
		putpixel_(p, xx, y, col);
		y += 2;
	}
}

void
updatefb(void)
{
	u32 *p;
	int i;
	int x, y;

	p = finalfb;

	for(y = 0; y < FBHEIGHT; y++)
		for(x = 0; x < FBWIDTH; x++)
			putpixel(p, x, y, bg);

	for(x = 0; x < TERMWIDTH; x++)
		for(y = 0; y < TERMHEIGHT; y++)
			drawchar_(p, x, y, font[fb[y][x]]);

	x = 2 + curx*(CWIDTH+2);
	y = 2 + cury*(CHEIGHT+VSPACE) + CHEIGHT;

	/* TODO: blink */
	for(i = 0; i < CWIDTH; i++)
		putpixel(p, x+i, y, fg);
}

void
updatefb_(void)
{
	u32 *p;
	int i;
	int x, y;

	p = finalfb;

/*
	for(y = 0; y < FBHEIGHT; y++)
		for(x = 0; x < FBWIDTH; x++)
//			putpixel(p, x, y, bg);
			putpixel(p, x, y, bgs[y&1]);
*/
	for(y = 0; y < HEIGHT; y++)
		for(x = 0; x < WIDTH; x++)
			putpixel_(p, x, y, bgs[y&1]);

	for(x = 0; x < TERMWIDTH; x++)
		for(y = 0; y < TERMHEIGHT; y++)
			drawchar(p, x, y, font[fb[y][x]]);

//	x = 2 + curx*(CWIDTH+2);
	x = 2 + curx*(CWIDTH);
	y = 2 + cury*(CHEIGHT+VSPACE) + CHEIGHT;

	/* TODO: blink */
	for(i = 0; i < CWIDTH; i++)
//		putpixel(p, x+i, y, fg);
		putpixel(p, x+i, y, cols[3]);
}

void
draw(void)
{
	if(updatebuf){
		updatebuf = 0;
		updatefb();
		SDL_UpdateTexture(screentex, nil, finalfb, WIDTH*sizeof(u32));
		updatescreen = 1;
	}
	if(updatescreen){
		updatescreen = 0;
		SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
		SDL_RenderClear(renderer);
		SDL_RenderCopy(renderer, screentex, nil, nil);
		SDL_RenderPresent(renderer);
	}
}

void
scroll(void)
{
	int x, y;
	for(y = 1; y < TERMHEIGHT; y++)
		for(x = 0; x < TERMWIDTH; x++) 
			fb[y-1][x] = fb[y][x];
	for(x = 0; x < TERMWIDTH; x++) 
		fb[TERMHEIGHT-1][x] = ' ';
}

void
recvchar(int c)
{
	int x, y;

	if(c >= 040){
		/* make characters printable */
		if(c >= 0140 && c < 0177)
			c &= ~040;
		if(c != 0177)
			fb[cury][curx++] = c;
	}else switch(c){
	case 007:	/* BEL */
		/* TODO: feep */
		break;
	case 011:	/* HT */
		/* TODO: stolen from VT05, is it the same? */
		if(curx >= 64)
			curx++;
		else
			curx = curx+8 & ~7;
		break;
	case 012:	/* LF */
	LF:
		cury++;
		if(cury >= TERMHEIGHT)
			scroll();
		break;
	case 015:	/* CR */
		curx = 0;
		break;

	case 013:	/* VT  - ^K */
		cury++;
		break;
	case 030:	/* CAN - ^X */
		curx++;
		break;
	case 010:	/* BS - ^H */
		/* is this correct?, also delete character perhaps? */
	case 031:	/* EM - ^Y */
		curx--;
		break;
	case 032:	/* SUB - ^Z */
		cury--;
		break;

	case 034:	/* FS, HOME DOWN - S-^L */
		curx = 0;
		cury = 24;
		break;
	case 035:	/* GS, HOME UP - S-^M */
		curx = 0;
		cury = 0;
		break;

	case 036:	/* RS, EOL */
		for(x = curx; x < TERMWIDTH; x++)
			fb[cury][x] = ' ';
		break;
	case 037:	/* US, EOF */
		for(x = curx; x < TERMWIDTH; x++)
			fb[cury][x] = ' ';
		for(y = cury+1; y < TERMHEIGHT; y++)
			for(x = 0; x < TERMWIDTH; x++)
				fb[y][x] = ' ';
		break;
	}

	if(curx < 0)
		curx = 0;
	if(curx >= TERMWIDTH){
		curx = 0;
		goto LF;
	}
	if(cury < 0)
		cury = 0;
	if(cury >= TERMHEIGHT)
		cury = TERMHEIGHT-1;

	updatebuf = 1;
}

/* Map SDL scancodes to ASCII */

int scancodemap_orig[SDL_NUM_SCANCODES] = {
	[SDL_SCANCODE_1] = '1',
	[SDL_SCANCODE_2] = '2',
	[SDL_SCANCODE_3] = '3',
	[SDL_SCANCODE_4] = '4',
	[SDL_SCANCODE_5] = '5',
	[SDL_SCANCODE_6] = '6',
	[SDL_SCANCODE_7] = '7',
	[SDL_SCANCODE_8] = '8',
	[SDL_SCANCODE_9] = '9',
	[SDL_SCANCODE_0] = '0',
	[SDL_SCANCODE_MINUS] = ':',
	[SDL_SCANCODE_EQUALS] = '-',
	[SDL_SCANCODE_BACKSPACE] = 010,
	[SDL_SCANCODE_DELETE] = 0177,

	[SDL_SCANCODE_ESCAPE] = 033,
	[SDL_SCANCODE_TAB] = 033,	/* Or map this to tab */
	[SDL_SCANCODE_Q] = 'Q',
	[SDL_SCANCODE_W] = 'W',
	[SDL_SCANCODE_E] = 'E',
	[SDL_SCANCODE_R] = 'R',
	[SDL_SCANCODE_T] = 'T',
	[SDL_SCANCODE_Y] = 'Y',
	[SDL_SCANCODE_U] = 'U',
	[SDL_SCANCODE_I] = 'I',
	[SDL_SCANCODE_O] = 'O',
	[SDL_SCANCODE_P] = 'P',
	[SDL_SCANCODE_LEFTBRACKET] = 012,
	[SDL_SCANCODE_RIGHTBRACKET] = 015,

	[SDL_SCANCODE_A] = 'A',
	[SDL_SCANCODE_S] = 'S',
	[SDL_SCANCODE_D] = 'D',
	[SDL_SCANCODE_F] = 'F',
	[SDL_SCANCODE_G] = 'G',
	[SDL_SCANCODE_H] = 'H',
	[SDL_SCANCODE_J] = 'J',
	[SDL_SCANCODE_K] = 'K',
	[SDL_SCANCODE_L] = 'L',
	[SDL_SCANCODE_SEMICOLON] = ';',
	[SDL_SCANCODE_APOSTROPHE] = 0177,
	[SDL_SCANCODE_RETURN] = 015,

	[SDL_SCANCODE_Z] = 'Z',
	[SDL_SCANCODE_X] = 'X',
	[SDL_SCANCODE_C] = 'C',
	[SDL_SCANCODE_V] = 'V',
	[SDL_SCANCODE_B] = 'B',
	[SDL_SCANCODE_N] = 'N',
	[SDL_SCANCODE_M] = 'M',
	[SDL_SCANCODE_COMMA] = ',',
	[SDL_SCANCODE_PERIOD] = '.',
	[SDL_SCANCODE_SLASH] = '/',
	[SDL_SCANCODE_SPACE] = ' ',
};

char *scancodemap[SDL_NUM_SCANCODES] = {
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
};

int ctrl;
int shift;

void
keydown(SDL_Keysym keysym)
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


	if(keysym.scancode == SDL_SCANCODE_F1){
		updatebuf = 1;
		updatescreen = 1;
		draw();
	}

/*
	key = scancodemap_orig[keysym.scancode];
	if(key == 0)
		return;
	if(shift)
		key ^= 020;
	if(ctrl)
		key &= 037;
*/
	keys = scancodemap[keysym.scancode];
	if(keys == nil)
		return;
	key = keys[shift];
	if(ctrl)
		key &= 037;
//	printf("%o(%d %d) %c\n", key, shift, ctrl, key);

	char c = key;
	write(pty, &c, 1);
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
		read(pty, &c, 1);
		recvchar(c);

		/* simulate baudrate, TODO: get from tty */
//		slp.tv_nsec = 1000*1000*1000 / (baud/11);
//		nanosleep(&slp, NULL);

		SDL_PushEvent(&ev);
	}
}

#if 0
void*
timethread(void *arg)
{
	(void)arg;
	SDL_Event ev;
	memset(&ev, 0, sizeof(ev));
	ev.type = userevent;
	struct timespec slp = { 0, 30*1000*1000 };
	for(;;){
		nanosleep(&slp, nil);
		SDL_PushEvent(&ev);
	}
}
#endif

void
sigchld(int s)
{
	exit(0);
}

void
shell(void)
{
	struct passwd *pw;

	setenv("TERM", "dumb", 1);

	pw = getpwuid(getuid());
	if(pw == NULL)
		panic("No user");
//	execl(pw->pw_shell, pw->pw_shell, nil);
//	execl("/home/aap/bin/supdup", "supdup", "its.pdp10.se", nil);
//	execl("/bin/telnet", "telnet", "its.svensson.org", nil);
	execl("/bin/telnet", "telnet", "maya", "10003", nil);
//	execl("/bin/telnet", "telnet", "localhost", "10000", nil);
//	execl("/bin/telnet", "telnet", "its.pdp10.se", "10003", nil);
//	execl("/bin/ssh", "ssh", "its@tty.livingcomputers.org", nil);
//	execl("/bin/cat", "cat", nil);
//	execl("/bin/telnet", "telnet", "its.pdp10.se", "1972", nil);

	exit(1);
}


char *argv0;

void
usage(void)
{
	panic("usage: %s [-b baudrate] [-x scalex] [-y scaley]", argv0);
}

int
main(int argc, char *argv[])
{
	SDL_Window *window;
	SDL_Event ev;
	int mod;
	int x, y;
	pthread_t thr1, thr2;
	struct winsize ws;

	ARGBEGIN{
	case 'b':
		baud = atoi(EARGF(usage()));
		break;

	case 'x':
		sclx = atoi(EARGF(usage()));
		break;

	case 'y':
		scly = atoi(EARGF(usage()));
		break;

	}ARGEND;

	pty = posix_openpt(O_RDWR);
	if(pty < 0 ||
	   grantpt(pty) < 0 ||
	   unlockpt(pty) < 0)
		panic("Couldn't get pty");

	char *name = ptsname(pty);

	ws.ws_row = TERMHEIGHT;
	ws.ws_col = TERMWIDTH;
	ws.ws_xpixel = FBWIDTH;
	ws.ws_ypixel = FBHEIGHT;
	ioctl(pty, TIOCSWINSZ, &ws);

	switch(fork()){
	case -1:
		panic("fork failed");

	case 0:
		close(pty);
		close(0);
		close(1);
		close(2);

		setsid();

		if(open(name, O_RDWR) != 0)
			exit(1);
		dup(0);
		dup(1);

		shell();

	default:
		signal(SIGCHLD, sigchld);
	}


	SDL_Init(SDL_INIT_EVERYTHING);
	if(SDL_CreateWindowAndRenderer(WIDTH, HEIGHT, 0, &window, &renderer) < 0)
		panic("SDL_CreateWindowAndRenderer() failed: %s\n", SDL_GetError());

	screentex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888,
			SDL_TEXTUREACCESS_STREAMING, WIDTH, HEIGHT);
	finalfb = malloc(WIDTH*HEIGHT*sizeof(u32));

	keystate = SDL_GetKeyboardState(nil);

	userevent = SDL_RegisterEvents(1);

	for(x = 0; x < TERMWIDTH; x++)
		for(y = 0; y < TERMHEIGHT; y++)
			fb[y][x] = ' ';

	pthread_create(&thr1, NULL, readthread, NULL);

	while(SDL_WaitEvent(&ev) >= 0){
		switch(ev.type){
		case SDL_QUIT:
			goto out;

		case SDL_KEYDOWN:
			keydown(ev.key.keysym);
			break;
		case SDL_KEYUP:
			keyup(ev.key.keysym);
			break;

		case SDL_USEREVENT:
			/* got a new character */
			draw();
			break;
		case SDL_WINDOWEVENT:
			switch(ev.window.event){
			case SDL_WINDOWEVENT_MOVED:
			case SDL_WINDOWEVENT_ENTER:
			case SDL_WINDOWEVENT_LEAVE:
			case SDL_WINDOWEVENT_FOCUS_GAINED:
			case SDL_WINDOWEVENT_FOCUS_LOST:
			case SDL_WINDOWEVENT_TAKE_FOCUS:
				break;
			default:
				/* redraw */
				updatescreen = 1;
				draw();
				break;
			}
			break;
		}
	}
out:
	SDL_Quit();
	return 0;
}
