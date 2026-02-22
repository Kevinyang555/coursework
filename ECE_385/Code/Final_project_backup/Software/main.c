#include <stdlib.h>
#include <stdint.h>
#include "platform.h"
#include "xparameters.h"
#include "xgpio.h"
#include "xil_io.h"
#include "lw_usb/GenericMacros.h"
#include "lw_usb/GenericTypeDefs.h"
#include "lw_usb/MAX3421E.h"
#include "lw_usb/USB.h"
#include "lw_usb/usb_ch9.h"
#include "lw_usb/transfer.h"
#include "lw_usb/HID.h"

// UPDATE THIS ADDRESS - check Vivado Address Editor!
#define GAME_GRAPHICS_BASEADDR  0x44000000

#define PLAYER_X_REG    (GAME_GRAPHICS_BASEADDR + 0x3000)
#define PLAYER_Y_REG    (GAME_GRAPHICS_BASEADDR + 0x3004)
#define PLAYER_FRAME_REG (GAME_GRAPHICS_BASEADDR + 0x3008)

// Enemy 0 (ranged) registers
#define ENEMY0_X_REG     (GAME_GRAPHICS_BASEADDR + 0x3030)
#define ENEMY0_Y_REG     (GAME_GRAPHICS_BASEADDR + 0x3034)
#define ENEMY0_FRAME_REG (GAME_GRAPHICS_BASEADDR + 0x3038)
#define ENEMY0_ACTIVE_REG (GAME_GRAPHICS_BASEADDR + 0x303C)

// Enemy 1 registers
#define ENEMY1_X_REG     (GAME_GRAPHICS_BASEADDR + 0x3060)
#define ENEMY1_Y_REG     (GAME_GRAPHICS_BASEADDR + 0x3064)
#define ENEMY1_FRAME_REG (GAME_GRAPHICS_BASEADDR + 0x3068)
#define ENEMY1_ACTIVE_REG (GAME_GRAPHICS_BASEADDR + 0x306C)

// Enemy 2 registers
#define ENEMY2_X_REG     (GAME_GRAPHICS_BASEADDR + 0x3070)
#define ENEMY2_Y_REG     (GAME_GRAPHICS_BASEADDR + 0x3074)
#define ENEMY2_FRAME_REG (GAME_GRAPHICS_BASEADDR + 0x3078)
#define ENEMY2_ACTIVE_REG (GAME_GRAPHICS_BASEADDR + 0x307C)

// Enemy 3 registers
#define ENEMY3_X_REG     (GAME_GRAPHICS_BASEADDR + 0x3080)
#define ENEMY3_Y_REG     (GAME_GRAPHICS_BASEADDR + 0x3084)
#define ENEMY3_FRAME_REG (GAME_GRAPHICS_BASEADDR + 0x3088)
#define ENEMY3_ACTIVE_REG (GAME_GRAPHICS_BASEADDR + 0x308C)

// Enemy 4 registers
#define ENEMY4_X_REG     (GAME_GRAPHICS_BASEADDR + 0x3090)
#define ENEMY4_Y_REG     (GAME_GRAPHICS_BASEADDR + 0x3094)
#define ENEMY4_FRAME_REG (GAME_GRAPHICS_BASEADDR + 0x3098)
#define ENEMY4_ACTIVE_REG (GAME_GRAPHICS_BASEADDR + 0x309C)

// Projectile registers (packed format: {active[31], is_boss[30], flip[29], y[25:16], x[9:0]})
#define PROJ_0_REG      (GAME_GRAPHICS_BASEADDR + 0x3040)
#define PROJ_1_REG      (GAME_GRAPHICS_BASEADDR + 0x3044)
#define PROJ_2_REG      (GAME_GRAPHICS_BASEADDR + 0x3048)
#define PROJ_3_REG      (GAME_GRAPHICS_BASEADDR + 0x304C)
#define PROJ_4_REG      (GAME_GRAPHICS_BASEADDR + 0x30D0)
#define PROJ_5_REG      (GAME_GRAPHICS_BASEADDR + 0x30D4)
#define PROJ_6_REG      (GAME_GRAPHICS_BASEADDR + 0x30D8)
#define PROJ_7_REG      (GAME_GRAPHICS_BASEADDR + 0x30DC)
#define PROJ_8_REG      (GAME_GRAPHICS_BASEADDR + 0x30E0)
#define PROJ_9_REG      (GAME_GRAPHICS_BASEADDR + 0x30E4)
#define PROJ_10_REG     (GAME_GRAPHICS_BASEADDR + 0x30E8)
#define PROJ_11_REG     (GAME_GRAPHICS_BASEADDR + 0x30EC)
#define PROJ_12_REG     (GAME_GRAPHICS_BASEADDR + 0x30F0)
#define PROJ_13_REG     (GAME_GRAPHICS_BASEADDR + 0x30F4)
#define PROJ_14_REG     (GAME_GRAPHICS_BASEADDR + 0x30F8)
#define PROJ_15_REG     (GAME_GRAPHICS_BASEADDR + 0x30FC)
#define PLAYER_HEALTH_REG (GAME_GRAPHICS_BASEADDR + 0x3050)
#define PLAYER_ARMOR_REG  (GAME_GRAPHICS_BASEADDR + 0x3054)

// Game state registers
#define GAME_STATE_REG    (GAME_GRAPHICS_BASEADDR + 0x30A0)
#define MENU_SELECT_REG   (GAME_GRAPHICS_BASEADDR + 0x30A4)
#define MAP_SELECT_REG    (GAME_GRAPHICS_BASEADDR + 0x30A8)
#define BREACH_OPEN_REG   (GAME_GRAPHICS_BASEADDR + 0x30AC)
#define BREACH_DIR_REG    (GAME_GRAPHICS_BASEADDR + 0x30B0)
#define LEVEL_REG         (GAME_GRAPHICS_BASEADDR + 0x302C)  // Level indicator for tile tinting

// Boss registers
#define BOSS_X_REG        (GAME_GRAPHICS_BASEADDR + 0x30B4)
#define BOSS_Y_REG        (GAME_GRAPHICS_BASEADDR + 0x30B8)
#define BOSS_FRAME_REG    (GAME_GRAPHICS_BASEADDR + 0x30BC)
#define BOSS_CONTROL_REG  (GAME_GRAPHICS_BASEADDR + 0x30C0)  // {hit[2], flip[1], active[0]}
#define BOSS_HEALTH_REG   (GAME_GRAPHICS_BASEADDR + 0x30C4)

// Player projectile frame register
#define PLAYER_PROJ_FRAME_REG (GAME_GRAPHICS_BASEADDR + 0x3110)

// Game state constants
#define GAME_STATE_MENU     0
#define GAME_STATE_PLAYING  1
#define GAME_STATE_GAMEOVER 2
#define GAME_STATE_WIN      3

// USB HID Keycodes
#define KEY_W   0x1A
#define KEY_A   0x04
#define KEY_S   0x16
#define KEY_D   0x07
#define KEY_J       0x0D  // Melee attack key
#define KEY_K       0x0E  // Ranged attack key (shuriken)
#define KEY_SPACE   0x2C  // Menu select key

// Game constants
#define PLAYER_SIZE     32
#define WALL_THICKNESS  16        // Wall is 1 tile (16 pixels) thick on all sides
#define GAME_AREA_LEFT      (160 + WALL_THICKNESS)
#define GAME_AREA_RIGHT     (640 - PLAYER_SIZE - WALL_THICKNESS)
#define GAME_AREA_TOP       WALL_THICKNESS
#define GAME_AREA_BOTTOM    (480 - PLAYER_SIZE - WALL_THICKNESS)

// Movement physics
#define MAX_SPEED       4
#define ACCELERATION    1
#define DECELERATION    4
#define TAP_DISTANCE    4         // Fixed distance for single tap (should be <= MAX_SPEED)
#define HOLD_THRESHOLD  8         // Frames before hold kicks in (~0.5 sec at 60fps)

// Animation constants (REDUCED FRAMES: using only sheet frames 1,3,5,7)
#define FRAMES_PER_ANIM     4     // 4 frames per movement animation (was 8)
#define ATTACK_FRAMES       5     // 5 frames per attack animation (unchanged)
#define ANIM_SPEED          8     // Change frame every N game loops (balanced for 4 frames)
#define ATTACK_ANIM_SPEED   4     // Attack animation speed (faster)
#define ATTACK_COOLDOWN     30    // Frames between melee attacks (~0.5 sec)
#define RANGED_COOLDOWN     30    // Frames between ranged attacks (same as melee)
#define PLAYER_PROJ_SPEED   5     // Player projectile speed (slightly faster than enemy)
#define PLAYER_PROJ_DAMAGE  1     // Damage per shuriken hit

// Battle system constants
#define PLAYER_MAX_HP       6    // Player health
#define ENEMY_MAX_HP        3     // Monster health (weak enemies)
#define ENEMY_STRONG_HP     5     // Monster health (strong enemies - types 0 and 5)
#define ENEMY_WEAK_DAMAGE   1     // Damage from weak enemies
#define ENEMY_STRONG_DAMAGE 2     // Damage from strong enemies (types 0 and 5)
#define ENEMY_SIZE          32    // Enemy sprite size
#define PROJECTILE_SIZE     16    // Projectile size (16x16 sprite from BRAM)
#define MAX_PROJECTILES     16    // Hardware supports 16 projectiles
#define MAX_ENEMIES         5     // 5 enemy slots

// Enemy behavior types (AI behavior)
#define ENEMY_TYPE_MELEE    0
#define ENEMY_TYPE_RANGED   1

// Enemy sprite types (BRAM index) - maps to behavior
// Sprite 0-2: MELEE, Sprite 3-5: RANGED
#define SPRITE_TYPE_0       0     // Frankie - MELEE (strong, Level 2 only)
#define SPRITE_TYPE_1       1     // MINION_10 - MELEE (weak, Level 1+2)
#define SPRITE_TYPE_2       2     // MINION_11 - MELEE (weak, Level 1+2)
#define SPRITE_TYPE_3       3     // MINION_12 - RANGED (weak, Level 1+2)
#define SPRITE_TYPE_4       4     // MINION_9 - RANGED (weak, Level 1+2)
#define SPRITE_TYPE_5       5     // Ghost - RANGED (strong, Level 2 only)
#define NUM_SPRITE_TYPES    6     // Total sprite types available
#define SOURCE_TYPE_BOSS    255   // Boss projectile source type

// Ranged enemy AI constants
#define RANGED_SPEED            1     // Pixels per frame (slow)
#define RANGED_SHOOT_COOLDOWN   150   // Frames between shots (~2.5 sec)
#define RANGED_SHOOT_RANGE      300   // Distance to start shooting
#define RANGED_RETREAT_DIST     120   // Too close - back away
#define RANGED_IDEAL_DIST       180   // Sweet spot - stand and shoot
#define RANGED_CHASE_DIST       250   // Too far - chase player
#define PROJECTILE_SPEED        4     // Projectile speed

// Melee enemy AI constants
#define MELEE_SPEED             1     // Normal walk speed
#define MELEE_DASH_SPEED        4     // Dash speed (same as player max)
#define MELEE_ATTACK_COOLDOWN   120   // Frames between attacks (~2 sec)
#define MELEE_WANDER_RANGE      30    // Max distance to wander from current pos
#define MELEE_WANDER_CHANGE     60    // Frames before picking new wander direction
#define MELEE_MAX_DASH_TIME     90    // Max frames for dash (~1.5 sec) to prevent stuck

// Enemy-to-enemy collision
#define ENEMY_COLLISION_DIST    48    // Minimum distance between enemy centers (increased to prevent sprite overlap)
#define ENEMY_PUSH_SPEED        3     // Speed to push enemies apart

// Knockback constants
#define KNOCKBACK_DISTANCE      20    // Pixels to push monster when player hits
#define PLAYER_KNOCKBACK_DIST   30    // Pixels to push player when melee hits

// Hit effect constants
#define HIT_FLASH_DURATION      15    // Frames to show red tint when enemy hit (~0.25 sec)

// Player combat constants
#define PLAYER_INVINCIBILITY  60  // Invincibility frames after taking damage (~1 sec)
#define ATTACK_HITBOX_SIZE    32  // Size of melee attack hitbox

// Armor system constants
#define PLAYER_MAX_ARMOR      3     // Maximum armor points
#define ARMOR_REGEN_DELAY     300   // 5 seconds at 60fps before regen starts
#define ARMOR_REGEN_RATE      120   // 2 seconds at 60fps per armor point

// Room template constants (tilemap IDs - matches COE generator)
// Level 2 reuses same maps but hardware applies offset to obstacle tiles (10-11 -> 22-23)
#define TEMPLATE_EMPTY      0   // Map 0: empty spawn/transition room
#define TEMPLATE_I_SHAPE    1   // Map 1: I-shape obstacle room
#define TEMPLATE_CROSS      2   // Map 2: Cross-shape obstacle room
#define TEMPLATE_PILLARS    3   // Map 3: Four pillars room
#define TEMPLATE_STAIR      4   // Map 4: Stair room (level transition)
#define TEMPLATE_BOSS       3   // Boss room uses pillars layout for cover
#define NUM_BATTLE_TEMPLATES 3  // I_SHAPE, CROSS, PILLARS (for random selection)

// Breach direction constants (matches hardware)
#define DIR_BREACH_RIGHT    0
#define DIR_BREACH_LEFT     1
#define DIR_BREACH_UP       2
#define DIR_BREACH_DOWN     3

// Level structure
#define LEVEL1_ROOMS        5   // Spawn + 3 battle + stairs
#define LEVEL2_ROOMS        5   // Empty + 3 battle + boss
#define MAX_LEVELS          2

#define COLLISION_MAP_WIDTH   30
#define COLLISION_MAP_HEIGHT  30

// Breach (door) constants - 4 tiles wide in center of each wall
#define BREACH_CENTER_START 13  // Breach center starts at tile 13
#define BREACH_CENTER_END   16  // Breach center ends at tile 16 (4 tiles)
#define BREACH_PIXEL_START  (BREACH_CENTER_START * 16)
#define BREACH_PIXEL_END    ((BREACH_CENTER_END + 1) * 16)

// Stair tile location (center of stair room - 3x2 tiles)
#define STAIR_COL_START     13
#define STAIR_COL_END       15
#define STAIR_ROW_START     14
#define STAIR_ROW_END       15
#define STAIR_PIXEL_X_START ((STAIR_COL_START * 16) + 160)  // +160 for game area offset
#define STAIR_PIXEL_X_END   (((STAIR_COL_END + 1) * 16) + 160)
#define STAIR_PIXEL_Y_START (STAIR_ROW_START * 16)
#define STAIR_PIXEL_Y_END   ((STAIR_ROW_END + 1) * 16)

// Enemy counts per room template
#define ENEMIES_I_SHAPE     4   // 4-5 enemies (randomly 4 or 5)
#define ENEMIES_CROSS       4
#define ENEMIES_PILLARS     5
#define ENEMIES_BOSS        1   // Boss room (Level 2 final)

