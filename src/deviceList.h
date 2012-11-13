#ifndef DEVICELIST_H
#define DEVICELIST_H

typedef struct
{
    const char* name;
    const char** valueList;
    int fuseByte;
    int mask;
} TFuseData;

typedef struct
{
    const char* name;
    const uchar signature[2];
    int flashSize;  //Flash size, in words
    int pageSize;   //Page size, in words
    TFuseData* fuseData;
} TDevice;

extern TDevice deviceList[];

TDevice* getDevice();

#endif//DEVICELIST_H
