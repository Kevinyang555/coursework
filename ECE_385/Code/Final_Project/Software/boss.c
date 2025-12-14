//boss.c - Boss AI and Management

#include "boss.h"
#include "enemy.h"
#include "collision.h"
#include "hardware_regs.h"
#include "xil_io.h"
#include <stdlib.h>

//Forward declarations for functions from other modules
extern void init_single_enemy(int idx, int x, int y, int sprite_type, int behavior_type);
extern void get_attack_hitbox(int *x, int *y, int *w, int *h);
extern void update_game_state_hardware(void);

//External player variables
extern int player_x, player_y;
extern int is_attacking;
extern int attack_hit_registered;

//External game state
extern int game_state;
extern int menu_selection;

//External arrays
extern Enemy enemies[MAX_ENEMIES];
extern Projectile projectiles[MAX_PROJECTILES];

//Boss instance
Boss boss;

//abs_val is provided by collision.c

//BOSS INITIALIZATION

void init_boss(void) {
    //Boss spawning position
    boss.x = 450;  //Right side of room
    boss.y = 200;  //Centered vertically
    boss.health = BOSS_MAX_HP;
    boss.active = 1;
    boss.frame = 0;
    boss.flip = 0;  //Face left toward player
    boss.hit_timer = 0;

    //Separate cooldowns
    boss.attack_cooldown = BOSS_ATTACK_SPEED;  //Basic ranged attack
    boss.summon_cooldown = BOSS_CD_SUMMON;     //Summon ability starts on cooldown
    boss.burst_cooldown = BOSS_CD_BURST;
    boss.homing_cooldown = BOSS_CD_HOMING;

    boss.last_attack = -1;  //No previous attack
    boss.phase = 1;         //Phase 1 (above 50% HP)
    boss.anim_state = 0;    //Idle
    boss.anim_frame = 0;
    boss.anim_counter = 0;
    boss.is_dying = 0;
    boss.death_timer = 0;

    //Movement
    boss.wander_timer = BOSS_WANDER_CHANGE;
    boss.wander_dir_x = 0;
    boss.wander_dir_y = 0;
}

//HARDWARE UPDATE

void update_boss_hardware(void) {
    if (!boss.active) {
        Xil_Out32(BOSS_CONTROL_REG, 0);  //Inactive
        return;
    }

    Xil_Out32(BOSS_X_REG, boss.x);
    Xil_Out32(BOSS_Y_REG, boss.y);
    Xil_Out32(BOSS_FRAME_REG, boss.frame);

    //Control register: {hit[2], flip[1], active[0]}
    u32 control = boss.active;
    if (boss.flip) control |= (1 << 1);
    if (boss.hit_timer > 0) control |= (1 << 2);
    Xil_Out32(BOSS_CONTROL_REG, control);

    Xil_Out32(BOSS_HEALTH_REG, boss.health);
}

//BOSS ATTACKS

void boss_spawn_projectile(void) {
    //Find inactive projectile slot
    for (int i = 0; i < MAX_PROJECTILES; i++) {
        if (!projectiles[i].active) {
            //Calculate boss center and player center
            int boss_cx = boss.x + (BOSS_SIZE / 2);
            int boss_cy = boss.y + (BOSS_SIZE / 2);
            int player_cx = player_x + (PLAYER_SIZE / 2);
            int player_cy = player_y + (PLAYER_SIZE / 2);

            //Calculate direction vector to player
            int dx = player_cx - boss_cx;
            int dy = player_cy - boss_cy;

            //Normalize to get velocity components
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

            //Start projectile at boss center
            projectiles[i].x = boss_cx - (PROJECTILE_SIZE / 2);
            projectiles[i].y = boss_cy - (PROJECTILE_SIZE / 2);
            projectiles[i].active = 1;
            projectiles[i].is_player_proj = 0;
            projectiles[i].is_homing = 0;
            projectiles[i].is_boss_proj = 0;
            projectiles[i].flip = 0;
            projectiles[i].source_type = SOURCE_TYPE_BOSS;
            return;
        }
    }
}