// Boss constants
#define BOSS_SIZE           64    // 64x64 sprite
#define BOSS_MAX_HP         30    // Boss health
#define BOSS_ATTACK_COOLDOWN 120  // ~2 sec between attacks
#define BOSS_HIT_FLASH      15    // Frames of red tint when hit

// Boss animation frames (in boss_rom)
#define BOSS_ANIM_IDLE_START    0   // Frames 0-3
#define BOSS_ANIM_IDLE_FRAMES   4
#define BOSS_ANIM_FLY_START     4   // Frames 4-7
#define BOSS_ANIM_FLY_FRAMES    4
#define BOSS_ANIM_ATTACK_START  8   // Frames 8-15
#define BOSS_ANIM_ATTACK_FRAMES 8
#define BOSS_ANIM_DEATH_START   16  // Frames 16-19
#define BOSS_ANIM_DEATH_FRAMES  4

// Boss attack types
#define BOSS_ATTACK_SINGLE      0   // Single projectile at player
#define BOSS_ATTACK_BURST       1   // 12-direction burst (360掳 spread)
#define BOSS_ATTACK_SUMMON      2   // Summon bat enemies (only when HP <= 50%)
#define BOSS_ATTACK_HOMING      3   // Homing projectiles (TODO: needs tracking logic)
#define BOSS_NUM_ATTACKS        2   // Single + Summon for now (burst/homing need more work)

// Boss attack timing
#define BOSS_ATTACK_SPEED       150  // ~2.5 sec between basic ranged attacks
#define BOSS_CD_SUMMON          900  // ~15 sec - summon bats (only below 50% HP)
#define BOSS_CD_BURST           480  // ~8 sec - 16-direction burst
#define BOSS_CD_HOMING          360  // ~6 sec - 3 homing projectiles

// Boss movement
#define BOSS_MOVE_SPEED         1    // Slow movement (1 pixel per frame)
#define BOSS_CHASE_SPEED        2    // Faster when chasing player
#define BOSS_WANDER_CHANGE      60   // Change direction every ~1 sec
#define BOSS_IDEAL_DIST         150  // Preferred distance from player
#define BOSS_TOO_FAR            250  // Chase player if farther
#define BOSS_TOO_CLOSE          80   // Retreat if closer

// Boss phase thresholds
#define BOSS_PHASE2_THRESHOLD   15   // 50% of 30 HP = phase 2 unlocks summon

// Boss bat summon uses SPRITE_TYPE_3 or SPRITE_TYPE_4 (melee bats)
#define BOSS_BAT_SPRITE         SPRITE_TYPE_3  // MINION_12 as bat

// Homing projectile settings
#define HOMING_DURATION         120  // Track for 2 seconds, then go straight
#define HOMING_SPEED            2    // Slower tracking projectile
#define HOMING_TURN_RATE        2    // How fast it can turn toward player
#define HOMING_DAMAGE           1    // Homing projectiles deal less damage

// Runtime collision map (0=passable, 1=solid)
// Updated when loading a room or opening a breach
uint8_t COLLISION_MAP[COLLISION_MAP_HEIGHT][COLLISION_MAP_WIDTH];

// Forward declarations for functions used before definition
void update_player_hardware(void);
void update_enemies_hardware(void);
void update_projectiles_hardware(void);
void update_game_state_hardware(void);
void init_enemies(void);
int all_enemies_dead(void);

// Combat forward declarations
void get_attack_hitbox(int *hx, int *hy, int *hw, int *hh);
int rect_overlap(int x1, int y1, int w1, int h1, int x2, int y2, int w2, int h2);

// Boss forward declarations
void init_boss(void);
void update_boss_ai(void);
void update_boss_hardware(void);
void check_player_attack_boss(void);
void boss_spawn_projectile(void);
void boss_summon_bats(void);
void boss_burst_attack(void);
void boss_homing_attack(void);
int boss_check_ability_ready(void);
void boss_execute_attack(int attack_type);
void boss_update_movement(void);

// Level and room tracking
int current_level = 1;           // Current level (1 or 2)
int current_room = 0;            // Room index within level (0 to LEVEL_ROOMS-1)
int current_template = TEMPLATE_EMPTY;  // Current room template being displayed
int entry_direction = DIR_BREACH_RIGHT; // Direction player entered from (opposite of exit)
int exit_direction = DIR_BREACH_RIGHT;  // Direction of exit breach
int room_cleared = 0;            // 1 if all enemies in current room defeated
int breach_opened = 0;           // 1 if door has been opened after clearing

// Level configurations: room templates for each room in level
// Level 1: Empty (spawn) -> Battle -> Battle -> Battle -> Stairs
// Level 2: Empty (spawn) -> Battle -> Battle -> Battle -> Boss (uses same maps, hardware applies obstacle offset)
int level1_templates[LEVEL1_ROOMS] = {TEMPLATE_EMPTY, TEMPLATE_I_SHAPE, TEMPLATE_CROSS, TEMPLATE_PILLARS, TEMPLATE_STAIR};
int level2_templates[LEVEL2_ROOMS] = {TEMPLATE_EMPTY, TEMPLATE_I_SHAPE, TEMPLATE_CROSS, TEMPLATE_PILLARS, TEMPLATE_BOSS};

// Obstacle templates are now built dynamically via build_*_collision() functions

// Build I-shape collision template at runtime
// I-shape: horizontal bars at top/bottom, vertical bar in center
void build_i_shape_collision(void) {
    // Top horizontal bar (rows 5-6, cols 10-19 in tile coords, interior = rows 4-5, cols 9-18)
    for (int r = 5; r <= 6; r++) {
        for (int c = 10; c <= 19; c++) {
            COLLISION_MAP[r][c] = 1;
        }
    }

    // Vertical bar (rows 7-22, cols 13-16)
    for (int r = 7; r <= 22; r++) {
        for (int c = 13; c <= 16; c++) {
            COLLISION_MAP[r][c] = 1;
        }
    }

    // Bottom horizontal bar (rows 23-24, cols 10-19)
    for (int r = 23; r <= 24; r++) {
        for (int c = 10; c <= 19; c++) {
            COLLISION_MAP[r][c] = 1;
        }
    }
}

// Build Cross-shape collision template
// Cross: horizontal bar + vertical bar crossing in center
// Leaves gaps at breach areas so player can enter/exit from any direction
void build_cross_collision(void) {
    // Horizontal bar (rows 13-16, cols 6-23)
    // Leave gap at cols 1-5 (left breach path) and cols 24-28 (right breach path)
    for (int r = 13; r <= 16; r++) {
        for (int c = 6; c <= 23; c++) {
            COLLISION_MAP[r][c] = 1;
        }
    }

    // Vertical bar (rows 6-23, cols 13-16)
    // Leave gap at rows 1-5 (top breach path) and rows 24-28 (bottom breach path)
    for (int r = 6; r <= 23; r++) {
        for (int c = 13; c <= 16; c++) {
            COLLISION_MAP[r][c] = 1;
        }
    }
}

// Build Pillars collision template
// Four 2x2 pillars - must match tilemap in tiles_to_coe_rgb16.py
void build_pillars_collision(void) {
    // Pillar positions from tilemap: (7,7), (7,21), (21,7), (21,21)

    // Top-left pillar (rows 7-8, cols 7-8)
    for (int r = 7; r <= 8; r++) {
        for (int c = 7; c <= 8; c++) {
            COLLISION_MAP[r][c] = 1;
        }
    }

    // Top-right pillar (rows 7-8, cols 21-22)
    for (int r = 7; r <= 8; r++) {
        for (int c = 21; c <= 22; c++) {
            COLLISION_MAP[r][c] = 1;
        }
    }

    // Bottom-left pillar (rows 21-22, cols 7-8)
    for (int r = 21; r <= 22; r++) {
        for (int c = 7; c <= 8; c++) {
            COLLISION_MAP[r][c] = 1;
        }
    }

    // Bottom-right pillar (rows 21-22, cols 21-22)
    for (int r = 21; r <= 22; r++) {
        for (int c = 21; c <= 22; c++) {
            COLLISION_MAP[r][c] = 1;
        }
    }
}

// Load collision map for a room template
void load_collision_map(int template_id) {
    // Clear collision map
    for (int r = 0; r < COLLISION_MAP_HEIGHT; r++) {
        for (int c = 0; c < COLLISION_MAP_WIDTH; c++) {
            COLLISION_MAP[r][c] = 0;
        }
    }

    // Set wall borders (row 0, row 29, col 0, col 29)
    for (int i = 0; i < COLLISION_MAP_WIDTH; i++) {
        COLLISION_MAP[0][i] = 1;              // Top wall
        COLLISION_MAP[COLLISION_MAP_HEIGHT-1][i] = 1;  // Bottom wall
    }
    for (int i = 0; i < COLLISION_MAP_HEIGHT; i++) {
        COLLISION_MAP[i][0] = 1;              // Left wall
        COLLISION_MAP[i][COLLISION_MAP_WIDTH-1] = 1;  // Right wall
    }

    // Build obstacle collision based on template
    switch (template_id) {
        case TEMPLATE_I_SHAPE:
            build_i_shape_collision();
            break;
        case TEMPLATE_CROSS:
            build_cross_collision();
            break;
        case TEMPLATE_PILLARS:
            build_pillars_collision();
            break;
        case TEMPLATE_EMPTY:
        case TEMPLATE_STAIR:
        default:
            // No obstacles, just walls
            break;
    }
}

// Forward declarations for room functions (defined after variable declarations)
void load_room(int room_id);
void open_breach(void);
void check_room_cleared(void);
void check_room_transition(void);
int is_boss_room(void);

// Direction enum (matches sprite sheet layout)
typedef enum {
    DIR_DOWN  = 0,
    DIR_RIGHT = 1,
    DIR_UP    = 2,
    DIR_LEFT  = 3
} Direction;

// Animation base frames (REDUCED FRAMES layout)
#define IDLE_BASE   0     // Idle animations start at frame 0
#define RUN_BASE    16    // Run animations start at frame 16 (was 32)
#define ATTACK_BASE 32    // Attack animations start at frame 32 (was 64)

// Attack direction mapping (different order: Down, Left, Right, Up)
// Attack frame = ATTACK_BASE + attack_dir * 5 + attack_anim_frame
#define ATTACK_DIR_DOWN   0
#define ATTACK_DIR_LEFT   1
#define ATTACK_DIR_RIGHT  2
#define ATTACK_DIR_UP     3

// Enemy structure
typedef struct {
    int x, y;
    int health;
    int frame;
    Direction direction;
    int attack_cooldown;    // Cooldown for shooting (ranged) or dashing (melee)
    int anim_counter;
    int anim_frame;
    int active;
    int enemy_type;         // ENEMY_TYPE_RANGED or ENEMY_TYPE_MELEE (AI behavior)
    int sprite_type;        // BRAM sprite index (0-4)

    // Melee-specific: dash attack state
    int is_dashing;         // 1 if currently dashing toward target
    int dash_target_x;      // Target X position (player pos when dash started)
    int dash_target_y;      // Target Y position
    int dash_timer;         // Frames spent dashing (to prevent stuck)
    int is_attacking;       // 1 if in attack state (for blue tint)

    // Hit effect state
    int hit_timer;          // Countdown for red tint when hit by player

    // Melee wander state
    int wander_timer;       // Countdown until direction change
    int wander_dir_x;       // Current wander direction X (-1, 0, 1)
    int wander_dir_y;       // Current wander direction Y (-1, 0, 1)
} Enemy;

// Projectile structure (optimized for size)
typedef struct {
    int16_t x, y;         // Position (0-640, 0-480 fit in 16-bit)
    int8_t vx, vy;        // Velocity (small values: -4 to +4)
    uint8_t active;
    uint8_t is_homing;
    uint8_t homing_timer; // Max 180 frames fits in 8-bit
    uint8_t is_boss_proj;
    uint8_t flip;
    uint8_t source_type;  // Sprite type of enemy that fired (for damage calc)
    uint8_t is_player_proj; // 1 = player shuriken (uses tile_rom), 0 = enemy (uses enemy_rom)
} Projectile;             // 13 bytes

// Boss structure
typedef struct {
    int x, y;
    int health;
    int active;
    int frame;
    int flip;           // Horizontal flip (face left/right)
    int hit_timer;      // Red tint countdown when hit

    // Attack system - separate basic attack and ability cooldowns
    int attack_cooldown;    // Basic ranged attack cooldown
    int summon_cooldown;    // Summon ability cooldown
    int burst_cooldown;     // Burst ability cooldown (TODO)
    int homing_cooldown;    // Homing ability cooldown (TODO)

    int last_attack;    // Previous attack (to avoid repeats)
    int phase;          // 1 = normal, 2 = below 50% HP (unlocks summon)
    int anim_state;     // 0=idle, 1=fly, 2=attack, 3=death
    int anim_frame;     // Frame within current animation
    int anim_counter;   // Animation timing counter
    int is_dying;       // Death animation in progress
    int death_timer;    // Frames until despawn after death

    // Movement
    int wander_timer;   // Countdown until direction change
    int wander_dir_x;   // Current wander direction X (-1, 0, 1)
    int wander_dir_y;   // Current wander direction Y (-1, 0, 1)
} Boss;

extern HID_DEVICE hid_device;

// Player state
int player_x = 320;
int player_y = 240;

// Velocity for smooth movement
int vel_x = 0;
int vel_y = 0;

// Hold detection counters
int hold_up = 0;
int hold_down = 0;
int hold_left = 0;
int hold_right = 0;

// Animation state
Direction player_dir = DIR_DOWN;  // Current facing direction
int is_moving = 0;                // 1 if player is moving
int anim_frame = 0;               // Current frame within animation (0-7)
int anim_counter = 0;             // Counter for animation timing

// Attack state
int is_attacking = 0;             // 1 if attack animation is playing
int attack_anim_frame = 0;        // Current attack frame (0-4)
int attack_anim_counter = 0;      // Counter for attack animation timing
int attack_cooldown = 0;          // Cooldown counter (starts when attack ends)
int attack_hit_registered = 0;    // Prevent multiple hits per attack

// Ranged attack state
int ranged_cooldown = 0;          // Cooldown counter for K key ranged attack
int player_proj_frame = 0;        // Player projectile animation frame (0-3)
int player_proj_anim_counter = 0; // Counter for player projectile animation

// Battle system state
int player_health = PLAYER_MAX_HP;
int player_invincible = 0;        // Invincibility frames remaining

// Armor system state
int player_armor = PLAYER_MAX_ARMOR;  // Current armor (0-3)
int armor_regen_cooldown = 0;         // Countdown until regen starts (5 sec after hit)
int armor_regen_timer = 0;            // Countdown until next armor point regenerates

// Game state
int game_state = GAME_STATE_MENU;
int menu_selection = 0;           // 0 = first option (RESTART), 1 = second (EXIT)
int prev_key_w = 0, prev_key_s = 0, prev_key_space = 0;  // For edge detection

// Frame counter for random seed - increments every main loop iteration
// Used as entropy source: the exact frame when player presses J is unpredictable
unsigned int frame_counter = 0;

// Enemy instances (0 = ranged, 1 = melee)
Enemy enemies[MAX_ENEMIES] = {0};

