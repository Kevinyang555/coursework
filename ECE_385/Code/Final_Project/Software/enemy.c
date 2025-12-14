//enemy.c - Enemy AI and Management

#include "enemy.h"
#include "collision.h"
#include "hardware_regs.h"
#include "xil_io.h"
#include <stdlib.h>

extern int player_x, player_y;
extern int current_level;


Enemy enemies[MAX_ENEMIES];
Projectile projectiles[MAX_PROJECTILES];

static unsigned int rand_seed = 12345;
int simple_rand(void) {
    rand_seed = rand_seed * 1103515245 + 12345;
    return (rand_seed >> 16) & 0x7FFF;
}

//Pack projectile data for hardware register
u32 pack_projectile(Projectile *p) {
    if (!p->active) return 0;
    u32 result = ((u32)1 << 31) | ((p->y & 0x3FF) << 16) | (p->x & 0x3FF);
    if (p->is_player_proj) result |= ((u32)1 << 30);
    if (p->flip) result |= ((u32)1 << 29);
    return result;
}

//ENEMY INITIALIZATION
void init_single_enemy(int idx, int x, int y, int sprite_type, int behavior_type) {
    enemies[idx].x = x;
    enemies[idx].y = y;
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

    if (behavior_type == ENEMY_TYPE_RANGED) {
        enemies[idx].attack_cooldown = RANGED_SHOOT_COOLDOWN;
    } else {
        enemies[idx].attack_cooldown = MELEE_ATTACK_COOLDOWN;
    }
}

int get_behavior_for_sprite(int sprite_type) {
    switch (sprite_type) {
        case SPRITE_TYPE_0: return ENEMY_TYPE_MELEE;
        case SPRITE_TYPE_1: return ENEMY_TYPE_MELEE;
        case SPRITE_TYPE_2: return ENEMY_TYPE_MELEE;
        case SPRITE_TYPE_3: return ENEMY_TYPE_RANGED;
        case SPRITE_TYPE_4: return ENEMY_TYPE_RANGED;
        case SPRITE_TYPE_5: return ENEMY_TYPE_RANGED;
        default: return ENEMY_TYPE_MELEE;
    }
}

void get_random_spawn_pos(int *x, int *y) {
    int attempts = 0;
    do {
        *x = 180 + (simple_rand() % 400);
        *y = 50 + (simple_rand() % 350);
        attempts++;
    } while (check_tile_collision(*x, *y, ENEMY_SIZE) && attempts < 50);
}

int get_level_sprite_type(void) {
    if (current_level == 1) {
        //Level 1: Pick from 4 weaker enemy
        return SPRITE_TYPE_1 + (simple_rand() % 4);
    } else {
        //Level 2: All 6 enemy
        return simple_rand() % NUM_SPRITE_TYPES;
    }
}

void init_enemies(void) {
    int num_enemies;
    if (current_level == 1) {
        num_enemies = 3 + (rand() % 2);
    } else {
        num_enemies = MAX_ENEMIES;
    }

    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (i < num_enemies) {
            int sprite_type = get_level_sprite_type();
            int behavior = get_behavior_for_sprite(sprite_type);
            int x, y;
            get_random_spawn_pos(&x, &y);
            init_single_enemy(i, x, y, sprite_type, behavior);
        } else {
            enemies[i].active = 0;
        }
    }
}

//ENEMY AI
void clamp_enemy_position(Enemy *e) {
    if (e->x < GAME_AREA_LEFT) e->x = GAME_AREA_LEFT;
    if (e->x > GAME_AREA_RIGHT - ENEMY_SIZE) e->x = GAME_AREA_RIGHT - ENEMY_SIZE;
    if (e->y < GAME_AREA_TOP) e->y = GAME_AREA_TOP;
    if (e->y > GAME_AREA_BOTTOM - ENEMY_SIZE) e->y = GAME_AREA_BOTTOM - ENEMY_SIZE;
}

