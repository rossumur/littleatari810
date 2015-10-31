

#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "dio.h"

typedef unsigned char byte;

#define min(_a,_b) ((_a) < (_b) ? (_a) : (_b))

char gItems[16*1024];	// Static buffer for Items?
byte* gScreen;

int _total;
int _current = -1;
dhandle_t _drive;

typedef struct
{
	byte	sig[4];
	byte	sectors;
	byte	strLength;
	short	total;
	byte	pad[8];
	char	strings[240];	// selected,spare,disk1..disk4
} Header;
Header _header;

#define MAGICSECTOR 64

char* GetDriveName(int index)
{
	return _header.strings + (index+2)*16;
}

char* GetItem(int index)
{
	if (!gItems)
		return "";
	return gItems + index*16;
}

signed char GetDrive(int item)
{
	byte b;
	const char* name = GetItem(item);
	for (b = 0; b < 8; b++)
		if (strncmp(name,GetDriveName(b),16) == 0)
			return b;
	return -1;
}

int FindItem(char* name)
{
	int i;
	for (i = 0; i < _total; i++)
		if (strncmp(name,GetItem(i),16) == 0)
			return i;
	return -1;
}

//  ATASCII To screen
//  0-31	64-95
//  32-95	0-63
//  96-127	96-127

void Draw(const char* src, byte* dst)
{
    byte i;
    for (i = 0; i < 12; i++)
    {
        char c = src[i];
        if (c == 0)
            break;
        if (c >= 32 && c < 96)
            c -= 32;
        dst[i] = c;
    }
}

#define PERCOL 18
#define PERPAGE (PERCOL*2)

#if 0

static char _hex[] = "0123456789ABCDEF";
byte* _console;
char Hex(byte c)
{
	return _hex[c & 0xF];
}

void PrintChar(char c)
{
	char s[2];
	s[1] = 0;
	s[0] = c;
	Draw(s,_console++);
}

void PrintHex(int c)
{
	PrintChar(Hex(c >> 12));
	PrintChar(Hex(c >> 8));
	PrintChar(Hex(c >> 4));
	PrintChar(Hex(c >> 0));
}
#endif

byte* ItemAddr(int i)
{
	byte* screen = gScreen + 40*3;
    if (i < 0)
        return 0;
    while (i >= PERPAGE)
        i -= PERPAGE;
    while (i >= PERCOL)
    {
        i -= PERCOL;
        screen += 20;
    }
    screen += i*40;
	return screen;
}

int _page = -1;
void DrawPage(int i)
{
	signed char drive;
    int count;
    
    if (_page == i)
        return;
        
    if (_page != -1)
        memset(gScreen + 3*40,0,18*40); // Clear last page
    _page = i;
    i *= PERPAGE;
        
    count = i + PERPAGE;
    count = min(_total,count);
    for (; i < count; i++)
    {
		byte* s = ItemAddr(i);
        Draw(GetItem(i),s+2);

		drive = GetDrive(i);
		if (drive != -1)
		{
			s[1] = drive + '1' - 32;
			if (i != _current)
				s[1] |= 0x80;
		}
    }
}

//	Clear all but current selection
void LeaveSelection()
{
	byte* c = ItemAddr(_current);
	memset(gScreen,0,c-gScreen);
	c += 20;
	memset(c,0,gScreen+40*24-c);
}

void Hilite(int i)
{
    byte b;
	byte* screen = ItemAddr(i);
	if (!screen)
		return;
    for (b = 1; b < (1 + 16 + 1); b++)
        screen[b] ^= 0x80;
}

//  instead of the 2k lib
char* toa(int a, char* d)
{
    char b[16];
    byte i = 0;
 //   bool n = a < 0;
 //   if (a < 0)
 //       a = -a;
    while (a)
    {
        b[i++] = (a % 10) + '0';
        a /= 10;
    }
 //   if (n)
 //       b[i++] = '-';
    while (i--)
        *d++ = b[i];
    d[0] = 0;
    return d;
}