// Projectile pool
Projectile projectiles[MAX_PROJECTILES] = {0};

// Boss instance
Boss boss = {0};

// ============= ROOM SYSTEM FUNCTIONS =============
// (Defined here after all variables are declared)

// Pick a random exit direction (different from entry)
int pick_exit_direction(int entry_dir) {
    // Build array of valid exit directions (exclude entry direction)
    int valid_dirs[3];
    int count = 0;

    for (int dir = 0; dir < 4; dir++) {
        if (dir != entry_dir) {
            valid_dirs[count++] = dir;
        }
    }

    // Pick random from valid directions
    int choice = rand() % count;
    return valid_dirs[choice];
}

// Get spawn position based on entry direction
// Player spawns near the entry wall at breach center, but offset to avoid obstacles
void get_spawn_position(int entry_dir, int *spawn_x, int *spawn_y) {
    // Breach center Y for left/right entry (tiles 13-16, center at 14.5 = ~232 pixels)
    int breach_center_y = (BREACH_CENTER_START + BREACH_CENTER_END + 1) * 16 / 2 - PLAYER_SIZE / 2;
    // Breach center X for top/bottom entry (160 + tile offset)
    int breach_center_x = 160 + (BREACH_CENTER_START + BREACH_CENTER_END + 1) * 16 / 2 - PLAYER_SIZE / 2;

    // Offset from wall - needs to clear obstacles
    // Cross obstacle starts at row/col 5 (pixel 80), so spawn at row/col 2-3 (pixel 32-48)
    // Use 2 tiles (32 pixels) from GAME_AREA boundary
    int wall_offset = 32;

    switch (entry_dir) {
        case DIR_BREACH_RIGHT:
            // Entered from right wall -> spawn near right wall, centered on breach Y
            *spawn_x = GAME_AREA_RIGHT - wall_offset;
            *spawn_y = breach_center_y;
            break;
        case DIR_BREACH_LEFT:
            // Entered from left wall -> spawn near left wall, centered on breach Y
            *spawn_x = GAME_AREA_LEFT + wall_offset;
            *spawn_y = breach_center_y;
            break;
        case DIR_BREACH_UP:
            // Entered from top wall -> spawn near top wall, centered on breach X
            *spawn_x = breach_center_x;
            *spawn_y = GAME_AREA_TOP + wall_offset;
            break;
        case DIR_BREACH_DOWN:
            // Entered from bottom wall -> spawn near bottom wall, centered on breach X
            *spawn_x = breach_center_x;
            *spawn_y = GAME_AREA_BOTTOM - wall_offset;
            break;
        default:
            *spawn_x = GAME_AREA_LEFT + wall_offset;
            *spawn_y = breach_center_y;
            break;
    }
}

// Get template for current room in current level
int get_room_template(int level, int room_idx) {
    if (level == 1) {
        if (room_idx < LEVEL1_ROOMS) {
            return level1_templates[room_idx];
        }
    } else if (level == 2) {
        if (room_idx < LEVEL2_ROOMS) {
            return level2_templates[room_idx];
        }
    }
    return TEMPLATE_EMPTY;
}

// Check if room has enemies (battle room)
int is_battle_room(int template_id) {
    return (template_id == TEMPLATE_I_SHAPE ||
            template_id == TEMPLATE_CROSS ||
            template_id == TEMPLATE_PILLARS);
}

// Check if current room is boss room (level 2, room 3)
int is_boss_room(void) {
    return (current_level == 2 && current_room == LEVEL2_ROOMS - 1);
}

// Load a room by template (update hardware and collision map)
void load_room(int template_id) {
    current_template = template_id;

    // Update hardware registers
    Xil_Out32(MAP_SELECT_REG, template_id);
    Xil_Out32(BREACH_OPEN_REG, 0);  // Close breach when entering new room
    Xil_Out32(BREACH_DIR_REG, exit_direction);  // Set breach direction for when it opens

    // Build collision map
    load_collision_map(template_id);

    // Reset room state
    room_cleared = 0;
    breach_opened = 0;

    // Deactivate all enemies first
    for (int i = 0; i < MAX_ENEMIES; i++) {
        enemies[i].active = 0;
    }

    // Clear projectiles
    for (int i = 0; i < MAX_PROJECTILES; i++) {
        projectiles[i].active = 0;
    }

    // Spawn enemies or boss based on room type
    if (is_boss_room()) {
        // Boss room - spawn boss, no regular enemies
        init_boss();
        update_boss_hardware();
    } else if (is_battle_room(template_id)) {
        // Normal battle room - spawn enemies
        init_enemies();
    }
    // Empty/Stair rooms - no enemies or boss

    update_enemies_hardware();
    update_projectiles_hardware();
}

// Clear collision for breach tiles based on direction
void clear_breach_collision(int breach_dir) {
    switch (breach_dir) {
        case DIR_BREACH_RIGHT:
            // Clear right wall at breach location
            for (int row = BREACH_CENTER_START; row <= BREACH_CENTER_END; row++) {
                COLLISION_MAP[row][COLLISION_MAP_WIDTH-1] = 0;
            }
            break;
        case DIR_BREACH_LEFT:
            // Clear left wall at breach location
            for (int row = BREACH_CENTER_START; row <= BREACH_CENTER_END; row++) {
                COLLISION_MAP[row][0] = 0;
            }
            break;
        case DIR_BREACH_UP:
            // Clear top wall at breach location
            for (int col = BREACH_CENTER_START; col <= BREACH_CENTER_END; col++) {
                COLLISION_MAP[0][col] = 0;
            }
            break;
        case DIR_BREACH_DOWN:
            // Clear bottom wall at breach location
            for (int col = BREACH_CENTER_START; col <= BREACH_CENTER_END; col++) {
                COLLISION_MAP[COLLISION_MAP_HEIGHT-1][col] = 0;
            }
            break;
    }
}

// Open breach in wall when room is cleared
void open_breach(void) {
    if (breach_opened) return;

    breach_opened = 1;

    // Tell hardware to display breach (floor + arrows)
    Xil_Out32(BREACH_OPEN_REG, 1);
    Xil_Out32(BREACH_DIR_REG, exit_direction);  // Ensure direction is set

    // Clear collision for breach wall
    clear_breach_collision(exit_direction);

    // Breach collision cleared
}

// Check if current room is cleared (all enemies dead or auto-clear)
void check_room_cleared(void) {
    if (room_cleared) return;

    // Empty spawn room: auto-cleared
    if (current_template == TEMPLATE_EMPTY) {
        room_cleared = 1;
        // Spawn room picks random exit direction (all 4 directions valid)
        exit_direction = rand() % 4;
        Xil_Out32(BREACH_DIR_REG, exit_direction);  // Update hardware with correct direction
        open_breach();
        return;
    }

    // Stair room: auto-cleared (player uses stairs to proceed)
    if (current_template == TEMPLATE_STAIR) {
        room_cleared = 1;
        // No breach in stair room - player uses stairs instead
        return;
    }

    // Boss room: win condition handled in update_boss() when boss dies
    if (is_boss_room()) {
        // Don't check enemies - boss death triggers win
        return;
    }

    // Battle room: cleared when all enemies dead
    if (is_battle_room(current_template) && all_enemies_dead()) {
        room_cleared = 1;

        // Not last room - open breach to next room
        exit_direction = pick_exit_direction(entry_direction);
        open_breach();
    }
}

// Check if player is in breach area for given direction
int player_in_breach(int breach_dir) {
    // Check if player has moved far enough into breach to trigger transition
    switch (breach_dir) {
        case DIR_BREACH_RIGHT:
            // Player's right edge past screen boundary triggers transition
            return (player_x >= 640 - PLAYER_SIZE - 8 &&
                    player_y >= BREACH_PIXEL_START &&
                    player_y <= BREACH_PIXEL_END - PLAYER_SIZE);
        case DIR_BREACH_LEFT:
            // Player's left edge near left boundary
            return (player_x <= 160 + 8 &&
                    player_y >= BREACH_PIXEL_START &&
                    player_y <= BREACH_PIXEL_END - PLAYER_SIZE);
        case DIR_BREACH_UP:
            // Player's top edge near top boundary
            return (player_y <= 8 &&
                    player_x >= BREACH_PIXEL_START + 160 &&
                    player_x <= BREACH_PIXEL_END + 160 - PLAYER_SIZE);
        case DIR_BREACH_DOWN:
            // Player's bottom edge near bottom boundary
            return (player_y >= 480 - PLAYER_SIZE - 8 &&
                    player_x >= BREACH_PIXEL_START + 160 &&
                    player_x <= BREACH_PIXEL_END + 160 - PLAYER_SIZE);
    }
    return 0;
}

// Advance to next room
void advance_to_next_room(void) {
    current_room++;
    int max_rooms = (current_level == 1) ? LEVEL1_ROOMS : LEVEL2_ROOMS;

    if (current_room >= max_rooms) {
        // Shouldn't happen - should hit WIN condition first
        // Error: exceeded max rooms
        return;
    }

    // Entry direction is opposite of exit direction
    switch (exit_direction) {
        case DIR_BREACH_RIGHT: entry_direction = DIR_BREACH_LEFT; break;
        case DIR_BREACH_LEFT:  entry_direction = DIR_BREACH_RIGHT; break;
        case DIR_BREACH_UP:    entry_direction = DIR_BREACH_DOWN; break;
        case DIR_BREACH_DOWN:  entry_direction = DIR_BREACH_UP; break;
    }

    int new_template = get_room_template(current_level, current_room);
    // Transitioning to next room

    load_room(new_template);

    // Spawn player based on entry direction
    int spawn_x, spawn_y;
    get_spawn_position(entry_direction, &spawn_x, &spawn_y);
    player_x = spawn_x;
    player_y = spawn_y;
    update_player_hardware();
}

// Check if player walked through breach to transition rooms
void check_room_transition(void) {
    if (!breach_opened) return;

    // Check if player entered the exit breach
    if (player_in_breach(exit_direction)) {
        advance_to_next_room();
    }
}

// Check if player is on stairs and pressed J to go to next level
int player_on_stairs(void) {
    // Check if player center is within stair tile bounds
    int player_cx = player_x + PLAYER_SIZE / 2;
    int player_cy = player_y + PLAYER_SIZE / 2;

    return (player_cx >= STAIR_PIXEL_X_START && player_cx < STAIR_PIXEL_X_END &&
            player_cy >= STAIR_PIXEL_Y_START && player_cy < STAIR_PIXEL_Y_END);
}

// Advance to next level (called when player uses stairs)
void advance_to_next_level(void) {
    if (current_level >= MAX_LEVELS) {
        // Already at max level - shouldn't have stairs
        // Error: beyond max level
        return;
    }

    current_level++;
    current_room = 0;
    entry_direction = DIR_BREACH_LEFT;  // Level 2 starts from left entry
    exit_direction = DIR_BREACH_RIGHT;

    // Level started

    // Set level tint: Level 2+ gets darker/desaturated tint
    Xil_Out32(LEVEL_REG, current_level - 1);  // Level 1=0, Level 2=1, etc.

    int new_template = get_room_template(current_level, current_room);
    load_room(new_template);

    // Spawn player on left side
    player_x = GAME_AREA_LEFT + 32;
    player_y = 240;
    update_player_hardware();
}

// Convert movement direction to attack direction (different order)
int get_attack_dir(Direction dir) {
    switch (dir) {
        case DIR_DOWN:  return ATTACK_DIR_DOWN;
        case DIR_LEFT:  return ATTACK_DIR_LEFT;
        case DIR_RIGHT: return ATTACK_DIR_RIGHT;
        case DIR_UP:    return ATTACK_DIR_UP;
        default:        return ATTACK_DIR_DOWN;
    }
}

// Helper: absolute value
int abs_val(int x) {
    return (x < 0) ? -x : x;
}

// ============= TILE COLLISION SYSTEM =============
#define TILE_SIZE 16
#define GAME_AREA_X_OFFSET 160  // Game area starts at X=160

// Convert screen X coordinate to tile column
int screen_to_tile_x(int screen_x) {
    return (screen_x - GAME_AREA_X_OFFSET) / TILE_SIZE;
}

// Convert screen Y coordinate to tile row
int screen_to_tile_y(int screen_y) {
    return screen_y / TILE_SIZE;
}

// Check if a tile is solid (wall or obstacle)
int is_tile_solid(int tile_x, int tile_y) {
    // Out of bounds = solid
    if (tile_x < 0 || tile_x >= COLLISION_MAP_WIDTH ||
        tile_y < 0 || tile_y >= COLLISION_MAP_HEIGHT) {
        return 1;
    }
    return COLLISION_MAP[tile_y][tile_x];
}

// Check if an entity at (x,y) with given size collides with any solid tile
int check_tile_collision(int entity_x, int entity_y, int size) {
    // Get tile coordinates for all four corners of entity bounding box
    int tile_x1 = screen_to_tile_x(entity_x);
    int tile_y1 = screen_to_tile_y(entity_y);
    int tile_x2 = screen_to_tile_x(entity_x + size - 1);
    int tile_y2 = screen_to_tile_y(entity_y + size - 1);

    // Check all tiles the entity overlaps
    for (int ty = tile_y1; ty <= tile_y2; ty++) {
        for (int tx = tile_x1; tx <= tile_x2; tx++) {
            if (is_tile_solid(tx, ty)) {
                return 1;  // Collision detected
            }
        }
    }
    return 0;  // No collision
}

// Check if attack hitbox is blocked by obstacle
int is_attack_blocked(int attack_x, int attack_y, int attack_size) {
    int center_x = attack_x + attack_size / 2;
    int center_y = attack_y + attack_size / 2;
    int tile_x = screen_to_tile_x(center_x);
    int tile_y = screen_to_tile_y(center_y);
    return is_tile_solid(tile_x, tile_y);
}

// Helper: pack projectile data for hardware register
// Format: {active[31], is_player[30], flip[29], unused[28:26], y[25:16], unused[15:10], x[9:0]}
// is_player: 1 = player shuriken (uses tile_rom 12-bit), 0 = enemy projectile (uses enemy_rom 6-bit)
u32 pack_projectile(Projectile *p) {
    if (!p->active) return 0;
    u32 result = ((u32)1 << 31) | ((p->y & 0x3FF) << 16) | (p->x & 0x3FF);
    if (p->is_player_proj) result |= ((u32)1 << 30);  // Set player projectile flag for tile_rom sprite
    if (p->flip) result |= ((u32)1 << 29);            // Set flip flag for horizontal flip
    return result;
}

// Spawn player shuriken projectile in facing direction
void spawn_player_projectile(void) {
    for (int i = 0; i < MAX_PROJECTILES; i++) {
        if (!projectiles[i].active) {
            projectiles[i].x = player_x + (PLAYER_SIZE - PROJECTILE_SIZE) / 2;
            projectiles[i].y = player_y + (PLAYER_SIZE - PROJECTILE_SIZE) / 2;
            projectiles[i].vx = (player_dir == DIR_RIGHT) ? PLAYER_PROJ_SPEED : (player_dir == DIR_LEFT) ? -PLAYER_PROJ_SPEED : 0;
            projectiles[i].vy = (player_dir == DIR_DOWN) ? PLAYER_PROJ_SPEED : (player_dir == DIR_UP) ? -PLAYER_PROJ_SPEED : 0;
            projectiles[i].active = 1;
            projectiles[i].is_player_proj = 1;
            projectiles[i].is_homing = 0;
            projectiles[i].is_boss_proj = 0;
            return;
        }
    }
}