void spawn_projectile(Enemy *e) {
    for (int i = 0; i < MAX_PROJECTILES; i++) {
        if (!projectiles[i].active) {
            int enemy_cx = e->x + (ENEMY_SIZE / 2);
            int enemy_cy = e->y + (ENEMY_SIZE / 2);
            int player_cx = player_x + (PLAYER_SIZE / 2);
            int player_cy = player_y + (PLAYER_SIZE / 2);

            int dx = player_cx - enemy_cx;
            int dy = player_cy - enemy_cy;
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

            projectiles[i].x = enemy_cx - (PROJECTILE_SIZE / 2);
            projectiles[i].y = enemy_cy - (PROJECTILE_SIZE / 2);
            projectiles[i].active = 1;
            projectiles[i].is_player_proj = 0;
            projectiles[i].is_homing = 0;
            projectiles[i].is_boss_proj = 0;
            projectiles[i].source_type = e->sprite_type;
            return;
        }
    }
}

void update_ranged_enemy_ai(Enemy *e) {
    int dx = (player_x + PLAYER_SIZE/2) - (e->x + ENEMY_SIZE/2);
    int dy = (player_y + PLAYER_SIZE/2) - (e->y + ENEMY_SIZE/2);
    int dist = abs_val(dx) + abs_val(dy);

    if (e->attack_cooldown > 0) {
        e->attack_cooldown--;
    }

    //Face player
    if (abs_val(dx) > abs_val(dy)) {
        e->direction = (dx > 0) ? DIR_RIGHT : DIR_LEFT;
    } else {
        e->direction = (dy > 0) ? DIR_DOWN : DIR_UP;
    }

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
            move_x = move_away ? ((dx > 0) ? -RANGED_SPEED : RANGED_SPEED)
                               : ((dx > 0) ? RANGED_SPEED : -RANGED_SPEED);
        } else {
            move_y = move_away ? ((dy > 0) ? -RANGED_SPEED : RANGED_SPEED)
                               : ((dy > 0) ? RANGED_SPEED : -RANGED_SPEED);
        }

        int old_x = e->x, old_y = e->y;
        e->x += move_x;
        e->y += move_y;
        clamp_enemy_position(e);

        if (check_tile_collision(e->x, e->y, ENEMY_SIZE)) {
            e->x = old_x;
            e->y = old_y;
        }
    }

    if (dist < RANGED_SHOOT_RANGE && e->attack_cooldown == 0) {
        spawn_projectile(e);
        e->attack_cooldown = RANGED_SHOOT_COOLDOWN;
    }

    e->anim_counter++;
    if (e->anim_counter >= ANIM_SPEED) {
        e->anim_counter = 0;
        e->anim_frame = (e->anim_frame + 1) % FRAMES_PER_ANIM;
    }

    int local_frame = should_move ? 4 : 0;
    local_frame += e->anim_frame;
    e->frame = local_frame;
}

