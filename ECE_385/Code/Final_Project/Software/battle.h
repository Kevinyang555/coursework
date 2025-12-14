//battle.h - Combat and Damage System

#ifndef BATTLE_H
#define BATTLE_H

#include "game_types.h"

//GAME STATE
extern int game_state;
extern int menu_selection;
extern unsigned int frame_counter;

//Previous key state
extern int prev_key_w, prev_key_s, prev_key_space;



//Check rectangle
int rect_overlap(int x1, int y1, int w1, int h1, int x2, int y2, int w2, int h2);

//Get attack hitbox based on player position and direction
void get_attack_hitbox(int *hx, int *hy, int *hw, int *hh);

//Check if attack is blocked
int is_attack_blocked(int hx, int hy, int size);


//Check player melee attack collision
void check_player_attack_hit(void);

//Check player attack destroying projectiles
void check_player_attack_projectiles(void);

//Check player shuriken
void check_player_projectile_hits(void);


//Check melee enemy dash collision
void check_melee_dash_collisions(void);

//Check projectile collision
void check_projectile_collisions(void);


//damage to player
void player_take_damage(int damage);

//Apply knockback to player
void apply_player_knockback(int attacker_x, int attacker_y);


//Update game state
void update_game_state_hardware(void);

//Reset game for new game
void reset_game(void);

//Update all battle system logic per frame
void update_battle_system(void);

#endif