// Helper to initialize a single enemy
void init_single_enemy(int idx, int x, int y, int sprite_type, int behavior_type) {
    enemies[idx].x = x;
    enemies[idx].y = y;
    // Strong enemies (types 0 and 5) have more health
    if (sprite_type == SPRITE_TYPE_0 || sprite_type == SPRITE_TYPE_5) {
        enemies[idx].health = ENEMY_STRONG_HP;
    } else {
        enemies[idx].health = ENEMY_MAX_HP;
    }
    enemies[idx].frame = 0;
    enemies[idx].direction = DIR_LEFT;
    enemies[idx].anim_counter = 0;
    enemies[idx].anim_frame = 0;
    enemies[idx].active = 1;
    enemies[idx].sprite_type = sprite_type;
    enemies[idx].enemy_type = behavior_type;
    enemies[idx].is_dashing = 0;
    enemies[idx].dash_timer = 0;
    enemies[idx].is_attacking = 0;
    enemies[idx].hit_timer = 0;
    enemies[idx].wander_timer = 0;
    enemies[idx].wander_dir_x = 0;
    enemies[idx].wander_dir_y = 0;

    // Set appropriate cooldown based on behavior
    if (behavior_type == ENEMY_TYPE_RANGED) {
        enemies[idx].attack_cooldown = RANGED_SHOOT_COOLDOWN;
    } else {
        enemies[idx].attack_cooldown = MELEE_ATTACK_COOLDOWN;
    }
}

// Sprite type to behavior mapping
// Sprite 0-2: Melee, Sprite 3-5: Ranged
int get_behavior_for_sprite(int sprite_type) {
    switch (sprite_type) {
        case SPRITE_TYPE_0: return ENEMY_TYPE_MELEE;   // Frankie - MELEE
        case SPRITE_TYPE_1: return ENEMY_TYPE_MELEE;   // MINION_10 - MELEE
        case SPRITE_TYPE_2: return ENEMY_TYPE_MELEE;   // MINION_11 - MELEE
        case SPRITE_TYPE_3: return ENEMY_TYPE_RANGED;  // MINION_12 - RANGED
        case SPRITE_TYPE_4: return ENEMY_TYPE_RANGED;  // MINION_9 - RANGED
        case SPRITE_TYPE_5: return ENEMY_TYPE_RANGED;  // Ghost - RANGED
        default: return ENEMY_TYPE_MELEE;
    }
}

// Random spawn positions (avoid player start area and obstacles)
void get_random_spawn_pos(int *x, int *y) {
    // Simplified: random position in game area, retry if collision
    int attempts = 0;
    do {
        // Random x in game area (180-580), y in game area (50-400)
        *x = 180 + (rand() % 400);
        *y = 50 + (rand() % 350);
        attempts++;
    } while (check_tile_collision(*x, *y, ENEMY_SIZE) && attempts < 50);
}

// Get random sprite type based on current level
// Level 1: Only weaker monsters (types 1-4)
// Level 2: All 6 monster types (including strong types 0 and 5)
int get_level_sprite_type(void) {
    if (current_level == 1) {
        // Level 1: spawn only weaker monsters (types 1-4)
        return SPRITE_TYPE_1 + (rand() % 4);  // 1, 2, 3, or 4
    } else {
        // Level 2: spawn all 6 monster types (including strong types 0 and 5)
        return rand() % NUM_SPRITE_TYPES;  // 0, 1, 2, 3, 4, or 5
    }
}

// Initialize enemies - spawn based on level
// Level 1: 3-4 enemies per room
// Level 2: 5 enemies per room (max supported by hardware)
void init_enemies(void) {
    int num_enemies;
    if (current_level == 1) {
        num_enemies = 3 + (rand() % 2);  // 3 or 4
    } else {
        num_enemies = MAX_ENEMIES;  // 5 (hardware limit)
    }
    // Spawning enemies

    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (i < num_enemies) {
            // Get sprite type based on current level
            int sprite_type = get_level_sprite_type();
            int behavior = get_behavior_for_sprite(sprite_type);

            // Random position
            int x, y;
            get_random_spawn_pos(&x, &y);

            // Enemy spawned

            init_single_enemy(i, x, y, sprite_type, behavior);
        } else {
            // Deactivate unused slots
            enemies[i].active = 0;
            // Enemy inactive
        }
    }
}

// ============= BOSS FUNCTIONS =============

// Initialize boss at given position
void init_boss(void) {
    // Boss spawning
    boss.x = 450;  // Right side of room
    boss.y = 200;  // Centered vertically
    boss.health = BOSS_MAX_HP;
    boss.active = 1;
    boss.frame = 0;
    boss.flip = 0;  // Face left toward player
    boss.hit_timer = 0;

    // Separate cooldowns
    boss.attack_cooldown = BOSS_ATTACK_SPEED;  // Basic ranged attack
    boss.summon_cooldown = BOSS_CD_SUMMON;     // Summon ability starts on cooldown
    boss.burst_cooldown = BOSS_CD_BURST;
    boss.homing_cooldown = BOSS_CD_HOMING;

    boss.last_attack = -1;  // No previous attack
    boss.phase = 1;         // Phase 1 (above 50% HP)
    boss.anim_state = 0;  // Idle
    boss.anim_frame = 0;
    boss.anim_counter = 0;
    boss.is_dying = 0;
    boss.death_timer = 0;

    // Movement
    boss.wander_timer = BOSS_WANDER_CHANGE;
    boss.wander_dir_x = 0;
    boss.wander_dir_y = 0;

    // Boss spawned
}

// Update boss hardware registers
void update_boss_hardware(void) {
    if (!boss.active) {
        Xil_Out32(BOSS_CONTROL_REG, 0);  // Inactive
        return;
    }

    Xil_Out32(BOSS_X_REG, boss.x);
    Xil_Out32(BOSS_Y_REG, boss.y);
    Xil_Out32(BOSS_FRAME_REG, boss.frame);

    // Control register: {hit[2], flip[1], active[0]}
    u32 control = boss.active;
    if (boss.flip) control |= (1 << 1);
    if (boss.hit_timer > 0) control |= (1 << 2);
    Xil_Out32(BOSS_CONTROL_REG, control);

    Xil_Out32(BOSS_HEALTH_REG, boss.health);
}

// Boss spawns projectile toward player
void boss_spawn_projectile(void) {
    // Find inactive projectile slot
    for (int i = 0; i < MAX_PROJECTILES; i++) {
        if (!projectiles[i].active) {
            // Calculate boss center and player center
            int boss_cx = boss.x + (BOSS_SIZE / 2);
            int boss_cy = boss.y + (BOSS_SIZE / 2);
            int player_cx = player_x + (PLAYER_SIZE / 2);
            int player_cy = player_y + (PLAYER_SIZE / 2);

            // Calculate direction vector to player
            int dx = player_cx - boss_cx;
            int dy = player_cy - boss_cy;

            // Normalize to get velocity components
            int abs_dx = abs_val(dx);
            int abs_dy = abs_val(dy);

            if (abs_dx == 0 && abs_dy == 0) {
                projectiles[i].vx = 0;
                projectiles[i].vy = PROJECTILE_SPEED;
            } else if (abs_dx > abs_dy) {
                projectiles[i].vx = (dx > 0) ? PROJECTILE_SPEED : -PROJECTILE_SPEED;
                projectiles[i].vy = (dy * PROJECTILE_SPEED) / abs_dx;
            } else {
                projectiles[i].vy = (dy > 0) ? PROJECTILE_SPEED : -PROJECTILE_SPEED;
                projectiles[i].vx = (dx * PROJECTILE_SPEED) / abs_dy;
            }

            // Start projectile at boss center (16x16 sprite like regular projectiles)
            projectiles[i].x = boss_cx - (PROJECTILE_SIZE / 2);
            projectiles[i].y = boss_cy - (PROJECTILE_SIZE / 2);
            projectiles[i].active = 1;
            projectiles[i].is_player_proj = 0;  // Boss projectile (enemy)
            projectiles[i].is_homing = 0;
            projectiles[i].is_boss_proj = 0;
            projectiles[i].flip = 0;
            projectiles[i].source_type = SOURCE_TYPE_BOSS;

            // Boss projectile fired
            return;
        }
    }
}

// Boss summons bat enemies around itself
void boss_summon_bats(void) {
    // Spawn offsets: TL, TR, BL (only need 3)
    int ox[] = {-80, 80, -80};
    int oy[] = {-40, -40, 40};

    int spawned = 0;
    for (int i = 0; i < MAX_ENEMIES && spawned < 3; i++) {
        if (!enemies[i].active) {
            int spawn_x = boss.x + BOSS_SIZE/2 + ox[spawned] - ENEMY_SIZE/2;
            int spawn_y = boss.y + BOSS_SIZE/2 + oy[spawned] - ENEMY_SIZE/2;

            // Clamp to game area
            if (spawn_x < GAME_AREA_LEFT) spawn_x = GAME_AREA_LEFT;
            if (spawn_x > GAME_AREA_RIGHT - ENEMY_SIZE) spawn_x = GAME_AREA_RIGHT - ENEMY_SIZE;
            if (spawn_y < GAME_AREA_TOP) spawn_y = GAME_AREA_TOP;
            if (spawn_y > GAME_AREA_BOTTOM - ENEMY_SIZE) spawn_y = GAME_AREA_BOTTOM - ENEMY_SIZE;

            // Check tile collision - skip if blocked
            if (check_tile_collision(spawn_x, spawn_y, ENEMY_SIZE)) {
                continue;
            }

            // Spawn a melee bat using BOSS_BAT_SPRITE
            init_single_enemy(i, spawn_x, spawn_y, BOSS_BAT_SPRITE, ENEMY_TYPE_MELEE);
            spawned++;
        }
    }
}

// Boss burst attack - 16 directions
void boss_burst_attack(void) {
    int boss_cx = boss.x + BOSS_SIZE / 2;
    int boss_cy = boss.y + BOSS_SIZE / 2;
    // 16 directions at 22.5掳 intervals (vx, vy pre-calculated)
    int8_t dir_vx[] = {4, 4, 3, 2, 0, -2, -3, -4, -4, -4, -3, -2, 0, 2, 3, 4};
    int8_t dir_vy[] = {0, 2, 3, 4, 4, 4, 3, 2, 0, -2, -3, -4, -4, -4, -3, -2};

    for (int d = 0; d < 16; d++) {
        for (int i = 0; i < MAX_PROJECTILES; i++) {
            if (!projectiles[i].active) {
                // Center 16x16 projectile on boss center
                projectiles[i].x = boss_cx - (PROJECTILE_SIZE / 2);
                projectiles[i].y = boss_cy - (PROJECTILE_SIZE / 2);
                projectiles[i].vx = dir_vx[d];
                projectiles[i].vy = dir_vy[d];
                projectiles[i].active = 1;
                projectiles[i].is_player_proj = 0;  // Boss projectile (enemy)
                projectiles[i].is_homing = 0;
                projectiles[i].homing_timer = 0;
                projectiles[i].is_boss_proj = 0;  // Use same sprite as regular projectiles
                projectiles[i].flip = 0;
                projectiles[i].source_type = SOURCE_TYPE_BOSS;
                break;
            }
        }
    }
}

// Boss homing attack - 3 projectiles that track player (uses enemy projectile)
void boss_homing_attack(void) {


    int boss_cx = boss.x + BOSS_SIZE / 2;
    int boss_cy = boss.y + BOSS_SIZE / 2;
    int player_cx = player_x + PLAYER_SIZE / 2;
    int player_cy = player_y + PLAYER_SIZE / 2;

    // Calculate initial direction toward player
    int dx = player_cx - boss_cx;
    int dy = player_cy - boss_cy;
    int abs_dx = abs_val(dx);
    int abs_dy = abs_val(dy);

    int base_vx, base_vy;
    if (abs_dx == 0 && abs_dy == 0) {
        base_vx = 0;
        base_vy = HOMING_SPEED;
    } else if (abs_dx > abs_dy) {
        base_vx = (dx > 0) ? HOMING_SPEED : -HOMING_SPEED;
        base_vy = (dy * HOMING_SPEED) / abs_dx;
    } else {
        base_vy = (dy > 0) ? HOMING_SPEED : -HOMING_SPEED;
        base_vx = (dx * HOMING_SPEED) / abs_dy;
    }

    // Spawn 3 homing projectiles with slight spread
    int h_offsets[] = {0, -20, 20};  // X offsets only

    int spawned = 0;
    for (int h = 0; h < 3; h++) {
        for (int i = 0; i < MAX_PROJECTILES; i++) {
            if (!projectiles[i].active) {
                // Center 16x16 projectile on boss center with spread offset
                projectiles[i].x = boss_cx - (PROJECTILE_SIZE / 2) + h_offsets[h];
                projectiles[i].y = boss_cy - (PROJECTILE_SIZE / 2);
                projectiles[i].vx = base_vx;
                projectiles[i].vy = base_vy;
                projectiles[i].active = 1;
                projectiles[i].is_player_proj = 0;  // Boss projectile (enemy)
                projectiles[i].is_homing = 1;
                projectiles[i].homing_timer = HOMING_DURATION;
                projectiles[i].is_boss_proj = 0;  // Use same sprite as regular projectiles
                projectiles[i].flip = 0;
                projectiles[i].source_type = SOURCE_TYPE_BOSS;
                spawned++;
                break;
            }
        }
    }

}

// Check if any ability is ready (off cooldown)
// Returns attack type if ability ready, or -1 if none
// Priority: Summon (phase 2) > Burst > Homing
int boss_check_ability_ready(void) {
    // Check phase transition
    if (boss.health <= BOSS_PHASE2_THRESHOLD && boss.phase == 1) {
        boss.phase = 2;
        boss.summon_cooldown = 0;  // Summon ready immediately when phase 2 starts
    }

    // Summon: only in phase 2, cooldown ready, and enemy slots available
    if (boss.phase >= 2 && boss.summon_cooldown == 0) {
        int has_slots = 0;
        for (int i = 0; i < MAX_ENEMIES; i++) {
            if (!enemies[i].active) {
                has_slots = 1;
                break;
            }
        }
        if (has_slots) {
            return BOSS_ATTACK_SUMMON;
        }
    }

    // Burst: 8-direction spread (always available when off cooldown)
    if (boss.burst_cooldown == 0) {
        return BOSS_ATTACK_BURST;
    }

    // Homing: 3 tracking projectiles (always available when off cooldown)
    if (boss.homing_cooldown == 0) {
        return BOSS_ATTACK_HOMING;
    }

    return -1;  // No ability ready
}

