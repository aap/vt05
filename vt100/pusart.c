#include "vt100.h"

// Intel 8251 USART.

#define XMIT 0x01

/* Status bits. */
#define TX_EMPTY    0x01
#define RX_RDY      0x02
#define TX_RDY      0x04
#define ERR_PARITY  0x08
#define ERR_ORUN    0x10
#define ERR_FRAME   0x20
#define SYNDET      0x40
#define DSR         0x80

/* Mode bits. */
#define DIVISOR   0x03
#define LENGTH    0x0C
#define PENABLE   0x10
#define EVENP     0x20
#define STOPBITS  0xC0

/* Command bits. */
#define TX_ENABLE  0x01
#define DTR        0x02
#define RX_ENABLE  0x04
#define SBRK       0x08
#define ERESET     0x10
#define RTS        0x20
#define IRESET     0x40
#define HUNT       0x80

static u8 rx_baud;
static u8 tx_baud;
static u8 tx_shift;
static u8 rx_data;
static u8 tx_data;
static u8 cmd;
static u8 mode;
static u8 status;
static void (*handle) (u8 data);
static void set_mode (u8 data);

static float rate[16] = {
  50, 75, 110, 134.5, 150, 200, 300, 600, 1200,
  1800, 2000, 2400, 3600, 4800, 9600, 19200
};
static int rx_cycles_per_character;
static int tx_cycles_per_character;

void pusart_rx (u8 data)
{
  if ((cmd & RX_ENABLE) == 0) {
    logger ("UART", "RX character %02X (discarded)", data); 
    return;
  }

  logger ("UART", "RX character %02X", data); 

  if (status & RX_RDY) {
    logger ("UART", "RX overrun"); 
    status |= ERR_ORUN;
  }

  rx_data = data;
  status |= RX_RDY;
  raise_interrupt (2);
}

static u8 pusart_in_data (u8 port)
{
  logger ("UART", "IN rx data %02X", rx_data); 
  clear_interrupt (2);
  status &= ~RX_RDY;
  return rx_data;
}

static void tx_empty (void);
static EVENT (tx_event, tx_empty);

static void tx_start (void)
{
  tx_shift = tx_data;
  status &= ~TX_EMPTY;
  status |= TX_RDY;
  vt100_flags |= XMIT;
  add_event (tx_cycles_per_character, &tx_event);
}

static void tx_empty (void)
{
  sendchar (tx_shift);
  if (status & TX_RDY)
    status |= TX_EMPTY;
  else
    tx_start ();
}

static void command (u8 data)
{
  cmd = data;
  if (data & TX_ENABLE) {
    logger ("UART", "tx enable", data);
  }
  if (data & DTR) {
    logger ("UART", "dtr", data);
  }
  set_dtr (data & DTR);
  if (data & RX_ENABLE) {
    logger ("UART", "rx enable", data);
  }
  if (data & SBRK) {
    logger ("UART", "break", data);
  }
  if (data & ERESET) {
    logger ("UART", "clear errors", data);
    status &= ~(ERR_ORUN | ERR_FRAME | ERR_PARITY);
  }
  if (data & RTS) {
    logger ("UART", "rts", data);
  }
  if (data & HUNT) {
    logger ("UART", "hunt", data);
  }
  if (data & IRESET) {
    logger ("UART", "reset", data);
    handle = set_mode;
  }
}

static int compute_rate (float rate)
{
  float cycles_per_bit = 44236800.0 / rate;
  float bits_per_character = 5 + ((mode & LENGTH) >> 2);
  switch (mode & DIVISOR) {
  case 0x00: break;
  case 0x01: cycles_per_bit /= 1.0; break;
  case 0x02: cycles_per_bit /= 16.0; break;
  case 0x03: cycles_per_bit /= 64.0; break;
  }
  switch (mode & STOPBITS) { 
  case 0x00: break;
  case 0x40: bits_per_character += 1; break;
  case 0x80: bits_per_character += 1.5; break;
  case 0xC0: bits_per_character += 2; break;
  } 
  return (int)(cycles_per_bit * bits_per_character + .4);
}

static void set_mode (u8 data)
{
  logger ("UART", "set mode %02X", data); 
  mode = data;
  handle = command;
  rx_cycles_per_character = compute_rate (rate[rx_baud]);
  tx_cycles_per_character = compute_rate (rate[tx_baud]);
}

static void pusart_out_data (u8 port, u8 data)
{
  if ((cmd & TX_ENABLE) == 0) {
    logger ("UART", "OUT tx data %02X (discarded)", data); 
    return;
  }

  logger ("UART", "OUT tx data %02X", data); 

  tx_data = data;
  status &= ~TX_RDY;
  vt100_flags &= ~XMIT;

  if (status & TX_EMPTY)
    tx_start ();
}

static u8 pusart_in_status (u8 port)
{
  logger ("UART", "IN status %02X", status); 
  return status;
}

static void pusart_out_command (u8 port, u8 data)
{
  handle (data);
}

static u8 baud_in (u8 port)
{
  // This port is write only.
  return 0;
}

static void baud_out (u8 port, u8 data)
{
  if (data != ((tx_baud << 4) | rx_baud))
    logger ("BAUD", "RX %d, TX %d", rate[data & 0x0F], rate[data >> 4]);
  rx_baud = data & 0x0F;
  tx_baud = data >> 4;
  rx_cycles_per_character = compute_rate (rate[rx_baud]);
  tx_cycles_per_character = compute_rate (rate[tx_baud]);
}

void reset_pusart (void)
{
  register_port (0x00, pusart_in_data, pusart_out_data);
  register_port (0x01, pusart_in_status, pusart_out_command);
  register_port (0x02, baud_in, baud_out);
  rx_baud = 0;
  tx_baud = 0;
  vt100_flags |= XMIT;
  status = TX_RDY | TX_EMPTY | DSR;
  cmd = 0;
  set_mode (0x8E);
  handle = set_mode;
}
