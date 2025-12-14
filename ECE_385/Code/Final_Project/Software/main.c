#include <stdlib.h>
#include "platform.h"
#include "xil_io.h"
#include "hardware_regs.h"
#include "game_types.h"
#include "collision.h"
#include "room_system.h"
#include "enemy.h"
#include "boss.h"
#include "player.h"
#include "battle.h"
#include "lw_usb/GenericMacros.h"
#include "lw_usb/MAX3421E.h"
#include "lw_usb/USB.h"


extern HID_DEVICE hid_device;


int main() {
    init_platform();

    //Initialize level/room tracking
    current_level = 1;
    current_room = 0;
    entry_direction = DIR_BREACH_RIGHT;
    exit_direction = DIR_BREACH_RIGHT;

    //Initialize room system - start with empty room (map 0)
    load_collision_map(TEMPLATE_EMPTY);
    Xil_Out32(MAP_SELECT_REG, TEMPLATE_EMPTY);
    Xil_Out32(BREACH_OPEN_REG, 0);
    Xil_Out32(BREACH_DIR_REG, DIR_BREACH_RIGHT);

    //Initialize hardware displays
    update_player_hardware();
    update_projectiles_hardware();
    update_player_health_hardware();

    //Initialize game state to MENU
    game_state = GAME_STATE_MENU;
    menu_selection = 0;
    update_game_state_hardware();

    //Clear enemies for menu
    for (int i = 0; i < MAX_ENEMIES; i++) {
        enemies[i].active = 0;
    }
    update_enemies_hardware();

    BYTE rcode;
    BOOT_KBD_REPORT kbdbuf = {0};
    BOOT_KBD_REPORT last_kbdbuf = {0};
    BYTE running = 0;

    MAX3421E_init();
    USB_init();

    while (1) {
        MAX3421E_Task();
        USB_Task();

        if (GetUsbTaskState() == USB_STATE_RUNNING) {
            if (!running) {
                running = 1;
            }

            rcode = kbdPoll(&kbdbuf);
            if (rcode == 0) {
                last_kbdbuf = kbdbuf;
            }

            //Handle input based on game state
            switch (game_state) {
                case GAME_STATE_MENU:
                    handle_menu_input(&last_kbdbuf);
                    break;

                case GAME_STATE_PLAYING: {
                    //Normal game input processing
                    process_keyboard(&last_kbdbuf);

                    update_battle_system();

                    //Check game over
                    if (player_health <= 0) {
                        game_state = GAME_STATE_GAMEOVER;
                        menu_selection = 0;
                        update_game_state_hardware();
                    }

                    check_room_cleared();
                    check_room_transition();

                    //Stair
                    if (current_template == TEMPLATE_STAIR && player_on_stairs()) {
                        int j_pressed = 0;
                        for (int i = 0; i < 6; i++) {
                            if (last_kbdbuf.keycode[i] == KEY_J) {
                                j_pressed = 1;
                                break;
                            }
                        }
                        static int stair_prev_j = 0;
                        if (j_pressed && !stair_prev_j && !is_attacking) {
                            advance_to_next_level();
                        }
                        stair_prev_j = j_pressed;
                    }
                    break;
                }

                case GAME_STATE_GAMEOVER:
                    handle_gameover_input(&last_kbdbuf);
                    break;

                case GAME_STATE_WIN:
                    handle_win_input(&last_kbdbuf);
                    break;
            }

        } else {
            if (running) {
                running = 0;
            }
            BOOT_KBD_REPORT empty = {0};
            last_kbdbuf = empty;

            if (game_state == GAME_STATE_PLAYING) {
                process_keyboard(&empty);
            }
        }

        //Increment frame counter (for random seed entropy)
        frame_counter++;

        //Small delay for consistent frame rate (~60fps feel)
        for (volatile int i = 0; i < 5000; i++);
    }
    return 0;
}
