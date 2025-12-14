//room_system.c - Room and Level System

#include "room_system.h"
#include "collision.h"
#include "hardware_regs.h"
#include "xil_io.h"
#include <stdlib.h>

extern void init_enemies(void);
extern void init_boss(void);
extern void update_enemies_hardware(void);
extern void update_boss_hardware(void);
extern void update_projectiles_hardware(void);
extern void update_player_hardware(void);
extern int all_enemies_dead(void);

extern int player_x, player_y;

extern Enemy enemies[MAX_ENEMIES];
extern Projectile projectiles[MAX_PROJECTILES];

//LEVEL TEMPLATES
int level1_templates[LEVEL1_ROOMS] = {
    TEMPLATE_EMPTY,     //Room 0: Spawn
    TEMPLATE_I_SHAPE,   //Room 1: Battle
    TEMPLATE_CROSS,     //Room 2: Battle
    TEMPLATE_PILLARS,   //Room 3: Battle
    TEMPLATE_STAIR      //Room 4: Stairs to Level 2
};

int level2_templates[LEVEL2_ROOMS] = {
    TEMPLATE_EMPTY,     //Room 0: Entry
    TEMPLATE_I_SHAPE,   //Room 1: Battle
    TEMPLATE_CROSS,     //Room 2: Battle
    TEMPLATE_PILLARS,   //Room 3: Battle
    TEMPLATE_BOSS       //Room 4: Boss (uses pillars layout)
};

//ROOM STATE
int current_level = 1;
int current_room = 0;
int current_template = TEMPLATE_EMPTY;
int entry_direction = DIR_BREACH_RIGHT;
int exit_direction = DIR_BREACH_RIGHT;
int room_cleared = 0;
int breach_opened = 0;


//Pick a random exit direction (different from entry)
int pick_exit_direction(int entry_dir) {
    int valid_dirs[3];
    int count = 0;

    for (int dir = 0; dir < 4; dir++) {
        if (dir != entry_dir) {
            valid_dirs[count++] = dir;
        }
    }

    int choice = rand() % count;
    return valid_dirs[choice];
}

//Get spawn position based on entry direction
void get_spawn_position(int entry_dir, int *spawn_x, int *spawn_y) {
    //Breach center Y for left/right entry
    int breach_center_y = (BREACH_CENTER_START + BREACH_CENTER_END + 1) * 16 / 2 - PLAYER_SIZE / 2;
    //Breach center X for top/bottom entry
    int breach_center_x = 160 + (BREACH_CENTER_START + BREACH_CENTER_END + 1) * 16 / 2 - PLAYER_SIZE / 2;

    int wall_offset = 32;

    switch (entry_dir) {
        case DIR_BREACH_RIGHT:
            *spawn_x = GAME_AREA_RIGHT - wall_offset;
            *spawn_y = breach_center_y;
            break;
        case DIR_BREACH_LEFT:
            *spawn_x = GAME_AREA_LEFT + wall_offset;
            *spawn_y = breach_center_y;
            break;
        case DIR_BREACH_UP:
            *spawn_x = breach_center_x;
            *spawn_y = GAME_AREA_TOP + wall_offset;
            break;
        case DIR_BREACH_DOWN:
            *spawn_x = breach_center_x;
            *spawn_y = GAME_AREA_BOTTOM - wall_offset;
            break;
        default:
            *spawn_x = GAME_AREA_LEFT + wall_offset;
            *spawn_y = breach_center_y;
            break;
    }
}

//Get template
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


int is_battle_room(int template_id) {
    return (template_id == TEMPLATE_I_SHAPE ||
            template_id == TEMPLATE_CROSS ||
            template_id == TEMPLATE_PILLARS);
}


int is_boss_room(void) {
    return (current_level == 2 && current_room == LEVEL2_ROOMS - 1);
}


void load_room(int template_id) {
    current_template = template_id;

    //Update hardware registers
    Xil_Out32(MAP_SELECT_REG, template_id);
    Xil_Out32(BREACH_OPEN_REG, 0);
    Xil_Out32(BREACH_DIR_REG, exit_direction);

    //Build collision map
    load_collision_map(template_id);

    //Reset room state
    room_cleared = 0;
    breach_opened = 0;

    //Deactivate all enemies first
    for (int i = 0; i < MAX_ENEMIES; i++) {
        enemies[i].active = 0;
    }

    //Clear projectiles
    for (int i = 0; i < MAX_PROJECTILES; i++) {
        projectiles[i].active = 0;
    }

    //Spawn enemies or boss
    if (is_boss_room()) {
        init_boss();
        update_boss_hardware();
    } else if (is_battle_room(template_id)) {
        init_enemies();
    }

    update_enemies_hardware();
    update_projectiles_hardware();
}