void boss_summon_bats(void) {
    //Spawn offsets: TL, TR, BL (only need 3)
    int ox[] = {-80, 80, -80};
    int oy[] = {-40, -40, 40};

    int spawned = 0;
    for (int i = 0; i < MAX_ENEMIES && spawned < 3; i++) {
        if (!enemies[i].active) {
            int spawn_x = boss.x + BOSS_SIZE/2 + ox[spawned] - ENEMY_SIZE/2;
            int spawn_y = boss.y + BOSS_SIZE/2 + oy[spawned] - ENEMY_SIZE/2;

            //Clamp to game area
            if (spawn_x < GAME_AREA_LEFT) spawn_x = GAME_AREA_LEFT;
            if (spawn_x > GAME_AREA_RIGHT - ENEMY_SIZE) spawn_x = GAME_AREA_RIGHT - ENEMY_SIZE;
            if (spawn_y < GAME_AREA_TOP) spawn_y = GAME_AREA_TOP;
            if (spawn_y > GAME_AREA_BOTTOM - ENEMY_SIZE) spawn_y = GAME_AREA_BOTTOM - ENEMY_SIZE;

            //Check tile collision - skip if blocked
            if (check_tile_collision(spawn_x, spawn_y, ENEMY_SIZE)) {
                continue;
            }

            //Spawn a melee bat using BOSS_BAT_SPRITE
            init_single_enemy(i, spawn_x, spawn_y, BOSS_BAT_SPRITE, ENEMY_TYPE_MELEE);
            spawned++;
        }
    }
}

void boss_burst_attack(void) {
    int boss_cx = boss.x + BOSS_SIZE / 2;
    int boss_cy = boss.y + BOSS_SIZE / 2;

    // 16 directions at 22.5 degree intervals (vx, vy pre-calculated)
    int8_t dir_vx[] = {4, 4, 3, 2, 0, -2, -3, -4, -4, -4, -3, -2, 0, 2, 3, 4};
    int8_t dir_vy[] = {0, 2, 3, 4, 4, 4, 3, 2, 0, -2, -3, -4, -4, -4, -3, -2};

    for (int d = 0; d < 16; d++) {
        for (int i = 0; i < MAX_PROJECTILES; i++) {
            if (!projectiles[i].active) {
                //Center projectile on boss center
                projectiles[i].x = boss_cx - (PROJECTILE_SIZE / 2);
                projectiles[i].y = boss_cy - (PROJECTILE_SIZE / 2);
                projectiles[i].vx = dir_vx[d];
                projectiles[i].vy = dir_vy[d];
                projectiles[i].active = 1;
                projectiles[i].is_player_proj = 0;
                projectiles[i].is_homing = 0;
                projectiles[i].homing_timer = 0;
                projectiles[i].is_boss_proj = 0;
                projectiles[i].flip = 0;
                projectiles[i].source_type = SOURCE_TYPE_BOSS;
                break;
            }
        }
    }
}

void boss_homing_attack(void) {
    int boss_cx = boss.x + BOSS_SIZE / 2;
    int boss_cy = boss.y + BOSS_SIZE / 2;
    int player_cx = player_x + PLAYER_SIZE / 2;
    int player_cy = player_y + PLAYER_SIZE / 2;

    //Calculate initial direction toward player
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

    //Spawn 3 homing projectiles with slight spread
    int h_offsets[] = {0, -20, 20};

    for (int h = 0; h < 3; h++) {
        for (int i = 0; i < MAX_PROJECTILES; i++) {
            if (!projectiles[i].active) {
                projectiles[i].x = boss_cx - (PROJECTILE_SIZE / 2) + h_offsets[h];
                projectiles[i].y = boss_cy - (PROJECTILE_SIZE / 2);
                projectiles[i].vx = base_vx;
                projectiles[i].vy = base_vy;
                projectiles[i].active = 1;
                projectiles[i].is_player_proj = 0;
                projectiles[i].is_homing = 1;
                projectiles[i].homing_timer = HOMING_DURATION;
                projectiles[i].is_boss_proj = 0;
                projectiles[i].flip = 0;
                projectiles[i].source_type = SOURCE_TYPE_BOSS;
                break;
            }
        }
    }
}

int boss_check_ability_ready(void) {
    //Check phase transition
    if (boss.health <= BOSS_PHASE2_THRESHOLD && boss.phase == 1) {
        boss.phase = 2;
        boss.summon_cooldown = 0;  //Summon ready immediately when phase 2 starts
    }

    //Summon: only in phase 2, cooldown ready, and enemy slots available
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

    //Burst
    if (boss.burst_cooldown == 0) {
        return BOSS_ATTACK_BURST;
    }

    //Tracking
    if (boss.homing_cooldown == 0) {
        return BOSS_ATTACK_HOMING;
    }

    return -1;
}

