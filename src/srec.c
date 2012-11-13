#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "srec.h"

int hexValue(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'A' && c <= 'F')
        return c + 10 - 'A';
    if (c >= 'a' && c <= 'f')
        return c + 10 - 'a';
    return 0;
}
int hexValue2(char* c)
{
    return (hexValue(c[0]) << 4) | hexValue(c[1]);
}
int hexValue4(char* c)
{
    return (hexValue2(c) << 8) | hexValue2(c+2);
}

TSRec readSRecFile(const char* filename)
{
    int i;
    char buffer[1024];
    TSRec ret;
    ret.size = 0;
    ret.data = NULL;
    
    FILE* f = fopen(filename, "r");
    if (!f)
        return ret;
    
    while(fgets(buffer, 1024, f))
    {
        if (buffer[0] == ':')
        {
            //Intel HEX format
            int count = hexValue2(buffer + 1);
            int addr = hexValue4(buffer + 3);
            int type = hexValue2(buffer + 7);
            int checksum = count + (addr & 0xFF) + (addr >> 8) + type;
            switch(type)
            {
            case 0x00://Data
                if (addr + count > ret.size)
                {
                    ret.size = addr + count;
                    ret.data = (unsigned char*)realloc(ret.data, ret.size);
                }
                for(i=0;i<count;i++)
                {
                    ret.data[addr] = hexValue2(buffer + 9 + i * 2);
                    checksum += ret.data[addr];
                    addr++;
                }
                checksum = (~(checksum - 1)) & 0xFF;
                if (checksum != hexValue2(buffer + 9 + count * 2))
                {
                    fprintf(stderr, "Checksum mismatch in %s\n", filename);
                    free(ret.data);
                    ret.size = 0;
                    ret.data = NULL;
                    return ret;
                }
                break;
            }
        }
    }
    
    fclose(f);
    return ret;
}
