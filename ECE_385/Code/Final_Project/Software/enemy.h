//enemy.h - Enemy AI and Management

#ifndef ENEMY_H
#define ENEMY_H

#include "game_types.h"
#include "xil_types.h"


extern Enemy enemies[MAX_ENEMIES];
extern Projectile projectiles[MAX_PROJECTILES];


//Initialize a single enemy
void init_single_enemy(int idx, int x, int y, int sprite_type, int behavior_type);

//Get behavior
int get_behavior_for_sprite(int sprite_type);

//Get random spawn position
void get_random_spawn_pos(int *x, int *y);

//Get random sprite type
int get_level_sprite_type(void);

//Initialize enemies
void init_enemies(void);


//Clamp enemy position to game area
void clamp_enemy_position(Enemy *e);

void update_ranged_enemy_ai(Enemy *e);

void update_melee_enemy_ai(Enemy *e);

void update_all_enemies_ai(void);

//Handle enemy-to-enemy collision
void handle_enemy_collision(void);

//Check if all enemies are dead
int all_enemies_dead(void);


//Spawn projectile from enemy
void spawn_projectile(Enemy *e);

//Update all projectiles
void update_projectiles(void);


//Write enemy state to hardware
void write_enemy_hardware(int idx, u32 x_reg, u32 y_reg, u32 frame_reg, u32 active_reg);

//Update all enemies
void update_enemies_hardware(void);

//Update all projectiles in hardware
void update_projectiles_hardware(void);

int simple_rand(void);

u32 pack_projectile(Projectile *p);

#endif
