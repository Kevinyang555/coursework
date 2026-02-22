#ifndef HDMI_TEXT_CONTROLLER_H
#define HDMI_TEXT_CONTROLLER_H

/****************** Include Files ********************/
#include "xil_types.h"
#include "xstatus.h"
#include "xparameters.h"

#define COLUMNS 80
#define ROWS    30
#define PALETTE_START 0x2000

#define STUDENT1NETID "yifei28"
#define STUDENT2NETID "ky23"

struct TEXT_HDMI_STRUCT {
    uint8_t  VRAM[ROWS*COLUMNS*2]; // Week 2 - extended VRAM (4800 bytes: 0x0000-0x12BF)
    uint8_t  PADDING[0x2000 - (ROWS*COLUMNS*2)]; // 3392 bytes padding
    uint32_t COLOR_PALETTE[8];  // 8 palette registers (32 bytes: 0x2000-0x201F)
    uint32_t FRAME_COUNT;       // Control register at 0x2020
    uint32_t DRAWX;             // Control register at 0x2024
    uint32_t DRAWY;             // Control register at 0x2028
};

struct COLOR {
    char    name[20];
    uint8_t red;
    uint8_t green;
    uint8_t blue;
};

// You may have to change this line depending on your platform designer
static volatile struct TEXT_HDMI_STRUCT* hdmi_ctrl =
    (volatile struct TEXT_HDMI_STRUCT*) XPAR_HDMI_TEXT_CONTROLLER_0_AXI_BASEADDR;

// CGA colors with names (4-bit per channel)
static struct COLOR colors[] = {
    {"black",          0x0, 0x0, 0x0},
    {"blue",           0x0, 0x0, 0xA},
    {"green",          0x0, 0xA, 0x0},
    {"cyan",           0x0, 0xA, 0xA},
    {"red",            0xA, 0x0, 0x0},
    {"magenta",        0xA, 0x0, 0xA},
    {"brown",          0xA, 0x5, 0x0},
    {"light gray",     0xA, 0xA, 0xA},
    {"dark gray",      0x5, 0x5, 0x5},
    {"light blue",     0x5, 0x5, 0xF},
    {"light green",    0x5, 0xF, 0x5},
    {"light cyan",     0x5, 0xF, 0xF},
    {"light red",      0xF, 0x5, 0x5},
    {"light magenta",  0xF, 0x5, 0xF},
    {"yellow",         0xF, 0xF, 0x5},
    {"white",          0xF, 0xF, 0xF}
};

/************************** Function Prototypes ****************************/

void textHDMIColorClr();
void textHDMIDrawColorText(char* str, int x, int y, uint8_t background, uint8_t foreground);
void setColorPalette (uint8_t color, uint8_t red, uint8_t green, uint8_t blue);
void sleepframe(uint32_t frames);
void paletteTest();
void textHDMIColorScreenSaver();
void hdmiTestWeek2(); // Call this for your Week 2 demo

#endif // HDMI_TEXT_CONTROLLER_H
