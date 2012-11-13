#ifndef SREC_H
#define SREC_H

typedef struct
{
    int size;
    unsigned char* data;
} TSRec;

TSRec readSRecFile(const char* filename);

#endif//SREC_H