// Execute boss attack and set appropriate cooldown
void boss_execute_attack(int attack_type) {
    // Start attack animation
    boss.anim_state = 2;  // Attack state
    boss.anim_frame = 0;
    boss.anim_counter = 0;

    switch (attack_type) {
        case BOSS_ATTACK_SINGLE:

            boss_spawn_projectile();
            boss.attack_cooldown = BOSS_ATTACK_SPEED;  // Reset basic attack cooldown
            break;
        case BOSS_ATTACK_SUMMON:

            boss_summon_bats();
            boss.summon_cooldown = BOSS_CD_SUMMON;  // Reset summon cooldown
            break;
        case BOSS_ATTACK_BURST:

            boss_burst_attack();
            boss.burst_cooldown = BOSS_CD_BURST;
            break;
        case BOSS_ATTACK_HOMING:

            boss_homing_attack();
            boss.homing_cooldown = BOSS_CD_HOMING;
            break;
    }
    boss.last_attack = attack_type;
}

// Update boss movement - smarter AI that maintains distance from player
void boss_update_movement(void) {
    // Calculate distance to player
    int boss_cx = boss.x + BOSS_SIZE / 2;
    int boss_cy = boss.y + BOSS_SIZE / 2;
    int player_cx = player_x + PLAYER_SIZE / 2;
    int player_cy = player_y + PLAYER_SIZE / 2;

    int dx = player_cx - boss_cx;
    int dy = player_cy - boss_cy;
    int dist = abs_val(dx) + abs_val(dy);  // Manhattan distance

    int move_x = 0, move_y = 0;
    int speed = BOSS_MOVE_SPEED;

    if (dist > BOSS_TOO_FAR) {
        // Too far from player - chase them
        speed = BOSS_CHASE_SPEED;
        if (abs_val(dx) > abs_val(dy)) {
            move_x = (dx > 0) ? 1 : -1;
        } else {
            move_y = (dy > 0) ? 1 : -1;
        }
    } else if (dist < BOSS_TOO_CLOSE) {
        // Too close - retreat
        if (abs_val(dx) > abs_val(dy)) {
            move_x = (dx > 0) ? -1 : 1;
        } else {
            move_y = (dy > 0) ? -1 : 1;
        }
    } else {
        // Good distance - strafe/wander randomly
        boss.wander_timer--;
        if (boss.wander_timer <= 0) {
            // Pick perpendicular movement (strafe around player)
            if (abs_val(dx) > abs_val(dy)) {
                // Player is more horizontal - move vertically
                boss.wander_dir_x = 0;
                boss.wander_dir_y = (rand() % 2) ? 1 : -1;
            } else {
                // Player is more vertical - move horizontally
                boss.wander_dir_x = (rand() % 2) ? 1 : -1;
                boss.wander_dir_y = 0;
            }
            boss.wander_timer = BOSS_WANDER_CHANGE;
        }
        move_x = boss.wander_dir_x;
        move_y = boss.wander_dir_y;
    }

    // Calculate new position
    int new_x = boss.x + move_x * speed;
    int new_y = boss.y + move_y * speed;

    // Clamp to game area (accounting for boss size)
    if (new_x < GAME_AREA_LEFT) new_x = GAME_AREA_LEFT;
    if (new_x > GAME_AREA_RIGHT - BOSS_SIZE) new_x = GAME_AREA_RIGHT - BOSS_SIZE;
    if (new_y < GAME_AREA_TOP) new_y = GAME_AREA_TOP;
    if (new_y > GAME_AREA_BOTTOM - BOSS_SIZE) new_y = GAME_AREA_BOTTOM - BOSS_SIZE;

    // Check tile collision before moving
    if (!check_tile_collision(new_x, new_y, BOSS_SIZE)) {
        boss.x = new_x;
        boss.y = new_y;
    } else {
        // Blocked - try just X or just Y movement
        int try_x = boss.x + move_x * speed;
        int try_y = boss.y + move_y * speed;

        // Clamp
        if (try_x < GAME_AREA_LEFT) try_x = GAME_AREA_LEFT;
        if (try_x > GAME_AREA_RIGHT - BOSS_SIZE) try_x = GAME_AREA_RIGHT - BOSS_SIZE;
        if (try_y < GAME_AREA_TOP) try_y = GAME_AREA_TOP;
        if (try_y > GAME_AREA_BOTTOM - BOSS_SIZE) try_y = GAME_AREA_BOTTOM - BOSS_SIZE;

        // Try X only
        if (!check_tile_collision(try_x, boss.y, BOSS_SIZE)) {
            boss.x = try_x;
        }
        // Try Y only
        else if (!check_tile_collision(boss.x, try_y, BOSS_SIZE)) {
            boss.y = try_y;
        }
        // Both blocked - pick new direction
        else {
            boss.wander_timer = 0;
        }
    }
}

// Update boss AI
void update_boss_ai(void) {
    if (!boss.active) return;

    // Handle death animation
    if (boss.is_dying) {
        // Continue death animation
        boss.anim_counter++;
        if (boss.anim_counter >= 12) {  // Slower death animation
            boss.anim_counter = 0;
            boss.anim_frame++;
            if (boss.anim_frame >= BOSS_ANIM_DEATH_FRAMES) {
                boss.anim_frame = BOSS_ANIM_DEATH_FRAMES - 1;  // Hold last frame
            }
            boss.frame = BOSS_ANIM_DEATH_START + boss.anim_frame;
        }

        boss.death_timer--;
        if (boss.death_timer <= 0) {
            boss.active = 0;

            // Now trigger win screen after death animation finished
            game_state = GAME_STATE_WIN;
            menu_selection = 0;
            update_game_state_hardware();
        }
        return;
    }

    // Decrement hit timer
    if (boss.hit_timer > 0) {
        boss.hit_timer--;
    }

    // Decrement all cooldowns
    if (boss.attack_cooldown > 0) boss.attack_cooldown--;
    if (boss.summon_cooldown > 0) boss.summon_cooldown--;
    if (boss.burst_cooldown > 0) boss.burst_cooldown--;
    if (boss.homing_cooldown > 0) boss.homing_cooldown--;

    // Update movement (random wander)
    boss_update_movement();

    // Face player (flip when player is on the left - sprite in BRAM faces right)
    int player_cx = player_x + PLAYER_SIZE / 2;
    int boss_cx = boss.x + BOSS_SIZE / 2;
    boss.flip = (player_cx < boss_cx) ? 1 : 0;

    // Attack logic: check if ability ready first, otherwise use basic ranged
    int ability = boss_check_ability_ready();
    if (ability >= 0) {
        // Ability is ready - use it!
        boss_execute_attack(ability);
    } else if (boss.attack_cooldown == 0) {
        // No ability ready, but basic attack is ready - use ranged
        boss_execute_attack(BOSS_ATTACK_SINGLE);
    }

    // Update animation
    boss.anim_counter++;
    if (boss.anim_counter >= 8) {  // Animation speed
        boss.anim_counter = 0;
        boss.anim_frame++;

        // Handle animation state transitions
        int max_frames;
        int start_frame;

        switch (boss.anim_state) {
            case 0:  // Idle
                max_frames = BOSS_ANIM_IDLE_FRAMES;
                start_frame = BOSS_ANIM_IDLE_START;
                break;
            case 1:  // Fly
                max_frames = BOSS_ANIM_FLY_FRAMES;
                start_frame = BOSS_ANIM_FLY_START;
                break;
            case 2:  // Attack
                max_frames = BOSS_ANIM_ATTACK_FRAMES;
                start_frame = BOSS_ANIM_ATTACK_START;
                if (boss.anim_frame >= max_frames) {
                    // Return to idle after attack
                    boss.anim_state = 0;
                    boss.anim_frame = 0;
                }
                break;
            case 3:  // Death
                max_frames = BOSS_ANIM_DEATH_FRAMES;
                start_frame = BOSS_ANIM_DEATH_START;
                break;
            default:
                max_frames = BOSS_ANIM_IDLE_FRAMES;
                start_frame = BOSS_ANIM_IDLE_START;
                break;
        }

        // Loop within current animation (except death)
        if (boss.anim_state != 3 && boss.anim_frame >= max_frames) {
            boss.anim_frame = 0;
        }

        boss.frame = start_frame + boss.anim_frame;
    }

    // Calculate frame if animation state changed mid-update
    int start_frame;
    switch (boss.anim_state) {
        case 0: start_frame = BOSS_ANIM_IDLE_START; break;
        case 1: start_frame = BOSS_ANIM_FLY_START; break;
        case 2: start_frame = BOSS_ANIM_ATTACK_START; break;
        case 3: start_frame = BOSS_ANIM_DEATH_START; break;
        default: start_frame = BOSS_ANIM_IDLE_START; break;
    }
    boss.frame = start_frame + boss.anim_frame;
}

// Check if player attack hits boss
void check_player_attack_boss(void) {
    if (!boss.active || boss.is_dying) return;
    if (!is_attacking) return;
    if (attack_hit_registered) return;  // Already hit something this attack

    // Use shared hitbox calculation
    int attack_x, attack_y, attack_w, attack_h;
    get_attack_hitbox(&attack_x, &attack_y, &attack_w, &attack_h);

    // Check collision with boss using rect_overlap
    if (rect_overlap(attack_x, attack_y, attack_w, attack_h,
                     boss.x, boss.y, BOSS_SIZE, BOSS_SIZE)) {
        // Hit!
        boss.health--;
        boss.hit_timer = BOSS_HIT_FLASH;
        attack_hit_registered = 1;


        if (boss.health <= 0) {
            // Start death animation
            boss.is_dying = 1;
            boss.anim_state = 3;  // Death
            boss.anim_frame = 0;
            boss.death_timer = 120;  // 2 seconds

        }
    }
}

// Spawn a projectile toward the player's current position (from a specific enemy)
void spawn_projectile(Enemy *e) {
    // Find inactive projectile slot
    for (int i = 0; i < MAX_PROJECTILES; i++) {
        if (!projectiles[i].active) {
            // Calculate enemy center and player center
            int enemy_cx = e->x + (ENEMY_SIZE / 2);
            int enemy_cy = e->y + (ENEMY_SIZE / 2);
            int player_cx = player_x + (PLAYER_SIZE / 2);
            int player_cy = player_y + (PLAYER_SIZE / 2);

            // Calculate direction vector to player
            int dx = player_cx - enemy_cx;
            int dy = player_cy - enemy_cy;

            // Normalize to get velocity components
            int abs_dx = abs_val(dx);
            int abs_dy = abs_val(dy);

            if (abs_dx == 0 && abs_dy == 0) {
                // Player is exactly on enemy, shoot down
                projectiles[i].vx = 0;
                projectiles[i].vy = PROJECTILE_SPEED;
            } else if (abs_dx > abs_dy) {
                // Horizontal is dominant
                projectiles[i].vx = (dx > 0) ? PROJECTILE_SPEED : -PROJECTILE_SPEED;
                projectiles[i].vy = (dy * PROJECTILE_SPEED) / abs_dx;
            } else {
                // Vertical is dominant
                projectiles[i].vy = (dy > 0) ? PROJECTILE_SPEED : -PROJECTILE_SPEED;
                projectiles[i].vx = (dx * PROJECTILE_SPEED) / abs_dy;
            }

            // Start projectile at enemy center
            projectiles[i].x = enemy_cx - (PROJECTILE_SIZE / 2);
            projectiles[i].y = enemy_cy - (PROJECTILE_SIZE / 2);
            projectiles[i].active = 1;
            projectiles[i].is_player_proj = 0;  // Enemy projectile
            projectiles[i].is_homing = 0;
            projectiles[i].is_boss_proj = 0;
            projectiles[i].source_type = e->sprite_type;
            return;
        }
    }
}

// Clamp enemy position to game boundaries
void clamp_enemy_position(Enemy *e) {
    if (e->x < GAME_AREA_LEFT) e->x = GAME_AREA_LEFT;
    if (e->x > GAME_AREA_RIGHT - ENEMY_SIZE) e->x = GAME_AREA_RIGHT - ENEMY_SIZE;
    if (e->y < GAME_AREA_TOP) e->y = GAME_AREA_TOP;
    if (e->y > GAME_AREA_BOTTOM - ENEMY_SIZE) e->y = GAME_AREA_BOTTOM - ENEMY_SIZE;
}

// Update ranged enemy AI
void update_ranged_enemy_ai(Enemy *e) {
    // Calculate distance to player (use center of sprites)
    int dx = (player_x + PLAYER_SIZE/2) - (e->x + ENEMY_SIZE/2);
    int dy = (player_y + PLAYER_SIZE/2) - (e->y + ENEMY_SIZE/2);
    int dist = abs_val(dx) + abs_val(dy);  // Manhattan distance

    // Decrement attack cooldown
    if (e->attack_cooldown > 0) {
        e->attack_cooldown--;
    }

    // ALWAYS face the player (backpedal behavior)
    if (abs_val(dx) > abs_val(dy)) {
        e->direction = (dx > 0) ? DIR_RIGHT : DIR_LEFT;
    } else {
        e->direction = (dy > 0) ? DIR_DOWN : DIR_UP;
    }

    // Ranged enemy AI: maintain distance from player
    int should_move = 0;
    int move_away = 0;

    if (dist < RANGED_RETREAT_DIST) {
        should_move = 1;
        move_away = 1;
    } else if (dist > RANGED_CHASE_DIST) {
        should_move = 1;
        move_away = 0;
    }

    if (should_move) {
        int move_x = 0, move_y = 0;

        if (abs_val(dx) > abs_val(dy)) {
            if (dx > 0) {
                move_x = move_away ? -RANGED_SPEED : RANGED_SPEED;
            } else {
                move_x = move_away ? RANGED_SPEED : -RANGED_SPEED;
            }
        } else {
            if (dy > 0) {
                move_y = move_away ? -RANGED_SPEED : RANGED_SPEED;
            } else {
                move_y = move_away ? RANGED_SPEED : -RANGED_SPEED;
            }
        }

        int old_x = e->x, old_y = e->y;
        e->x += move_x;
        e->y += move_y;
        clamp_enemy_position(e);

        // Check tile collision - revert if blocked
        if (check_tile_collision(e->x, e->y, ENEMY_SIZE)) {
            e->x = old_x;
            e->y = old_y;
        }
    }

    // Shoot if in range and cooldown ready
    if (dist < RANGED_SHOOT_RANGE && e->attack_cooldown == 0) {
        spawn_projectile(e);
        e->attack_cooldown = RANGED_SHOOT_COOLDOWN;
    }

    // Update animation (walk when moving, idle when standing)
    e->anim_counter++;
    if (e->anim_counter >= ANIM_SPEED) {
        e->anim_counter = 0;
        e->anim_frame = (e->anim_frame + 1) % FRAMES_PER_ANIM;
    }

    // Calculate frame (enemy type 0 uses frames 0-7)
    // Frame within enemy type: {is_walk, anim_frame[1:0]}
    int local_frame = should_move ? 4 : 0;  // Walk starts at frame 4
    local_frame += e->anim_frame;
    e->frame = local_frame;  // 0-7 for ranged enemy
}

// Simple pseudo-random number generator
static unsigned int rand_seed = 12345;
int simple_rand(void) {
    rand_seed = rand_seed * 1103515245 + 12345;
    return (rand_seed >> 16) & 0x7FFF;
}

