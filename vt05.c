/*
 * This is a very simple VT05 emulator without a lot of features.
 *
 * F1 toggles between full and reduced ASCII input.
 * -b sets a baudrate to simulate.
 * -x/-y scale the framebuffer.
 *
 * BUGS: - too fast input confuses it somehow.
 *       - keyboard layout is handled in an ugly way.
 */

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

#include "args.h"

typedef uint32_t u32;

SDL_Surface *screen;

/* pixel fmt: ARGB */
u32 fg = 0xFFFFFFFF;
u32 bg = 0xFF000000;

#include "vt05chars.h"

#define TERMWIDTH 72
#define TERMHEIGHT 20

#define FBWIDTH (TERMWIDTH*(CWIDTH+2)+2*2)
#define FBHEIGHT (TERMHEIGHT*(CHEIGHT+2)+2*2)

int sclx = 2;
int scly = 3;

#define WIDTH  (sclx*FBWIDTH)
#define HEIGHT (scly*FBHEIGHT)

char fb[TERMHEIGHT][TERMWIDTH];
int curx, cury;
int cad, cady;
int fullascii = 1;
int baud = 330;

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
	for(j = 0; j < CHEIGHT; j++)
		for(i = 0; i < CWIDTH; i++)
			putpixel(p, x+i, y+j, c[j*CWIDTH+i] == '*' ? fg : bg);
}

void
draw(void)
{
	u32 *p;
	int i;
	int x, y;

	SDL_LockSurface(screen);
	p = screen->pixels;

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

	SDL_UnlockSurface(screen);
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

/* Fold down the upper codepoints */
char
mapchar(char c)
{
	c &= 0177;
	if(c >= '@')
		c &= ~040;
	return c;
}

void
recvchar(char c)
{
	char pc;
	int x, y;

	/* Handle cursor addresing */
	if(cad == 1){
		cady = c-' ';
		cad = 2;
		return;
	}else if(cad == 2){
		c -= ' ';
		if(cady >= 0 && cady < TERMHEIGHT &&
		   c >= 0 && c < TERMWIDTH){
			curx = c;
			cury = cady;
		}
		cad = 0;
		return;
	}

	/* printable characters */
	if(c >= 040 && c <= 0174 || c == 0176)
		fb[cury][curx++] = mapchar(c);
	else switch(c){
	case '\a':
		/* TODO: feep */
		break;

	case '\b':
		curx--;
		break;

	case '\t':
		if(curx >= 64)
			curx++;
		else
			curx = curx+8 & ~7;
		break;

	case '\n':
		cury++;
		if(cury >= TERMHEIGHT)
			scroll();
		break;

	case '\v':
		cury++;
		break;

	case '\r':
		curx = 0;
		break;

	case 016:	/* SO */
		cad = 1;
		break;

	case 030:	/* CAN */
		curx++;
		break;

	case 032:	/* SUB */
		cury--;
		break;

	case 035:	/* GS */
		curx = 0;
		cury = 0;
		break;

	case 036:
		for(x = curx; x < TERMWIDTH; x++)
			fb[cury][x] = ' ';
		break;

	case 037:
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
}

void
sendchar(char c, int mod)
{
	int x, y;

	/* SHIFT */
	if(mod & 1){
		/* The VT05 has an ascii-aware keyboard layout, so we
		 * can't use its shifting logic as is. Do some shifting
		 * manually and clear the shift flag as a workaround.
		 * Very ugly as it only works with US qwerty right now. */
		switch(c){
		case ';': c = ':'; break;
		case '`': c = '~'; break;
		case '2': c = '@'; break;
		case '6': c = '^'; break;
		case '7': c = '&'; break;
		case '8': c = '*'; break;
		case '9': c = '('; break;
		case '0': c = ')'; break;
		case '-': c = '_'; break;
		case '=': c = '+'; break;
		case '\'': c = '"'; break;
		default:
			goto noshift;
		}
		mod &= ~1;
	}
noshift:
	/* This is the real VT05 shifting logic */
	if(mod & 1 && c >= 040){
		if(c & 0100)
			c ^= 040;
		else
			c ^= 020;
	}

	/* CTRL */
	if(mod & 2)
		c &= ~0140;

	if(!fullascii && c != 0177)
		c = mapchar(c);

	write(pty, &c, 1);
}

void*
readthread(void *p)
{
	char c;
	SDL_Event ev;
	static struct timespec slp;

	SDL_memset(&ev, 0, sizeof(SDL_Event));
	ev.type = SDL_USEREVENT;

	while(1){
		read(pty, &c, 1);
		ev.user.code = c;

		/* simulate baudrate, TODO: get from tty */
		slp.tv_nsec = 1000*1000*1000 / (baud/11);
		nanosleep(&slp, NULL);

		SDL_PushEvent(&ev);
	}
}

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
	execl(pw->pw_shell, pw->pw_shell, NULL);

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
	SDL_Event ev;
	SDLKey k;
	int mod;
	int x, y;
	pthread_t thr;
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

	if(SDL_Init(SDL_INIT_VIDEO) < 0){
	error:
		fprintf(stderr, "Error: %s\n", SDL_GetError());
		return 1;
	}

	screen = SDL_SetVideoMode(WIDTH, HEIGHT, 32, SDL_DOUBLEBUF);
	if(screen == NULL)
		goto error;

	SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY,
		SDL_DEFAULT_REPEAT_INTERVAL);

	pthread_create(&thr, NULL, readthread, NULL);

	for(x = 0; x < TERMWIDTH; x++)
		for(y = 0; y < TERMHEIGHT; y++)
			fb[y][x] = ' ';

	while(SDL_WaitEvent(&ev) >= 0){
		switch(ev.type){
		case SDL_QUIT:
			goto out;

		case SDL_USEREVENT:
			recvchar(ev.user.code);
			break;

		case SDL_KEYUP:
			k = ev.key.keysym.sym;
			switch(k){
			case SDLK_RSHIFT:
			case SDLK_LSHIFT:
				mod &= ~1;
				break;

			case SDLK_RCTRL:
			case SDLK_LCTRL:
				mod &= ~2;
				break;
			}

			break;
		case SDL_KEYDOWN:
			k = ev.key.keysym.sym;
			if(k >= 040 && k < 0200)
				sendchar(k, mod);
			else switch(k){
			case SDLK_F1:
				fullascii = !fullascii;
				break;

			case '\b':
			case '\t':
			case '\r':
			case '\n':
				sendchar(k, mod);
				break;

			case SDLK_UP:
				sendchar(032, mod);
				break;
			case SDLK_DOWN:
				sendchar('\v', mod);
				break;
			case SDLK_LEFT:
				sendchar('\b', mod);
				break;
			case SDLK_RIGHT:
				sendchar(030, mod);
				break;

			case SDLK_HOME:
				sendchar(035, mod);
				break;

			case SDLK_RSHIFT:
			case SDLK_LSHIFT:
				mod |= 1;
				break;

			case SDLK_RCTRL:
			case SDLK_LCTRL:
				mod |= 2;
				break;
			}
		}
		draw();
		SDL_Flip(screen);
	}
out:
	SDL_Quit();
	return 0;
}
