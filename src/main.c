#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include "io.h"
#include "deviceList.h"
#include "srec.h"

int getShiftedNum(int nr, int mask)
{
    while((mask & 0x01) == 0x00)
    {
        nr >>= 1;
        mask >>= 1;
    }
    return nr & mask;
}

int main(int argc, char **argv)
{
    int i, j;
    int verify = 0;
    int load = 0;
    int startDevice = 0;
    int ret = 0;
    int writeFuses = 0;
    int newFuse[3] = {-1, -1, -1};
    char* filename = NULL;
    
    for(i=1; i<argc; i++)
    {
        if (strcmp(argv[i], "-v") == 0)
            verify = 1;
        else if (strcmp(argv[i], "-l") == 0)
            load = 1;
        else if (strcmp(argv[i], "-s") == 0)
            startDevice = 1;
        else if (strcmp(argv[i], "-wf") == 0)
            writeFuses = 1;
        else if (strcmp(argv[i], "-f") == 0 && i < argc - 1)
        {
            filename = argv[i+1];
            i++;
        }
        else if (strcmp(argv[i], "-lf") == 0 && i < argc - 1)
        {
            newFuse[0] = strtol(argv[i+1], NULL, 16);
            i++;
        }
        else if (strcmp(argv[i], "-hf") == 0 && i < argc - 1)
        {
            newFuse[1] = strtol(argv[i+1], NULL, 16);
            i++;
        }else{
            printf("Unknown option: %s\n", argv[i]);
            printf("Usage:\n");
            printf("\t-v: verify flash\n");
            printf("\t-l: load flash\n");
            printf("\t-s: start device\n");
            printf("\t-f [filename]: hex file to load/verify\n");
            printf("\t-lf [value]: new low fuse value\n");
            printf("\t-hf [value]: new high fuse value\n");
            printf("\t-wf: write new fuse values\n");
            exit(0);
        }
    }
    
    if (initIO())
        return 1;
    
    TDevice* dev = getDevice();
    if (!dev)
        return 1;
    int fuse[3];
    fuse[0] = ioReadLowFuseByte();
    fuse[1] = ioReadHighFuseByte();
    fuse[2] = ioReadExtendedFuseByte();
    printf("Fuse bytes: %02x %02x %02x\n", fuse[0], fuse[1], fuse[2]);
    if (dev->fuseData != NULL)
    {
        TFuseData* fuseData;
        int lastFuseNr = dev->fuseData->fuseByte;
        for(fuseData = dev->fuseData; fuseData->name; fuseData++)
        {
            if (lastFuseNr != fuseData->fuseByte)
            {
                printf("\n");
                lastFuseNr = fuseData->fuseByte;
            }
            int num = getShiftedNum(fuse[fuseData->fuseByte], fuseData->mask);
            if (fuseData->valueList)
                printf(" %s: %i: %s\n", fuseData->name, num, fuseData->valueList[num]);
            else
                printf(" %s: %i\n", fuseData->name, num);
        }
        
        if (newFuse[0] != -1 || newFuse[1] != -1 || newFuse[2] != -1)
        {
            printf("\nNew fuse values:\n");
            for(fuseData = dev->fuseData; fuseData->name; fuseData++)
            {
                if (newFuse[fuseData->fuseByte] == -1)
                    continue;
                int num = getShiftedNum(newFuse[fuseData->fuseByte], fuseData->mask);
                if (fuseData->valueList)
                    printf(" %s: %i: %s\n", fuseData->name, num, fuseData->valueList[num]);
                else
                    printf(" %s: %i\n", fuseData->name, num);
            }
            if (writeFuses)
            {
                if (newFuse[0] != -1)
                    ioWriteLowFuseByte(newFuse[0]);
                if (newFuse[1] != -1)
                    ioWriteHighFuseByte(newFuse[1]);
            }
        }
    }

    if (filename == NULL)
    {
        if (load || verify)
        {
            printf("Need a filename with -f for load/verify");
            return 1;
        }
    }else{
        TSRec file = readSRecFile(filename);
        if (file.size < 1)
            return 1;

        if (dev->flashSize * 2 < file.size)
        {
            fprintf(stderr, "File is 0x%04x bytes in size, but chip only has 0x%04x bytes flash\n", file.size, dev->flashSize * 2);
            return 1;
        }

        if (load)
        {
            int pageWriteCount = (file.size + ((dev->pageSize * 2) - 1)) / (dev->pageSize * 2);
            printf("File is: 0x%04x bytes in size, need to write: %i pages\n", file.size, pageWriteCount);
            printf("Erasing chip\n");
            
            if (ioEraseChip())
                return 1;
            
            for(i=0;i<pageWriteCount;i++)
            {
                printf("Writing page: %i: 0x%04x 0x%04x\n", i, i * (dev->pageSize * 2), i * (dev->pageSize * 2) + (dev->pageSize * 2) - 1);
                for(j=0;j<dev->pageSize;j++)
                {
                    int offset = i * dev->pageSize * 2;
                    offset += j * 2;
                    if (offset < file.size)
                    {
                        if (ioLoadProgMem(file.data[offset], file.data[offset+1], offset / 2))
                            return 1;
                    }else{
                        if (ioLoadProgMem(0xFF, 0xFF, offset / 2))
                            return 1;
                    }
                }
                if (ioWriteProgPage(i * dev->pageSize))
                    return 1;
            }
        }
        if (verify)
        {
            printf("Verifing flash\n");
            for(i=0;i<file.size/2;i++)
            {
                if (ioReadProgMem(i, &j))
                    return 1;
                if ((j & 0xFF) != file.data[i*2])
                {
                    printf("Error at memory location: 0x%04x: 0x%02x != 0x%02x\n", i * 2, j & 0xFF, file.data[i*2]);
                    ret = 1;
                }
                if (((j >> 8) & 0xFF) != file.data[i*2+1])
                {
                    printf("Error at memory location: 0x%04x: 0x%02x != 0x%02x\n", i * 2 + 1, (j >> 8) & 0xFF, file.data[i*2+1]);
                    ret = 1;
                }
            }
        }
    }
    
    if (startDevice)
    {
        printf("Starting device");
        if (ioStartDevice())
            return 1;
    }
    
    return ret;
}
