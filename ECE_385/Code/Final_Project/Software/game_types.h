//game_types.h - Game Data Types, Constants, and Enumerations

#ifndef GAME_TYPES_H
#define GAME_TYPES_H

#include <stdint.h>
#include "lw_usb/GenericTypeDefs.h"
#include "lw_usb/HID.h"

//GAME STATE CONSTANTS
#define GAME_STATE_MENU     0
#define GAME_STATE_PLAYING  1
#define GAME_STATE_GAMEOVER 2
#define GAME_STATE_WIN      3

//USB HID KEYCODES
#define KEY_W       0x1A
#define KEY_A       0x04
#define KEY_S       0x16
#define KEY_D       0x07
#define KEY_J       0x0D  //Melee attack
#define KEY_K       0x0E  //Ranged attack (shuriken)
#define KEY_SPACE   0x2C  //Menu select

//SCREEN AND GAME AREA
#define PLAYER_SIZE         32
#define WALL_THICKNESS      16        //Wall is 1 tile (16 pixels) thick
#define GAME_AREA_LEFT      (160 + WALL_THICKNESS)
#define GAME_AREA_RIGHT     (640 - PLAYER_SIZE - WALL_THICKNESS)
#define GAME_AREA_TOP       WALL_THICKNESS
#define GAME_AREA_BOTTOM    (480 - PLAYER_SIZE - WALL_THICKNESS)
#define TILE_SIZE           16
#define GAME_AREA_X_OFFSET  160  //Game area starts at X=160

//PLAYER CONSTANTS
#define MAX_SPEED           4
#define ACCELERATION        1
#define DECELERATION        4
#define TAP_DISTANCE        4
#define HOLD_THRESHOLD      8

//Animation
#define FRAMES_PER_ANIM     4
#define ATTACK_FRAMES       5
#define ANIM_SPEED          8
#define ATTACK_ANIM_SPEED   4
#define ATTACK_COOLDOWN     30
#define RANGED_COOLDOWN     30
#define PLAYER_PROJ_SPEED   5
#define PLAYER_PROJ_DAMAGE  1
#define PLAYER_PROJ_SIZE    16

//Stats
#define PLAYER_MAX_HP       6
#define PLAYER_MAX_ARMOR    3
#define ARMOR_REGEN_DELAY   300   // 5 seconds at 60fps
#define ARMOR_REGEN_RATE    120   // 2 seconds per armor point
#define PLAYER_INVINCIBILITY 60

//Combat
#define ATTACK_HITBOX_SIZE  32
#define KNOCKBACK_DISTANCE  20
#define PLAYER_KNOCKBACK_DIST 30

//Animation base frames
#define IDLE_BASE           0
#define RUN_BASE            16
#define ATTACK_BASE         32

//Attack direction mapping (different order: Down, Left, Right, Up)
#define ATTACK_DIR_DOWN     0
#define ATTACK_DIR_LEFT     1
#define ATTACK_DIR_RIGHT    2
#define ATTACK_DIR_UP       3

//ENEMY CONSTANTS
#define ENEMY_MAX_HP        3
#define ENEMY_STRONG_HP     5
#define ENEMY_WEAK_DAMAGE   1
#define ENEMY_STRONG_DAMAGE 2
#define ENEMY_SIZE          32
#define MAX_ENEMIES         5

//Enemy behavior types (AI)
#define ENEMY_TYPE_MELEE    0
#define ENEMY_TYPE_RANGED   1

//Enemy sprite types (BRAM index)
#define SPRITE_TYPE_0       0     //Frankie - MELEE (strong, Level 2)
#define SPRITE_TYPE_1       1     //MINION_10 - MELEE (weak)
#define SPRITE_TYPE_2       2     //MINION_11 - MELEE (weak)
#define SPRITE_TYPE_3       3     //MINION_12 - RANGED (weak)
#define SPRITE_TYPE_4       4     //MINION_9 - RANGED (weak)
#define SPRITE_TYPE_5       5     //Ghost - RANGED (strong, Level 2)
#define NUM_SPRITE_TYPES    6
#define SOURCE_TYPE_BOSS    255

//Ranged enemy behavior
#define RANGED_SPEED            1
#define RANGED_SHOOT_COOLDOWN   150
#define RANGED_SHOOT_RANGE      300
#define RANGED_RETREAT_DIST     120
#define RANGED_IDEAL_DIST       180
#define RANGED_CHASE_DIST       250

//Melee enemy behavior
#define MELEE_SPEED             1
#define MELEE_DASH_SPEED        4
#define MELEE_ATTACK_COOLDOWN   120
#define MELEE_WANDER_RANGE      30
#define MELEE_WANDER_CHANGE     60
#define MELEE_MAX_DASH_TIME     90

//Enemy collision
#define ENEMY_COLLISION_DIST    48
#define ENEMY_PUSH_SPEED        3
#define HIT_FLASH_DURATION      15

//PROJECTILE CONSTANTS
#define PROJECTILE_SIZE     16
#define MAX_PROJECTILES     16
#define PROJECTILE_SPEED    4

//Homing projectile settings
#define HOMING_DURATION     120
#define HOMING_SPEED        2
#define HOMING_TURN_RATE    2
#define HOMING_DAMAGE       1

//BOSS CONSTANTS
#define BOSS_SIZE               64
#define BOSS_MAX_HP             30
#define BOSS_ATTACK_COOLDOWN    120
#define BOSS_HIT_FLASH          15

