/**
 * hardware_regs.h - AXI Register Definitions for Game Graphics Controller
 *
 * All hardware register addresses for communication between MicroBlaze and FPGA
 */

#ifndef HARDWARE_REGS_H
#define HARDWARE_REGS_H

//Base address - check Vivado Address Editor if changed
#define GAME_GRAPHICS_BASEADDR  0x44000000

//Player registers
#define PLAYER_X_REG            (GAME_GRAPHICS_BASEADDR + 0x3000)
#define PLAYER_Y_REG            (GAME_GRAPHICS_BASEADDR + 0x3004)
#define PLAYER_FRAME_REG        (GAME_GRAPHICS_BASEADDR + 0x3008)
#define PLAYER_HEALTH_REG       (GAME_GRAPHICS_BASEADDR + 0x3050)
#define PLAYER_ARMOR_REG        (GAME_GRAPHICS_BASEADDR + 0x3054)
#define PLAYER_PROJ_FRAME_REG   (GAME_GRAPHICS_BASEADDR + 0x3110)

//Enemy 0 registers
#define ENEMY0_X_REG            (GAME_GRAPHICS_BASEADDR + 0x3030)
#define ENEMY0_Y_REG            (GAME_GRAPHICS_BASEADDR + 0x3034)
#define ENEMY0_FRAME_REG        (GAME_GRAPHICS_BASEADDR + 0x3038)
#define ENEMY0_ACTIVE_REG       (GAME_GRAPHICS_BASEADDR + 0x303C)

//Enemy 1 registers
#define ENEMY1_X_REG            (GAME_GRAPHICS_BASEADDR + 0x3060)
#define ENEMY1_Y_REG            (GAME_GRAPHICS_BASEADDR + 0x3064)
#define ENEMY1_FRAME_REG        (GAME_GRAPHICS_BASEADDR + 0x3068)
#define ENEMY1_ACTIVE_REG       (GAME_GRAPHICS_BASEADDR + 0x306C)

//Enemy 2 registers
#define ENEMY2_X_REG            (GAME_GRAPHICS_BASEADDR + 0x3070)
#define ENEMY2_Y_REG            (GAME_GRAPHICS_BASEADDR + 0x3074)
#define ENEMY2_FRAME_REG        (GAME_GRAPHICS_BASEADDR + 0x3078)
#define ENEMY2_ACTIVE_REG       (GAME_GRAPHICS_BASEADDR + 0x307C)

//Enemy 3 registers
#define ENEMY3_X_REG            (GAME_GRAPHICS_BASEADDR + 0x3080)
#define ENEMY3_Y_REG            (GAME_GRAPHICS_BASEADDR + 0x3084)
#define ENEMY3_FRAME_REG        (GAME_GRAPHICS_BASEADDR + 0x3088)
#define ENEMY3_ACTIVE_REG       (GAME_GRAPHICS_BASEADDR + 0x308C)

//Enemy 4 registers
#define ENEMY4_X_REG            (GAME_GRAPHICS_BASEADDR + 0x3090)
#define ENEMY4_Y_REG            (GAME_GRAPHICS_BASEADDR + 0x3094)
#define ENEMY4_FRAME_REG        (GAME_GRAPHICS_BASEADDR + 0x3098)
#define ENEMY4_ACTIVE_REG       (GAME_GRAPHICS_BASEADDR + 0x309C)

//Projectile registers (packed: {active[31], is_boss[30], flip[29], y[25:16], x[9:0]})
#define PROJ_0_REG              (GAME_GRAPHICS_BASEADDR + 0x3040)
#define PROJ_1_REG              (GAME_GRAPHICS_BASEADDR + 0x3044)
#define PROJ_2_REG              (GAME_GRAPHICS_BASEADDR + 0x3048)
#define PROJ_3_REG              (GAME_GRAPHICS_BASEADDR + 0x304C)
#define PROJ_4_REG              (GAME_GRAPHICS_BASEADDR + 0x30D0)
#define PROJ_5_REG              (GAME_GRAPHICS_BASEADDR + 0x30D4)
#define PROJ_6_REG              (GAME_GRAPHICS_BASEADDR + 0x30D8)
#define PROJ_7_REG              (GAME_GRAPHICS_BASEADDR + 0x30DC)
#define PROJ_8_REG              (GAME_GRAPHICS_BASEADDR + 0x30E0)
#define PROJ_9_REG              (GAME_GRAPHICS_BASEADDR + 0x30E4)
#define PROJ_10_REG             (GAME_GRAPHICS_BASEADDR + 0x30E8)
#define PROJ_11_REG             (GAME_GRAPHICS_BASEADDR + 0x30EC)
#define PROJ_12_REG             (GAME_GRAPHICS_BASEADDR + 0x30F0)
#define PROJ_13_REG             (GAME_GRAPHICS_BASEADDR + 0x30F4)
#define PROJ_14_REG             (GAME_GRAPHICS_BASEADDR + 0x30F8)
#define PROJ_15_REG             (GAME_GRAPHICS_BASEADDR + 0x30FC)

//Game state registers
#define GAME_STATE_REG          (GAME_GRAPHICS_BASEADDR + 0x30A0)
#define MENU_SELECT_REG         (GAME_GRAPHICS_BASEADDR + 0x30A4)
#define MAP_SELECT_REG          (GAME_GRAPHICS_BASEADDR + 0x30A8)
#define BREACH_OPEN_REG         (GAME_GRAPHICS_BASEADDR + 0x30AC)
#define BREACH_DIR_REG          (GAME_GRAPHICS_BASEADDR + 0x30B0)
#define LEVEL_REG               (GAME_GRAPHICS_BASEADDR + 0x302C)

//Boss registers
#define BOSS_X_REG              (GAME_GRAPHICS_BASEADDR + 0x30B4)
#define BOSS_Y_REG              (GAME_GRAPHICS_BASEADDR + 0x30B8)
#define BOSS_FRAME_REG          (GAME_GRAPHICS_BASEADDR + 0x30BC)
#define BOSS_CONTROL_REG        (GAME_GRAPHICS_BASEADDR + 0x30C0)
#define BOSS_HEALTH_REG         (GAME_GRAPHICS_BASEADDR + 0x30C4)

#endif //HARDWARE_REGS_H