void update_melee_enemy_ai(Enemy *e) {
    if (e->attack_cooldown > 0) {
        e->attack_cooldown--;
    }

    if (e->is_dashing) {
        e->is_attacking = 1;
        e->dash_timer++;

        if (e->dash_timer >= MELEE_MAX_DASH_TIME) {
            e->is_dashing = 0;
            e->is_attacking = 0;
            e->dash_timer = 0;
            e->attack_cooldown = MELEE_ATTACK_COOLDOWN;
        } else {
            int target_dx = e->dash_target_x - e->x;
            int target_dy = e->dash_target_y - e->y;
            int target_dist = abs_val(target_dx) + abs_val(target_dy);

            if (target_dist <= MELEE_DASH_SPEED) {
                e->x = e->dash_target_x;
                e->y = e->dash_target_y;
                e->is_dashing = 0;
                e->is_attacking = 0;
                e->dash_timer = 0;
                e->attack_cooldown = MELEE_ATTACK_COOLDOWN;
            } else {
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

                if (check_tile_collision(e->x, e->y, ENEMY_SIZE)) {
                    e->x = old_x;
                    e->y = old_y;
                    e->is_dashing = 0;
                    e->is_attacking = 0;
                    e->dash_timer = 0;
                    e->attack_cooldown = MELEE_ATTACK_COOLDOWN;
                }

                if (abs_val(target_dx) > abs_val(target_dy)) {
                    e->direction = (target_dx > 0) ? DIR_RIGHT : DIR_LEFT;
                } else {
                    e->direction = (target_dy > 0) ? DIR_DOWN : DIR_UP;
                }
            }
        }
    } else {
        e->is_attacking = 0;

        if (e->attack_cooldown == 0) {
            e->is_dashing = 1;
            e->is_attacking = 1;
            e->dash_timer = 0;
            e->dash_target_x = player_x;
            e->dash_target_y = player_y;
        } else {
            e->wander_timer--;

            if (e->wander_timer <= 0) {
                int r = simple_rand();
                e->wander_dir_x = (r % 3) - 1;
                e->wander_dir_y = ((r >> 4) % 3) - 1;
                e->wander_timer = MELEE_WANDER_CHANGE;
            }

            int old_x = e->x, old_y = e->y;
            e->x += e->wander_dir_x * MELEE_SPEED;
            e->y += e->wander_dir_y * MELEE_SPEED;
            clamp_enemy_position(e);

            if (check_tile_collision(e->x, e->y, ENEMY_SIZE)) {
                e->x = old_x;
                e->y = old_y;
                e->wander_timer = 0;
            }

            if (e->wander_dir_x > 0) e->direction = DIR_RIGHT;
            else if (e->wander_dir_x < 0) e->direction = DIR_LEFT;
            else if (e->wander_dir_y > 0) e->direction = DIR_DOWN;
            else if (e->wander_dir_y < 0) e->direction = DIR_UP;
        }
    }

    e->anim_counter++;
    if (e->anim_counter >= ANIM_SPEED) {
        e->anim_counter = 0;
        e->anim_frame = (e->anim_frame + 1) % FRAMES_PER_ANIM;
    }

    int is_moving = e->is_dashing || (e->wander_dir_x != 0) || (e->wander_dir_y != 0);
    int local_frame = is_moving ? 4 : 0;
    local_frame += e->anim_frame;
    e->frame = local_frame;
}

