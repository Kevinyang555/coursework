//collision.h - Collision Detection System

#ifndef COLLISION_H
#define COLLISION_H

#include "game_types.h"

//COLLISION MAP
extern uint8_t COLLISION_MAP[COLLISION_MAP_HEIGHT][COLLISION_MAP_WIDTH];


//collision for specific room
void build_i_shape_collision(void);
void build_cross_collision(void);
void build_pillars_collision(void);

//Load collision map
void load_collision_map(int template_id);

//Clear breach tiles
void clear_breach_collision(int breach_dir);


//Convert screen coordinates to tile coordinates
int screen_to_tile_x(int screen_x);
int screen_to_tile_y(int screen_y);

//Check if a tile is solid
int is_tile_solid(int tile_x, int tile_y);

//Check if an entity collides with any solid tiles
int check_tile_collision(int entity_x, int entity_y, int size);

//Check if attack hitbox is blocked by obstacles
int is_attack_blocked(int attack_x, int attack_y, int attack_size);

//Check if two rectangles overlap
int rect_overlap(int x1, int y1, int w1, int h1, int x2, int y2, int w2, int h2);

int abs_val(int x);

#endif
