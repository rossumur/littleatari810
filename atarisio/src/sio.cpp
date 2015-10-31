/*
 * sio.cpp
 *
 *  Created on: Apr 21, 2011
 *      Author: Peter
 */

#include "Platform.h"
#include "File.h"
#include "Fat32.h"
#include "stdlib.h"


void UART_PutChar(int c);
int UART_GetChar(void);
bool CommandAsserted();
void SetSpeed(int baud);
void SetLED(int on);

bool microSDReady();
void delay(int ms);
uint32_t ticks();

enum FileType
{
	None,
	RUR,
	ATR,
	XFD,
	XEX,
	OBJ,
	BAS,
	COM
};

const char* _fileTypes[] = {
	"None",
	"rur",
	"atr",
	"xfd",
	"xex",
	"obj",
	"bas",
	"com",
	0
};

int tolower(char c)
{
	if (c >= 'A' && c <= 'Z')
		return c - 'A' + 'a';
	return c;
}

int Cmp(const char * s1, const char * s2)
{
	int n;
	for(; (n = (tolower(*s1) - tolower(*s2))) == 0; ++s1, ++s2)
		if(*s1 == 0)
			return 0;
	return n;
}

FileType GetFileType(const char* s)
{
	if (strlen(s) > 3)
	{
		s = s + strlen(s) - 3;
		for (int i = 1; _fileTypes[i]; i++)
			if (Cmp(s,_fileTypes[i]) == 0)
				return (FileType)i;
	}
	return None;
}

void SetDiskImage(int index, const char* name);

//=====================================================================================
//=====================================================================================
//	Bootapp Handler
//	Scan the disk for images to boot

class BootFiles
{
public:
	int _count;
	int _sector;
	u8*	_buffer;
	char _bootApp[16];

	bool AddFile(const char* name)
	{
		if (_sector != -1 && _buffer)
		{
			int s = _count -_sector*8;
			if (s >= 0 && s < 8)	// In the sector we want?
				strcpy((char*)_buffer + s*16,name);
		}
		_count++;
		return false;
	}

	bool ScanFiles(DirectoryEntry* d, int index)
	{

		char name[16];
		FAT_Name(name,d);
		FileType t = GetFileType(name);
		if (t == RUR)
			strncpy(_bootApp,name,16);
		else if (t != None)
			return AddFile(name);
		return false;
	}

	static bool ScanFiles(DirectoryEntry* d, int index, void* ref)
	{
		return ((BootFiles*)ref)->ScanFiles(d,index);
	}

	//
	int	GetSector(int index, u8* buf = 0)
	{
		_sector = index;	// Sector we are looking for
		_bootApp[0] = 0;
		_count = 0;
		_buffer = buf;
		if (buf)
			memset(buf,0,128);
		u8 fs[512];
		FAT_Directory(ScanFiles,fs,this);
		return _count;
	}
};
BootFiles _bootFiles;

//	Patching reading and writing for boot app

#define MAGIC_SECTOR 65
#define DIR_SECTOR (MAGIC_SECTOR+2)
u8 _settings[256];
bool _settingsChanged = false;

//  Header
//  byte sig[4]
//  byte sectors;   // normally 2
//  byte stringLen; // normally 16
//  short total;    // total number of records
//  byte pad[8];
//  char records[10*16];    // boot, alt, disk 1 .. disk 8

void RURWrite(u8* src, int sector)
{
	if (sector == MAGIC_SECTOR)
	{
		memcpy(_settings,src,128);
	}
	else if (sector == (MAGIC_SECTOR+1))
	{
		memcpy(_settings+128,src,128);	// (Double denisty... TODO)
		_settingsChanged = true;
	}
}

void RURRead(u8* src, int sector)
{
	if (sector == MAGIC_SECTOR)
	{
		src[4] = (_bootFiles._count + 7) >> 3;		// # of sectors of names to load
		*((short*)(src+6)) = _bootFiles._count;		// # of records
	}
	else if (sector >= DIR_SECTOR)
	{
		_bootFiles.GetSector(sector-DIR_SECTOR,src);
	}
}

void RURSettings()
{
	if (_settingsChanged)
	{
		_settingsChanged = 0;
		for (int i = 2; i <= 8; i++)
			SetDiskImage(i,(char*)_settings+32 + i*16);	// Disks 1..8
		SetDiskImage(1,(char*)_settings+16);				// Startup disk in drive 1
	}
}

