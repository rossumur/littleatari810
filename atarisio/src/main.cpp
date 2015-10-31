/*
===============================================================================
 Name        : main.cpp
 Author      : Peter Barrett
 Version     :
 Copyright   : (C) Copyright Peter Barrett
 Description : main definition
===============================================================================
*/

#include "Platform.h"
#include "MMC.h"
#include "Fat32.h"
#include "File.h"
#include "stdarg.h"

//	Minimal semihost
//	Need to force semihosting On in Debug As/Debug Configurations/Debugger/Target Config
int WRITE2(int file, const char *str, int len)
{
	asm volatile (
		".syntax unified\n"
			"push	{r2}\n"
			"push	{r1}\n"
			"push	{r0}\n"
			"movs	r0, #5\n"	// write
			"mov	r1, sp\n"
			"bkpt	0x00ab\n"
			"add	sp, #12\n"
		".syntax divided\n"
	);
}

int _slen =0;
char _sbuffer[128];
int WRITE(int file, const char *str, int len)
{
	while (len--)
	{
		_sbuffer[_slen++] = *str++;
		if (str[-1] == '\n' || _slen == sizeof(_sbuffer))
		{
			WRITE2(file,_sbuffer,_slen);
			_slen = 0;
		}
	}
	return 0;
}

void writesn(const char* s, int n)
{
	if (n)
		WRITE(0,s,n);
}
void writes(const char* s)
{
	if (s)
		WRITE(0,s,strlen(s));
}

void writec(char c)
{
	WRITE(0,&c,1);
}

const char _hex[] = "0123456789ABCDEF";	// Smaller than instructions to add
//	works for bases 2..16
//	hex always unsigned
char HEX(int n)
{
	if (n >= 10)
		return 'A' + n-10;
	return n + '0';
}

char* itoa(int n, char* dst, int base, char padChar, char padCount)
{
    char s[32];
    int i = 0;
    char* r = dst;
    if (base == 16)
    {
    	unsigned int un = n;
		do {
			s[i++] =HEX(un & 0xF);
		} while ((un >>= 4) > 0);
    } else {
	    if (n < 0)
	    {
	    	*r++ = '-';
	        n = -n;
	    }
		do {
			s[i++] = HEX(n % base);	// long divide. eww
		} while ((n /= base) > 0);
    }
    while (i < padCount && i < (int)sizeof(s))
    	s[i++] = padChar;
    while (i--)
    	*r++ = s[i];
    *r++ = 0;
    return dst;
}

extern "C"
int xprintf(const char *fmt, ...)
{
	const char* m = fmt;
	const char *p;
	va_list argp;
	int i;
	char fmtbuf[256];

	va_start(argp, fmt);

	for(p = fmt; *p != '\0'; p++)
	{
		if (*p != '%')
			continue;
		writesn(m,p-m);
		p++;

		char c;
		char padChar = ' ';
		char padCount = 0;
		if (p[0] == '0')
		{
			padChar = '0';
			++p;
		}

		c = p[0];
		while ((c >= '0') && (c <= '9'))
		{
			padCount = padCount*10 + (c - '0');
			c = *++p;
		}

		switch(c)
		{
			case 'c':
				i = va_arg(argp, int);
				writec(i);
				break;

			case 'd':
				i = va_arg(argp, int);
				writes(itoa(i, fmtbuf, 10,padChar, padCount));
				break;

			case 's':
				writes(va_arg(argp, char *));
				break;

			case 'X':
			case 'x':
				i = va_arg(argp, int);
				writes(itoa(i, fmtbuf, 16,padChar, padCount));
				break;

			default:
				writec(c);
				break;
		}
			m = p+1;
	}
	va_end(argp);
	writesn(m,p-m);
	return 0;
}

//==========================================================================
//==========================================================================

#define CLR(_p,_b) _p->MASKED_ACCESS[1 << _b] = 0
#define SET(_p,_b) _p->MASKED_ACCESS[1 << _b] = -1
typedef unsigned char u8;


/* SSP CR1 register */
#define SSPCR1_LBM		(1 << 0)
#define SSPCR1_SSE		(1 << 1)
#define SSPCR1_MS		(1 << 2)
#define SSPCR1_SOD		(1 << 3)

#define SSP_BUFSIZE		16
#define FIFOSIZE		8

