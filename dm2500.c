#define _XOPEN_SOURCE 600
#define _DEFAULT_SOURCE

#include <SDL.h>
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
#include <assert.h>
#include <math.h>

#include "terminal.h"
#include "args.h"

Col phos1 = { 0xFF, 0x9F, 0xFF, 0x47 };
Col phos2 = { 0xFF, 0x68, 0xCC, 0x0C };
float Gamma = 1.0/2.2f;

#include "dmchars.h"

#define TERMWIDTH 80
#define TERMHEIGHT 24

#define HSPACE 2
#define VSPACE 5

#define FBWIDTH (TERMWIDTH*(CWIDTH+HSPACE)+2*2)
#define FBHEIGHT (TERMHEIGHT*(CHEIGHT+VSPACE)+2*2)

#define WIDTH  (2*FBWIDTH)
#define HEIGHT (2*FBHEIGHT)

SDL_Surface *screen;
SDL_Window *window;
SDL_Renderer *renderer;
SDL_Texture *screentex;
const u8 *keystate;
char fb[TERMHEIGHT][TERMWIDTH];
int curx, cury;
int updatebuf = 1;
int updatescreen = 1;
int blink;
int arrows = 0;
int rerun = 0;
int scale = 1;
int full = 0;

SDL_Texture *fonttex[129];

int pty;

#define TEXW ((CWIDTH*2 + BLURRADIUS*2))
#define TEXH ((CHEIGHT*2 + BLURRADIUS*2))