//	KBOOT loader
const u8 _bootloader[] =
{
	// length at offset 9
	0x00,0x03,0x00,0x07,0x14,0x07,0x4C,0x14,0x07,0x12,0x34,0x00,0x00,0xA9,0x46,0x8D,
	0xC6,0x02,0xD0,0xFE,0xA0,0x00,0xA9,0x6B,0x91,0x58,0x20,0xD9,0x07,0xB0,0xEE,0x20,
	0xC4,0x07,0xAD,0x7A,0x08,0x0D,0x76,0x08,0xD0,0xE3,0xA5,0x80,0x8D,0xE0,0x02,0xA5,
	0x81,0x8D,0xE1,0x02,0xA9,0x00,0x8D,0xE2,0x02,0x8D,0xE3,0x02,0x20,0xEB,0x07,0xB0,
	0xCC,0xA0,0x00,0x91,0x80,0xA5,0x80,0xC5,0x82,0xD0,0x06,0xA5,0x81,0xC5,0x83,0xF0,
	0x08,0xE6,0x80,0xD0,0x02,0xE6,0x81,0xD0,0xE3,0xAD,0x76,0x08,0xD0,0xAF,0xAD,0xE2,
	0x02,0x8D,0x70,0x07,0x0D,0xE3,0x02,0xF0,0x0E,0xAD,0xE3,0x02,0x8D,0x71,0x07,0x20,
	0xFF,0xFF,0xAD,0x7A,0x08,0xD0,0x13,0xA9,0x00,0x8D,0xE2,0x02,0x8D,0xE3,0x02,0x20,

	0xAE,0x07,0xAD,0x7A,0x08,0xD0,0x03,0x4C,0x3C,0x07,0xA9,0x00,0x85,0x80,0x85,0x81,
	0x85,0x82,0x85,0x83,0xAD,0xE0,0x02,0x85,0x0A,0x85,0x0C,0xAD,0xE1,0x02,0x85,0x0B,
	0x85,0x0D,0xA9,0x01,0x85,0x09,0xA9,0x00,0x8D,0x44,0x02,0x6C,0xE0,0x02,0x20,0xEB,
	0x07,0x85,0x80,0x20,0xEB,0x07,0x85,0x81,0xA5,0x80,0xC9,0xFF,0xD0,0x10,0xA5,0x81,
	0xC9,0xFF,0xD0,0x0A,0x20,0xEB,0x07,0x85,0x80,0x20,0xEB,0x07,0x85,0x81,0x20,0xEB,
	0x07,0x85,0x82,0x20,0xEB,0x07,0x85,0x83,0x60,0x20,0xEB,0x07,0xC9,0xFF,0xD0,0x09,
	0x20,0xEB,0x07,0xC9,0xFF,0xD0,0x02,0x18,0x60,0x38,0x60,0xAD,0x09,0x07,0x0D,0x0A,
	0x07,0x0D,0x0B,0x07,0xF0,0x79,0xAC,0x79,0x08,0x10,0x50,0xEE,0x77,0x08,0xD0,0x03,

	0xEE,0x78,0x08,0xA9,0x31,0x8D,0x00,0x03,0xA9,0x01,0x8D,0x01,0x03,0xA9,0x52,0x8D,
	0x02,0x03,0xA9,0x40,0x8D,0x03,0x03,0xA9,0x80,0x8D,0x04,0x03,0xA9,0x08,0x8D,0x05,
	0x03,0xA9,0x1F,0x8D,0x06,0x03,0xA9,0x80,0x8D,0x08,0x03,0xA9,0x00,0x8D,0x09,0x03,
	0xAD,0x77,0x08,0x8D,0x0A,0x03,0xAD,0x78,0x08,0x8D,0x0B,0x03,0x20,0x59,0xE4,0xAD,
	0x03,0x03,0xC9,0x02,0xB0,0x22,0xA0,0x00,0x8C,0x79,0x08,0xB9,0x80,0x08,0xAA,0xAD,
	0x09,0x07,0xD0,0x0B,0xAD,0x0A,0x07,0xD0,0x03,0xCE,0x0B,0x07,0xCE,0x0A,0x07,0xCE,
	0x09,0x07,0xEE,0x79,0x08,0x8A,0x18,0x60,0xA0,0x01,0x8C,0x76,0x08,0x38,0x60,0xA0,
	0x01,0x8C,0x7A,0x08,0x38,0x60,0x00,0x03,0x00,0x80,0x00,0x00,0x00,0x00,0x00,0x00
};

