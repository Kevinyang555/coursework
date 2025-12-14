# Soul Knight - FPGA Edition

A top-down shooter game running on Spartan-7 FPGA with MicroBlaze soft processor.

## Controls

| Key | Action |
|-----|--------|
| W/A/S/D | Move |
| J | Melee attack / Use stairs |
| K | Throw shuriken (Level 2 only) |
| Space | Start game / Confirm menu |

## How to Play

1. Press SPACE to start
2. Kill all enemies in each room to open the door
3. Walk through the door to enter next room
4. Clear 4 battle rooms, then take the stairs to Level 2
5. Beat the boss to win!

## Features

- 2 levels with 5 rooms each
- 6 enemy types (melee + ranged)
- Boss fight with multiple attack patterns (burst, homing, summon)
- Health (6 hearts) + Armor (3 bars, regenerates after 5 sec)
- Melee attacks can deflect enemy projectiles
- Tile-based collision with 4 room layouts

## Tips

- Melee enemies dash to where you *were* - keep moving
- Ranged enemies try to keep distance - chase them down
- Boss unlocks summon attack at 50% HP
- Shuriken is your friend against the boss

Built for ECE 385 @ UIUC
