/***************************** Include Files *******************************/
#include "hdmi_text_controller.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "sleep.h"

/************************** Function Definitions ***************************/

void paletteTest()
{
    textHDMIColorClr();

    for (int i = 0; i < 8; i ++)
    {
        char color_string[80];
        sprintf(color_string, "Foreground: %d background %d", 2*i, 2*i+1);
        textHDMIDrawColorText (color_string, 0, 2*i, 2*i, 2*i+1);
        sprintf(color_string, "Foreground: %d background %d", 2*i+1, 2*i);
        textHDMIDrawColorText (color_string, 40, 2*i, 2*i+1, 2*i);
    }
    textHDMIDrawColorText ("The above text should cycle through random colors", 0, 25, 0, 1);

    for (int i = 0; i < 10; i++)
    {
        sleep_MB (1);
        for (int j = 0; j < 16; j++)
            setColorPalette(j, rand() % 16, rand() % 16, rand() % 16);
    }
}

void textHDMIColorClr()
{
    for (int i = 0; i < (ROWS*COLUMNS) * 2; i+=2)
    {
        hdmi_ctrl->VRAM[i]     = 0xEE;  
        hdmi_ctrl->VRAM[i+1]     = ' ';
    }
}

void textHDMIDrawColorText(char* str, int x, int y, uint8_t background, uint8_t foreground)
{
	int i = 0;
	while (str[i]!=0)
	{
	       int idx = (y*COLUMNS + x + i) * 2;
	       uint8_t fg_compensated = foreground ^ 0xE;
	       uint8_t bg_compensated = background ^ 0xE;
	       hdmi_ctrl->VRAM[idx]     = (fg_compensated << 4) | (bg_compensated & 0xF);
	       hdmi_ctrl->VRAM[idx + 1] = str[i];
	       i++;
	}
}

void setColorPalette (uint8_t color, uint8_t red, uint8_t green, uint8_t blue)
{
    // Pack RGB into 12-bit value: {R[11:8], G[7:4], B[3:0]}
    uint16_t rgb_12bit = ((red & 0xF) << 8) | ((green & 0xF) << 4) | (blue & 0xF);

    uint8_t reg_index = color / 2;  // 0-7
    uint8_t is_odd    = color % 2;  // 0 = even, 1 = odd

    if (is_odd) {
        // Odd color to [27:16], keep [11:0]
        hdmi_ctrl->COLOR_PALETTE[reg_index] =
            (hdmi_ctrl->COLOR_PALETTE[reg_index] & 0x0000FFFF) | (rgb_12bit << 16);
    } else {
        // Even color to [11:0], keep [27:16]
        hdmi_ctrl->COLOR_PALETTE[reg_index] =
            (hdmi_ctrl->COLOR_PALETTE[reg_index] & 0xFFFF0000) | rgb_12bit;
    }
}

void sleepframe(uint32_t frames)
{
    uint32_t last_frame_count = hdmi_ctrl->FRAME_COUNT;
    while (hdmi_ctrl->FRAME_COUNT < last_frame_count + frames)
    {}
}

void textHDMIColorScreenSaver()
{
    char color_string[80];
    int fg, bg, x, y;
    char dvd_string[80];
    uint8_t old_string[160];
    int dvd_x = 0;
    int dvd_y = 0;
    int dvd_dx = 1;
    int dvd_dy = 1;

    int8_t dvd_colors[3] = {0x07, 0x07, 0x07};
    int8_t dvd_d_colors[3] = {-1, +1, -1};

    paletteTest();
    textHDMIColorClr();

    memset(old_string, 0, sizeof(old_string));
    sprintf(dvd_string, "%s and %s completed ECE 385!", STUDENT1NETID, STUDENT2NETID);

    // Initialize palette
    for (int i = 0; i < 16; i++)
    {
        setColorPalette (i, colors[i].red, colors[i].green, colors[i].blue);
    }

    while (1)
    {
        if (hdmi_ctrl->FRAME_COUNT % 10 == 0) // Every 10 frames update foreground
        {
            // Restore VRAM bytes into background to undo 'DVD' text
            memcpy(&(hdmi_ctrl->VRAM[(dvd_y*COLUMNS + dvd_x) * 2]), old_string, strlen(dvd_string)*2);

            if ( (dvd_x + dvd_dx >= 80-strlen(dvd_string)) || (dvd_x + dvd_dx < 0)) // Check X bound
                dvd_dx = -1*dvd_dx;
            if ( (dvd_y + dvd_dy >= 30) || (dvd_y + dvd_dy < 0)) // Check Y bound
                dvd_dy = -1*dvd_dy;
            dvd_x += dvd_dx;
            dvd_y += dvd_dy;

            // Store VRAM bytes into buffer before overwriting with 'DVD' text.
            memcpy(old_string, &(hdmi_ctrl->VRAM[(dvd_y*COLUMNS + dvd_x) * 2]), strlen(dvd_string)*2);
            textHDMIDrawColorText (dvd_string, dvd_x, dvd_y, 0, (rand() % 7) + 9);
        }

        if (hdmi_ctrl->FRAME_COUNT % 30 == 0) // Every 30 frames update background
        {
            fg = rand() % 16;
            bg = rand() % 16;
            while (fg == bg)
            {
                fg = rand() % 16;
                bg = rand() % 16;
            }
            sprintf(color_string, "Drawing %s text with %s background", colors[fg].name, colors[bg].name);
            x = rand() % (80-strlen(color_string));
            y = rand() % 30;

            textHDMIDrawColorText (color_string, x, y, bg, fg);
        }

        sleepframe(1); // Sleep the rest of the frame
    }
}

// Call this function for your Week 2 test
void hdmiTestWeek2()
{
    paletteTest();
    printf ("Palette test passed, beginning screensaver loop\n\r");

    textHDMIColorScreenSaver();
}
