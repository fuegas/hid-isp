#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include "usbcalls.h"
#include "io.h"

#define IDENT_VENDOR_NUM        0x16c0
#define IDENT_VENDOR_STRING     "obdev.at"
#define IDENT_PRODUCT_NUM       1503
#define IDENT_PRODUCT_STRING    "HIDBoot"

#define IO_CMD_SET_BUFFER(p1,p2,p3,p4) { buffer[0] = p1; buffer[1] = p2; buffer[2] = p3; buffer[3] = p4; }

#define DEBUG_PRINT(...)

static usbDevice_t *device;

char *usbErrorMessage(int errCode)
{
    static char buffer[80];

    switch(errCode){
        case USB_ERROR_ACCESS:      return "Access to device denied";
        case USB_ERROR_NOTFOUND:    return "The specified device was not found";
        case USB_ERROR_BUSY:        return "The device is used by another application";
        case USB_ERROR_IO:          return "Communication error with device";
        default:
            sprintf(buffer, "Unknown USB error %d", errCode);
            return buffer;
    }
    return NULL;    /* not reached */
}

int ioUSBLowLvlCommand(unsigned char cmdNr, unsigned char* sendRecvBuffer)
{
    static uchar seq = 0x3a;
    uchar buffer[9];
    int len, err;

    buffer[0] = 2;
    buffer[1] = seq;
    buffer[2] = cmdNr;
    buffer[3] = sendRecvBuffer[0];
    buffer[4] = sendRecvBuffer[1];
    buffer[5] = sendRecvBuffer[2];
    buffer[6] = sendRecvBuffer[3];
    if((err = usbSetReport(device, USB_HID_REPORT_TYPE_FEATURE, (char*)buffer, 9)) != 0)
    {
        fprintf(stderr, "Error usbSetReport: %s\n", usbErrorMessage(err));
        return err;
    }
    DEBUG_PRINT("Send: %02x %02x %02x %02x %02x %02x %02x %02x\n", buffer[0], buffer[1], buffer[2], buffer[3], buffer[4], buffer[5], buffer[6], buffer[7]);

    memset(buffer, 0xFF, 9);
    len = 8;
    if((err = usbGetReport(device, USB_HID_REPORT_TYPE_FEATURE, 1, (char*)buffer, &len)) != 0)
    {
        fprintf(stderr, "Error usbGetReport: %s\n", usbErrorMessage(err));
        return err;
    }
    DEBUG_PRINT("Recv: %02x %02x %02x %02x %02x %02x %02x %02x %02x\n", buffer[0], buffer[1], buffer[2], buffer[3], buffer[4], buffer[5], buffer[6], buffer[7], buffer[8]);
    if (buffer[1] != seq)
    {
        fprintf(stderr, "Error seqNr not copied\n");
        return 1;
    }
    sendRecvBuffer[0] = buffer[2];
    sendRecvBuffer[1] = buffer[3];
    sendRecvBuffer[2] = buffer[4];
    sendRecvBuffer[3] = buffer[5];

    seq++;
    return 0;
}

int initIO()
{
    int         err = 0;
    uchar buffer[4];

    if((err = usbOpenDevice(&device, IDENT_VENDOR_NUM, IDENT_VENDOR_STRING, IDENT_PRODUCT_NUM, IDENT_PRODUCT_STRING, 1)) != 0)
    {
        fprintf(stderr, "Error opening HIDBoot device: %s\n", usbErrorMessage(err));
        return 1;
    }

    buffer[0] = 0;
    if (ioUSBLowLvlCommand(1, buffer))
    {
        return 1;
    }

    buffer[0] = 1;
    if (ioUSBLowLvlCommand(1, buffer))
    {
        return 1;
    }

    IO_CMD_SET_BUFFER(0xAC, 0x53, 0x00, 0x00);
    if (ioUSBLowLvlCommand(2, buffer))
        return 1;
    if (buffer[2] != 0x53)
    {
        fprintf(stderr, "Failed to put device in program mode, no device connected?\n");
        return 2;
    }
    return 0;
}