//Boss animation frames
#define BOSS_ANIM_IDLE_START    0
#define BOSS_ANIM_IDLE_FRAMES   4
#define BOSS_ANIM_FLY_START     4
#define BOSS_ANIM_FLY_FRAMES    4
#define BOSS_ANIM_ATTACK_START  8
#define BOSS_ANIM_ATTACK_FRAMES 8
#define BOSS_ANIM_DEATH_START   16
#define BOSS_ANIM_DEATH_FRAMES  4

//Boss attack types
#define BOSS_ATTACK_SINGLE      0
#define BOSS_ATTACK_BURST       1
#define BOSS_ATTACK_SUMMON      2
#define BOSS_ATTACK_HOMING      3
#define BOSS_NUM_ATTACKS        2

//Boss cooldowns
#define BOSS_ATTACK_SPEED       150
#define BOSS_CD_SUMMON          900
#define BOSS_CD_BURST           480
#define BOSS_CD_HOMING          360

//Boss movement
#define BOSS_MOVE_SPEED         1
#define BOSS_CHASE_SPEED        2
#define BOSS_WANDER_CHANGE      60
#define BOSS_IDEAL_DIST         150
#define BOSS_TOO_FAR            250
#define BOSS_TOO_CLOSE          80
#define BOSS_PHASE2_THRESHOLD   15
#define BOSS_BAT_SPRITE         SPRITE_TYPE_3

//ROOM AND LEVEL CONSTANTS
#define TEMPLATE_EMPTY          0
#define TEMPLATE_I_SHAPE        1
#define TEMPLATE_CROSS          2
#define TEMPLATE_PILLARS        3
#define TEMPLATE_STAIR          4
#define TEMPLATE_BOSS           3
#define NUM_BATTLE_TEMPLATES    3

//Breach directions
#define DIR_BREACH_RIGHT        0
#define DIR_BREACH_LEFT         1
#define DIR_BREACH_UP           2
#define DIR_BREACH_DOWN         3

//Level configuration
#define LEVEL1_ROOMS            5
#define LEVEL2_ROOMS            5
#define MAX_LEVELS              2

//Collision map
#define COLLISION_MAP_WIDTH     30
#define COLLISION_MAP_HEIGHT    30

//Breach area
#define BREACH_CENTER_START     13
#define BREACH_CENTER_END       16
#define BREACH_PIXEL_START      (BREACH_CENTER_START * 16)
#define BREACH_PIXEL_END        ((BREACH_CENTER_END + 1) * 16)

//Stair area
#define STAIR_COL_START         13
#define STAIR_COL_END           15
#define STAIR_ROW_START         14
#define STAIR_ROW_END           15
#define STAIR_PIXEL_X_START     ((STAIR_COL_START * 16) + 160)
#define STAIR_PIXEL_X_END       (((STAIR_COL_END + 1) * 16) + 160)
#define STAIR_PIXEL_Y_START     (STAIR_ROW_START * 16)
#define STAIR_PIXEL_Y_END       ((STAIR_ROW_END + 1) * 16)

//Enemies per room type
#define ENEMIES_I_SHAPE         4
#define ENEMIES_CROSS           4
#define ENEMIES_PILLARS         5
#define ENEMIES_BOSS            1

//ENUMERATIONS

//Direction enum (matches sprite sheet layout)
typedef enum {
    DIR_DOWN  = 0,
    DIR_RIGHT = 1,
    DIR_UP    = 2,
    DIR_LEFT  = 3
} Direction;

//DATA STRUCTURES

//Enemy structure
typedef struct {
    int x, y;
    int health;
    int frame;
    Direction direction;
    int attack_cooldown;
    int anim_counter;
    int anim_frame;
    int active;
    int enemy_type;         //ENEMY_TYPE_RANGED or ENEMY_TYPE_MELEE (AI behavior)
    int sprite_type;        //BRAM sprite index (0-5)

    //Melee-specific: dash attack state
    int is_dashing;
    int dash_target_x;
    int dash_target_y;
    int dash_timer;         //Max frames for dash to prevent stuck

    //Wander state (for melee when not attacking)
    int wander_dir_x;
    int wander_dir_y;
    int wander_timer;

    //Visual feedback
    int hit_timer;          //Red flash when hit
    int is_attacking;       //Blue tint when melee is dashing
} Enemy;

//Projectile structure
typedef struct {
    int16_t x, y;
    int8_t vx, vy;
    uint8_t active;
    uint8_t is_homing;
    uint8_t homing_timer;
    uint8_t is_boss_proj;
    uint8_t flip;
    uint8_t source_type;    //Sprite type of enemy that fired
    uint8_t is_player_proj; // 1 = player shuriken, 0 = enemy projectile
} Projectile;

//Boss structure
typedef struct {
    int x, y;
    int health;
    int active;
    int frame;
    int flip;               //Horizontal flip (face left/right)
    int hit_timer;          //Red tint countdown when hit

    //Attack system - separate basic attack and ability cooldowns
    int attack_cooldown;    //Basic ranged attack cooldown
    int summon_cooldown;    //Summon ability cooldown
    int burst_cooldown;     //Burst ability cooldown
    int homing_cooldown;    //Homing ability cooldown

    int last_attack;        //Previous attack (to avoid repeats)
    int phase;              // 1 = normal, 2 = below 50% HP (unlocks summon)
    int anim_state;         // 0=idle, 1=fly, 2=attack, 3=death
    int anim_frame;         //Frame within current animation
    int anim_counter;       //Animation timing counter
    int is_dying;           //Death animation in progress
    int death_timer;        //Frames until despawn after death

    //Movement
    int wander_timer;       //Countdown until direction change
    int wander_dir_x;       //Current wander direction X (-1, 0, 1)
    int wander_dir_y;       //Current wander direction Y (-1, 0, 1)
} Boss;

#endif //GAME_TYPES_H