//=====================================================================================
//=====================================================================================
//	Disk io


class Disk
{
public:
	File _file;
	int _sectorSize;
	int _paragraphs;
	FileType _fileType;

	Disk() : _fileType(None)
	{
	}

	void Init()
	{
		// Need separate init 'cause constructors are not called in codered/newlib mess
		_file.Init();
	}

	void Flush()
	{
		_file.Flush();
	}

	void Open(const char* path)
	{
		Flush();
		_fileType = GetFileType(path);
		if (path[0] == 0 || !_file.Open(path))
		{
			_sectorSize = 0;
			_fileType = None;
		} else {
			if (_fileType == XFD || _fileType == XEX)
			{
				_paragraphs = 0;
				_sectorSize = 128;
			} else {
				u16 hdr[8];
				_file.Read(&hdr,16);
				_paragraphs = hdr[1];
				_sectorSize = hdr[2];
			}
			printf("Opened %s %d %d\n",path,_paragraphs,_sectorSize);
		}
	}

	bool Present()
	{
		return _fileType != None;
	}

	void Status(u8* data)
    {
        data[0] = _sectorSize == 256 ? 0x30 : 0x10;
        data[1] = 0x00;
        data[2] = 0x10;
        data[3] = 0x00;
    }

	void Configuration(u8* d)
	{
		int sectorSize = GetSectorSize(4);
		int sectors = _file.GetFileLength();
		if (sectorSize == 256)
			sectors += 3*128;
		switch (_fileType)
		{
			case XEX:	sectors += 3*128;	break;
			case ATR:	sectors -= 16;		break;
		}
		if (sectorSize)
			sectors = (sectors + sectorSize -1)/sectorSize;
		else
			sectors = 0;

		memset(d,0,12);
		d[0] = 1;	// tracks;
		d[2] = sectors >> 8;
		d[3] = sectors;
		d[6] = sectorSize >> 8;
		d[7] = sectorSize;
		d[8] = 0xFF;	// Online
	}

    //  First 3 sectors are always 128 bytes
	int GetSectorSize(int sector)
    {
        return sector < 4 ? 0x80 : _sectorSize;
    }

    int GetSectorOffset(int sector)
    {
        if (_fileType == XFD)
            return (sector - 1) * _sectorSize;

        int m = 16;
        if (sector<4)
	        m += (sector-1) * 0x80;
        else
            m += 3 * 0x80 + (sector - 4) * _sectorSize;
        return m;
    }

    int ReadXEX(u8* dst, int sector)
    {
    	//  XEX files are turned into bootable disks prepending a 3 sector bootloader
		if (sector < 4)
		{
			memcpy(dst,_bootloader + (sector-1)*128,128);
			if (sector == 1)
			{
				int len = _file.GetFileLength();
				dst[9] = len;
				dst[10] = len >> 8;
			}
		} else {
			int offset = (sector-4)*128;
			_file.SetPos(offset);
			_file.Read(dst,128);
		}
		return 128;
    }

	int Read(u8* dst, int sector)
	{
		int sectorSize = GetSectorSize(sector);
		int pos = GetSectorOffset(sector);

		if (_fileType == XEX)
			return ReadXEX(dst,sector);

		_file.SetPos(pos);
		_file.Read(dst,sectorSize);
		if (_fileType == RUR)
			RURRead(dst,sector);
		return sectorSize;
	}

	int Write(u8* src, int sector)
	{
		int sectorSize = GetSectorSize(sector);
		int pos = GetSectorOffset(sector);

		if (_fileType == XEX)
			return -1;

		_file.SetPos(pos);
		_file.Write(src,sectorSize);


		if (_fileType == RUR)
		{
			if (sector == (MAGIC_SECTOR+1))
				_file.Flush();
			RURWrite(src,sector);
		}

		return sectorSize;
	}

	//	Format disk!
	void Format()
	{
		int offset = 0;
		if (_fileType == ATR)
			offset = 16;
		else if (_fileType != XFD)
			return;
		u8 z = 0;
		_file.SetPos(offset);
		int len = _file.GetFileLength() - offset;
		while (len--)
			_file.Write(&z,1);
	}
};

Disk _disks[8];
Disk* FindDisk(int d)
{
	return &_disks[d-1];
}

void SetDiskImage(int index, const char* name)
{
	Disk* disk = FindDisk(index);
	disk->Open(name);
}

//=====================================================================================
//=====================================================================================
//	SIO stuff

