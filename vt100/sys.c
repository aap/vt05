#include "vt100/vt100.h"

u8 memory[0x10000];
u16 starta = 0;
void (*halt) (u16);
u8 vt100_flags;
static SDL_atomic_t imask;

u8 (*device_in[256]) (u8 port);
void (*device_out[256]) (u8 port, u8 data);

void raise_interrupt (int mask)
{
  int x = SDL_AtomicGet (&imask);
  x |= mask;
  SDL_AtomicSet (&imask, x);
  if (x)
    interrupt (RST0 | x << 3);
  else
    interrupt (-1);
}

void clear_interrupt (int mask)
{
  int x = SDL_AtomicGet (&imask);
  x &= ~mask;
  SDL_AtomicSet (&imask, x);
  if (x)
    interrupt (RST0 | x << 3);
  else
    interrupt (-1);
}

void register_port (u8 port, u8 (*in) (u8), void (*out) (u8, u8))
{
  device_in[port] = in;
  device_out[port] = out;
}

static void no_out (u8 port, u8 data)
{
}

static u8 no_in (u8 port)
{
  return 0;
}

void reset (void)
{
  int i;

  debug = stderr;

  cpu_reset ();

  for (i = 0; i < 256; i++) {
    device_out[i] = no_out;
    device_in[i] = no_in;
  }

  starta = 0;
  SDL_AtomicSet(&imask, 0);

  memset (mtype, 2, 64);
  mtype[0] = 1; //8K RROM
  mtype[1] = 1;
  mtype[2] = 1;
  mtype[3] = 1;
  mtype[4] = 1;
  mtype[5] = 1;
  mtype[6] = 1;
  mtype[7] = 1;
  mtype[8] = 0; //3K RAM
  mtype[9] = 0;
  mtype[10] = 0;

  reset_pusart ();
  reset_nvr ();
  reset_brightness ();
  SDL_CreateThread (timer, "Time", NULL);
  //SDL_CreateThread (video, "Video", NULL);
  reset_video ();
  reset_keyboard ();
  reset_sound ();
}