void
createchar(u32 *raster, int c)
{
	int i, j;
	char *chr = font[c];

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
	int i;
	int w, h;
	u32 *ras1, *ras2;
	w = TEXW;
	h = TEXH;
	ras1 = malloc(w*h*sizeof(u32));
	ras2 = malloc(w*h*sizeof(u32));
	for(i = 0; i < 129; i++){
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
				if(blink && x == curx && y == cury)
					c = 0200;
				r.x = (2 + x*(CWIDTH+HSPACE))*2 - BLURRADIUS;
				r.y = (2 + y*(CHEIGHT+VSPACE))*2 - BLURRADIUS;
				SDL_RenderCopy(renderer, fonttex[c], nil, &r);
			}
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

void
recvchar(int c)
{
	static int last_cr = 0;
	static int roll_mode = 0;
	static int insdel_mode = 0;

	int x, y;

	if(c >= 040 && c < 0177)
	  fprintf(stderr, "%c", c);
	else
	  fprintf(stderr, "[%c/%o]", c, c);

	switch(c){
	case 000:
	case 0177:
		break;
	case 007:	/* BEL */
		/* TODO: feep */
		break;
	case 010:	/* BS - ^H */
		if (insdel_mode) {
			memmove(&fb[cury][curx], &fb[cury][curx+1], TERMWIDTH-curx-1);
			fb[cury][TERMWIDTH-1] = ' ';
		} else
			curx--;
		break;
	case 011:	/* HT */
		curx = curx+8 & ~7;
		break;
	case 014:	/* FF ^L */
		read(pty, &c, 1);
		fprintf(stderr, "[%o]", c);
		curx = c ^ 0140;
		read(pty, &c, 1);
		fprintf(stderr, "[%o]", c);
		cury = c ^ 0140;
		fprintf(stderr, "{%d,%d}", curx, cury);
		break;
	case 015:	/* CR */
		curx = 0;
		/* Fall through. */
	case 012:	/* LF */
	LF:
		if (insdel_mode) {
			memmove(&fb[cury+1][0], &fb[cury][0], TERMWIDTH*(TERMHEIGHT-cury-1));
			for(x = 0; x < TERMWIDTH; x++) 
				fb[cury][x] = ' ';
			break;
		}
		if(last_cr)
			break;
		cury++;
		break;
#if 0
	case 013:	/* VT */
		for(x = curx; x < TERMWIDTH; x++)
			fb[cury][x] = ' ';
		for(y = cury+1; y < TERMHEIGHT; y++)
			for(x = 0; x < TERMWIDTH; x++)
				fb[y][x] = ' ';
		break;
#endif
	case 027:	/* ^W */
		for(x = curx; x < TERMWIDTH; x++)
			fb[cury][x] = ' ';
		break;
	case 031:	/* ^_ */
	case 036:	/* ^^ */
		for(x = 0; x < TERMWIDTH; x++)
			for(y = 0; y < TERMHEIGHT; y++)
				fb[y][x] = ' ';
		/* Fall through. */
	case 002:	/* ^B */
		curx = cury = 0;
		break;
	case 020:	/* ^P */
		fprintf(stderr, "[INSERT/DELETE]");
		insdel_mode = 1;
		break;
	case 030:	/* CAN ^X */
		roll_mode = 0;
		insdel_mode = 0;
		break;
	case 032:	/* ^Z */
		if (insdel_mode) {
			memmove(&fb[cury][0], &fb[cury+1][0], TERMWIDTH*(TERMHEIGHT-cury-1));
			for(x = 0; x < TERMWIDTH; x++) 
				fb[TERMHEIGHT-1][x] = ' ';
		} else
			cury--;
		break;
	case 034:	/* FS ^\ */
		if (insdel_mode) {
			memmove(&fb[cury][curx+1], &fb[cury][curx], TERMWIDTH-curx-1);
			fb[cury][curx] = ' ';
		} else
			curx++;
		break;
	case 035:	/* ^] */
		roll_mode = 1;
		break;
	default:
		if (insdel_mode)
			memmove(&fb[cury][curx+1], &fb[cury][curx], TERMWIDTH-curx-1);
		fb[cury][curx++] = c;
		break;
	}
	last_cr = (c == 015);

	if(curx < 0)
		curx = 0;
	if(curx >= TERMWIDTH){
		curx = 0;
		goto LF;
	}
	if(cury < 0)
		cury = 0;
	if(cury >= TERMHEIGHT) {
		if(roll_mode) {
			scroll();
			cury = TERMHEIGHT - 1;
		} else {
			cury = 0;
		}
	}

	updatebuf = 1;
}

void*
timethread(void *arg)
{
	(void)arg;

	struct timespec slp = { 0, 1000*1000*1000/3.75f };
	for(;;){
		blink = !blink;
		updatebuf = 1;
		nanosleep(&slp, nil);
	}
}

char TERM[] = "dumb";
char *argv0;
char *name;

void
usage(void)
{
	panic("usage: %s [-2] [-B] [-f] [-b baudrate] command...", argv0);
}

int
main(int argc, char *argv[])
{
	int x, y;
	pthread_t thr1, thr2;
	struct winsize ws;

	scancodemap = scancodemap_both;

	ARGBEGIN{
	case 'b':
		baud = atoi(EARGF(usage()));
		break;
	case 'B':
		/* Backspace is Rubout. */
		scancodemap[SDL_SCANCODE_BACKSPACE] = "\177\177";
		break;
	case 'r':
		rerun = 1;
		break;
	case '2':
		scale++;
		break;
	case 'f':
		full = 1;
		break;
	default:
		usage();
		break;
	}ARGEND;

	if(argc == 0)
		usage();

	cmd = &argv[0];

	mkpty(&ws, TERMHEIGHT, TERMWIDTH, FBWIDTH, FBHEIGHT);
	spawn();

	mkwindow(&window, &renderer, "Datamedia Elite 2500", WIDTH*scale, HEIGHT*scale);

	screentex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888,
			SDL_TEXTUREACCESS_TARGET, WIDTH, HEIGHT);

	keystate = SDL_GetKeyboardState(nil);

	for(x = 0; x < TERMWIDTH; x++)
		for(y = 0; y < TERMHEIGHT; y++)
			fb[y][x] = ' ';
	initblur(1.5);
	createfont();

	pthread_create(&thr1, nil, readthread, nil);
	pthread_create(&thr2, nil, timethread, nil);

	if(full)
		toggle_fullscreen();

	mainloop();

	SDL_Quit();
	return 0;
}