void SSPInit( void )
{
  uint8_t i, Dummy;

  LPC_SYSCON->PRESETCTRL |= (0x1<<0);
  LPC_SYSCON->SYSAHBCLKCTRL |= (1<<11);
  LPC_SYSCON->SYSTICKCLKDIV  = 0x02;			/* Divided by 2 */
  LPC_IOCON->PIO0_8 &= ~0x07;	/*  SSP I/O config */
  LPC_IOCON->PIO0_8 |= 0x01;	/* SSP MISO */
  LPC_IOCON->PIO0_9  &= ~0x07;
  LPC_IOCON->PIO0_9 |= 0x01;	/* SSP MOSI */

  LPC_IOCON->SCK_LOC = 0x02;
  LPC_IOCON->PIO0_6 = 0x02;		/* P0.6 function 2 is SSP clock, need to combined with IOCONSCKLOC register setting */

  LPC_IOCON->PIO0_2 &= ~0x07;		/* SSP SSEL is a GPIO pin */

  /* port0, bit 2 is set to GPIO output and high */
  LPC_GPIO0->DIR |= 1 << 2;
  SET(LPC_GPIO0,2);

  LPC_SSP0->CR0 = 0x01C7;	// 8-bit, 12Mhz CPOL = 1, CPHA = 1
	LPC_SSP0->CR0 = 0x00C7;	// 8-bit, 24Mhz CPOL = 1, CPHA = 1

  /* SSPCPSR clock prescale register, master mode, minimum divisor is 0x02 */
  LPC_SSP0->CPSR = 0x2;

  for ( i = 0; i < FIFOSIZE; i++ )
	Dummy = LPC_SSP0->DR;		/* clear the RxFIFO */

  /* Master mode */
  LPC_SSP0->CR1 = SSPCR1_SSE;
}

#define SSPSR_BSY		(1 << 4)

u8 spiTransferByte(u8 data)
{
	LPC_SSP0->DR = data;
	while ( LPC_SSP0->SR & SSPSR_BSY )
		;
	data = LPC_SSP0->DR;
	return data;
}

void SPI_SS_LOW()
{
	CLR(LPC_GPIO0,2);
}
void SPI_SS_HIGH()
{
	SET(LPC_GPIO0,2);
}

u8 SPI_ReceiveByte(u8 data)
{
	LPC_SSP0->DR = data;
	while ( LPC_SSP0->SR & SSPSR_BSY )
		;
	data = LPC_SSP0->DR;
	return data;
}

void SPI_Init() {};
void SPI_Enable() {};
void SPI_Disable() {};

void SPI_Send(u8* data, int len)
{
	while (len--)
		SPI_ReceiveByte(*data++);
}

//	Receives a sector including CRC
void SPI_Receive(u8* data, int len)
{
	while (len--)
		*data++ = SPI_ReceiveByte(0xFF);
	SPI_ReceiveByte(0xFF);
	SPI_ReceiveByte(0xFF);	// Strip CRC
}

void MMC_SS_LOW()
{
	CLR(LPC_GPIO0,2);
}

void MMC_SS_HIGH()
{
	SET(LPC_GPIO0,2);
}

void SetLED(int on)
{
	if (on)
		CLR(LPC_GPIO0,4);
	else
		SET(LPC_GPIO0,4);
}

//==========================================================================
//==========================================================================


#define IER_RBR	0x01
#define IER_THRE	0x02
#define IER_RLS	0x04

#define IIR_PEND	0x01
#define IIR_RLS	0x03
#define IIR_RDA	0x02
#define IIR_CTI	0x06
#define IIR_THRE	0x01

#define LSR_RDR	0x01
#define LSR_OE		0x02
#define LSR_PE		0x04
#define LSR_FE		0x08
#define LSR_BI		0x10
#define LSR_THRE	0x20
#define LSR_TEMT	0x40
#define LSR_RXFE	0x80


void UART_PutChar(int c)
{
	while (!(LPC_UART->LSR & 0x20))
		;
    LPC_UART->THR = c;
}

int UART_GetChar(void)
{
  	if (LPC_UART->LSR & 0x01)
  		return LPC_UART->RBR;
  	return -1;
}

// TODO: insert include files here
void UARTInit(int baudrate)
{
  uint32_t Fdiv;
  uint32_t regVal;

  LPC_IOCON->PIO1_6 &= ~0x07;    /*  UART I/O config */
  LPC_IOCON->PIO1_6 |= 0x01;     /* UART RXD */
  LPC_IOCON->PIO1_7 &= ~0x07;
  LPC_IOCON->PIO1_7 |= 0x01;     /* UART TXD */

  /* Enable UART clock */
  LPC_SYSCON->SYSAHBCLKCTRL |= (1<<12);
  LPC_SYSCON->UARTCLKDIV = 0x1;     /* divided by 1 */

  LPC_UART->LCR = 0x83;             /* 8 bits, no Parity, 1 Stop bit */
  regVal = LPC_SYSCON->UARTCLKDIV;

  Fdiv = (((12000000*LPC_SYSCON->SYSAHBCLKDIV)/regVal)/16)/baudrate ;	/*baud rate */

  LPC_UART->DLM = Fdiv >> 8;
  LPC_UART->DLL = (uint8_t)Fdiv;
  LPC_UART->LCR = 0x03;		/* DLAB = 0 */
  LPC_UART->FCR = 0x07;		/* Enable and reset TX and RX FIFO. */

  /* Read to clear the line status. */
  regVal = LPC_UART->LSR;

  /* Ensure a clean start, no data in either TX or RX FIFO. */
// CodeRed - added parentheses around comparison in operand of &
  while (( LPC_UART->LSR & (LSR_THRE|LSR_TEMT)) != (LSR_THRE|LSR_TEMT) );

  while ( LPC_UART->LSR & LSR_RDR )
	regVal = LPC_UART->RBR;	/* Dump data from RX FIFO */

}

