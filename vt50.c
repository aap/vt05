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

#include "terminal.h"
#include "args.h"

Col phos1 = { 0xFF, 0xFF, 0xFF, 0xFF };
Col phos2 = { 0xFF, 0xFF, 0xB0, 0x40 };
float Gamma = 1.0/2.2f;


#include "vt50rom.h"

#define TERMWIDTH 80
#define TERMHEIGHT 12

#define CWIDTH 9	// original: 1 blank, 5 rom, 4 blank, we do 2 5 3
#define CHEIGHT 10	// 8 rom, 2 blank

#define FBWIDTH (TERMWIDTH*CWIDTH+2*2)
#define FBHEIGHT (TERMHEIGHT*2*CHEIGHT+2*2)

#define WIDTH  (FBWIDTH)
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
int scale = 1;
int full = 0;

SDL_Texture *fonttex[64];
SDL_Texture *cursortex;

int pty;

#define TEXW ((CWIDTH + BLURRADIUS*2))
#define TEXH ((CHEIGHT*2 + BLURRADIUS*2))

void
createcursor(u32 *raster)
{
	int j;

	memset(raster, 0, TEXW*TEXH*sizeof(u32));
	raster = &raster[BLURRADIUS*TEXW + BLURRADIUS];

	for(j = 0; j < 9; j++){
		raster[(8*2+0)*TEXW + j] = 0xFFFFFFFF;
	// uncomment to disable scanlines
		//raster[(8*2+1)*TEXW + j] = 0xFFFFFFFF;
	}
}

void
createchar(u32 *raster, int c)
{
	int i, j;
	u8 *chr = &vt50rom[c*8];

	memset(raster, 0, TEXW*TEXH*sizeof(u32));
	raster = &raster[BLURRADIUS*TEXW + BLURRADIUS];

	for(i = 0; i < 8; i++){
		for(j = 0; j < 5; j++){
			if(chr[i]&(020>>j)){
				raster[(i*2+0)*TEXW + j+1] = 0xFFFFFFFF;
			// uncomment to disable scanlines
				//raster[(i*2+1)*TEXW + j+1] = 0xFFFFFFFF;
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
	for(i = 0; i < 64; i++){
		createchar(ras1, i);
		blurchar(ras2, ras1);

		fonttex[i] = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888,
			SDL_TEXTUREACCESS_STREAMING, w, h);
		SDL_SetTextureBlendMode(fonttex[i], SDL_BLENDMODE_ADD);
		SDL_UpdateTexture(fonttex[i], nil, ras2, w*sizeof(u32));
	}
	createcursor(ras1);
	blurchar(ras2, ras1);

	cursortex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888,
		SDL_TEXTUREACCESS_STREAMING, w, h);
	SDL_SetTextureBlendMode(cursortex, SDL_BLENDMODE_ADD);
	SDL_UpdateTexture(cursortex, nil, ras2, w*sizeof(u32));
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
				if(c < 128){
					r.x = (2 + x*CWIDTH) - BLURRADIUS;
					r.y = (2 + y*2*CHEIGHT + CHEIGHT/2)*2 - BLURRADIUS;
					SDL_RenderCopy(renderer, fonttex[c-040], nil, &r);
				}
				if(blink && x == curx && y == cury){
					r.x = (2 + x*CWIDTH) - BLURRADIUS;
					r.y = (2 + y*2*CHEIGHT + CHEIGHT/2)*2 - BLURRADIUS;
					SDL_RenderCopy(renderer, cursortex, nil, &r);
				}
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
scrollup(void)
{
	int x, y;
	for(y = TERMHEIGHT-1; y > 0; y--)
		for(x = 0; x < TERMWIDTH; x++) 
			fb[y][x] = fb[y-1][x];
	for(x = 0; x < TERMWIDTH; x++) 
		fb[0][x] = ' ';
}

int esc;
int cad, cady;
int hold;
int rerun = 0;

/* TODO: implement hold mode */

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
	int x, y;

	/* Handle cursor addresing */
	if(cad == 1){
		cady = c-' ';
		cad = 2;
		return;
	}else if(cad == 2){
		if(cady >= 0 && cady < TERMHEIGHT)
			cury = cady;
		else
			cury = 11;
		// TODO: what if c < ' '?
		curx = c - ' ';
		cad = 0;
		return;
	}

	if(esc){
		switch(c){
		case 'A':
			cury--;
			break;
		case 'B':
			cury++;		// VT50H only
			break;
		case 'C':
			curx++;
			break;
		case 'D':
			curx--;
			break;

		case 'H':
			curx = 0;
			cury = 0;
			break;

		case 'K':
			for(x = curx; x < TERMWIDTH; x++)
				fb[cury][x] = ' ';
			break;
		case 'J':
			for(x = curx; x < TERMWIDTH; x++)
				fb[cury][x] = ' ';
			for(y = cury+1; y < TERMHEIGHT; y++)
				for(x = 0; x < TERMWIDTH; x++)
					fb[y][x] = ' ';
			break;

		case 'Y':
			cad = 1;	// VT50H only
			break;

		case 'Z':
			write(pty, "\033/H", 3);	// identify as VT50H
			break;

		case '[':
			hold = 1;
			break;
		case '\\':
			hold = 0;
			break;
		}
		esc = 0;
	}else if(c >= 040){
		if(c != 0177)
			fb[cury][curx++] = mapchar(c);
	}else switch(c){
	case 007:	/* BEL */
		/* TODO: feep */
		break;
	case 011:	/* HT */
		if(curx >= 72)
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

	case 016:	/* SO */
		cad = 1;	// VT50H only
		break;

	case 010:	/* BS - ^H */
		curx--;
		break;

	case 033:
		esc = 1;
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

char TERM[] = "vt52";
char *argv0;
char *name;

void
usage(void)
{
	panic("usage: %s [-B] [-r] [-2] [-f] [-b baudrate] command...", argv0);
}

int
main(int argc, char *argv[])
{
	int x, y;
	pthread_t thr1, thr2;
	struct winsize ws;

	scancodemap = scancodemap_upper;

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

	if (argc == 0)
		usage();

	cmd = &argv[0];

	mkpty(&ws, TERMHEIGHT, TERMWIDTH, FBWIDTH, FBHEIGHT);
	spawn();

	mkwindow(&window, &renderer, "VT50", WIDTH*scale, HEIGHT*scale);

	screentex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888,
			SDL_TEXTUREACCESS_TARGET, WIDTH, HEIGHT);

	keystate = SDL_GetKeyboardState(nil);

	for(x = 0; x < TERMWIDTH; x++)
		for(y = 0; y < TERMHEIGHT; y++)
			fb[y][x] = ' ';
	initblur(1.3);
	createfont();

	pthread_create(&thr1, nil, readthread, nil);
	pthread_create(&thr2, nil, timethread, nil);

	if(full)
		toggle_fullscreen();

	mainloop();

	SDL_Quit();
	return 0;
}