void handle_enemy_collision(void) {
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (!enemies[i].active) continue;

        for (int j = i + 1; j < MAX_ENEMIES; j++) {
            if (!enemies[j].active) continue;

            int cx_i = enemies[i].x + ENEMY_SIZE / 2;
            int cy_i = enemies[i].y + ENEMY_SIZE / 2;
            int cx_j = enemies[j].x + ENEMY_SIZE / 2;
            int cy_j = enemies[j].y + ENEMY_SIZE / 2;

            int dx = cx_j - cx_i;
            int dy = cy_j - cy_i;
            int dist = abs_val(dx) + abs_val(dy);

            if (dist < ENEMY_COLLISION_DIST) {
                int push_x = 0, push_y = 0;

                if (dist == 0) {
                    push_x = ENEMY_PUSH_SPEED;
                } else if (abs_val(dx) > abs_val(dy)) {
                    push_x = (dx > 0) ? -ENEMY_PUSH_SPEED : ENEMY_PUSH_SPEED;
                } else {
                    push_y = (dy > 0) ? -ENEMY_PUSH_SPEED : ENEMY_PUSH_SPEED;
                }

                int old_i_x = enemies[i].x, old_i_y = enemies[i].y;
                enemies[i].x += push_x;
                enemies[i].y += push_y;
                clamp_enemy_position(&enemies[i]);
                if (check_tile_collision(enemies[i].x, enemies[i].y, ENEMY_SIZE)) {
                    enemies[i].x = old_i_x;
                    enemies[i].y = old_i_y;
                }

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

void update_all_enemies_ai(void) {
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (!enemies[i].active) continue;

        if (enemies[i].enemy_type == ENEMY_TYPE_RANGED) {
            update_ranged_enemy_ai(&enemies[i]);
        } else {
            update_melee_enemy_ai(&enemies[i]);
        }
    }
    handle_enemy_collision();
}

int all_enemies_dead(void) {
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (enemies[i].active) return 0;
    }
    return 1;
}

//PROJECTILE FUNCTIONS
void update_projectiles(void) {
    for (int i = 0; i < MAX_PROJECTILES; i++) {
        if (!projectiles[i].active) continue;

        //Handle tracking projectiles
        if (projectiles[i].is_homing && projectiles[i].homing_timer > 0) {
            projectiles[i].homing_timer--;

            int proj_cx = projectiles[i].x + PROJECTILE_SIZE / 2;
            int proj_cy = projectiles[i].y + PROJECTILE_SIZE / 2;
            int player_cx = player_x + PLAYER_SIZE / 2;
            int player_cy = player_y + PLAYER_SIZE / 2;

            int dx = player_cx - proj_cx;
            int dy = player_cy - proj_cy;
            int abs_dx = abs_val(dx);
            int abs_dy = abs_val(dy);

            if (abs_dx > 0 || abs_dy > 0) {
                int target_vx, target_vy;
                if (abs_dx > abs_dy) {
                    target_vx = (dx > 0) ? HOMING_SPEED : -HOMING_SPEED;
                    target_vy = (abs_dx > 0) ? (dy * HOMING_SPEED) / abs_dx : 0;
                } else {
                    target_vy = (dy > 0) ? HOMING_SPEED : -HOMING_SPEED;
                    target_vx = (abs_dy > 0) ? (dx * HOMING_SPEED) / abs_dy : 0;
                }

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

                if (projectiles[i].is_boss_proj) {
                    projectiles[i].flip = (projectiles[i].vx < 0) ? 1 : 0;
                }
            }
        }

        //Move projectile
        projectiles[i].x += projectiles[i].vx;
        projectiles[i].y += projectiles[i].vy;

        //Off-screen check
        if (projectiles[i].x < 0 || projectiles[i].x > 640 ||
            projectiles[i].y < 0 || projectiles[i].y > 480) {
            projectiles[i].active = 0;
            continue;
        }

        //Obstacle collision
        if (check_tile_collision(projectiles[i].x, projectiles[i].y, PROJECTILE_SIZE)) {
            projectiles[i].active = 0;
        }
    }
}

//HARDWARE UPDATE
void write_enemy_hardware(int idx, u32 x_reg, u32 y_reg, u32 frame_reg, u32 active_reg) {
    Xil_Out32(x_reg, enemies[idx].x);
    Xil_Out32(y_reg, enemies[idx].y);
    Xil_Out32(frame_reg, enemies[idx].frame);
    int flip = (enemies[idx].direction == DIR_LEFT) ? 1 : 0;
    int attack = enemies[idx].is_attacking ? 1 : 0;
    int hit = (enemies[idx].hit_timer > 0) ? 1 : 0;
    Xil_Out32(active_reg, (enemies[idx].sprite_type << 4) | (hit << 3) | (attack << 2) | (flip << 1) | enemies[idx].active);
}

void update_enemies_hardware(void) {
    write_enemy_hardware(0, ENEMY0_X_REG, ENEMY0_Y_REG, ENEMY0_FRAME_REG, ENEMY0_ACTIVE_REG);
    write_enemy_hardware(1, ENEMY1_X_REG, ENEMY1_Y_REG, ENEMY1_FRAME_REG, ENEMY1_ACTIVE_REG);
    write_enemy_hardware(2, ENEMY2_X_REG, ENEMY2_Y_REG, ENEMY2_FRAME_REG, ENEMY2_ACTIVE_REG);
    write_enemy_hardware(3, ENEMY3_X_REG, ENEMY3_Y_REG, ENEMY3_FRAME_REG, ENEMY3_ACTIVE_REG);
    write_enemy_hardware(4, ENEMY4_X_REG, ENEMY4_Y_REG, ENEMY4_FRAME_REG, ENEMY4_ACTIVE_REG);
}

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
