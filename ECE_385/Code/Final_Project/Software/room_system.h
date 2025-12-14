//room_system.h - Room and Level System

#ifndef ROOM_SYSTEM_H
#define ROOM_SYSTEM_H

#include "game_types.h"

extern int level1_templates[LEVEL1_ROOMS];
extern int level2_templates[LEVEL2_ROOMS];

//ROOM STATE
extern int current_level;
extern int current_room;
extern int current_template;
extern int entry_direction;
extern int exit_direction;
extern int room_cleared;
extern int breach_opened;


//Pick a random exit
int pick_exit_direction(int entry_dir);

//Get spawn position based on entry direction
void get_spawn_position(int entry_dir, int *spawn_x, int *spawn_y);

//Get template for current room in current level
int get_room_template(int level, int room_idx);

//Check if room has enemies
int is_battle_room(int template_id);

//Check boss room
int is_boss_room(void);

//Load a room by template
void load_room(int template_id);

//Open breach
void open_breach(void);

//Check if cleared
void check_room_cleared(void);

//Check if player is in breach
int player_in_breach(int breach_dir);

void advance_to_next_room(void);

void check_room_transition(void);

//Check on stairs
int player_on_stairs(void);

//Advance to next level
void advance_to_next_level(void);

//Convert movement direction to attack direction (different order)
int get_attack_dir(Direction dir);

#endif