// Update melee enemy AI - simple behavior:
// - When cooldown ready: dash to player's current position
// - When cooldown not ready: wander randomly in a small area
void update_melee_enemy_ai(Enemy *e) {
    // Decrement attack cooldown
    if (e->attack_cooldown > 0) {
        e->attack_cooldown--;
    }

    // If currently dashing, continue the dash
    if (e->is_dashing) {
        e->is_attacking = 1;  // Red tint while dashing
        e->dash_timer++;      // Increment dash duration

        // Check if dash has gone on too long (stuck on wall)
        if (e->dash_timer >= MELEE_MAX_DASH_TIME) {
            e->is_dashing = 0;
            e->is_attacking = 0;
            e->dash_timer = 0;
            e->attack_cooldown = MELEE_ATTACK_COOLDOWN;
        } else {
            // Calculate direction to dash target
            int target_dx = e->dash_target_x - e->x;
            int target_dy = e->dash_target_y - e->y;
            int target_dist = abs_val(target_dx) + abs_val(target_dy);

            // Check if reached target (within dash speed distance)
            if (target_dist <= MELEE_DASH_SPEED) {
                // Reached target, stop dashing
                e->x = e->dash_target_x;
                e->y = e->dash_target_y;
                e->is_dashing = 0;
                e->is_attacking = 0;
                e->dash_timer = 0;
                e->attack_cooldown = MELEE_ATTACK_COOLDOWN;
            } else {
                // Move toward target at dash speed
                int move_x = 0, move_y = 0;

                if (abs_val(target_dx) > abs_val(target_dy)) {
                    move_x = (target_dx > 0) ? MELEE_DASH_SPEED : -MELEE_DASH_SPEED;
                    move_y = (target_dy * MELEE_DASH_SPEED) / abs_val(target_dx);
                } else if (abs_val(target_dy) > 0) {
                    move_y = (target_dy > 0) ? MELEE_DASH_SPEED : -MELEE_DASH_SPEED;
                    move_x = (target_dx * MELEE_DASH_SPEED) / abs_val(target_dy);
                }

                int old_x = e->x, old_y = e->y;
                e->x += move_x;
                e->y += move_y;
                clamp_enemy_position(e);

                // Check tile collision during dash - stop dash if blocked
                if (check_tile_collision(e->x, e->y, ENEMY_SIZE)) {
                    e->x = old_x;
                    e->y = old_y;
                    e->is_dashing = 0;        // Stop the dash
                    e->is_attacking = 0;       // End attack state
                    e->dash_timer = 0;
                    e->attack_cooldown = MELEE_ATTACK_COOLDOWN;  // Reset cooldown
                }

                // Update facing direction during dash
                if (abs_val(target_dx) > abs_val(target_dy)) {
                    e->direction = (target_dx > 0) ? DIR_RIGHT : DIR_LEFT;
                } else {
                    e->direction = (target_dy > 0) ? DIR_DOWN : DIR_UP;
                }
            }
        }
    } else {
        // Not dashing
        e->is_attacking = 0;

        // Check if cooldown is ready - start dash attack!
        if (e->attack_cooldown == 0) {
            // Start dash attack to player's current position
            e->is_dashing = 1;
            e->is_attacking = 1;
            e->dash_timer = 0;  // Reset dash timer
            e->dash_target_x = player_x;
            e->dash_target_y = player_y;

        } else {
            // Cooldown not ready - wander randomly in small area
            e->wander_timer--;

            if (e->wander_timer <= 0) {
                // Pick new random wander direction
                int r = simple_rand();
                e->wander_dir_x = (r % 3) - 1;  // -1, 0, or 1
                e->wander_dir_y = ((r >> 4) % 3) - 1;  // -1, 0, or 1
                e->wander_timer = MELEE_WANDER_CHANGE;
            }

            // Move in wander direction
            int old_x = e->x, old_y = e->y;
            e->x += e->wander_dir_x * MELEE_SPEED;
            e->y += e->wander_dir_y * MELEE_SPEED;
            clamp_enemy_position(e);

            // Check tile collision - revert if blocked
            if (check_tile_collision(e->x, e->y, ENEMY_SIZE)) {
                e->x = old_x;
                e->y = old_y;
                // Pick new wander direction on next frame
                e->wander_timer = 0;
            }

            // Face wander direction - prioritize left/right for flip
            if (e->wander_dir_x > 0) {
                e->direction = DIR_RIGHT;
            } else if (e->wander_dir_x < 0) {
                e->direction = DIR_LEFT;
            } else if (e->wander_dir_y > 0) {
                e->direction = DIR_DOWN;
            } else if (e->wander_dir_y < 0) {
                e->direction = DIR_UP;
            }
        }
    }

    // Update animation
    e->anim_counter++;
    if (e->anim_counter >= ANIM_SPEED) {
        e->anim_counter = 0;
        e->anim_frame = (e->anim_frame + 1) % FRAMES_PER_ANIM;
    }

    // Calculate frame: walk animation when dashing or wandering, idle otherwise
    int is_moving = e->is_dashing || (e->wander_dir_x != 0) || (e->wander_dir_y != 0);
    int local_frame = is_moving ? 4 : 0;
    local_frame += e->anim_frame;
    e->frame = local_frame;
}

// Handle enemy-to-enemy collision - push apart if overlapping
// Also checks tile collision to prevent pushing into obstacles
void handle_enemy_collision(void) {
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (!enemies[i].active) continue;

        for (int j = i + 1; j < MAX_ENEMIES; j++) {
            if (!enemies[j].active) continue;

            // Calculate distance between enemy centers
            int cx_i = enemies[i].x + ENEMY_SIZE / 2;
            int cy_i = enemies[i].y + ENEMY_SIZE / 2;
            int cx_j = enemies[j].x + ENEMY_SIZE / 2;
            int cy_j = enemies[j].y + ENEMY_SIZE / 2;

            int dx = cx_j - cx_i;
            int dy = cy_j - cy_i;
            int dist = abs_val(dx) + abs_val(dy);

            // If too close, push apart
            if (dist < ENEMY_COLLISION_DIST) {
                // Determine push direction
                int push_x = 0, push_y = 0;

                if (dist == 0) {
                    // Exactly overlapping - push in arbitrary direction
                    push_x = ENEMY_PUSH_SPEED;
                } else if (abs_val(dx) > abs_val(dy)) {
                    // Push horizontally
                    push_x = (dx > 0) ? -ENEMY_PUSH_SPEED : ENEMY_PUSH_SPEED;
                } else {
                    // Push vertically
                    push_y = (dy > 0) ? -ENEMY_PUSH_SPEED : ENEMY_PUSH_SPEED;
                }

                // Push enemy i in opposite direction from enemy j
                // Check tile collision before applying push
                int old_i_x = enemies[i].x, old_i_y = enemies[i].y;
                enemies[i].x += push_x;
                enemies[i].y += push_y;
                clamp_enemy_position(&enemies[i]);
                if (check_tile_collision(enemies[i].x, enemies[i].y, ENEMY_SIZE)) {
                    enemies[i].x = old_i_x;
                    enemies[i].y = old_i_y;
                }

                // Push enemy j away from enemy i
                int old_j_x = enemies[j].x, old_j_y = enemies[j].y;
                enemies[j].x -= push_x;
                enemies[j].y -= push_y;
                clamp_enemy_position(&enemies[j]);
                if (check_tile_collision(enemies[j].x, enemies[j].y, ENEMY_SIZE)) {
                    enemies[j].x = old_j_x;
                    enemies[j].y = old_j_y;
                }
            }
        }
    }
}

// Update all enemies
void update_all_enemies_ai(void) {
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (!enemies[i].active) continue;

        if (enemies[i].enemy_type == ENEMY_TYPE_RANGED) {
            update_ranged_enemy_ai(&enemies[i]);
        } else {
            update_melee_enemy_ai(&enemies[i]);
        }
    }

    // Handle enemy-to-enemy collision after AI updates
    handle_enemy_collision();
}

// Check if all enemies are defeated
int all_enemies_dead(void) {
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (enemies[i].active) return 0;
    }
    return 1;
}

// Update all projectiles
void update_projectiles(void) {
    for (int i = 0; i < MAX_PROJECTILES; i++) {
        if (!projectiles[i].active) continue;

        // Handle homing projectiles
        if (projectiles[i].is_homing && projectiles[i].homing_timer > 0) {
            projectiles[i].homing_timer--;

            // Recalculate velocity toward player
            int proj_cx = projectiles[i].x + PROJECTILE_SIZE / 2;
            int proj_cy = projectiles[i].y + PROJECTILE_SIZE / 2;
            int player_cx = player_x + PLAYER_SIZE / 2;
            int player_cy = player_y + PLAYER_SIZE / 2;

            int dx = player_cx - proj_cx;
            int dy = player_cy - proj_cy;
            int abs_dx = abs_val(dx);
            int abs_dy = abs_val(dy);

            // Gradually adjust velocity toward player
            if (abs_dx > 0 || abs_dy > 0) {
                int target_vx, target_vy;
                if (abs_dx > abs_dy) {
                    target_vx = (dx > 0) ? HOMING_SPEED : -HOMING_SPEED;
                    target_vy = (abs_dx > 0) ? (dy * HOMING_SPEED) / abs_dx : 0;
                } else {
                    target_vy = (dy > 0) ? HOMING_SPEED : -HOMING_SPEED;
                    target_vx = (abs_dy > 0) ? (dx * HOMING_SPEED) / abs_dy : 0;
                }

                // Smooth turn toward target (gradual adjustment)
                if (projectiles[i].vx < target_vx) {
                    projectiles[i].vx += HOMING_TURN_RATE;
                    if (projectiles[i].vx > target_vx) projectiles[i].vx = target_vx;
                } else if (projectiles[i].vx > target_vx) {
                    projectiles[i].vx -= HOMING_TURN_RATE;
                    if (projectiles[i].vx < target_vx) projectiles[i].vx = target_vx;
                }

                if (projectiles[i].vy < target_vy) {
                    projectiles[i].vy += HOMING_TURN_RATE;
                    if (projectiles[i].vy > target_vy) projectiles[i].vy = target_vy;
                } else if (projectiles[i].vy > target_vy) {
                    projectiles[i].vy -= HOMING_TURN_RATE;
                    if (projectiles[i].vy < target_vy) projectiles[i].vy = target_vy;
                }

                // Update flip for boss projectiles based on current velocity
                if (projectiles[i].is_boss_proj) {
                    projectiles[i].flip = (projectiles[i].vx < 0) ? 1 : 0;
                }
            }
        }

        // Move projectile
        projectiles[i].x += projectiles[i].vx;
        projectiles[i].y += projectiles[i].vy;

        // Check if projectile is off-screen
        if (projectiles[i].x < 0 || projectiles[i].x > 640 ||
            projectiles[i].y < 0 || projectiles[i].y > 480) {
            projectiles[i].active = 0;
            continue;
        }

        // Check if projectile hits obstacle
        if (check_tile_collision(projectiles[i].x, projectiles[i].y, PROJECTILE_SIZE)) {
            projectiles[i].active = 0;  // Destroy on obstacle hit
        }
    }
}

// Get attack hitbox based on player direction
void get_attack_hitbox(int *hx, int *hy, int *hw, int *hh) {
    *hw = ATTACK_HITBOX_SIZE;
    *hh = ATTACK_HITBOX_SIZE;

    switch (player_dir) {
        case DIR_DOWN:
            *hx = player_x;
            *hy = player_y + 16;    // Below player
            break;
        case DIR_UP:
            *hx = player_x;
            *hy = player_y - 16;    // Above player (changed from -32 to -16 for consistency)
            break;
        case DIR_RIGHT:
            *hx = player_x + 16;    // Right of player
            *hy = player_y;
            break;
        case DIR_LEFT:
            *hx = player_x - 16;    // Left of player (changed from -32 to -16 for consistency)
            *hy = player_y;
            break;
    }
}

// Check rectangle collision
int rect_overlap(int x1, int y1, int w1, int h1, int x2, int y2, int w2, int h2) {
    return (x1 < x2 + w2) && (x1 + w1 > x2) &&
           (y1 < y2 + h2) && (y1 + h1 > y2);
}

// Check player attack collision with all enemies
void check_player_attack_hit(void) {
    if (!is_attacking || attack_hit_registered) return;

    int hx, hy, hw, hh;
    get_attack_hitbox(&hx, &hy, &hw, &hh);

    // Check if attack hitbox is blocked by obstacle
    if (is_attack_blocked(hx, hy, hw)) {
        return;  // Attack blocked by obstacle
    }

    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (!enemies[i].active) continue;

        // Check if enemy overlaps with directional hitbox
        if (rect_overlap(hx, hy, hw, hh, enemies[i].x, enemies[i].y, ENEMY_SIZE, ENEMY_SIZE)) {
            enemies[i].health--;
            enemies[i].hit_timer = HIT_FLASH_DURATION;  // Start red flash effect
            attack_hit_registered = 1;  // Only hit once per attack


            // Apply knockback - push enemy away from player
            int dx = enemies[i].x - player_x;
            int dy = enemies[i].y - player_y;
            int old_ex = enemies[i].x, old_ey = enemies[i].y;

            if (abs_val(dx) > abs_val(dy)) {
                enemies[i].x += (dx > 0) ? KNOCKBACK_DISTANCE : -KNOCKBACK_DISTANCE;
            } else {
                enemies[i].y += (dy > 0) ? KNOCKBACK_DISTANCE : -KNOCKBACK_DISTANCE;
            }

            clamp_enemy_position(&enemies[i]);

            // Check tile collision - revert if knocked into obstacle
            if (check_tile_collision(enemies[i].x, enemies[i].y, ENEMY_SIZE)) {
                enemies[i].x = old_ex;
                enemies[i].y = old_ey;
            }

            // Cancel dash if melee enemy was hit during dash
            if (enemies[i].is_dashing) {
                enemies[i].is_dashing = 0;
                enemies[i].is_attacking = 0;
            }

            if (enemies[i].health <= 0) {
                enemies[i].active = 0;

            }
            break;  // Only hit one enemy per attack
        }
    }
}

// Check player attack collision with projectiles (destroy them)
// Only destroys projectiles in the direction player is attacking
void check_player_attack_projectiles(void) {
    if (!is_attacking) return;

    int hx, hy, hw, hh;
    get_attack_hitbox(&hx, &hy, &hw, &hh);

    // Player center
    int player_cx = player_x + PLAYER_SIZE / 2;
    int player_cy = player_y + PLAYER_SIZE / 2;

    // Destroy projectiles that overlap with attack hitbox AND are in attack direction
    for (int i = 0; i < MAX_PROJECTILES; i++) {
        if (!projectiles[i].active) continue;

        // Projectile center
        int proj_cx = projectiles[i].x + PROJECTILE_SIZE / 2;
        int proj_cy = projectiles[i].y + PROJECTILE_SIZE / 2;

        // Check if projectile is in the attack direction
        int in_direction = 0;
        switch (player_dir) {
            case DIR_UP:
                in_direction = (proj_cy < player_cy);
                break;
            case DIR_DOWN:
                in_direction = (proj_cy > player_cy);
                break;
            case DIR_LEFT:
                in_direction = (proj_cx < player_cx);
                break;
            case DIR_RIGHT:
                in_direction = (proj_cx > player_cx);
                break;
        }

        if (!in_direction) continue;

        if (rect_overlap(hx, hy, hw, hh,
                        projectiles[i].x, projectiles[i].y,
                        PROJECTILE_SIZE, PROJECTILE_SIZE)) {
            projectiles[i].active = 0;  // Destroy projectile
        }
    }
}

