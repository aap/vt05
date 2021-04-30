#include "vt100.h"

static int columns;
static int hertz;
static int interlace;

static void refresh (void);
static EVENT (refresh_event, refresh);

static u8 video_a2_in (u8 port)
{
  logger ("VID", "A2 IN"); 
  return 0;
}

static void video_a2_out (u8 port, u8 data)
{
  switch (data & 0x0F) {
  case 0x00: logger ("VID", "Load low order scroll latch 00"); break;
  case 0x01: logger ("VID", "Load low order scroll latch 01"); break;
  case 0x02: logger ("VID", "Load low order scroll latch 10"); break;
  case 0x03: logger ("VID", "Load low order scroll latch 11"); break;
  case 0x04: logger ("VID", "Load high order scroll latch 00"); break;
  case 0x05: logger ("VID", "Load high order scroll latch 01"); break;
  case 0x06: logger ("VID", "Load high order scroll latch 10"); break;
  case 0x07: logger ("VID", "Load high order scroll latch 11"); break;
  case 0x08: logger ("VID", "Toggle blink flip-flop"); break;
  case 0x09: clear_interrupt (4); break;
  case 0x0A: logger ("VID", "Set reverse field on"); break;
  case 0x0B: logger ("VID", "Set reverse field off"); break;
  case 0x0C: logger ("VID", "Set basic attribute to underline"); break;
  case 0x0D: logger ("VID", "Set basic attribute to reverse video"); break;
  case 0x0E:
  case 0x0F: logger ("VID", "Reserved"); break;
  }
}

static u8 video_c2_in (u8 port)
{
  logger ("VID", "C2 IN"); 
  return 0;
}

static void video_c2_out (u8 port, u8 data)
{
  switch (data & 0x30) {
  case 0x00: columns = 80; interlace = 1; break;
  case 0x10: columns = 132; interlace = 1; break;
  case 0x20: hertz = 60; interlace = 0; break;
  case 0x30: hertz = 50; interlace = 0; break;
  }
}

static u16 video_line (int n, u16 addr)
{
  static char buffer[200];
  char *p = buffer;
  u8 data;
  int skip = hertz == 60 ? 2 : 5;

  for (;;) {
    if (addr < 0x2000 || addr >= 0x2C00) {
      logger ("VID", "Address outside RAM: %04X", addr);
      return 0x2000;
    }
    if (p - buffer > columns) {
      logger ("VID", "Line too long: %d", p - buffer);
      return 0x2000;
    }

    data = memory[addr++];
    switch (data) {
    case 0x7F:
      addr = ((memory[addr] & 0x0F) << 8) | memory[addr + 1];
      return 0x2000 | addr;
      break;
    default:
      if (n >= skip)
        charxy (p - buffer, n - skip, data);
      if (data < 0x20)
        data = '.';
      *p++ = data;
      break;
    }
  }
}

static void refresh (void)
{
  int i;
  u16 addr;

  raise_interrupt (4);

  addr = 0x2000;
  for (i = 0; i < 27; i++) {
    addr = video_line (i, addr);
  }
  
  checkupdate ();
  add_event (2764800/hertz, &refresh_event);
}

void reset_video (void)
{
  register_port (0xA2, video_a2_in, video_a2_out);
  register_port (0xC2, video_c2_in, video_c2_out);

  columns = 80;
  hertz = 60;
  interlace = 0;
  refresh ();
}

int video (void *arg)
{
  register_port (0xA2, video_a2_in, video_a2_out);
  register_port (0xC2, video_c2_in, video_c2_out);
  columns = 80;
  hertz = 60;
  interlace = 0;

  SDL_Delay (1000);
  for (;;) {
    SDL_Delay (1000/hertz);
    refresh ();
  }

  return 0;
}