void boss_execute_attack(int attack_type) {
    //Start attack animation
    boss.anim_state = 2;  //Attack state
    boss.anim_frame = 0;
    boss.anim_counter = 0;

    switch (attack_type) {
        case BOSS_ATTACK_SINGLE:
            boss_spawn_projectile();
            boss.attack_cooldown = BOSS_ATTACK_SPEED;
            break;
        case BOSS_ATTACK_SUMMON:
            boss_summon_bats();
            boss.summon_cooldown = BOSS_CD_SUMMON;
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



void boss_update_movement(void) {
    //Calculate distance to player
    int boss_cx = boss.x + BOSS_SIZE / 2;
    int boss_cy = boss.y + BOSS_SIZE / 2;
    int player_cx = player_x + PLAYER_SIZE / 2;
    int player_cy = player_y + PLAYER_SIZE / 2;

    int dx = player_cx - boss_cx;
    int dy = player_cy - boss_cy;
    int dist = abs_val(dx) + abs_val(dy);

    int move_x = 0, move_y = 0;
    int speed = BOSS_MOVE_SPEED;

    if (dist > BOSS_TOO_FAR) {
        //Too far
        speed = BOSS_CHASE_SPEED;
        if (abs_val(dx) > abs_val(dy)) {
            move_x = (dx > 0) ? 1 : -1;
        } else {
            move_y = (dy > 0) ? 1 : -1;
        }
    } else if (dist < BOSS_TOO_CLOSE) {
        //Too close - retreat
        if (abs_val(dx) > abs_val(dy)) {
            move_x = (dx > 0) ? -1 : 1;
        } else {
            move_y = (dy > 0) ? -1 : 1;
        }
    } else {
        //Good distance we roam
        boss.wander_timer--;
        if (boss.wander_timer <= 0) {
            //Pick perpendicular movement
            if (abs_val(dx) > abs_val(dy)) {
                //Player is more horizontal - move vertically
                boss.wander_dir_x = 0;
                boss.wander_dir_y = (rand() % 2) ? 1 : -1;
            } else {
                //Player is more vertical - move horizontally
                boss.wander_dir_x = (rand() % 2) ? 1 : -1;
                boss.wander_dir_y = 0;
            }
            boss.wander_timer = BOSS_WANDER_CHANGE;
        }
        move_x = boss.wander_dir_x;
        move_y = boss.wander_dir_y;
    }

    //Calculate new position
    int new_x = boss.x + move_x * speed;
    int new_y = boss.y + move_y * speed;

    //Clamp to game area
    if (new_x < GAME_AREA_LEFT) new_x = GAME_AREA_LEFT;
    if (new_x > GAME_AREA_RIGHT - BOSS_SIZE) new_x = GAME_AREA_RIGHT - BOSS_SIZE;
    if (new_y < GAME_AREA_TOP) new_y = GAME_AREA_TOP;
    if (new_y > GAME_AREA_BOTTOM - BOSS_SIZE) new_y = GAME_AREA_BOTTOM - BOSS_SIZE;

    //Check tile collision
    if (!check_tile_collision(new_x, new_y, BOSS_SIZE)) {
        boss.x = new_x;
        boss.y = new_y;
    } else {
        //Blocked - try just X or just Y movement
        int try_x = boss.x + move_x * speed;
        int try_y = boss.y + move_y * speed;

        //Clamp
        if (try_x < GAME_AREA_LEFT) try_x = GAME_AREA_LEFT;
        if (try_x > GAME_AREA_RIGHT - BOSS_SIZE) try_x = GAME_AREA_RIGHT - BOSS_SIZE;
        if (try_y < GAME_AREA_TOP) try_y = GAME_AREA_TOP;
        if (try_y > GAME_AREA_BOTTOM - BOSS_SIZE) try_y = GAME_AREA_BOTTOM - BOSS_SIZE;

        //Try X only
        if (!check_tile_collision(try_x, boss.y, BOSS_SIZE)) {
            boss.x = try_x;
        }
        //Try Y only
        else if (!check_tile_collision(boss.x, try_y, BOSS_SIZE)) {
            boss.y = try_y;
        }
        //Both blocked - pick new direction
        else {
            boss.wander_timer = 0;
        }
    }
}



void update_boss_ai(void) {
    if (!boss.active) return;

    //Handle death animation
    if (boss.is_dying) {
        //Continue death animation
        boss.anim_counter++;
        if (boss.anim_counter >= 12) {  //Slower death animation
            boss.anim_counter = 0;
            boss.anim_frame++;
            if (boss.anim_frame >= BOSS_ANIM_DEATH_FRAMES) {
                boss.anim_frame = BOSS_ANIM_DEATH_FRAMES - 1;  //Hold last frame
            }
            boss.frame = BOSS_ANIM_DEATH_START + boss.anim_frame;
        }

        boss.death_timer--;
        if (boss.death_timer <= 0) {
            boss.active = 0;

            //Now win screen
            game_state = GAME_STATE_WIN;
            menu_selection = 0;
            update_game_state_hardware();
        }
        return;
    }

    //Decrement hit timer
    if (boss.hit_timer > 0) {
        boss.hit_timer--;
    }

    //Decrement all cooldowns
    if (boss.attack_cooldown > 0) boss.attack_cooldown--;
    if (boss.summon_cooldown > 0) boss.summon_cooldown--;
    if (boss.burst_cooldown > 0) boss.burst_cooldown--;
    if (boss.homing_cooldown > 0) boss.homing_cooldown--;

    //Update movement
    boss_update_movement();

    //Face player (flip when player is on the left - sprite in BRAM faces right)
    int player_cx = player_x + PLAYER_SIZE / 2;
    int boss_cx = boss.x + BOSS_SIZE / 2;
    boss.flip = (player_cx < boss_cx) ? 1 : 0;

    //Attack
    int ability = boss_check_ability_ready();
    if (ability >= 0) {
        //Ability is ready
        boss_execute_attack(ability);
    } else if (boss.attack_cooldown == 0) {
        //use ranged
        boss_execute_attack(BOSS_ATTACK_SINGLE);
    }

    //Update animation
    boss.anim_counter++;
    if (boss.anim_counter >= 8) {
        boss.anim_counter = 0;
        boss.anim_frame++;

        //Handle animation state transitions
        int max_frames;
        int start_frame;

        switch (boss.anim_state) {
            case 0:  //Idle
                max_frames = BOSS_ANIM_IDLE_FRAMES;
                start_frame = BOSS_ANIM_IDLE_START;
                break;
            case 1:  //Fly
                max_frames = BOSS_ANIM_FLY_FRAMES;
                start_frame = BOSS_ANIM_FLY_START;
                break;
            case 2:  //Attack
                max_frames = BOSS_ANIM_ATTACK_FRAMES;
                start_frame = BOSS_ANIM_ATTACK_START;
                if (boss.anim_frame >= max_frames) {
                    //Return to idle after attack
                    boss.anim_state = 0;
                    boss.anim_frame = 0;
                }
                break;
            case 3:  //Death
                max_frames = BOSS_ANIM_DEATH_FRAMES;
                start_frame = BOSS_ANIM_DEATH_START;
                break;
            default:
                max_frames = BOSS_ANIM_IDLE_FRAMES;
                start_frame = BOSS_ANIM_IDLE_START;
                break;
        }


        if (boss.anim_state != 3 && boss.anim_frame >= max_frames) {
            boss.anim_frame = 0;
        }

        boss.frame = start_frame + boss.anim_frame;
    }

    //Calculate frame if animation state changed mid-update
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

void check_player_attack_boss(void) {
    if (!boss.active || boss.is_dying) return;
    if (!is_attacking) return;
    if (attack_hit_registered) return;  //Already hit something this attack

    //Use shared hitbox calculation
    int attack_x, attack_y, attack_w, attack_h;
    get_attack_hitbox(&attack_x, &attack_y, &attack_w, &attack_h);

    //Check collision
    if (rect_overlap(attack_x, attack_y, attack_w, attack_h,
                     boss.x, boss.y, BOSS_SIZE, BOSS_SIZE)) {

        boss.health--;
        boss.hit_timer = BOSS_HIT_FLASH;
        attack_hit_registered = 1;

        if (boss.health <= 0) {
            //Start death animation
            boss.is_dying = 1;
            boss.anim_state = 3;  //Death
            boss.anim_frame = 0;
            boss.death_timer = 120;
        }
    }
}
