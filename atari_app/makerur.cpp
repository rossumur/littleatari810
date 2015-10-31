//
//  makerur.cpp
//  makerur
//
//  Created by Peter Barrett on 10/30/11.
//  Copyright (c) 2011 Peter Barrett. All rights reserved.
//

#include <stdlib.h>
#include <stdio.h>
#include <memory.h>

#define RECORDS 128
#define DISK_SIZE (64 + 2 + (RECORDS*16/128))

typedef struct {
    short magic;
    short paragraphs;
    short secSize;
    char pad[10];
} atr;

typedef struct {
    char sig[4];
    char sectors;
    char stringlen;
    short total;
    char pad[8];
    char records[10*16];    // boot, alt, disk 1 .. disk 8
} rur;

int usage()
{
    fprintf(stderr,"makerur app.xex [boot.rur]\n");
    return -1;
}

uint8_t boot[]
{
    0x00,0x00,0x00,0x07,0x77,0xE4   //
};

int main(int argc, const char * argv[])
{
    const char* o = "boot.rur";
    char records[RECORDS*16] = {0};
    for (int i = 0; i < RECORDS; i++)
        sprintf(records + i*16,"ATR/XEX.%d",i);

    atr hdr = {0};
    hdr.magic = 0x296;
    hdr.paragraphs = DISK_SIZE << 4;
    hdr.secSize = 128;

    if (argc < 2)
        return usage();
    FILE* src = fopen(argv[1],"rb");
    if (!src)
        return usage();

    if (argc == 3)
        o = argv[2];
    FILE* dst = fopen(o,"wb");
    if (!dst)
        return usage();

    // read xex
    fseek(src, 0L, SEEK_END);
    size_t sz = ftell(src);
    fseek(src, 0L, SEEK_SET);
    char* xex = new char[sz];
    fread(xex,1,sz,src);
    fclose(src);

    size_t len = (sz-7) + 6;
    boot[1] = (len + 127)/128; // # of sectors in xex

    fwrite(&hdr,1,16,dst);      // atr
    fwrite(boot,1,6,dst);       // boot record
    fwrite(xex+7,1,sz-7,dst);   // code

    // pad
    char blank[128] = {0};
    size_t mark = sz-1;
    while (mark < 64*128) {
        fwrite(blank,1,1,dst);
        mark++;
    }

    // this stuff is just for testing; will be synthesied by mcu
    rur rhdr = {0};
    rhdr.sig[0] = 'R';
    rhdr.sig[1] = 'U';
    rhdr.sig[2] = 'R';
    rhdr.sig[3] = ' ';
    rhdr.sectors = 2;
    rhdr.total = RECORDS;
    rhdr.stringlen = 16;
    fwrite(&rhdr,1,128,dst);
    fwrite(blank,1,128,dst);
    fwrite(records,1,sizeof(records),dst);

    fclose(dst);
    return 0;
}
