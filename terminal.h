#ifndef TERMINAL_H
#define TERMINAL_H

typedef uint32_t u32;
typedef uint8_t u8;
#define nil NULL

void* readthread(void *p);
void keyup(SDL_Keysym keysym);
void keydown(SDL_Keysym keysym, int repeat);
void draw(void);
void recvchar(int c);

extern char **scancodemap;
extern char *scancodemap_both[];
extern char *scancodemap_upper[];
extern u8 *keystate;
extern u32 userevent;
extern int updatebuf;
extern int updatescreen;
extern int pty;
extern SDL_Window *window;

#endif /* TERMINAL_H */