int ioGetSignature(uchar* signature)
{
    uchar buffer[4];

    IO_CMD_SET_BUFFER(0x30, 0x00, 0x00, 0x00);
    if (ioUSBLowLvlCommand(2, buffer))
    {
        return 1;
    }
    if (buffer[3] != 0x1E)
    {
        fprintf(stderr, "First signature byte not 0x1E, not an Atmel device? Or no device.\n");
        return 1;
    }
    IO_CMD_SET_BUFFER(0x30, 0x00, 0x01, 0x00);
    if (ioUSBLowLvlCommand(2, buffer))
    {
        return 1;
    }
    signature[0] = buffer[3];
    IO_CMD_SET_BUFFER(0x30, 0x00, 0x02, 0x00);
    if (ioUSBLowLvlCommand(2, buffer))
    {
        return 1;
    }
    signature[1] = buffer[3];

    return 0;
}

int ioWaitReady()
{
    uchar buffer[4];

    do
    {
        IO_CMD_SET_BUFFER(0xF0, 0x00, 0x00, 0x00);
        if (ioUSBLowLvlCommand(2, buffer))
        {
            return 1;
        }
    } while(buffer[3] & 0x01);

    return 0;
}

int ioEraseChip()
{
    uchar buffer[4];

    IO_CMD_SET_BUFFER(0xAC, 0x80, 0x00, 0x00);
    if (ioUSBLowLvlCommand(2, buffer))
    {
        return 1;
    }

    return ioWaitReady();
}

int ioLoadProgMem(uchar lowData, uchar hiData, int addr)
{
    uchar buffer[4];

    IO_CMD_SET_BUFFER(0x40, 0x00, addr, lowData);
    if (ioUSBLowLvlCommand(2, buffer))
    {
        return 1;
    }
    IO_CMD_SET_BUFFER(0x48, 0x00, addr, hiData);
    if (ioUSBLowLvlCommand(2, buffer))
    {
        return 1;
    }
    return 0;
}

int ioWriteProgPage(int addr)
{
    uchar buffer[4];

    IO_CMD_SET_BUFFER(0x4C, (addr >> 8), addr, 0x00);
    if (ioUSBLowLvlCommand(2, buffer))
    {
        return 1;
    }

    return ioWaitReady();
}

int ioReadProgMem(int addr, int* data)
{
    uchar buffer[4];

    IO_CMD_SET_BUFFER(0x20, (addr >> 8), addr, 0x00);
    if (ioUSBLowLvlCommand(2, buffer))
    {
        return 1;
    }
    *data = buffer[3];
    IO_CMD_SET_BUFFER(0x28, (addr >> 8), addr, 0x00);
    if (ioUSBLowLvlCommand(2, buffer))
    {
        return 1;
    }
    *data |= buffer[3] << 8;

    return 0;
}

int ioReadLowFuseByte()
{
    uchar buffer[4];

    IO_CMD_SET_BUFFER(0x50, 0x00, 0x00, 0x00);
    if (ioUSBLowLvlCommand(2, buffer))
    {
        return 1;
    }
    return buffer[3];
}

int ioReadHighFuseByte()
{
    uchar buffer[4];

    IO_CMD_SET_BUFFER(0x58, 0x08, 0x00, 0x00);
    if (ioUSBLowLvlCommand(2, buffer))
    {
        return 1;
    }
    return buffer[3];
}

int ioReadExtendedFuseByte()
{
    /* TODO
    uchar buffer[4];

    IO_CMD_SET_BUFFER(0x58, 0x08, 0x00, 0x00);
    if (ioUSBLowLvlCommand(2, buffer))
    {
        return 1;
    }
    return buffer[3];
    */
    return 0;
}

int ioWriteLowFuseByte(int data)
{
    uchar buffer[4];

    IO_CMD_SET_BUFFER(0xAC, 0xA0, 0x00, data);
    if (ioUSBLowLvlCommand(2, buffer))
    {
        return 1;
    }
    return 0;
}

int ioWriteHighFuseByte(int data)
{
    uchar buffer[4];

    IO_CMD_SET_BUFFER(0xAC, 0xA8, 0x00, data);
    if (ioUSBLowLvlCommand(2, buffer))
    {
        return 1;
    }
    return 0;
}

int ioWriteExtendedFuseByte(int data)
{
    /* TODO
    uchar buffer[4];

    IO_CMD_SET_BUFFER(0xAC, 0xA0, 0x00, data);
    if (ioUSBLowLvlCommand(2, buffer))
    {
        return 1;
    }
    */
    return 0;
}


int ioStartDevice()
{
    uchar buffer[4];

    buffer[0] = 0;
    if (ioUSBLowLvlCommand(1, buffer))
    {
        return 1;
    }
    return 0;
}
