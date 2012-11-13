#include <avr/io.h>
#include <avr/wdt.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <string.h>

#include <avr/pgmspace.h>
#include "usb/usbdrv.h"
#include "requests.h"

#define RESET_PIN_BIT 0x02
#define MOSI_PIN_BIT 0x08
#define MISO_PIN_BIT 0x10
#define SCK_PIN_BIT 0x20

/* ------------------------------------------------------------------------- */
/* ----------------------------- USB interface ----------------------------- */
/* ------------------------------------------------------------------------- */

PROGMEM char usbHidReportDescriptor[42] = {
    0x06, 0x00, 0xff,              // USAGE_PAGE (Generic Desktop)
    0x09, 0x01,                    // USAGE (Vendor Usage 1)
    0xa1, 0x01,                    // COLLECTION (Application)
    0x15, 0x00,                    //   LOGICAL_MINIMUM (0)
    0x26, 0xff, 0x00,              //   LOGICAL_MAXIMUM (255)
    0x75, 0x08,                    //   REPORT_SIZE (8)

    0x85, 0x01,                    //   REPORT_ID (1)
    0x95, 0x07,                    //   REPORT_COUNT (7)
    0x09, 0x00,                    //   USAGE (Undefined)
    0xb2, 0x02, 0x01,              //   FEATURE (Data,Var,Abs,Buf)

    0x85, 0x02,                    //   REPORT_ID (2)
    0x95, 0x06,                    //   REPORT_COUNT (6)
    0x09, 0x00,                    //   USAGE (Undefined)
    0xb2, 0x02, 0x01,              //   FEATURE (Data,Var,Abs,Buf)

    0x85, 0x03,                    //   REPORT_ID (3)
    0x95, 0x84,                    //   REPORT_COUNT (132)
    0x09, 0x00,                    //   USAGE (Undefined)
    0xb2, 0x02, 0x01,              //   FEATURE (Data,Var,Abs,Buf)

    0xc0                           // END_COLLECTION
};
/**
 * REPORT_ID 1 (len 7)
 *  0:   Reset status
 *  1:   Last recieved sequence number
 *  2-5: Awnser to last ISP command
 * REPORT_ID 2 (len 2+4)
 *  0:   Sequence number
 *  1:   Command
 *  2-5: Command data
 * REPORT_ID 3 (len 4+4*32)
 *  0:     Sequence number
 *  1:     Command
 *  2:     Number of actions
 *  3-131: Command data
 */

/* ------------------------------------------------------------------------- */

uchar sendDataBuffer[8];
uchar recvDataOffset;
uchar recvDataSize;
uchar recvDataBuffer[133];

usbMsgLen_t usbFunctionSetup(uchar data[8])
{
    usbRequest_t    *rq = (void *)data;

    if(rq->bRequest == USBRQ_HID_SET_REPORT)
    {
        if(rq->wValue.bytes[0] == 2)
        {
            recvDataOffset = 0;
            recvDataSize = 7;
            return USB_NO_MSG;
        }else if(rq->wValue.bytes[0] == 3)
        {
            recvDataOffset = 0;
            recvDataSize = 133;
            return USB_NO_MSG;
        }
    }else if(rq->bRequest == USBRQ_HID_GET_REPORT)
    {
        usbMsgPtr = sendDataBuffer;
        sendDataBuffer[0] = (PORTB & RESET_PIN_BIT);
        return 8;
    }
    return 0;   /* default for not implemented requests: return no data back to host */
}

//We need to wait 2/3 clock cycles here, on the slowest speed. Assuming that is 1Mhz we need to wait 3us.
//  However a device could run on an external clock which is even slower. But not on our board.
#define ISP_DELAY _delay_us(10)
uchar ispMsg(uchar msg)
{
    uchar recv = 0;
    uchar i;
    for(i=0;i<8;i++)
    {
        //Set new data ready
        if (msg & 0x80) PORTB |= MOSI_PIN_BIT; else PORTB &=~MOSI_PIN_BIT;
        msg <<= 1;
        ISP_DELAY;
        PORTB |= SCK_PIN_BIT;//Raise SCK
        //Read the data
        recv <<= 1;
        if (PINB & MISO_PIN_BIT) recv |= 0x01;
        ISP_DELAY;
        PORTB &=~SCK_PIN_BIT;//Lower SCK
    }

    return recv;
}

uchar usbFunctionWrite(uchar *data, uchar len)
{
    uchar i, j;
    if (len + recvDataOffset > recvDataSize)
        len = recvDataSize - recvDataOffset;
    memcpy(recvDataBuffer + recvDataOffset, data, len);
    recvDataOffset += len;
    if (recvDataOffset == recvDataSize)
    {
        //Sequence number
        sendDataBuffer[1] = recvDataBuffer[1];
        switch(recvDataBuffer[2])
        {
        case COMMAND_RESET_DEVICE:
            if (recvDataBuffer[3])
            {
                //Set the reset line low to reset the device
                PORTB &=~RESET_PIN_BIT;
            }else{
                //Raise the reset line, powers the device
                PORTB |= RESET_PIN_BIT;
            }
            break;
        case COMMAND_ISP_COMMAND:
            if (recvDataSize == 7)
            {
                sendDataBuffer[2] = ispMsg(recvDataBuffer[3]);
                sendDataBuffer[3] = ispMsg(recvDataBuffer[4]);
                sendDataBuffer[4] = ispMsg(recvDataBuffer[5]);
                sendDataBuffer[5] = ispMsg(recvDataBuffer[6]);
            }else{
                j = 4;
                recvDataBuffer[3] &= 0x1f;
                for(i = 0; i < recvDataBuffer[3]; i++)
                {
                    sendDataBuffer[2] = ispMsg(recvDataBuffer[j++]);
                    sendDataBuffer[3] = ispMsg(recvDataBuffer[j++]);
                    sendDataBuffer[4] = ispMsg(recvDataBuffer[j++]);
                    sendDataBuffer[5] = ispMsg(recvDataBuffer[j++]);
                }
            }
            break;
        }
        return 1;
    }
    return 0;
}

/* ------------------------------------------------------------------------- */

int __attribute__((noreturn)) main(void)
{
    /* bugfix for bootload, leaves timer interrupt on! */
    TIMSK = 0x00;
    TCCR1A = 0x00;
    TCCR1B = 0x00;

    /* Set RESET target line to output and HI, putting the device in running */
    DDRB |= RESET_PIN_BIT;
    PORTB |= RESET_PIN_BIT;

    uchar i;
    /* RESET status: all port bits are inputs without pull-up.
     * That's the way we need D+ and D-. Therefore we don't need any
     * additional hardware initialization.
     */
    usbInit();
    usbDeviceDisconnect();  /* enforce re-enumeration, do this while interrupts are disabled! */
    i = 0;
    while(--i){             /* fake USB disconnect for > 250 ms */
        wdt_reset();
        _delay_ms(1);
    }
    usbDeviceConnect();
    sei();

    /* Set RESET target line LOW, putting the device in RESET mode */
    PORTB &=~RESET_PIN_BIT;

    /* Set MOSI and SCK to output, and MISO as input, and MOSI and SCK low */
    DDRB |= MOSI_PIN_BIT | SCK_PIN_BIT;
    DDRB &=~MISO_PIN_BIT;
    PORTB &=~MOSI_PIN_BIT | SCK_PIN_BIT;

    for(;;){                /* main event loop */
        wdt_reset();
        usbPoll();
    }
}

/* ------------------------------------------------------------------------- */
