#include <stdio.h>
#include <stdlib.h>

#include "io.h"
#include "deviceList.h"

const char* resetItems[] = {"PC6 is IO", "PC6 is RESET"};
const char* wdtItems[] = {"WTD is always on", "WTD disabled on boot"};
const char* spiItems[] = {"Serial programming enabled", "Serial programming disabled"};
const char* jtagItems[] = {"JTAG enabled", "JTAG disabled"};
const char* ocdItems[] = {"OCD enabled", "OCD disabled"};
const char* eesaveItems[] = {"EEPROM is preserved when chip is cleared", "EEPROM is cleared when chip is cleared"};
const char* mega8BootSizeItems[] = {"1024 words, 0xC00-0xFFF", "512 words, 0xE00-0xFFF", "256 words, 0xF00-0xFFF", "128 words, 0xF80-0xFFF"};
const char* mega644BootSizeItems[] = {"4096 words, 0xF000-0x7FFF", "2048 words, 0xF800-0x7FFF", "1024 words, 0xFC00-0x7FFF", "512 words, 0xFE00-0x7FFF"};
const char* bootReset[] = {"Start at bootloader", "Start at address 0x0000"};
const char* clockDivideBy8[] = {"Clock not divided by 8", "Clock divided by 8"};
const char* clockOutputItems[] = {"Clock not outputted", "Clock outputted"};

const char* bodLevel88[] = {"", "", "", "", "Brownout: min 4.1v, max 4.5v", "Brownout: min 2.5v, max 2.9v", "Brownout: min 1.7v, max 2.0v", "Brownout disabled"};
const char* bodLevel644[] = {"", "", "", "", "Brownout: typ 4.3v", "Brownout: typ 2.7v", "Brownout: typ 1.8v", "Brownout disabled"};
const char* bodLevel[] = {"Brownout at 4.0v", "Brownout at 2.6v"};
const char* bodEnable[] = {"Brownout detector enabled", "Brownout detector disabled"};

const char* clockSrc[] = {"External Clock",
    "Internal RC: 1Mhz", "Internal RC: 2Mhz", "Internal RC: 4Mhz", "Internal RC: 8Mhz",
    "External RC: 0.4-0.9Mhz", "External RC: 0.9-3Mhz", "External RC: 3-8Mhz", "External RC: 8-12Mhz",
    "Low freq crystal",
    "Crystal: ?", "Crystal: ?", "Crystal: ?", "Crystal: ?", "Crystal: ?", "Crystal: ?"
};
const char* clockSrc644[] = {
    "External Clock",
    "Reserved",
    "Internal RC: 8Mhz", "Internal RC: 128Khz",
    "Low Freq crystal", "Low Freq crystal",
    "Full Swing crystal", "Full Swing crystal",
    "Low power crystal", "Low power crystal", "Low power crystal", "Low power crystal",
    "Low power crystal", "Low power crystal", "Low power crystal", "Low power crystal"
};

TFuseData fuseDataAtMega8[] = {
    // Name,      valuelist,   fusebyte, mask
    { "RSTDISBL", resetItems,         1, 0x80},
    { "WDTON   ", wdtItems,           1, 0x40},
    { "SPIEN   ", spiItems,           1, 0x20},
    { "CKOPT   ", NULL,               1, 0x10},
    { "EESAVE  ", eesaveItems,        1, 0x08},
    { "BOOTSZ  ", mega8BootSizeItems, 1, 0x06},
    { "BOOTRST ", bootReset,          1, 0x01},

    { "BODLEVEL", bodLevel,           0, 0x80},
    { "BODEN   ", bodEnable,          0, 0x40},
    { "SUT     ", NULL,               0, 0x30},
    { "CKSEL   ", clockSrc,           0, 0x0F},

    { NULL }
};
TFuseData fuseDataAtMega48[] = {
    // Name,      valuelist,   fusebyte, mask
    { "RSTDISBL", resetItems,         1, 0x80},
    { "SPIEN   ", spiItems,           1, 0x20},
    { "WDTON   ", wdtItems,           1, 0x10},
    { "EESAVE  ", eesaveItems,        1, 0x08},
    { "BODLEVEL", bodLevel88,         1, 0x07},

    { "SUT     ", NULL,               1, 0x30},
    { "CKSEL   ", clockSrc,           0, 0x0F},
    { NULL }
};
TFuseData fuseDataAtMega88[] = {
    // Name,      valuelist,   fusebyte, mask
    { "BOOTSZ ",  mega8BootSizeItems,   2, 0x06},
    { "BOOTRST ", bootReset,            2, 0x01},

    { "RSTDISBL", resetItems,           1, 0x80},
    { "SPIEN   ", spiItems,             1, 0x20},
    { "WDTON   ", wdtItems,             1, 0x10},
    { "EESAVE  ", eesaveItems,          1, 0x08},
    { "BODLEVEL", bodLevel88,           1, 0x07},

    { "SUT     ", NULL,                 1, 0x30},
    { "CKSEL   ", clockSrc,             0, 0x0F},
    { NULL }
};
TFuseData fuseDataAtMega644[] = {
    // Name,      valuelist,     fusebyte, mask
    { "BODLEVEL", bodLevel644,          2, 0x07},

    { "OCDEN   ", ocdItems,             1, 0x80 },
    { "JTAGEN  ", jtagItems,            1, 0x40 },
    { "SPIEN   ", spiItems,             1, 0x20 },
    { "WDTON   ", wdtItems,             1, 0x10 },
    { "EESAVE  ", eesaveItems,          1, 0x08 },
    { "BOOTSZ  ", mega644BootSizeItems, 1, 0x06 },
    { "BOOTRST ", bootReset,            1, 0x01 },

    { "CKDIV8  ", clockDivideBy8,       0, 0x80 },
    { "CKOUT   ", clockOutputItems,     0, 0x40 },
    { "SUT     ", NULL,                 0, 0x30 },
    { "CKSEL   ", clockSrc644,          0, 0x0F },
    { NULL }
};

TDevice deviceList[] = {
    //Name          Signature     Flash Page  FuseData
    { "ATTiny2313", {0x91, 0x0A},  1024,  16},
    { "ATMega8",    {0x93, 0x07},  4096,  32, fuseDataAtMega8},
    { "ATMega32",   {0x95, 0x02}, 16384,  64},
    { "ATMega48",   {0x92, 0x05},  2048,  32, fuseDataAtMega48},
    { "ATMega88",   {0x93, 0x0A},  4096,  32, fuseDataAtMega88},
    { "ATMega168",  {0x94, 0x06},  8192,  64, fuseDataAtMega88},
    { "ATMega644",  {0x96, 0x09}, 32768, 128, fuseDataAtMega644},
    { NULL,         {0x00, 0x00},     0}
};

TDevice* getDevice()
{
    uchar signature[2];
    TDevice* dev;

    if (ioGetSignature(signature))
        return NULL;

    printf("Found device with signature: %02x %02x\n", signature[0], signature[1]);

    for(dev = deviceList; dev->name; dev++)
    {
        if (dev->signature[0] == signature[0] && dev->signature[1] == signature[1])
        {
            printf("Which is a: %s\n", dev->name);
            return dev;
        }
    }
    fprintf(stderr, "Failed to find device\n");
    return NULL;
}
