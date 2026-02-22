

/***************************** Include Files *******************************/
#include "hdmi_text_controller.h"
#include "stdio.h"
#include "string.h"
#include "sleep.h"

/************************** Function Definitions ***************************/

void hdmiSetColor(int background, int foreground)
{
	hdmi_ctrl->COLOR = foreground << 16 | background;
}

void hdmiClr()
{
	for (int i = 0; i<(ROWS*COLUMNS); i++)
	{
		hdmi_ctrl->VRAM[i] = 0x00;
	}
}

void hdmiTestWeek1()
{
	hdmiSetColor(BLACK, WHITE);
	hdmiClr();

	//Register write and readback test
	uint32_t checksum[ROWS], readsum[ROWS];

	for (int j = 0; j < ROWS; j++)
	{
		checksum[j] = 0;
		for (int i = 0; i < COLUMNS; i++)
		{
			hdmi_ctrl->VRAM[j*COLUMNS + i] = i + j;
			checksum[j] += i + j;
		}
	}
	
	for (int j = 0; j < ROWS; j++)
	{
		readsum[j] = 0;
		for (int i = 0; i < COLUMNS; i++)
		{
			readsum[j] += hdmi_ctrl->VRAM[j*COLUMNS + i];
			//printf ("%x \n\r", hdmi_ctrl->VRAM[j*COLUMNS + i]);
		}
		printf ("Row: %d, Checksum: %x, Read-back Checksum: %x\n\r", j, checksum[j], readsum[j]);
		if (checksum[j] != readsum[j])
		{
			printf ("Checksum mismatch!, check your AXI read/write logic\n\r");
			while (1){};
		}
	}
	
	printf ("Checksum code passed!...starting color test \n\r");
	sleep_MB (5);
	hdmiSetColor(DIM_GRN, BRIGHT_GRN);
	hdmiClr();
	char greentest[] = "This text should draw in green";
	{
		for (int j = 0; j < ROWS; j++)
		{
			memcpy((void*)&hdmi_ctrl->VRAM[j*COLUMNS]+j,greentest, sizeof(greentest));
		}
	}
	sleep_MB (5);
	hdmiSetColor(DIM_RED, BRIGHT_RED);
	hdmiClr();
	char redtest[] = "This text should draw in red";
	{
		for (int j = 0; j < ROWS; j++)
		{
			memcpy((void*)&hdmi_ctrl->VRAM[j*COLUMNS]+(ROWS-j),redtest, sizeof(redtest));
		}
	}
	sleep_MB (5);
	hdmiSetColor(DIM_BLU, BRIGHT_BLU);
	hdmiClr();
	char blutest[] = "This text should draw in blue";
	{
		for (int j = 0; j < ROWS; j++)
		{
			memcpy((void*)&hdmi_ctrl->VRAM[j*COLUMNS],blutest, sizeof(blutest));
		}
	}
	sleep_MB (5);
	hdmiClr();
	char inverted[] = "This text should draw inverted";
	for (int i = 0; i < sizeof(inverted); i++)
		inverted[i] |= 0x80;

	hdmiSetColor(DIM_GRN, BRIGHT_GRN);
	{
		for (int j = 0; j < ROWS; j++)
		{
			if (j%2==0)
				memcpy((void*)&hdmi_ctrl->VRAM[j*COLUMNS]+j,greentest, sizeof(greentest));
			else
				memcpy((void*)&hdmi_ctrl->VRAM[j*COLUMNS]+j,inverted, sizeof(inverted));
		}
	}
	sleep_MB (5);

	hdmiSetColor(BLACK, WHITE);
	hdmiClr();

	char completed[] = "Synchronization registers test, verify frame counter updates on display";
	memcpy((void*)&hdmi_ctrl->VRAM[0],completed, sizeof(completed));
	printf( "%s \n\r", completed);

	uint32_t initial_frame = hdmi_ctrl->FRAME_COUNT;
	while ( (hdmi_ctrl->FRAME_COUNT - initial_frame) < 60*10)
	{
		uint32_t old_frame_counter = hdmi_ctrl->FRAME_COUNT;
		while (old_frame_counter == hdmi_ctrl->FRAME_COUNT); //wait for new frame
		sprintf ( (&hdmi_ctrl->VRAM[0] + 80*29), "Frame Counter: %d; Vsync DrawX: %d; Vsync DrawY: %d", hdmi_ctrl->FRAME_COUNT, hdmi_ctrl->DRAWX, hdmi_ctrl->DRAWY);
	}
}
