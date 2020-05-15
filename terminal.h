#ifndef TERMINAL_H
#define TERMINAL_H

typedef uint32_t u32;
typedef uint8_t u8;
#define nil NULL

typedef struct Col Col;
struct Col
{
	u8 a, b, g, r;
};

extern Col phos1, phos2;
extern float Gamma;
extern int altesc;

void panic(char *fmt, ...);

#define BLURRADIUS 4
Col getblur(Col *src, int width, int height, int x, int y);
void initblur(float sig);

void* readthread(void *p);
void keyup(SDL_Keysym keysym);
void keydown(SDL_Keysym keysym, int repeat);
void draw(void);
void recvchar(int c);
void shell(void);
void spawn(void);
void toggle_fullscreen(void);
void stretch(SDL_Rect *r, int width, int height);

extern int baud;
extern int rerun;
extern char **scancodemap;
extern char *scancodemap_both[];
extern char *scancodemap_upper[];
extern const u8 *keystate;
extern u32 userevent;
extern int updatebuf;
extern int updatescreen;
extern int pty;
extern SDL_Window *window;
extern char *name;
extern SDL_Renderer *renderer;

#endif /* TERMINAL_H */