void open_breach(void) {
    if (breach_opened) return;

    breach_opened = 1;

    //Tell hardware to display breach
    Xil_Out32(BREACH_OPEN_REG, 1);
    Xil_Out32(BREACH_DIR_REG, exit_direction);

    //Clear collision for breach wall
    clear_breach_collision(exit_direction);
}


void check_room_cleared(void) {
    if (room_cleared) return;

    if (current_template == TEMPLATE_EMPTY) {
        room_cleared = 1;
        exit_direction = rand() % 4;
        Xil_Out32(BREACH_DIR_REG, exit_direction);
        open_breach();
        return;
    }

    if (current_template == TEMPLATE_STAIR) {
        room_cleared = 1;
        return;
    }

    if (is_boss_room()) {
        return;
    }

    if (is_battle_room(current_template) && all_enemies_dead()) {
        room_cleared = 1;
        exit_direction = pick_exit_direction(entry_direction);
        open_breach();
    }
}


int player_in_breach(int breach_dir) {
    switch (breach_dir) {
        case DIR_BREACH_RIGHT:
            return (player_x >= 640 - PLAYER_SIZE - 8 &&
                    player_y >= BREACH_PIXEL_START &&
                    player_y <= BREACH_PIXEL_END - PLAYER_SIZE);
        case DIR_BREACH_LEFT:
            return (player_x <= 160 + 8 &&
                    player_y >= BREACH_PIXEL_START &&
                    player_y <= BREACH_PIXEL_END - PLAYER_SIZE);
        case DIR_BREACH_UP:
            return (player_y <= 8 &&
                    player_x >= BREACH_PIXEL_START + 160 &&
                    player_x <= BREACH_PIXEL_END + 160 - PLAYER_SIZE);
        case DIR_BREACH_DOWN:
            return (player_y >= 480 - PLAYER_SIZE - 8 &&
                    player_x >= BREACH_PIXEL_START + 160 &&
                    player_x <= BREACH_PIXEL_END + 160 - PLAYER_SIZE);
    }
    return 0;
}


void advance_to_next_room(void) {
    current_room++;
    int max_rooms = (current_level == 1) ? LEVEL1_ROOMS : LEVEL2_ROOMS;

    if (current_room >= max_rooms) {
        return;
    }

    //Entry direction is opposite of exit direction
    switch (exit_direction) {
        case DIR_BREACH_RIGHT: entry_direction = DIR_BREACH_LEFT; break;
        case DIR_BREACH_LEFT:  entry_direction = DIR_BREACH_RIGHT; break;
        case DIR_BREACH_UP:    entry_direction = DIR_BREACH_DOWN; break;
        case DIR_BREACH_DOWN:  entry_direction = DIR_BREACH_UP; break;
    }

    int new_template = get_room_template(current_level, current_room);
    load_room(new_template);

    //Spawn player based on entry direction
    int spawn_x, spawn_y;
    get_spawn_position(entry_direction, &spawn_x, &spawn_y);
    player_x = spawn_x;
    player_y = spawn_y;
    update_player_hardware();
}


void check_room_transition(void) {
    if (!breach_opened) return;

    if (player_in_breach(exit_direction)) {
        advance_to_next_room();
    }
}


int player_on_stairs(void) {
    int player_cx = player_x + PLAYER_SIZE / 2;
    int player_cy = player_y + PLAYER_SIZE / 2;

    return (player_cx >= STAIR_PIXEL_X_START && player_cx < STAIR_PIXEL_X_END &&
            player_cy >= STAIR_PIXEL_Y_START && player_cy < STAIR_PIXEL_Y_END);
}


void advance_to_next_level(void) {
    if (current_level >= MAX_LEVELS) {
        return;
    }

    current_level++;
    current_room = 0;
    entry_direction = DIR_BREACH_LEFT;
    exit_direction = DIR_BREACH_RIGHT;

    Xil_Out32(LEVEL_REG, current_level - 1);

    int new_template = get_room_template(current_level, current_room);
    load_room(new_template);

    player_x = GAME_AREA_LEFT + 32;
    player_y = 240;
    update_player_hardware();
}