void ShowItemNumber()
{
    char str[32];
    char* s;
    strcpy(str,"    ");
    s = toa(_current+1,str+4);
    strcpy(s," of ");
    s = toa(_total,s+4);
    Draw(str,gScreen + 38 + 22*40 - (s-str));
}

void Select(int i)
{
    Hilite(_current);
    DrawPage(i/PERPAGE);
    _current = i;
    Hilite(_current);
    ShowItemNumber();
}

void Move(int delta)
{
    int i = _current + delta;
    if (i < 0)
        return;
	if (i >= _total)
		i = _total-1;
    Select(i);
}

void SelectItem()
{
	byte* b = (byte*)&_header;
    LeaveSelection();
	strncpy(_header.strings,GetItem(_current),16);
    dio_write(_drive,MAGICSECTOR+1,b);
    dio_write(_drive,MAGICSECTOR+2,b+128);
    asm("jmp $E477");   // Coldstart
}

void Load()
{
    byte* screen = gScreen;
    int sector = MAGICSECTOR+1;
    int sectorCount,i;
	byte* b;

	//_console = gScreen;

    //  read header
    _drive = dio_open(0);
	b = (byte*)&_header;
    dio_read(_drive,sector++,b);
    dio_read(_drive,sector++,b+128);
    _total = _header.total;

    //  read all menu sectors
    sectorCount = (_header.total + 7) >> 3; // 8 per sector
	sectorCount = min(sectorCount,sizeof(gItems)/128);	// Static items
  //  gItems = (char*)malloc(sectorCount*128);
	{
		char* items = gItems;
		for (i = 0; i < sectorCount; i++)
		{
			dio_read(_drive,sector++,items);
			items += 128;
		}
	}

    Draw("Select Image",gScreen + 40 + 2);
	i = FindItem(_header.strings);
	if (i == -1)
		i = 0;
    Select(i);
}

void Disk(byte driveNum)
{
	int i = FindItem(GetDriveName(driveNum-1));
	if (i != -1)
		ItemAddr(i)[1] = 0;
	ItemAddr(_current)[1] = '0' + driveNum - 32;
	strncpy(GetDriveName(driveNum-1),GetItem(_current),16);
}

void Eject()
{
	signed char driveIndex = GetDrive(_current);
	if (driveIndex != -1)
	{
		ItemAddr(_current)[1] = 0x80;
		*GetDriveName(driveIndex) = 0;
	}
}

#define TIMEOUT 30000
//  

#define AKEY_ESC		0x1c
#define AKEY_0			0x32
#define AKEY_1			0x1f
#define AKEY_2			0x1e
#define AKEY_3			0x1a
#define AKEY_4			0x18
#define AKEY_5			0x1d
#define AKEY_6			0x1b
#define AKEY_7			0x33
#define AKEY_8			0x35
#define AKEY_9			0x30
#define AKEY_SPACE		0x21
#define AKEY_TAB		0x2c

static byte _numKeys[8] = { AKEY_1,AKEY_2,AKEY_3,AKEY_4,AKEY_5,AKEY_6,AKEY_7,AKEY_8 };

void testio()
{
	byte* d = 0;
	byte b = 4;
	while (b--)
		*((byte*)(0xD270)) = d[b];
}

int main()
{

	long timeout = TIMEOUT;
    byte* ch = (unsigned char*)0x2FC;  // Keys
    gScreen = (unsigned char*)(*((int*)88));  // Screen
    gScreen[2] = 0;  // clear boot cursor
    Load();
	testio();
    for (;;)
    {
        char c = *ch;
		//if (timeout-- < 0)
		//	SelectItem();
        if (c != 0xFF)
        {
			timeout = TIMEOUT;
            switch (c)
            {
                case 14:    Move(-1);       break;  // up
                case 15:    Move(1);        break;  // down
                case 6:     Move(-PERCOL);  break;  // left
                case 7:     Move(PERCOL);   break;  // right
                case 12:    SelectItem();   break;  // 33 is space

				default:
				{
					byte k;
					for (k = 0; k < 8; k++)
						if (_numKeys[k] == c)
							Disk(k+1);
				}

            }
            *ch = 0xFF;
        }
    }
    return 0;
}