//collision.c - Collision Detection System

#include "collision.h"
#include "game_types.h"

//COLLISION MAP
uint8_t COLLISION_MAP[COLLISION_MAP_HEIGHT][COLLISION_MAP_WIDTH];


int abs_val(int x) {
    return (x < 0) ? -x : x;
}


//Build I-shape collision template
void build_i_shape_collision(void) {
    for (int r = 5; r <= 6; r++) {
        for (int c = 10; c <= 19; c++) {
            COLLISION_MAP[r][c] = 1;
        }
    }

    for (int r = 7; r <= 22; r++) {
        for (int c = 13; c <= 16; c++) {
            COLLISION_MAP[r][c] = 1;
        }
    }

    for (int r = 23; r <= 24; r++) {
        for (int c = 10; c <= 19; c++) {
            COLLISION_MAP[r][c] = 1;
        }
    }
}

//Build Cross-shape collision template
void build_cross_collision(void) {
    for (int r = 13; r <= 16; r++) {
        for (int c = 6; c <= 23; c++) {
            COLLISION_MAP[r][c] = 1;
        }
    }

    for (int r = 6; r <= 23; r++) {
        for (int c = 13; c <= 16; c++) {
            COLLISION_MAP[r][c] = 1;
        }
    }
}

//Build Pillars collision template
void build_pillars_collision(void) {
    for (int r = 7; r <= 8; r++) {
        for (int c = 7; c <= 8; c++) {
            COLLISION_MAP[r][c] = 1;
        }
    }
    for (int r = 7; r <= 8; r++) {
        for (int c = 21; c <= 22; c++) {
            COLLISION_MAP[r][c] = 1;
        }
    }
    for (int r = 21; r <= 22; r++) {
        for (int c = 7; c <= 8; c++) {
            COLLISION_MAP[r][c] = 1;
        }
    }
    for (int r = 21; r <= 22; r++) {
        for (int c = 21; c <= 22; c++) {
            COLLISION_MAP[r][c] = 1;
        }
    }
}

//Load collision map for a room template
void load_collision_map(int template_id) {
    //Clear
    for (int r = 0; r < COLLISION_MAP_HEIGHT; r++) {
        for (int c = 0; c < COLLISION_MAP_WIDTH; c++) {
            COLLISION_MAP[r][c] = 0;
        }
    }

    //Set wall borders (row 0, row 29, col 0, col 29)
    for (int i = 0; i < COLLISION_MAP_WIDTH; i++) {
        COLLISION_MAP[0][i] = 1;                          //Top wall
        COLLISION_MAP[COLLISION_MAP_HEIGHT-1][i] = 1;     //Bottom wall
    }
    for (int i = 0; i < COLLISION_MAP_HEIGHT; i++) {
        COLLISION_MAP[i][0] = 1;                          //Left wall
        COLLISION_MAP[i][COLLISION_MAP_WIDTH-1] = 1;      //Right wall
    }

    //Build obstacle collision based on template
    switch (template_id) {
        case TEMPLATE_I_SHAPE:
            build_i_shape_collision();
            break;
        case TEMPLATE_CROSS:
            build_cross_collision();
            break;
        case TEMPLATE_PILLARS:
            build_pillars_collision();
            break;
        case TEMPLATE_EMPTY:
        case TEMPLATE_STAIR:
        default:
            //No obstacles, just walls
            break;
    }
}

//Clear breach tiles from collision when door opens
void clear_breach_collision(int breach_dir) {
    switch (breach_dir) {
        case DIR_BREACH_RIGHT:
            for (int row = BREACH_CENTER_START; row <= BREACH_CENTER_END; row++) {
                COLLISION_MAP[row][COLLISION_MAP_WIDTH-1] = 0;
            }
            break;
        case DIR_BREACH_LEFT:
            for (int row = BREACH_CENTER_START; row <= BREACH_CENTER_END; row++) {
                COLLISION_MAP[row][0] = 0;
            }
            break;
        case DIR_BREACH_UP:
            for (int col = BREACH_CENTER_START; col <= BREACH_CENTER_END; col++) {
                COLLISION_MAP[0][col] = 0;
            }
            break;
        case DIR_BREACH_DOWN:
            for (int col = BREACH_CENTER_START; col <= BREACH_CENTER_END; col++) {
                COLLISION_MAP[COLLISION_MAP_HEIGHT-1][col] = 0;
            }
            break;
    }
}

//Convert screen X coordinate to tile column
int screen_to_tile_x(int screen_x) {
    return (screen_x - GAME_AREA_X_OFFSET) / TILE_SIZE;
}

//Convert screen Y coordinate to tile row
int screen_to_tile_y(int screen_y) {
    return screen_y / TILE_SIZE;
}

//Check if a tile is solid (wall or obstacle)
int is_tile_solid(int tile_x, int tile_y) {
    if (tile_x < 0 || tile_x >= COLLISION_MAP_WIDTH ||
        tile_y < 0 || tile_y >= COLLISION_MAP_HEIGHT) {
        return 1;
    }
    return COLLISION_MAP[tile_y][tile_x];
}

//Check entity collision with tiles
int check_tile_collision(int entity_x, int entity_y, int size) {
    //Get tile coordinates for all four corners of entity bounding box
    int tile_x1 = screen_to_tile_x(entity_x);
    int tile_y1 = screen_to_tile_y(entity_y);
    int tile_x2 = screen_to_tile_x(entity_x + size - 1);
    int tile_y2 = screen_to_tile_y(entity_y + size - 1);

    //Check all tiles the entity overlaps
    for (int ty = tile_y1; ty <= tile_y2; ty++) {
        for (int tx = tile_x1; tx <= tile_x2; tx++) {
            if (is_tile_solid(tx, ty)) {
                return 1;
            }
        }
    }
    return 0;
}

//Check if attack hitbox is blocked by obstacle
int is_attack_blocked(int attack_x, int attack_y, int attack_size) {
    int center_x = attack_x + attack_size / 2;
    int center_y = attack_y + attack_size / 2;
    int tile_x = screen_to_tile_x(center_x);
    int tile_y = screen_to_tile_y(center_y);
    return is_tile_solid(tile_x, tile_y);
}


//Check if two rectangles overlap
int rect_overlap(int x1, int y1, int w1, int h1, int x2, int y2, int w2, int h2) {
    return (x1 < x2 + w2) && (x1 + w1 > x2) &&
           (y1 < y2 + h2) && (y1 + h1 > y2);
}
