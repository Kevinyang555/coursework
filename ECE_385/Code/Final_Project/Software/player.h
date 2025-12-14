//player.h - Player State and Input Handling

#ifndef PLAYER_H
#define PLAYER_H

#include "game_types.h"


extern int player_x, player_y;

extern int vel_x, vel_y;

extern int hold_up, hold_down, hold_left, hold_right;

extern Direction player_dir;
extern int is_moving;
extern int anim_frame;
extern int anim_counter;

extern int is_attacking;
extern int attack_anim_frame;
extern int attack_anim_counter;
extern int attack_cooldown;
extern int attack_hit_registered;

extern int ranged_cooldown;
extern int player_proj_frame;
extern int player_proj_anim_counter;

extern int player_health;
extern int player_invincible;

extern int player_armor;
extern int armor_regen_cooldown;
extern int armor_regen_timer;


//Update player animation
void update_animation(void);

//Update hardware
void update_player_hardware(void);

//Update player health
void update_player_health_hardware(void);

//Update player armor
void update_player_armor_hardware(void);


//Process keyboard input
void process_keyboard(BOOT_KBD_REPORT* kbd);

//Handle input in menu state
void handle_menu_input(BOOT_KBD_REPORT* kbd);

//Handle input in game over state
void handle_gameover_input(BOOT_KBD_REPORT* kbd);

//Handle input in win state
void handle_win_input(BOOT_KBD_REPORT* kbd);


//Spawn player shuriken
void spawn_player_projectile(void);


int get_attack_dir(Direction dir);

void get_attack_hitbox(int *x, int *y, int *w, int *h);

#endif
