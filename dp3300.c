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

#define FBWIDTH (TERMWIDTH*(CWIDTH+2)+2*2)
#define FBHEIGHT (TERMHEIGHT*(CHEIGHT+2)+2*2)

int sclx = 2;
int scly = 3;

#define WIDTH  (sclx*FBWIDTH)
#define HEIGHT (scly*FBHEIGHT)

SDL_Renderer *renderer;
SDL_Texture *screentex;
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
drawchar(u32 *p, int x, int y, char *c)
{
	int i, j;
	x = 2 + x*(CWIDTH+2);
	y = 2 + y*(CHEIGHT+2);
	assert(x >= 0);
	assert(x < FBWIDTH);
	assert(y >= 0);
	assert(y < FBHEIGHT);
	for(j = 0; j < CHEIGHT; j++)
		for(i = 0; i < CWIDTH; i++)
			putpixel(p, x+i, y+j, c[j*CWIDTH+i] == '*' ? fg : bg);
}

void
updatefb(void)
{
	u32 *p;
	int i;
	int x, y;

	/* do this early so recvchar update works right */
	updatebuf = 0;
	updatescreen = 1;

	p = finalfb;

	for(y = 0; y < FBHEIGHT; y++)
		for(x = 0; x < FBWIDTH; x++)
			putpixel(p, x, y, bg);

	for(x = 0; x < TERMWIDTH; x++)
		for(y = 0; y < TERMHEIGHT; y++)
			drawchar(p, x, y, font[fb[y][x]]);

	x = 2 + curx*(CWIDTH+2);
	y = 2 + cury*(CHEIGHT+2) + CHEIGHT;

	/* TODO: blink */
	for(i = 0; i < CWIDTH; i++)
		putpixel(p, x+i, y, fg);

	SDL_UpdateTexture(screentex, nil, finalfb, WIDTH*sizeof(u32));
}

void
draw(void)
{
	if(updatebuf)
		updatefb();
	if(updatescreen){
		SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
		SDL_RenderClear(renderer);
		SDL_RenderCopy(renderer, screentex, nil, nil);
		SDL_RenderPresent(renderer);
		updatescreen = 0;
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
		cury++;
		if(cury >= TERMHEIGHT)
			scroll();
		break;
	case 015:	/* CR */
		curx = 0;
		break;

	case 013:	/* VT */
		cury++;
		break;
	case 030:	/* CAN */
		curx++;
		break;
	case 010:
		/* is this correct?, also delete character perhaps? */
	case 031:	/* EM */
		curx--;
		break;
	case 032:	/* SUB */
		cury--;
		break;

	case 034:	/* FS, HOME DOWN */
		curx = 0;
		cury = 24;
		break;
	case 035:	/* GS, HOME UP */
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
	if(curx >= TERMWIDTH)
		curx = TERMWIDTH-1;
	if(cury < 0)
		cury = 0;
	if(cury >= TERMHEIGHT)
		cury = TERMHEIGHT-1;

	updatebuf = 1;
}

/* Map SDL scancodes to ASCII */
int scancodemap[SDL_NUM_SCANCODES] = {
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

int ctrl;
int shift;

void
keydown(SDL_Keysym keysym)
{
	int key;

	switch(keysym.scancode){
	case SDL_SCANCODE_LSHIFT:
	case SDL_SCANCODE_RSHIFT: shift = 1; return;
	case SDL_SCANCODE_CAPSLOCK:
	case SDL_SCANCODE_LCTRL:
	case SDL_SCANCODE_RCTRL: ctrl = 1; return;
	}

	if(keysym.scancode == SDL_SCANCODE_F1){
		updatebuf = 1;
		updatescreen = 1;
		draw();
	}

	key = scancodemap[keysym.scancode];
	if(key == 0)
		return;
	if(shift)
		key ^= 020;
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

//printf("push userevent\n");
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
//	execl("/bin/telnet", "telnet", "maya", "10000", nil);
//	execl("/bin/telnet", "telnet", "localhost", "10000", nil);
///	execl("/bin/telnet", "telnet", "its.pdp10.se", "10003", nil);
//	execl("/bin/ssh", "ssh", "its@tty.livingcomputers.org", nil);
	execl("/bin/cat", "cat", nil);

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

	userevent = SDL_RegisterEvents(1);

	for(x = 0; x < TERMWIDTH; x++)
		for(y = 0; y < TERMHEIGHT; y++)
			fb[y][x] = ' ';

	pthread_create(&thr1, NULL, readthread, NULL);
//	pthread_create(&thr2, nil, timethread, nil);

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
