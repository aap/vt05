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
//u32 fg = 0x0CCC68FF;	// nice green
u32 fg = 0xFFFFFFFF;
//u32 fg = 0xFFD300FF;	// amber
u32 bg = 0x000000FF;

typedef struct Col Col;
struct Col
{
	u8 a, b, g, r;
};

#include "vt52rom.h"

#define TERMWIDTH 80
#define TERMHEIGHT 24

#define CWIDTH 8	// 7 bits + dot stretching
#define CHEIGHT 8

#define VSPACE 2
#define HSPACE 2

#define FBWIDTH (TERMWIDTH*(CWIDTH+HSPACE)+2*2)
#define FBHEIGHT (TERMHEIGHT*(CHEIGHT+VSPACE)+2*2)

#define WIDTH  (FBWIDTH)
#define HEIGHT (2*FBHEIGHT)

SDL_Renderer *renderer;
SDL_Texture *screentex;
u8 *keystate;
char fb[TERMHEIGHT][TERMWIDTH];
int curx, cury;
int baud = 330;
u32 userevent;
int updatebuf = 1;
int updatescreen = 1;
int blink;

SDL_Texture *fonttex[128];

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

#define TEXW ((CWIDTH + BLURRADIUS*2))
#define TEXH ((CHEIGHT*2 + BLURRADIUS*2))

void
createchar(u32 *raster, int c)
{
	int i, j;
	u8 *chr = &vt52rom[c*8];

	memset(raster, 0, TEXW*TEXH*sizeof(u32));
	raster = &raster[BLURRADIUS*TEXW + BLURRADIUS];

	for(i = 0; i < CHEIGHT; i++){
		for(j = 0; j < 8; j++){
			if(chr[i]&(0300>>j)){
				raster[(i*2+0)*TEXW + j] = fg;
			// uncomment to disable scanlines
			//	raster[(i*2+1)*TEXW + j] = fg;
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
	for(i = 0; i < 128; i++){
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
					c = 2;
				if(c < 128){
					r.x = (2 + x*(CWIDTH+HSPACE)) - BLURRADIUS;
					r.y = (2 + y*(CHEIGHT+VSPACE))*2 - BLURRADIUS;
					SDL_RenderCopy(renderer, fonttex[c], nil, &r);
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
int altkey;
int graph;

/* TODO: implement hold and altkey mode */

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
		curx = c - ' ';;
		cad = 0;
		return;
	}

	if(esc){
		switch(c){
		case 'A':
			cury--;
			break;
		case 'B':
			cury++;
			break;
		case 'C':
			curx++;
			break;
		case 'D':
			curx--;
			break;
		case 'I':
			cury--;
			if(cury < 0)
				scrollup();
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
			cad = 1;
			break;

		case 'Z':
			write(pty, "\033/K", 3);	// identify as VT52
			break;

		case 'F':
			graph = 1;
			break;
		case 'G':
			graph = 0;
			break;
		case '=':
			altkey = 1;
			break;
		case '>':
			altkey = 0;
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
		if(c != 0177){
			if(graph && c >= 0136){
				if(c == 0136)
					fb[cury][curx++] = 0;
				else
					fb[cury][curx++] = c-0137;
			}else
				fb[cury][curx++] = c;
		}
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

/* Map SDL scancodes to ASCII */
char *scancodemap[SDL_NUM_SCANCODES] = {
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
		read(pty, &c, 1);
		recvchar(c);

		/* simulate baudrate, TODO: get from tty */
//		slp.tv_nsec = 1000*1000*1000 / (baud/11);
//		nanosleep(&slp, NULL);

		SDL_PushEvent(&ev);
	}
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

void
sigchld(int s)
{
	exit(0);
}

char **cmd;

void
shell(void)
{
	setenv("TERM", "dumb", 1);

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

	default:
		signal(SIGCHLD, sigchld);
	}


	SDL_Init(SDL_INIT_EVERYTHING);
	if(SDL_CreateWindowAndRenderer(WIDTH, HEIGHT, 0, &window, &renderer) < 0)
		panic("SDL_CreateWindowAndRenderer() failed: %s\n", SDL_GetError());

	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");

	screentex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888,
			SDL_TEXTUREACCESS_TARGET, WIDTH, HEIGHT);

	keystate = SDL_GetKeyboardState(nil);

	userevent = SDL_RegisterEvents(1);

	for(x = 0; x < TERMWIDTH; x++)
		for(y = 0; y < TERMHEIGHT; y++)
			fb[y][x] = ' ';
	initblur(1.5);
	createfont();

	pthread_create(&thr1, NULL, readthread, NULL);
	pthread_create(&thr2, NULL, timethread, NULL);

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