u8 csum(u8* d, int len)
{
	int c = 0;
	for (int i = 0; i < len; i++)
		c = ((c + d[i]) >> 8) + (u8)(c + d[i]);
	return c;
}

int WaitChar()
{
	int t = 10000000;
	while (t--)
	{
		int i = UART_GetChar();
		if (i != -1)
			return i;
	}
	return -1;
}

//	Wait for a command char
int GetCmdChar()
{
	for (;;)
	{
		if (!CommandAsserted())
			break;
		int i = UART_GetChar();
		if (i != -1)
			return i;
	}
	return -1;
}

void Send(u8 c)
{
	if (CommandAsserted())
		return;
	UART_PutChar(c);
}

void Ack()
{
	delay(1);
	Send('A');
	delay(1);
	Send('C');
}

void Send(u8* data, int len)
{
	u8 c = csum(data,len);
	while (len--)
		Send(*data++);
	Send(c);
}

//	Checksum here?
int Recv(u8* dst, int sectorSize)
{
	for (int i = 0; i < sectorSize; i++)
		dst[i] = WaitChar();
	u8 c = WaitChar();
	if (c != csum(dst,sectorSize))
		return -1;
	return sectorSize;
}

void SIOCommand(u8* cmd)
{
	int device = cmd[0];
	u8 buf[256];

	//	Disks
	if (device >= '1' && device <= '8')
	{
		int sector = (cmd[3] << 8) | cmd[2];
		int sectorSize;

		Disk* disk = FindDisk(device - '0');
		if (!disk)
			return;
		if (!disk->Present())
			return;

		SetLED(1);
		switch (cmd[1])
		{
			case 'S':	// Status
				Ack();
				delay(1);
				disk->Status(buf);
				Send(buf,4);
				break;

			case 'R':	// Read
				delay(1);
				Send('A');
				sectorSize = disk->Read(buf,sector);
				delay(1);
				Send('C');
				Send(buf,sectorSize);
				break;

			case 'P':	// Put
			case 'W':	// Write
				delay(1);
				Send('A');
				sectorSize = disk->GetSectorSize(sector);
				if (Recv(buf,sectorSize) != sectorSize)
				{
					Send('N');
					break;
				}

				delay(2);
				Send('A');
				delay(2);
				if (disk->Write(buf,sector) == sectorSize)
					Send('A');
				else
					Send('E');
				delay(2);
				Send('C');
				RURSettings();
				break;

			case '?':
				Ack();
				buf[0] = 0x8;	// 57600
				Send(buf,1);
				delay(5);
				SetSpeed(57600);
				break;

			case '!':
				delay(1);
				Send('A');

				sectorSize = disk->GetSectorSize(1);
				disk->Format();
				memset(buf,0,sectorSize);
                buf[0] = 0xFF;
                buf[1] = 0xFF;

				Send('C');
				delay(1);
                Send(buf,sectorSize);
				break;

			//	Get Configuration
			case 'N':
				Ack();
				delay(2);
				disk->Configuration(buf);
				Send(buf,12);
				break;

			//	Set Configuration
			case 'O':
				delay(1);
				Send('A');
				Recv(buf,12);
				Ack();
				break;

			default:
				printf("UNHANDLED %c:%c %02X %02X\n",cmd[0],cmd[1],cmd[2],cmd[3]);
				delay(1);
				Send('N');
				break;
		}
		SetLED(0);
	}
	else
	{
		// Poll etc
	}
}


void BootInit()
{
	for (int i = 0; i < 8; i++)
		_disks[i].Init();
	microSDReady();
	_bootFiles.GetSector(0);	// Load first 8 filenames
	if (_bootFiles._bootApp[0])
	{
		SetDiskImage(1,_bootFiles._bootApp);
	}
}

void SIOLoop()
{
	u32 flushTime = 0;	// Flush every 2 seconds
	int i;

	BootInit();
	SetLED(0);

	for (;;)
	{
		while (!CommandAsserted())
		{
			UART_GetChar();		// Dump until next command

			if (ticks() > flushTime)
			{
				flushTime = ticks() + 2*1000;	// Flush writes every 2 seconds
				for (i = 0; i < 8; i++)
					_disks[i].Flush();
			}
		}

		u8 buf[5];
		for (i = 0; i < 5; i++)
			buf[i] = GetCmdChar();

		while (CommandAsserted())
			;

		if (csum(buf,4) == buf[4])
			SIOCommand(buf);	// Call good command
		else
			SetSpeed(19200);	// Fall back if the commands get janky
	}
}
