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
//u32 fg = 0x00FF00FF;	// green
//u32 fg = 0x0CCC68FF;	// green
u32 fg = 0xFFFFFFFF;
//u32 fg = 0xFFD300FF;	// amber
u32 bg = 0x000000FF;

typedef struct Col Col;
struct Col
{
	u8 a, b, g, r;
};

#include "vt05chars.h"

#define TERMWIDTH 72
#define TERMHEIGHT 20

#define HSPACE 2
#define VSPACE 7

#define FBWIDTH (TERMWIDTH*(CWIDTH+HSPACE)+2*2)
#define FBHEIGHT (TERMHEIGHT*(CHEIGHT+VSPACE)+2*2)

#define WIDTH  (2*FBWIDTH)
#define HEIGHT (2*FBHEIGHT)

SDL_Window *window;
SDL_Renderer *renderer;
SDL_Texture *screentex;
u8 *keystate;
char fb[TERMHEIGHT][TERMWIDTH];
int curx, cury;
int baud = 330;
u32 userevent;
int updatebuf = 1;
int updatescreen = 1;

SDL_Texture *fonttex[64];


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

#define BLURRADIUS 4
#define MATSIZ (2*BLURRADIUS+1)

float blurmat[MATSIZ][MATSIZ];

void
initblur(float sig)
{
	int i, j;
	float dx, dy, dist;

	for(i = 0; i < MATSIZ; i++)
		for(j = 0; j < MATSIZ; j++){
			dx = i-BLURRADIUS;
			dy = j-BLURRADIUS;
			dist = sqrt(dx*dx + dy*dy);
			blurmat[i][j] = exp(-(dx*dx + dy*dy)/(2*sig*sig)) / (2*M_PI*sig*sig);
		}
}

float Gamma = 1.0/2.2f;

Col
getblur(Col *src, int width, int height, int x, int y)
{
	int xx, yy;
	Col *p;
	int i, j;
	int r, g, b, a;
	Col c;

	r = g = b = a = 0;
	for(i = 0, yy = y-BLURRADIUS; yy <= y+BLURRADIUS; yy++, i++){
		if(yy < 0 || yy >= height)
			continue;
		for(j = 0, xx = x-BLURRADIUS; xx <= x+BLURRADIUS; xx++, j++){
			if(xx < 0 || xx >= width)
				continue;
			p = &src[yy*width + xx];
			r += p->r * blurmat[i][j];
			g += p->g * blurmat[i][j];
			b += p->b * blurmat[i][j];
			a += p->a * blurmat[i][j];
		}
	}
	c.r = pow(r/255.0f, Gamma)*255;
	c.g = pow(g/255.0f, Gamma)*255;
	c.b = pow(b/255.0f, Gamma)*255;
	c.a = pow(a/255.0f, Gamma)*255;

	p = &src[y*width + x];
	if(p->r > c.r) c.r = p->r;
	if(p->g > c.g) c.g = p->g;
	if(p->b > c.b) c.b = p->b;
	if(p->a > c.a) c.a = p->a;

	return c;
}

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
				raster[(i*2+0)*TEXW + j*2] = fg;
				raster[(i*2+0)*TEXW + j*2+1] = fg;
			// uncomment to disable scanlines
			//	raster[(i*2+1)*TEXW + j*2] = fg;
			//	raster[(i*2+1)*TEXW + j*2+1] = fg;
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

/* Map SDL scancodes to ASCII */
// Can't map VT05 keyboard sensible....this is from Datapoint 3300
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

/*
	key = scancodemap[keysym.scancode];
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

/*
SDL_Event ev;
SDL_memset(&ev, 0, sizeof(SDL_Event));
ev.type = userevent;
recvchar(c);
SDL_PushEvent(&ev);
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
	panic("usage: %s [-b baudrate]", argv0);
}

int
main(int argc, char *argv[])
{
	SDL_Event ev;
	int mod;
	int x, y;
	pthread_t thr1, thr2;
	struct winsize ws;

	ARGBEGIN{
	case 'b':
		baud = atoi(EARGF(usage()));
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

	initblur(1.0);
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
