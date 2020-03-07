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

#include "terminal.h"
#include "args.h"

#include "vt05chars.h"

#define TERMWIDTH 72
#define TERMHEIGHT 20

#define HSPACE 2
#define VSPACE 7

#define FBWIDTH (TERMWIDTH*(CWIDTH+HSPACE)+2*2)
#define FBHEIGHT (TERMHEIGHT*(CHEIGHT+VSPACE)+2*2)

#define WIDTH  (2*FBWIDTH)
#define HEIGHT (2*FBHEIGHT)

Col phos1 = { 0xFF, 0xFF, 0xFF, 0xFF };
Col phos2 = { 0xFF, 0xFF, 0xB0, 0x40 };
float Gamma = 1.0/2.2f;


SDL_Surface *screen;
SDL_Window *window;
SDL_Renderer *renderer;
SDL_Texture *screentex;
const u8 *keystate;
char fb[TERMHEIGHT][TERMWIDTH];
int curx, cury;
int baud = 330;
u32 userevent;
int updatebuf = 1;
int updatescreen = 1;

SDL_Texture *fonttex[64];


int pty;

#define TEXW ((CWIDTH*2 + BLURRADIUS*2))
#define TEXH ((CHEIGHT*2 + BLURRADIUS*2))

void
createchar(u32 *raster, int c)
{
	int i, j;
	char *chr = font[c + ' '];

	memset(raster, 0, TEXW*TEXH*sizeof(u32));
	raster = &raster[BLURRADIUS*TEXW + BLURRADIUS];

	for(i = 0; i < CHEIGHT; i++){
		for(j = 0; j < CWIDTH; j++){
			if(chr[i*CWIDTH+j] == '*'){
				raster[(i*2+0)*TEXW + j*2] = 0xFF;
				raster[(i*2+0)*TEXW + j*2+1] = 0xFF;
			// uncomment to disable scanlines
			//	raster[(i*2+1)*TEXW + j*2] = 0xFF;
			//	raster[(i*2+1)*TEXW + j*2+1] = 0xFF;
			}
		}
	}
}

void
blurchar(u32 *dst, u32 *src)
{
	Col *s, *d, c;
	int x, y;

	s = (Col*)src;
	d = (Col*)dst;
	for(y = 0; y < TEXH; y++){
		for(x = 0; x < TEXW; x++){
			c = getblur(s, TEXW, TEXH, x, y);
			c.a = 255;
			d[y*TEXW + x] = c;
		}
	}
}

void
createfont(void)
{
	int i, j;
	int w, h;
	u32 *ras1, *ras2;
	w = TEXW;
	h = TEXH;
	ras1 = malloc(w*h*sizeof(u32));
	ras2 = malloc(w*h*sizeof(u32));
	for(i = 0; i < 64; i++){
		createchar(ras1, i);
		blurchar(ras2, ras1);


		fonttex[i] = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888,
			SDL_TEXTUREACCESS_STREAMING, w, h);
		SDL_SetTextureBlendMode(fonttex[i], SDL_BLENDMODE_ADD);
		SDL_UpdateTexture(fonttex[i], nil, ras2, w*sizeof(u32));
	}
}

void
draw(void)
{
	int x, y, c;
	SDL_Rect r;
	r.x = 0;
	r.y = 0;
	r.w = TEXW;
	r.h = TEXH;

	if(updatebuf){
		updatebuf = 0;

		SDL_SetRenderTarget(renderer, screentex);
		SDL_SetRenderDrawColor(renderer, 21, 13, 6, 0);
		SDL_RenderClear(renderer);
		for(x = 0; x < TERMWIDTH; x++)
			for(y = 0; y < TERMHEIGHT; y++){
				c = fb[y][x];
				if(c >= ' '){
					r.x = (2 + x*(CWIDTH+HSPACE))*2 - BLURRADIUS;
					r.y = (2 + y*(CHEIGHT+VSPACE))*2 - BLURRADIUS;
					SDL_RenderCopy(renderer, fonttex[c-' '], nil, &r);
				}
			}
		r.x = (2 + curx*(CWIDTH+HSPACE))*2 - BLURRADIUS;
		r.y = (2 + cury*(CHEIGHT+VSPACE))*2 - BLURRADIUS + 4;
		SDL_RenderCopy(renderer, fonttex['_'-' '], nil, &r);
		SDL_SetRenderTarget(renderer, nil);
		updatescreen = 1;
	}
	if(updatescreen){
		updatescreen = 0;
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

int cad, cady;

char
mapchar(char c)
{
	c &= 0177;
	if(c >= '@')
		c &= ~040;
	return c;
}

void
recvchar(int c)
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

	updatebuf = 1;
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

char **cmd;

void
shell(void)
{
	setenv("TERM", "dumb", 1);

	//execl("/bin/cat", "cat", nil);
	//execl("/usr/bin/telnet", "telnet", "localhost", "10002", nil);
	execv("/usr/bin/telnet", cmd);

	exit(1);
}


char *argv0;

void
usage(void)
{
	panic("usage: %s [-a] [-B] [-b baudrate]", argv0);
}

int
main(int argc, char *argv[])
{
	SDL_Event ev;
	int mod;
	int x, y;
	pthread_t thr1, thr2;
	struct winsize ws;

	scancodemap = scancodemap_upper;

	ARGBEGIN{
	case 'a':
		altesc = 1;
		break;
	case 'b':
		baud = atoi(EARGF(usage()));
		break;
	case 'B':
		/* Backspace is Rubout. */
		scancodemap[SDL_SCANCODE_BACKSPACE] = "\177\177";
		break;
	}ARGEND;

	cmd = &argv[0];

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
	}


	SDL_Init(SDL_INIT_EVERYTHING);
	if(SDL_CreateWindowAndRenderer(WIDTH, HEIGHT, 0, &window, &renderer) < 0)
		panic("SDL_CreateWindowAndRenderer() failed: %s\n", SDL_GetError());

	screentex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888,
			SDL_TEXTUREACCESS_TARGET, WIDTH, HEIGHT);

	keystate = SDL_GetKeyboardState(nil);

	userevent = SDL_RegisterEvents(1);

	for(x = 0; x < TERMWIDTH; x++)
		for(y = 0; y < TERMHEIGHT; y++)
			fb[y][x] = ' ';

	initblur(1.3);
	createfont();

	pthread_create(&thr1, NULL, readthread, NULL);
//	pthread_create(&thr2, nil, timethread, nil);

	while(SDL_WaitEvent(&ev) >= 0){
		switch(ev.type){
		case SDL_QUIT:
			goto out;

		case SDL_KEYDOWN:
			keydown(ev.key.keysym, ev.key.repeat);
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
#if (SDL_MAJOR_VERSION > 2) || (SDL_MAJOR_VERSION == 2 && \
    (SDL_MINOR_VERSION > 0) || (SDL_PATCHLEVEL > 4))
			case SDL_WINDOWEVENT_TAKE_FOCUS:
#endif
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
