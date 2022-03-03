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

#include "vt05chars.h"

#define TERMWIDTH 72
#define TERMHEIGHT 20

#define CWIDTH 7	// 5 rom, 2 blank
#define CHEIGHT 14	// 7 rom, 7 blank

#define FBWIDTH (TERMWIDTH*CWIDTH+2*2)
#define FBHEIGHT (TERMHEIGHT*CHEIGHT+2*2)

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
u32 userevent;
int updatebuf = 1;
int updatescreen = 1;
int rerun = 0;
int blink;
int scale = 1;
int full = 0;

SDL_Texture *fonttex[64];
SDL_Texture *cursortex;

int pty;

#define TEXW ((CWIDTH*2 + BLURRADIUS*2))
#define TEXH ((CHEIGHT*2 + BLURRADIUS*2))

void
createcursor(u32 *raster)
{
	int j;

	memset(raster, 0, TEXW*TEXH*sizeof(u32));
	raster = &raster[BLURRADIUS*TEXW + BLURRADIUS];

	for(j = 0; j < 7; j++){
		raster[(8*2+0)*TEXW + j*2] = 0xFFFFFFFF;
		raster[(8*2+0)*TEXW + j*2+1] = 0xFFFFFFFF;
	// uncomment to disable scanlines
		//raster[(8*2+1)*TEXW + j*2] = 0xFFFFFFFF;
		//raster[(8*2+1)*TEXW + j*2+1] = 0xFFFFFFFF;
	}
}

void
createchar(u32 *raster, int c)
{
	int i, j;
	char *chr = font[c];

	memset(raster, 0, TEXW*TEXH*sizeof(u32));
	raster = &raster[BLURRADIUS*TEXW + BLURRADIUS];

	for(i = 0; i < 7; i++){
		for(j = 0; j < 5; j++){
			if(chr[i*5+j] == '*'){
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
				if(c >= ' '){
					r.x = (2 + x*CWIDTH)*2 - BLURRADIUS;
					r.y = (2 + y*CHEIGHT)*2 - BLURRADIUS;
					SDL_RenderCopy(renderer, fonttex[c-' '], nil, &r);
				}
			}
		if(blink){
			r.x = (2 + curx*CWIDTH)*2 - BLURRADIUS;
			r.y = (2 + cury*CHEIGHT)*2 - BLURRADIUS;
			SDL_RenderCopy(renderer, cursortex, nil, &r);
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

void*
timethread(void *arg)
{
	(void)arg;
	SDL_Event ev;
	memset(&ev, 0, sizeof(ev));
	ev.type = userevent;
	struct timespec slp = { 0, 1000*1000*1000/3.75f };
	for(;;){
		blink = !blink;
		updatebuf = 1;
		nanosleep(&slp, nil);
		SDL_PushEvent(&ev);
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
	SDL_Event ev;
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

	mkwindow(&window, &renderer, "Datapoint 3300", WIDTH*scale, HEIGHT*scale);

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
	pthread_create(&thr2, nil, timethread, nil);

	if(full)
		toggle_fullscreen();

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
