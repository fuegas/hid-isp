#ifndef IO_H
#define IO_H

typedef unsigned char uchar;

//Initialize the USB, and put the device in program mode.
int initIO();

//Get the signature bytes, supply a buffer with the size of 2
int ioGetSignature(uchar* signature);

//Wait till the chip is ready.
int ioWaitReady();

int ioEraseChip();

int ioLoadProgMem(uchar lowData, uchar hiData, int addr);
int ioWriteProgPage(int addr);
int ioReadProgMem(int addr, int* data);

int ioReadLowFuseByte();
int ioReadHighFuseByte();
int ioReadExtendedFuseByte();

int ioWriteLowFuseByte(int data);
int ioWriteHighFuseByte(int data);
int ioWriteExtendedFuseByte(int data);

int ioStartDevice();

#endif//IO_H
