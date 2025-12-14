//battle.c - Combat and Damage System

#include "battle.h"
#include "player.h"
#include "enemy.h"
#include "boss.h"
#include "room_system.h"
#include "collision.h"
#include "hardware_regs.h"
#include "xil_io.h"
#include <stdlib.h>

extern void clamp_enemy_position(Enemy *e);
extern void update_all_enemies_ai(void);
extern void update_boss_ai(void);
extern void update_projectiles(void);
extern void update_enemies_hardware(void);
extern void update_boss_hardware(void);
extern void update_projectiles_hardware(void);

extern Enemy enemies[MAX_ENEMIES];
extern Projectile projectiles[MAX_PROJECTILES];
extern Boss boss;

extern int current_level;
extern int current_room;
extern int entry_direction;
extern int exit_direction;


int game_state = GAME_STATE_MENU;
int menu_selection = 0;
int prev_key_w = 0, prev_key_s = 0, prev_key_space = 0;
unsigned int frame_counter = 0;


//PLAYER ATTACK COLLISION
void check_player_attack_hit(void) {
    if (!is_attacking || attack_hit_registered) return;

    int hx, hy, hw, hh;
    get_attack_hitbox(&hx, &hy, &hw, &hh);

    //Check if attack hitbox is blocked by obstacle
    if (is_attack_blocked(hx, hy, hw)) {
        return;
    }

    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (!enemies[i].active) continue;

        if (rect_overlap(hx, hy, hw, hh, enemies[i].x, enemies[i].y, ENEMY_SIZE, ENEMY_SIZE)) {
            enemies[i].health--;
            enemies[i].hit_timer = HIT_FLASH_DURATION;
            attack_hit_registered = 1;

            //Apply knockback
            int dx = enemies[i].x - player_x;
            int dy = enemies[i].y - player_y;
            int old_ex = enemies[i].x, old_ey = enemies[i].y;

            if (abs_val(dx) > abs_val(dy)) {
                enemies[i].x += (dx > 0) ? KNOCKBACK_DISTANCE : -KNOCKBACK_DISTANCE;
            } else {
                enemies[i].y += (dy > 0) ? KNOCKBACK_DISTANCE : -KNOCKBACK_DISTANCE;
            }

            clamp_enemy_position(&enemies[i]);

            //Check tile collision - revert if knocked into obstacle
            if (check_tile_collision(enemies[i].x, enemies[i].y, ENEMY_SIZE)) {
                enemies[i].x = old_ex;
                enemies[i].y = old_ey;
            }

            //Cancel dash if melee enemy was hit during dash
            if (enemies[i].is_dashing) {
                enemies[i].is_dashing = 0;
                enemies[i].is_attacking = 0;
            }

            if (enemies[i].health <= 0) {
                enemies[i].active = 0;
            }
            break;
        }
    }
}

void check_player_attack_projectiles(void) {
    if (!is_attacking) return;

    int hx, hy, hw, hh;
    get_attack_hitbox(&hx, &hy, &hw, &hh);

    int player_cx = player_x + PLAYER_SIZE / 2;
    int player_cy = player_y + PLAYER_SIZE / 2;

    for (int i = 0; i < MAX_PROJECTILES; i++) {
        if (!projectiles[i].active) continue;

        int proj_cx = projectiles[i].x + PROJECTILE_SIZE / 2;
        int proj_cy = projectiles[i].y + PROJECTILE_SIZE / 2;

        //Check if projectile is in the attack direction
        int in_direction = 0;
        switch (player_dir) {
            case DIR_UP:    in_direction = (proj_cy < player_cy); break;
            case DIR_DOWN:  in_direction = (proj_cy > player_cy); break;
            case DIR_LEFT:  in_direction = (proj_cx < player_cx); break;
            case DIR_RIGHT: in_direction = (proj_cx > player_cx); break;
        }

        if (!in_direction) continue;

        if (rect_overlap(hx, hy, hw, hh,
                        projectiles[i].x, projectiles[i].y,
                        PROJECTILE_SIZE, PROJECTILE_SIZE)) {
            projectiles[i].active = 0;
        }
    }
}