// Check player projectiles hitting enemies and boss
void check_player_projectile_hits(void) {
    for (int i = 0; i < MAX_PROJECTILES; i++) {
        if (!projectiles[i].active || !projectiles[i].is_player_proj) continue;

        int px = projectiles[i].x, py = projectiles[i].y;

        // Check collision with boss
        if (boss.active && !boss.is_dying &&
            rect_overlap(px, py, PROJECTILE_SIZE, PROJECTILE_SIZE, boss.x, boss.y, BOSS_SIZE, BOSS_SIZE)) {
            boss.health -= PLAYER_PROJ_DAMAGE;
            boss.hit_timer = BOSS_HIT_FLASH;
            projectiles[i].active = 0;
            if (boss.health <= 0) {
                boss.is_dying = 1;
                boss.death_timer = 120;
                boss.anim_state = 3;
                boss.anim_frame = 0;
            }
            continue;
        }

        // Check collision with enemies
        for (int j = 0; j < MAX_ENEMIES; j++) {
            if (!enemies[j].active) continue;
            if (rect_overlap(px, py, PROJECTILE_SIZE, PROJECTILE_SIZE, enemies[j].x, enemies[j].y, ENEMY_SIZE, ENEMY_SIZE)) {
                enemies[j].health -= PLAYER_PROJ_DAMAGE;
                enemies[j].hit_timer = HIT_FLASH_DURATION;
                projectiles[i].active = 0;
                if (enemies[j].health <= 0) enemies[j].active = 0;
                break;
            }
        }
    }
}

// Apply knockback to player (push away from attacker)
void apply_player_knockback(int attacker_x, int attacker_y) {
    int dx = player_x - attacker_x;
    int dy = player_y - attacker_y;
    int old_x = player_x, old_y = player_y;

    if (abs_val(dx) > abs_val(dy)) {
        player_x += (dx > 0) ? PLAYER_KNOCKBACK_DIST : -PLAYER_KNOCKBACK_DIST;
    } else {
        player_y += (dy > 0) ? PLAYER_KNOCKBACK_DIST : -PLAYER_KNOCKBACK_DIST;
    }

    // Clamp player position
    if (player_x < GAME_AREA_LEFT) player_x = GAME_AREA_LEFT;
    if (player_x > GAME_AREA_RIGHT) player_x = GAME_AREA_RIGHT;
    if (player_y < GAME_AREA_TOP) player_y = GAME_AREA_TOP;
    if (player_y > GAME_AREA_BOTTOM) player_y = GAME_AREA_BOTTOM;

    // Check tile collision - revert if knocked into obstacle
    if (check_tile_collision(player_x, player_y, PLAYER_SIZE)) {
        player_x = old_x;
        player_y = old_y;
    }
}

// Check melee enemy dash collision with player
void check_melee_dash_collisions(void) {
    if (player_invincible > 0) return;

    // Player hitbox
    int px = player_x + 4;
    int py = player_y + 4;
    int pw = PLAYER_SIZE - 8;
    int ph = PLAYER_SIZE - 8;

    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (!enemies[i].active) continue;
        if (enemies[i].enemy_type != ENEMY_TYPE_MELEE) continue;
        if (!enemies[i].is_dashing) continue;

        // Check collision with dashing melee enemy
        if (rect_overlap(px, py, pw, ph,
                        enemies[i].x, enemies[i].y, ENEMY_SIZE, ENEMY_SIZE)) {
            // Player hit by melee dash!
            // Strong enemies (types 0 and 5) deal 2 damage
            int dmg = (enemies[i].sprite_type == SPRITE_TYPE_0 || enemies[i].sprite_type == SPRITE_TYPE_5)
                      ? ENEMY_STRONG_DAMAGE : ENEMY_WEAK_DAMAGE;
            player_take_damage(dmg);

            // Knockback player away from enemy
            apply_player_knockback(enemies[i].x, enemies[i].y);

            // Stop the dash
            enemies[i].is_dashing = 0;
            enemies[i].is_attacking = 0;
            enemies[i].attack_cooldown = MELEE_ATTACK_COOLDOWN;

            if (player_health <= 0) {

            }
            break;
        }
    }
}

// Check projectile collision with player
void check_projectile_collisions(void) {
    if (player_invincible > 0) return;  // Skip if invincible

    // Player hitbox (smaller than sprite for better gameplay)
    int px = player_x + 4;
    int py = player_y + 4;
    int pw = PLAYER_SIZE - 8;  // 24x24 hitbox
    int ph = PLAYER_SIZE - 8;

    for (int i = 0; i < MAX_PROJECTILES; i++) {
        if (!projectiles[i].active) continue;
        if (projectiles[i].is_player_proj) continue;  // Skip player's own projectiles

        if (rect_overlap(px, py, pw, ph,
                        projectiles[i].x, projectiles[i].y,
                        PROJECTILE_SIZE, PROJECTILE_SIZE)) {
            // Player hit by projectile
            projectiles[i].active = 0;
            // Homing projectiles deal reduced damage
            // Strong enemies (types 0, 5) and boss deal 2 damage, others deal 1
            int dmg;
            if (projectiles[i].is_homing) {
                dmg = HOMING_DAMAGE;  // Homing always deals 1 damage
            } else {
                int src = projectiles[i].source_type;
                dmg = (src == SPRITE_TYPE_0 || src == SPRITE_TYPE_5 || src == SOURCE_TYPE_BOSS)
                          ? ENEMY_STRONG_DAMAGE : ENEMY_WEAK_DAMAGE;
            }
            player_take_damage(dmg);


            if (player_health <= 0) {

            }
        }
    }
}

// Write enemy state to hardware
// ENEMY_ACTIVE_REG format: {enemy_type[7:4], hit[3], attacking[2], flip[1], active[0]}
// Helper to write a single enemy to hardware
void write_enemy_hardware(int idx, u32 x_reg, u32 y_reg, u32 frame_reg, u32 active_reg) {
    Xil_Out32(x_reg, enemies[idx].x);
    Xil_Out32(y_reg, enemies[idx].y);
    Xil_Out32(frame_reg, enemies[idx].frame);
    int flip = (enemies[idx].direction == DIR_LEFT) ? 1 : 0;
    int attack = enemies[idx].is_attacking ? 1 : 0;
    int hit = (enemies[idx].hit_timer > 0) ? 1 : 0;
    // Use sprite_type for hardware (BRAM index), not enemy_type (behavior)
    // Format: {type[7:4], hit[3], attacking[2], flip[1], active[0]}
    Xil_Out32(active_reg, (enemies[idx].sprite_type << 4) | (hit << 3) | (attack << 2) | (flip << 1) | enemies[idx].active);
}

void update_enemies_hardware(void) {
    // Enemy 0
    write_enemy_hardware(0, ENEMY0_X_REG, ENEMY0_Y_REG, ENEMY0_FRAME_REG, ENEMY0_ACTIVE_REG);

    // Enemy 1
    write_enemy_hardware(1, ENEMY1_X_REG, ENEMY1_Y_REG, ENEMY1_FRAME_REG, ENEMY1_ACTIVE_REG);

    // Enemy 2
    write_enemy_hardware(2, ENEMY2_X_REG, ENEMY2_Y_REG, ENEMY2_FRAME_REG, ENEMY2_ACTIVE_REG);

    // Enemy 3
    write_enemy_hardware(3, ENEMY3_X_REG, ENEMY3_Y_REG, ENEMY3_FRAME_REG, ENEMY3_ACTIVE_REG);

    // Enemy 4
    write_enemy_hardware(4, ENEMY4_X_REG, ENEMY4_Y_REG, ENEMY4_FRAME_REG, ENEMY4_ACTIVE_REG);
}

// Write projectile state to hardware
void update_projectiles_hardware(void) {
    Xil_Out32(PROJ_0_REG, pack_projectile(&projectiles[0]));
    Xil_Out32(PROJ_1_REG, pack_projectile(&projectiles[1]));
    Xil_Out32(PROJ_2_REG, pack_projectile(&projectiles[2]));
    Xil_Out32(PROJ_3_REG, pack_projectile(&projectiles[3]));
    Xil_Out32(PROJ_4_REG, pack_projectile(&projectiles[4]));
    Xil_Out32(PROJ_5_REG, pack_projectile(&projectiles[5]));
    Xil_Out32(PROJ_6_REG, pack_projectile(&projectiles[6]));
    Xil_Out32(PROJ_7_REG, pack_projectile(&projectiles[7]));
    Xil_Out32(PROJ_8_REG, pack_projectile(&projectiles[8]));
    Xil_Out32(PROJ_9_REG, pack_projectile(&projectiles[9]));
    Xil_Out32(PROJ_10_REG, pack_projectile(&projectiles[10]));
    Xil_Out32(PROJ_11_REG, pack_projectile(&projectiles[11]));
    Xil_Out32(PROJ_12_REG, pack_projectile(&projectiles[12]));
    Xil_Out32(PROJ_13_REG, pack_projectile(&projectiles[13]));
    Xil_Out32(PROJ_14_REG, pack_projectile(&projectiles[14]));
    Xil_Out32(PROJ_15_REG, pack_projectile(&projectiles[15]));
}

// Write player health to hardware (for status bar display)
void update_player_health_hardware(void) {
    Xil_Out32(PLAYER_HEALTH_REG, player_health);
}

// Write player armor to hardware (for armor bar display)
void update_player_armor_hardware(void) {
    Xil_Out32(PLAYER_ARMOR_REG, player_armor);
}

// Apply damage to player (armor absorbs first, then health)
void player_take_damage(int damage) {
    int remaining = damage;

    // Armor absorbs damage first
    while (remaining > 0 && player_armor > 0) {
        player_armor--;
        remaining--;
    }
    update_player_armor_hardware();

    // Remaining damage goes to health
    if (remaining > 0) {
        player_health -= remaining;
        if (player_health < 0) player_health = 0;
        update_player_health_hardware();
    }

    // Reset armor regeneration cooldown (5 seconds)
    armor_regen_cooldown = ARMOR_REGEN_DELAY;
    armor_regen_timer = 0;
    // Set invincibility
    player_invincible = PLAYER_INVINCIBILITY;
}

// Update game state to hardware
void update_game_state_hardware(void) {
    Xil_Out32(GAME_STATE_REG, game_state);
    Xil_Out32(MENU_SELECT_REG, menu_selection);
}

// Reset game state for new game
void reset_game(void) {

    // Reset player state - spawn in center of room
    player_x = 320;
    player_y = 240;
    player_health = PLAYER_MAX_HP;
    player_invincible = 0;
    vel_x = 0;
    vel_y = 0;
    player_dir = DIR_DOWN;
    is_moving = 0;
    anim_frame = 0;
    anim_counter = 0;
    is_attacking = 0;
    attack_anim_frame = 0;
    attack_anim_counter = 0;
    attack_cooldown = 0;
    attack_hit_registered = 0;

    // Reset hold counters
    hold_up = 0;
    hold_down = 0;
    hold_left = 0;
    hold_right = 0;

    // Reset armor system
    player_armor = PLAYER_MAX_ARMOR;
    armor_regen_cooldown = 0;
    armor_regen_timer = 0;

    // Reset level/room tracking
    current_level = 1;
    current_room = 0;
    entry_direction = DIR_BREACH_RIGHT;  // First room, no entry direction yet
    exit_direction = DIR_BREACH_RIGHT;   // Will open on right side

    // Reset boss (inactive until boss room)
    boss.active = 0;

    // Load first room (empty spawn room for level 1)
    int first_template = get_room_template(current_level, current_room);
    load_room(first_template);

    // Set level tinting (0 for level 1)
    Xil_Out32(LEVEL_REG, 0);

    // Update all hardware
    update_player_hardware();
    update_player_health_hardware();
    update_player_armor_hardware();
    update_boss_hardware();
}

// Update all battle system logic
void update_battle_system(void) {
    // Update invincibility timer
    if (player_invincible > 0) {
        player_invincible--;
    }

    // Update armor regeneration
    if (armor_regen_cooldown > 0) {
        // Waiting for regen to start (5 sec after hit)
        armor_regen_cooldown--;
    } else if (player_armor < PLAYER_MAX_ARMOR) {
        // Regenerating armor
        armor_regen_timer++;
        if (armor_regen_timer >= ARMOR_REGEN_RATE) {
            player_armor++;
            armor_regen_timer = 0;

            update_player_armor_hardware();
        }
    }

    // Update hit timers for all enemies (decrement red flash duration)
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (enemies[i].hit_timer > 0) {
            enemies[i].hit_timer--;
        }
    }

    // Update all enemy AI
    update_all_enemies_ai();

    // Update boss AI
    update_boss_ai();

    // Update projectiles
    update_projectiles();

    // Check collisions
    check_player_attack_hit();
    check_player_attack_boss();  // Check if player hits boss
    check_player_attack_projectiles();  // Destroy projectiles with attack
    check_player_projectile_hits();  // Check player shuriken hitting enemies/boss
    check_melee_dash_collisions();
    check_projectile_collisions();

    // Update player projectile animation frame (rotate 0-3)
    player_proj_anim_counter++;
    if (player_proj_anim_counter >= 4) {  // Fast rotation for shuriken
        player_proj_anim_counter = 0;
        player_proj_frame = (player_proj_frame + 1) & 0x3;  // 0-3 wrap
    }

    // Update ranged cooldown
    if (ranged_cooldown > 0) {
        ranged_cooldown--;
    }

    // Write to hardware
    update_enemies_hardware();
    update_boss_hardware();  // Update boss hardware
    update_projectiles_hardware();
    Xil_Out32(PLAYER_PROJ_FRAME_REG, player_proj_frame);  // Update player proj animation frame
    update_player_health_hardware();
}

void update_animation(void) {
    int frame;

    // Decrement attack cooldown
    if (attack_cooldown > 0) {
        attack_cooldown--;
    }

    // Handle attack animation (takes priority over movement)
    if (is_attacking) {
        attack_anim_counter++;
        if (attack_anim_counter >= ATTACK_ANIM_SPEED) {
            attack_anim_counter = 0;
            attack_anim_frame++;
            if (attack_anim_frame >= ATTACK_FRAMES) {
                // Attack animation complete - exit attack mode
                is_attacking = 0;
                attack_anim_frame = 0;
                // Fall through to idle/run animation below (don't calculate attack frame)
            }
        }
    }

    // Calculate frame based on current state
    if (is_attacking) {
        // Calculate attack frame: ATTACK_BASE + attack_dir * 5 + attack_anim_frame
        int attack_dir = get_attack_dir(player_dir);
        frame = ATTACK_BASE + (attack_dir * ATTACK_FRAMES) + attack_anim_frame;
    } else {
        // Normal movement animation
        anim_counter++;
        if (anim_counter >= ANIM_SPEED) {
            anim_counter = 0;
            anim_frame = (anim_frame + 1) % FRAMES_PER_ANIM;
        }

        // Calculate sprite frame number
        // Frame = base + direction * 8 + anim_frame
        int base = is_moving ? RUN_BASE : IDLE_BASE;
        frame = base + (player_dir * FRAMES_PER_ANIM) + anim_frame;
    }

    Xil_Out32(PLAYER_FRAME_REG, frame);
}

