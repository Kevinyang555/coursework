//boss.h - Boss AI and Management

#ifndef BOSS_H
#define BOSS_H

#include "game_types.h"

//BOSS DATA (extern declaration)
extern Boss boss;


//Initialize boss at starting position in boss room
void init_boss(void);

//Single projectile toward player
void boss_spawn_projectile(void);

//Summon bat enemies around boss
void boss_summon_bats(void);

//16-direction burst attack
void boss_burst_attack(void);

//3 tracking projectiles that track player
void boss_homing_attack(void);

//Check if any ability is ready
int boss_check_ability_ready(void);

//Execute boss attack and set cooldown
void boss_execute_attack(int attack_type);

// boss movement
void boss_update_movement(void);

//Main boss AI
void update_boss_ai(void);

//Check if player attack hits boss
void check_player_attack_boss(void);

//Write boss state to hardware registers
void update_boss_hardware(void);

#endif
