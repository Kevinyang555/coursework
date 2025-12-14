//player.c - Player State and Input Handling

#include "player.h"
#include "room_system.h"
#include "collision.h"
#include "hardware_regs.h"
#include "xil_io.h"
#include <stdlib.h>


extern void reset_game(void);
extern void update_game_state_hardware(void);


extern int breach_opened;
extern int exit_direction;
extern int current_level;


extern int game_state;
extern int menu_selection;
extern unsigned int frame_counter;


extern Projectile projectiles[MAX_PROJECTILES];


extern int prev_key_w, prev_key_s, prev_key_space;

//PLAYER STATE VARIABLES
int player_x = 320;
int player_y = 240;

int vel_x = 0;
int vel_y = 0;

int hold_up = 0;
int hold_down = 0;
int hold_left = 0;
int hold_right = 0;

Direction player_dir = DIR_DOWN;
int is_moving = 0;
int anim_frame = 0;
int anim_counter = 0;

int is_attacking = 0;
int attack_anim_frame = 0;
int attack_anim_counter = 0;
int attack_cooldown = 0;
int attack_hit_registered = 0;

int ranged_cooldown = 0;
int player_proj_frame = 0;
int player_proj_anim_counter = 0;

int player_health = PLAYER_MAX_HP;
int player_invincible = 0;

int player_armor = PLAYER_MAX_ARMOR;
int armor_regen_cooldown = 0;
int armor_regen_timer = 0;



int get_attack_dir(Direction dir) {
    switch (dir) {
        case DIR_DOWN:  return ATTACK_DIR_DOWN;
        case DIR_LEFT:  return ATTACK_DIR_LEFT;
        case DIR_RIGHT: return ATTACK_DIR_RIGHT;
        case DIR_UP:    return ATTACK_DIR_UP;
        default:        return ATTACK_DIR_DOWN;
    }
}

void get_attack_hitbox(int *x, int *y, int *w, int *h) {
    //Attack hitbox is 32x32 extended in facing direction
    *w = 32;
    *h = 32;

    switch (player_dir) {
        case DIR_DOWN:
            *x = player_x;
            *y = player_y + 16;
            break;
        case DIR_UP:
            *x = player_x;
            *y = player_y - 16;
            break;
        case DIR_LEFT:
            *x = player_x - 16;
            *y = player_y;
            break;
        case DIR_RIGHT:
            *x = player_x + 16;
            *y = player_y;
            break;
        default:
            *x = player_x;
            *y = player_y + 16;
            break;
    }
}

//HARDWARE UPDATE
void update_player_health_hardware(void) {
    Xil_Out32(PLAYER_HEALTH_REG, player_health);
}

void update_player_armor_hardware(void) {
    Xil_Out32(PLAYER_ARMOR_REG, player_armor);
}