#define SYSPLLCLKSEL_Val      0x00000000	// RC = 0, 1 for crystal
#define SYSPLL_SETUP          1
#define SYSPLLCTRL_Val        0x00000023
#define MAINCLKSEL_Val        0x00000000
#define SYSAHBCLKDIV_Val      0x00000001
#define AHBCLKCTRL_Val        0x0001005F
#define SSP0CLKDIV_Val        0x00000001
#define UARTCLKDIV_Val        0x00000001
#define SSP1CLKDIV_Val        0x00000001

void ClockInit()
{
	LPC_SYSCON->SYSMEMREMAP = 0x03;
	LPC_SYSCON->SYSAHBCLKDIV = 0x01;
	LPC_SYSCON->MAINCLKSEL = 0x00;
	LPC_SYSCON->MAINCLKUEN = 0x00;
	LPC_SYSCON->SYSOSCCTRL = 0x00;
	LPC_SYSCON->SYSAHBCLKCTRL |= 0x5F;

	LPC_SYSCON->MAINCLKSEL    = MAINCLKSEL_Val;     /* Select PLL Clock Output  */
	LPC_SYSCON->MAINCLKUEN    = 0x01;               /* Update MCLK Clock Source */
	LPC_SYSCON->MAINCLKUEN    = 0x00;               /* Toggle Update Register   */
	LPC_SYSCON->MAINCLKUEN    = 0x01;
	while (!(LPC_SYSCON->MAINCLKUEN & 0x01));       /* Wait Until Updated       */

	LPC_SYSCON->SYSAHBCLKDIV  = SYSAHBCLKDIV_Val;
	LPC_SYSCON->SYSAHBCLKCTRL = AHBCLKCTRL_Val;
	LPC_SYSCON->SSP0CLKDIV    = SSP0CLKDIV_Val;
	LPC_SYSCON->UARTCLKDIV    = UARTCLKDIV_Val;
	LPC_SYSCON->SSP1CLKDIV    = SSP1CLKDIV_Val;
}

bool CommandAsserted()
{
	return (LPC_GPIO1->DATA & (1 << 5)) == 0;
}

bool DirectoryP(DirectoryEntry* d, int index, void* ref)
{
	char name[16];
	FAT_Name(name,d);
	printf("> %s\n",name);
	return false;
}

bool _microSDReady = false;

bool microSDReady()
{
	union {
		u32 l[128];
		u8 b[512];
	};
	if (!_microSDReady)
	{
		u8 e = MMC_Init();
		if (e != 0)
			printf("MMC Failed %d\n",e);
		else {
			FAT_Init(b,MMC_ReadSector);
			//FAT_Directory(DirectoryP,b,0);
			_microSDReady = true;
		}
	}
}

void SIOLoop();


volatile uint32_t _ticks;                            /* counts 1ms timeTicks */
extern "C"
void SysTick_Handler(void)
{
	_ticks++;                        /* increment counter necessary in Delay() */
}

void delay(int ms)
{
	uint32_t t = ms + _ticks;
	while (_ticks < t)
		;
}

uint32_t ticks()
{
	return _ticks;
}

int _speed = 19200;
void SetSpeed(int baud)
{
	if (_speed == baud)
		return;
	_speed = baud;
	UARTInit(baud);
}

int main(void)
{
	LPC_GPIO1->DIR |= 1 << 7;
	ClockInit();

	LPC_GPIO0->DIR |= 1 << 4;
	SetLED(1);

	if (SysTick_Config(12000000 / 1000))
		while (1);
	if (!(SysTick->CTRL & (1<<SysTick_CTRL_CLKSOURCE_Msk)))
		LPC_SYSCON->SYSTICKCLKDIV = 0x08;

	UARTInit(19200);
	SSPInit();

	printf("Atari 810 micro\n");
	microSDReady();

	SIOLoop();
	return 0;
}