void update_player_hardware(void) {
    Xil_Out32(PLAYER_X_REG, player_x);
    Xil_Out32(PLAYER_Y_REG, player_y);
    update_animation();
}

void process_keyboard(BOOT_KBD_REPORT* kbd) {
    int move_up = 0, move_down = 0, move_left = 0, move_right = 0;
    int attack_pressed = 0;
    int ranged_pressed = 0;

    // Check all 6 possible keycodes in HID report
    for (int i = 0; i < 6; i++) {
        BYTE key = kbd->keycode[i];
        if (key == 0) continue;

        switch (key) {
            case KEY_W: move_up = 1; break;
            case KEY_S: move_down = 1; break;
            case KEY_A: move_left = 1; break;
            case KEY_D: move_right = 1; break;
            case KEY_J: attack_pressed = 1; break;
            case KEY_K: ranged_pressed = 1; break;  // Ranged attack key
        }
    }

    // Handle ranged attack (K key) - only available on Level 2
    if (ranged_pressed && ranged_cooldown == 0 && current_level == 2) {
        spawn_player_projectile();
        ranged_cooldown = RANGED_COOLDOWN;
    }

    // Handle attack (J key) - start attack if not already attacking and cooldown is done
    if (attack_pressed && !is_attacking && attack_cooldown == 0) {
        is_attacking = 1;
        attack_anim_frame = 0;
        attack_anim_counter = 0;
        attack_cooldown = ATTACK_COOLDOWN;  // Set cooldown immediately to prevent re-trigger
        attack_hit_registered = 0;          // Allow hit detection for this attack
    }

    // Update hold counters and apply movement
    // UP
    if (move_up) {
        if (hold_up == 0) {
            // First frame - tap: move fixed distance
            vel_y = -TAP_DISTANCE;
        } else if (hold_up >= HOLD_THRESHOLD) {
            // Holding - accelerate
            vel_y -= ACCELERATION;
            if (vel_y < -MAX_SPEED) vel_y = -MAX_SPEED;
        }
        hold_up++;
    } else {
        if (hold_up > 0 && hold_up < HOLD_THRESHOLD) {
            // Was a tap, velocity already set
        }
        hold_up = 0;
        // Decelerate
        if (vel_y < 0) {
            vel_y += DECELERATION;
            if (vel_y > 0) vel_y = 0;
        }
    }

    // DOWN
    if (move_down) {
        if (hold_down == 0) {
            vel_y = TAP_DISTANCE;
        } else if (hold_down >= HOLD_THRESHOLD) {
            vel_y += ACCELERATION;
            if (vel_y > MAX_SPEED) vel_y = MAX_SPEED;
        }
        hold_down++;
    } else {
        hold_down = 0;
        if (vel_y > 0) {
            vel_y -= DECELERATION;
            if (vel_y < 0) vel_y = 0;
        }
    }

    // LEFT
    if (move_left) {
        if (hold_left == 0) {
            vel_x = -TAP_DISTANCE;
        } else if (hold_left >= HOLD_THRESHOLD) {
            vel_x -= ACCELERATION;
            if (vel_x < -MAX_SPEED) vel_x = -MAX_SPEED;
        }
        hold_left++;
    } else {
        hold_left = 0;
        if (vel_x < 0) {
            vel_x += DECELERATION;
            if (vel_x > 0) vel_x = 0;
        }
    }

    // RIGHT
    if (move_right) {
        if (hold_right == 0) {
            vel_x = TAP_DISTANCE;
        } else if (hold_right >= HOLD_THRESHOLD) {
            vel_x += ACCELERATION;
            if (vel_x > MAX_SPEED) vel_x = MAX_SPEED;
        }
        hold_right++;
    } else {
        hold_right = 0;
        if (vel_x > 0) {
            vel_x -= DECELERATION;
            if (vel_x < 0) vel_x = 0;
        }
    }

    // Normalize diagonal movement to prevent faster speed when pressing two directions
    // When moving diagonally, scale velocity by ~0.7 (use 7/10 for integer math)
    // This keeps diagonal speed equal to cardinal direction speed
    int moving_x = (vel_x != 0);
    int moving_y = (vel_y != 0);
    int final_vel_x = vel_x;
    int final_vel_y = vel_y;

    if (moving_x && moving_y) {
        // Diagonal movement - scale both components by 7/10 (~0.707)
        final_vel_x = (vel_x * 7) / 10;
        final_vel_y = (vel_y * 7) / 10;
    }

    // Apply velocity to position
    int new_x = player_x + final_vel_x;
    int new_y = player_y + final_vel_y;

    // Clamp to game area boundaries (with breach exceptions)
    // When breach is open, allow player to move into breach area
    int right_limit = GAME_AREA_RIGHT;
    int left_limit = GAME_AREA_LEFT;
    int top_limit = GAME_AREA_TOP;
    int bottom_limit = GAME_AREA_BOTTOM;

    if (breach_opened) {
        // Extend limits in breach direction
        switch (exit_direction) {
            case DIR_BREACH_RIGHT:
                // Allow going further right into breach
                if (new_y >= BREACH_PIXEL_START && new_y <= BREACH_PIXEL_END - PLAYER_SIZE) {
                    right_limit = 640 - PLAYER_SIZE;  // Allow to edge of screen
                }
                break;
            case DIR_BREACH_LEFT:
                // Allow going further left into breach
                if (new_y >= BREACH_PIXEL_START && new_y <= BREACH_PIXEL_END - PLAYER_SIZE) {
                    left_limit = 160;  // Allow to left edge of game area
                }
                break;
            case DIR_BREACH_UP:
                // Allow going further up into breach
                if (new_x >= BREACH_PIXEL_START + 160 && new_x <= BREACH_PIXEL_END + 160 - PLAYER_SIZE) {
                    top_limit = 0;  // Allow to top edge
                }
                break;
            case DIR_BREACH_DOWN:
                // Allow going further down into breach
                if (new_x >= BREACH_PIXEL_START + 160 && new_x <= BREACH_PIXEL_END + 160 - PLAYER_SIZE) {
                    bottom_limit = 480 - PLAYER_SIZE;  // Allow to bottom edge
                }
                break;
        }
    }

    if (new_x < left_limit) {
        new_x = left_limit;
        vel_x = 0;
    }
    if (new_x > right_limit) {
        new_x = right_limit;
        vel_x = 0;
    }
    if (new_y < top_limit) {
        new_y = top_limit;
        vel_y = 0;
    }
    if (new_y > bottom_limit) {
        new_y = bottom_limit;
        vel_y = 0;
    }

    // Tile collision with wall sliding
    // Try X movement first
    if (check_tile_collision(new_x, player_y, PLAYER_SIZE)) {
        new_x = player_x;  // Revert X movement
        vel_x = 0;
    }
    // Then try Y movement (allows sliding along walls)
    if (check_tile_collision(new_x, new_y, PLAYER_SIZE)) {
        new_y = player_y;  // Revert Y movement
        vel_y = 0;
    }

    // Update position
    player_x = new_x;
    player_y = new_y;

    // Update direction based on movement (prioritize horizontal for diagonal)
    // Only change direction when actively pressing a key
    if (move_left) {
        player_dir = DIR_LEFT;
    } else if (move_right) {
        player_dir = DIR_RIGHT;
    } else if (move_up) {
        player_dir = DIR_UP;
    } else if (move_down) {
        player_dir = DIR_DOWN;
    }

    // Check if player is moving (has velocity)
    is_moving = (vel_x != 0 || vel_y != 0);

    // Update hardware
    update_player_hardware();
}

// Handle menu input (in MENU state)
void handle_menu_input(BOOT_KBD_REPORT* kbd) {
    int key_space = 0;

    // Check for SPACE key press
    for (int i = 0; i < 6; i++) {
        BYTE key = kbd->keycode[i];
        if (key == KEY_SPACE) key_space = 1;
    }

    // Detect SPACE key press (edge detection)
    if (key_space && !prev_key_space) {
        // Seed random with frame counter - true randomness from user timing
        srand(frame_counter);

        game_state = GAME_STATE_PLAYING;
        reset_game();
        update_game_state_hardware();
    }

    prev_key_space = key_space;
}

// Handle game over input (in GAMEOVER state)
void handle_gameover_input(BOOT_KBD_REPORT* kbd) {
    int key_w = 0, key_s = 0, key_space = 0;

    // Check for W, S, SPACE key presses
    for (int i = 0; i < 6; i++) {
        BYTE key = kbd->keycode[i];
        if (key == KEY_W) key_w = 1;
        if (key == KEY_S) key_s = 1;
        if (key == KEY_SPACE) key_space = 1;
    }

    // Toggle menu selection with W/S (edge detection)
    if (key_w && !prev_key_w) {
        menu_selection = 0;  // Select RESTART
        update_game_state_hardware();
    }
    if (key_s && !prev_key_s) {
        menu_selection = 1;  // Select EXIT
        update_game_state_hardware();
    }

    // Confirm selection with SPACE (edge detection)
    if (key_space && !prev_key_space) {
        if (menu_selection == 0) {
            // RESTART selected - reseed for new random enemies
            srand(frame_counter);

            game_state = GAME_STATE_PLAYING;
            reset_game();
            update_game_state_hardware();
        } else {
            // EXIT selected - return to menu
            game_state = GAME_STATE_MENU;
            menu_selection = 0;
            update_game_state_hardware();
        }
    }

    prev_key_w = key_w;
    prev_key_s = key_s;
    prev_key_space = key_space;
}

// Handle win input (in WIN state) - same options as game over
void handle_win_input(BOOT_KBD_REPORT* kbd) {
    int key_w = 0, key_s = 0, key_space = 0;

    // Check for W, S, SPACE key presses
    for (int i = 0; i < 6; i++) {
        BYTE key = kbd->keycode[i];
        if (key == KEY_W) key_w = 1;
        if (key == KEY_S) key_s = 1;
        if (key == KEY_SPACE) key_space = 1;
    }

    // Toggle menu selection with W/S (edge detection)
    if (key_w && !prev_key_w) {
        menu_selection = 0;  // Select PLAY AGAIN
        update_game_state_hardware();
    }
    if (key_s && !prev_key_s) {
        menu_selection = 1;  // Select EXIT
        update_game_state_hardware();
    }

    // Confirm selection with SPACE (edge detection)
    if (key_space && !prev_key_space) {
        if (menu_selection == 0) {
            // PLAY AGAIN selected - reseed for new random enemies
            srand(frame_counter);

            game_state = GAME_STATE_PLAYING;
            reset_game();
            update_game_state_hardware();
        } else {
            // EXIT selected - return to menu
            game_state = GAME_STATE_MENU;
            menu_selection = 0;
            update_game_state_hardware();
        }
    }

    prev_key_w = key_w;
    prev_key_s = key_s;
    prev_key_space = key_space;
}

int main() {
    init_platform();

    // Random seed will be set when player presses J (using frame_counter)
    // This gives true randomness based on human timing

    // Initialize level/room tracking
    current_level = 1;
    current_room = 0;
    entry_direction = DIR_BREACH_RIGHT;
    exit_direction = DIR_BREACH_RIGHT;

    // Initialize room system - start with empty room (map 0)
    load_collision_map(TEMPLATE_EMPTY);
    Xil_Out32(MAP_SELECT_REG, TEMPLATE_EMPTY);
    Xil_Out32(BREACH_OPEN_REG, 0);  // Breach closed initially
    Xil_Out32(BREACH_DIR_REG, DIR_BREACH_RIGHT);  // Initial breach direction

    // Initialize hardware displays
    update_player_hardware();
    update_projectiles_hardware();
    update_player_health_hardware();

    // Initialize game state to MENU
    game_state = GAME_STATE_MENU;
    menu_selection = 0;
    update_game_state_hardware();

    // Clear enemies for menu (no enemies shown on menu screen)
    for (int i = 0; i < MAX_ENEMIES; i++) {
        enemies[i].active = 0;
    }
    update_enemies_hardware();

    BYTE rcode;
    BOOT_KBD_REPORT kbdbuf = {0};
    BOOT_KBD_REPORT last_kbdbuf = {0};  // Remember last key state
    BYTE running = 0;

    MAX3421E_init();
    USB_init();

    while (1) {
        MAX3421E_Task();
        USB_Task();

        if (GetUsbTaskState() == USB_STATE_RUNNING) {
            if (!running) {
                running = 1;

            }

            // Poll keyboard
            rcode = kbdPoll(&kbdbuf);
            if (rcode == 0) {
                // Got new data, save it
                last_kbdbuf = kbdbuf;
            }

            // Handle input based on game state
            switch (game_state) {
                case GAME_STATE_MENU:
                    handle_menu_input(&last_kbdbuf);
                    break;

                case GAME_STATE_PLAYING: {
                    // Normal game input processing
                    process_keyboard(&last_kbdbuf);

                    // Update battle system (enemy AI, projectiles, collisions)
                    update_battle_system();

                    // Check for death - transition to game over
                    if (player_health <= 0) {

                        game_state = GAME_STATE_GAMEOVER;
                        menu_selection = 0;  // Default to RESTART
                        update_game_state_hardware();
                    }

                    // Room system: check if room cleared and handle transitions
                    check_room_cleared();      // Opens breach or triggers WIN
                    check_room_transition();   // Handles walking through breach

                    // Stair interaction: press J on stairs to go to next level
                    if (current_template == TEMPLATE_STAIR && player_on_stairs()) {
                        // Check for J key press (edge detection)
                        int j_pressed = 0;
                        for (int i = 0; i < 6; i++) {
                            if (last_kbdbuf.keycode[i] == KEY_J) {
                                j_pressed = 1;
                                break;
                            }
                        }
                        static int stair_prev_j = 0;
                        if (j_pressed && !stair_prev_j && !is_attacking) {

                            advance_to_next_level();
                        }
                        stair_prev_j = j_pressed;
                    }
                    break;
                }

                case GAME_STATE_GAMEOVER:
                    handle_gameover_input(&last_kbdbuf);
                    break;

                case GAME_STATE_WIN:
                    handle_win_input(&last_kbdbuf);
                    break;
            }

        } else {
            if (running) {
                running = 0;

            }
            // Clear input when disconnected
            BOOT_KBD_REPORT empty = {0};
            last_kbdbuf = empty;

            // Still process deceleration if in playing state
            if (game_state == GAME_STATE_PLAYING) {
                process_keyboard(&empty);
            }
        }

        // Increment frame counter (for random seed entropy)
        frame_counter++;

        // Small delay for consistent frame rate (~60fps feel)
        for (volatile int i = 0; i < 5000; i++);
    }
    return 0;
}