void update_animation(void) {
    int frame;

    //Decrement attack cooldown
    if (attack_cooldown > 0) {
        attack_cooldown--;
    }

    //Handle attack animation
    if (is_attacking) {
        attack_anim_counter++;
        if (attack_anim_counter >= ATTACK_ANIM_SPEED) {
            attack_anim_counter = 0;
            attack_anim_frame++;
            if (attack_anim_frame >= ATTACK_FRAMES) {
                is_attacking = 0;
                attack_anim_frame = 0;
            }
        }
    }

    if (is_attacking) {
        //Calculate attack frame: ATTACK_BASE + attack_dir * 5 + attack_anim_frame
        int attack_dir = get_attack_dir(player_dir);
        frame = ATTACK_BASE + (attack_dir * ATTACK_FRAMES) + attack_anim_frame;
    } else {
        anim_counter++;
        if (anim_counter >= ANIM_SPEED) {
            anim_counter = 0;
            anim_frame = (anim_frame + 1) % FRAMES_PER_ANIM;
        }

        //Calculate sprite frame number
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

//PLAYER PROJECTILE
void spawn_player_projectile(void) {
    //Find inactive projectile slot
    for (int i = 0; i < MAX_PROJECTILES; i++) {
        if (!projectiles[i].active) {
            //Start at player center
            int player_cx = player_x + PLAYER_SIZE / 2;
            int player_cy = player_y + PLAYER_SIZE / 2;

            projectiles[i].x = player_cx - PLAYER_PROJ_SIZE / 2;
            projectiles[i].y = player_cy - PLAYER_PROJ_SIZE / 2;

            //Velocity based on facing direction
            switch (player_dir) {
                case DIR_UP:
                    projectiles[i].vx = 0;
                    projectiles[i].vy = -PLAYER_PROJ_SPEED;
                    break;
                case DIR_DOWN:
                    projectiles[i].vx = 0;
                    projectiles[i].vy = PLAYER_PROJ_SPEED;
                    break;
                case DIR_LEFT:
                    projectiles[i].vx = -PLAYER_PROJ_SPEED;
                    projectiles[i].vy = 0;
                    break;
                case DIR_RIGHT:
                    projectiles[i].vx = PLAYER_PROJ_SPEED;
                    projectiles[i].vy = 0;
                    break;
            }

            projectiles[i].active = 1;
            projectiles[i].is_player_proj = 1;
            projectiles[i].is_homing = 0;
            projectiles[i].homing_timer = 0;
            projectiles[i].is_boss_proj = 0;
            projectiles[i].flip = 0;
            projectiles[i].source_type = 0;
            return;
        }
    }
}

//INPUT HANDLING
void process_keyboard(BOOT_KBD_REPORT* kbd) {
    int move_up = 0, move_down = 0, move_left = 0, move_right = 0;
    int attack_pressed = 0;
    int ranged_pressed = 0;

    //Check keycodes
    for (int i = 0; i < 6; i++) {
        BYTE key = kbd->keycode[i];
        if (key == 0) continue;

        switch (key) {
            case KEY_W: move_up = 1; break;
            case KEY_S: move_down = 1; break;
            case KEY_A: move_left = 1; break;
            case KEY_D: move_right = 1; break;
            case KEY_J: attack_pressed = 1; break;
            case KEY_K: ranged_pressed = 1; break;
        }
    }

    //Handle ranged attack
    if (ranged_pressed && ranged_cooldown == 0 && current_level == 2) {
        spawn_player_projectile();
        ranged_cooldown = RANGED_COOLDOWN;
    }

    //Handle attack
    if (attack_pressed && !is_attacking && attack_cooldown == 0) {
        is_attacking = 1;
        attack_anim_frame = 0;
        attack_anim_counter = 0;
        attack_cooldown = ATTACK_COOLDOWN;
        attack_hit_registered = 0;
    }

    //UP
    if (move_up) {
        if (hold_up == 0) {
            vel_y = -TAP_DISTANCE;
        } else if (hold_up >= HOLD_THRESHOLD) {
            vel_y -= ACCELERATION;
            if (vel_y < -MAX_SPEED) vel_y = -MAX_SPEED;
        }
        hold_up++;
    } else {
        hold_up = 0;
        if (vel_y < 0) {
            vel_y += DECELERATION;
            if (vel_y > 0) vel_y = 0;
        }
    }

    //DOWN
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

    //LEFT
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

    //RIGHT
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

    int moving_x = (vel_x != 0);
    int moving_y = (vel_y != 0);
    int final_vel_x = vel_x;
    int final_vel_y = vel_y;

    if (moving_x && moving_y) {
        final_vel_x = (vel_x * 7) / 10;
        final_vel_y = (vel_y * 7) / 10;
    }

    int new_x = player_x + final_vel_x;
    int new_y = player_y + final_vel_y;

    //Clamp to game area boundaries
    int right_limit = GAME_AREA_RIGHT;
    int left_limit = GAME_AREA_LEFT;
    int top_limit = GAME_AREA_TOP;
    int bottom_limit = GAME_AREA_BOTTOM;

    if (breach_opened) {
        switch (exit_direction) {
            case DIR_BREACH_RIGHT:
                if (new_y >= BREACH_PIXEL_START && new_y <= BREACH_PIXEL_END - PLAYER_SIZE) {
                    right_limit = 640 - PLAYER_SIZE;
                }
                break;
            case DIR_BREACH_LEFT:
                if (new_y >= BREACH_PIXEL_START && new_y <= BREACH_PIXEL_END - PLAYER_SIZE) {
                    left_limit = 160;
                }
                break;
            case DIR_BREACH_UP:
                if (new_x >= BREACH_PIXEL_START + 160 && new_x <= BREACH_PIXEL_END + 160 - PLAYER_SIZE) {
                    top_limit = 0;
                }
                break;
            case DIR_BREACH_DOWN:
                if (new_x >= BREACH_PIXEL_START + 160 && new_x <= BREACH_PIXEL_END + 160 - PLAYER_SIZE) {
                    bottom_limit = 480 - PLAYER_SIZE;
                }
                break;
        }
    }

    if (new_x < left_limit) { new_x = left_limit; vel_x = 0; }
    if (new_x > right_limit) { new_x = right_limit; vel_x = 0; }
    if (new_y < top_limit) { new_y = top_limit; vel_y = 0; }
    if (new_y > bottom_limit) { new_y = bottom_limit; vel_y = 0; }

    //Tile collision
    if (check_tile_collision(new_x, player_y, PLAYER_SIZE)) {
        new_x = player_x;
        vel_x = 0;
    }
    if (check_tile_collision(new_x, new_y, PLAYER_SIZE)) {
        new_y = player_y;
        vel_y = 0;
    }

    player_x = new_x;
    player_y = new_y;

    //Update direction
    if (move_left) {
        player_dir = DIR_LEFT;
    } else if (move_right) {
        player_dir = DIR_RIGHT;
    } else if (move_up) {
        player_dir = DIR_UP;
    } else if (move_down) {
        player_dir = DIR_DOWN;
    }

    is_moving = (vel_x != 0 || vel_y != 0);

    //Update hardware
    update_player_hardware();
}

void handle_menu_input(BOOT_KBD_REPORT* kbd) {
    int key_space = 0;

    for (int i = 0; i < 6; i++) {
        BYTE key = kbd->keycode[i];
        if (key == KEY_SPACE) key_space = 1;
    }

    //Detect SPACE
    if (key_space && !prev_key_space) {
        srand(frame_counter);
        game_state = GAME_STATE_PLAYING;
        reset_game();
        update_game_state_hardware();
    }

    prev_key_space = key_space;
}

void handle_gameover_input(BOOT_KBD_REPORT* kbd) {
    int key_w = 0, key_s = 0, key_space = 0;

    for (int i = 0; i < 6; i++) {
        BYTE key = kbd->keycode[i];
        if (key == KEY_W) key_w = 1;
        if (key == KEY_S) key_s = 1;
        if (key == KEY_SPACE) key_space = 1;
    }

    //Toggle menu selection
    if (key_w && !prev_key_w) {
        menu_selection = 0;
        update_game_state_hardware();
    }
    if (key_s && !prev_key_s) {
        menu_selection = 1;
        update_game_state_hardware();
    }

    //Confirm selection
    if (key_space && !prev_key_space) {
        if (menu_selection == 0) {
            srand(frame_counter);
            game_state = GAME_STATE_PLAYING;
            reset_game();
            update_game_state_hardware();
        } else {
            game_state = GAME_STATE_MENU;
            menu_selection = 0;
            update_game_state_hardware();
        }
    }

    prev_key_w = key_w;
    prev_key_s = key_s;
    prev_key_space = key_space;
}

void handle_win_input(BOOT_KBD_REPORT* kbd) {
    int key_w = 0, key_s = 0, key_space = 0;

    for (int i = 0; i < 6; i++) {
        BYTE key = kbd->keycode[i];
        if (key == KEY_W) key_w = 1;
        if (key == KEY_S) key_s = 1;
        if (key == KEY_SPACE) key_space = 1;
    }

    if (key_w && !prev_key_w) {
        menu_selection = 0;
        update_game_state_hardware();
    }
    if (key_s && !prev_key_s) {
        menu_selection = 1;
        update_game_state_hardware();
    }

    if (key_space && !prev_key_space) {
        if (menu_selection == 0) {
            srand(frame_counter);
            game_state = GAME_STATE_PLAYING;
            reset_game();
            update_game_state_hardware();
        } else {
            game_state = GAME_STATE_MENU;
            menu_selection = 0;
            update_game_state_hardware();
        }
    }

    prev_key_w = key_w;
    prev_key_s = key_s;
    prev_key_space = key_space;
}