void check_player_projectile_hits(void) {
    for (int i = 0; i < MAX_PROJECTILES; i++) {
        if (!projectiles[i].active || !projectiles[i].is_player_proj) continue;

        int px = projectiles[i].x, py = projectiles[i].y;

        //Check collision with boss
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

        //Check collision with enemies
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

//DAMAGE AND KNOCKBACK
void player_take_damage(int damage) {
    int remaining = damage;

    //Armor absorbs damage first
    while (remaining > 0 && player_armor > 0) {
        player_armor--;
        remaining--;
    }
    update_player_armor_hardware();

    //Remaining damage goes to health
    if (remaining > 0) {
        player_health -= remaining;
        if (player_health < 0) player_health = 0;
        update_player_health_hardware();
    }

    armor_regen_cooldown = ARMOR_REGEN_DELAY;
    armor_regen_timer = 0;
    player_invincible = PLAYER_INVINCIBILITY;
}

void apply_player_knockback(int attacker_x, int attacker_y) {
    int dx = player_x - attacker_x;
    int dy = player_y - attacker_y;
    int old_x = player_x, old_y = player_y;

    if (abs_val(dx) > abs_val(dy)) {
        player_x += (dx > 0) ? PLAYER_KNOCKBACK_DIST : -PLAYER_KNOCKBACK_DIST;
    } else {
        player_y += (dy > 0) ? PLAYER_KNOCKBACK_DIST : -PLAYER_KNOCKBACK_DIST;
    }

    //Clamp player position
    if (player_x < GAME_AREA_LEFT) player_x = GAME_AREA_LEFT;
    if (player_x > GAME_AREA_RIGHT) player_x = GAME_AREA_RIGHT;
    if (player_y < GAME_AREA_TOP) player_y = GAME_AREA_TOP;
    if (player_y > GAME_AREA_BOTTOM) player_y = GAME_AREA_BOTTOM;

    //Check tile collision - revert if knocked into obstacle
    if (check_tile_collision(player_x, player_y, PLAYER_SIZE)) {
        player_x = old_x;
        player_y = old_y;
    }
}

//ENEMY ATTACK COLLISION
void check_melee_dash_collisions(void) {
    if (player_invincible > 0) return;

    int px = player_x + 4;
    int py = player_y + 4;
    int pw = PLAYER_SIZE - 8;
    int ph = PLAYER_SIZE - 8;

    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (!enemies[i].active) continue;
        if (enemies[i].enemy_type != ENEMY_TYPE_MELEE) continue;
        if (!enemies[i].is_dashing) continue;

        if (rect_overlap(px, py, pw, ph,
                        enemies[i].x, enemies[i].y, ENEMY_SIZE, ENEMY_SIZE)) {
            int dmg = (enemies[i].sprite_type == SPRITE_TYPE_0 || enemies[i].sprite_type == SPRITE_TYPE_5)
                      ? ENEMY_STRONG_DAMAGE : ENEMY_WEAK_DAMAGE;
            player_take_damage(dmg);

            apply_player_knockback(enemies[i].x, enemies[i].y);

            enemies[i].is_dashing = 0;
            enemies[i].is_attacking = 0;
            enemies[i].attack_cooldown = MELEE_ATTACK_COOLDOWN;
            break;
        }
    }
}

void check_projectile_collisions(void) {
    if (player_invincible > 0) return;

    int px = player_x + 4;
    int py = player_y + 4;
    int pw = PLAYER_SIZE - 8;
    int ph = PLAYER_SIZE - 8;

    for (int i = 0; i < MAX_PROJECTILES; i++) {
        if (!projectiles[i].active) continue;
        if (projectiles[i].is_player_proj) continue;

        if (rect_overlap(px, py, pw, ph,
                        projectiles[i].x, projectiles[i].y,
                        PROJECTILE_SIZE, PROJECTILE_SIZE)) {
            projectiles[i].active = 0;
            int dmg;
            if (projectiles[i].is_homing) {
                dmg = HOMING_DAMAGE;
            } else {
                int src = projectiles[i].source_type;
                dmg = (src == SPRITE_TYPE_0 || src == SPRITE_TYPE_5 || src == SOURCE_TYPE_BOSS)
                          ? ENEMY_STRONG_DAMAGE : ENEMY_WEAK_DAMAGE;
            }
            player_take_damage(dmg);
        }
    }
}

//GAME STATE

void update_game_state_hardware(void) {
    Xil_Out32(GAME_STATE_REG, game_state);
    Xil_Out32(MENU_SELECT_REG, menu_selection);
}

void reset_game(void) {
    //Reset player state
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

    hold_up = 0;
    hold_down = 0;
    hold_left = 0;
    hold_right = 0;

    player_armor = PLAYER_MAX_ARMOR;
    armor_regen_cooldown = 0;
    armor_regen_timer = 0;

    current_level = 1;
    current_room = 0;
    entry_direction = DIR_BREACH_RIGHT;
    exit_direction = DIR_BREACH_RIGHT;

    boss.active = 0;

    //Load first room
    int first_template = get_room_template(current_level, current_room);
    load_room(first_template);

    Xil_Out32(LEVEL_REG, 0);

    update_player_hardware();
    update_player_health_hardware();
    update_player_armor_hardware();
    update_boss_hardware();
}

void update_battle_system(void) {
    if (player_invincible > 0) {
        player_invincible--;
    }

    if (armor_regen_cooldown > 0) {
        armor_regen_cooldown--;
    } else if (player_armor < PLAYER_MAX_ARMOR) {
        armor_regen_timer++;
        if (armor_regen_timer >= ARMOR_REGEN_RATE) {
            player_armor++;
            armor_regen_timer = 0;
            update_player_armor_hardware();
        }
    }

    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (enemies[i].hit_timer > 0) {
            enemies[i].hit_timer--;
        }
    }

    update_all_enemies_ai();

    update_boss_ai();

    update_projectiles();

    check_player_attack_hit();
    check_player_attack_boss();
    check_player_attack_projectiles();
    check_player_projectile_hits();
    check_melee_dash_collisions();
    check_projectile_collisions();

    player_proj_anim_counter++;
    if (player_proj_anim_counter >= 4) {
        player_proj_anim_counter = 0;
        player_proj_frame = (player_proj_frame + 1) & 0x3;
    }

    if (ranged_cooldown > 0) {
        ranged_cooldown--;
    }

    update_enemies_hardware();
    update_boss_hardware();
    update_projectiles_hardware();
    Xil_Out32(PLAYER_PROJ_FRAME_REG, player_proj_frame);
    update_player_health_hardware();
}
