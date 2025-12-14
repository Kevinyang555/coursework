# mp1.s -------- Your solution goes into this file --------
#
        # Tip: The below variables are instantiates in demo.c which is hidden from you on purpose.
        # You only get the final demo.o to linked with your mp1.o, 
        # Makefile handled all the linking for you.
        # Check skyline.h for the declaration/defination of these variables

        .extern skyline_star_list       # Head node for the linked list of stars
        .extern skyline_windows         # Array head location of Windows
        .extern skyline_win_cnt         # Current window counts
        .extern skyline_beacon          # The beacon struct address

        .equ width_fbuf, 640            # The width of the frame buffer
        .equ length_fbuf, 480           # The length of the frame buffer
        .equ max_window_cnt, 4000       # The maximum numbers of windows
        
        
        .text
        .global skyline_init
        .type   skyline_init, @function
        skyline_init:
               ret

        # ########## Students! Your turn! ##########
        # Finish the 9 functions required in the manual
        # .........
        # 
        #     |\__/,|   (`\
        #   _.|o o  |_   ) )
        # -(((---(((--------
        # 
        # Perhaps start with the meaning of ".global" and ".extern" ?
        # I might forget to finish writing them? Perhaps?
        # Or do I realllllly need them xD?
        # Your call!
        #
        # You are allowed to change anything inside this file
        # Only inside this file, please.

        
        # ============================
        # Function: add_star，void add_star(uint16_t x, uint16_t y, uint16_t color)
        #       
        #        Summary:
        #               Adds a new star to `skyline_star_list`. The new star is inserted at the head of the list.
        #
        #       Arguments:
        #               a0 - x position (uint16_t)
        #               a1 - y position (uint16_t)
        #               a2 - color (uint16_t)
        #               t1 - address of skyline_star_list
        #               t2 - the actual head node
        #               s1 - temp register for x pos
        #               s2 - temp register for y pos
        #               s3 - temp register for color
        #
        #       Return Value:
        #               None
        #
        #       Side Effects:
        #               - Modifies `skyline_star_list`
        #               - Allocates memory using `malloc`
        #
        #       Notes:
        #               - If `malloc` fails, function returns without modifying the list.
        #               - The new star’s `next` pointer is set to the previous head of `skyline_star_list`.
        #               - If the linked list is empty, the head pointer points to the new star
        #               - I used S registers for storage of passed in parameters not t register b/c malloc function uses t registers, changing them in the process.
        #               - S registers are callee saved, so they will remain the same for me.
        # ============================
               .global add_star
               .type   add_star, @function
        add_star:
                mv t1, zero
                mv t2, zero        
                addi sp, sp, -32                # allocate space on stack to callee save ra and s reg
                sd ra, 0(sp)
                sd s3, 8(sp)
                sd s2, 16(sp)
                sd s1, 24(sp)
                mv s1, a0
                mv s2, a1
                mv s3, a2
                li a0, 16                       #reserve space for malloc
                call malloc
                beq a0,zero, return_add_star    #if malloc fails, return
                sd zero, 0(a0)                  #set this star's next pointer to NULLstep
                sh s1, 8(a0) 
                sh s2, 10(a0)
                sh s3, 12(a0)  
                la t1, skyline_star_list 
                ld t2, 0(t1) 
                beq t2, zero, simple            # if list empty, make head points to new star
                sd t2, 0(a0) 
                sd a0, 0(t1) 
                j return_add_star               # finished adding at head, restore and return
        simple: sd a0, 0(t1) 
        return_add_star: ld ra, 0(sp)           # restore ra
                ld s1, 24(sp)
                ld s2, 16(sp)
                ld s3, 8(sp)
                addi sp, sp, 32                 # restore stack
                ret


        # ============================
        # Function: remove_star, void remove_star(uint16_t x, uint16_t y)
        # 
        #       Summary:
        #               Removes a star from `skyline_star_list` that matches the given (x, y) position. 
        #               If the head node is the star to be removed, we perform remove at head for typical linked list
        #               If not, we look ahead to the next star from the current star, and if the next star is the star to be removed
        #               we connect the next pointer of current to the current->next->next. We return if no such star is found.
        #       Arguments:
        #               a0 - x position of star to remove (uint16_t)
        #               a1 - y position of star to remove (uint16_t)
        #               t0 - next pointer of curr
        #               t1 - skyline_star_list address
        #               t2 - current star
        #               t3 - temp register for x and yes
        #               t4 - curr->next->next
        #       Return Value:
        #               None
        #
        #       Side Effects:
        #               - Modifies `skyline_star_list`
        #               - Frees memory using `free`
        #
        #       Notes:
        #               - If the star is not found, the list remains unchanged.
        #               - If the head star is removed, `skyline_star_list` is updated.
        # ============================
                .global remove_star
                .type   remove_star, @function
        remove_star:
                mv t0, zero 
                mv t1, zero 
                mv t2, zero 
                mv t3, zero
                la t1, skyline_star_list
                ld t2, 0(t1) 
                beq t2, zero, return_remove_star_final  #if list is empty, return direcly
                ld t0, 0(t2)                            # load the head star's next pointer into t0
                lh t3, 8(t2) 
                bne t3, a0, for_star                    # if t3 != a0(target star x pos), we skip this star
                lh t3, 10(t2) 
                bne t3, a1, for_star 
                sd t0, 0(t1)                            # make the linked list head pointer points to next
                mv a0, t2                               # set up a0 for parameter of free
                addi sp, sp, -8                         # reserve space for ra
                sd ra, 0(sp)
                call free
                j return_remove_star                    # found, restore and return
        for_star:       beq t0, zero, return_remove_star_final # if next pointer is NULL, we are at the end, return
                lh t3, 8(t0)
                bne t3, a0, skip 
                lh t3, 10(t0)   
                bne t3, a1, skip 
                ld t4, 0(t0) 
                sd t4, 0(t2)                            #current->next = current->next->next
                addi sp, sp, -8                         # allocate space on stack to callee save ra
                sd ra, 0(sp)
                mv a0, t0                               # pass target into a0
                call free
                j return_remove_star
        skip:   mv t2, t0                               # t2 = current->next
                ld t0, 0(t2)                            # t0 = curent->next->next
                j for_star 
        return_remove_star: ld ra, 0(sp) 
                addi sp, sp, 8                          # restore stack
        return_remove_star_final:        ret


        # ============================
        # Function: draw_star, void draw_star(uint16_t * fbuf, const struct skyline_star * star)
        # 
        #       Summary:
        #               Draws a star from onto the framebuffer. And only draws it if it is within the size of the frambuffer
        #
        #       Arguments:
        #               a0 - Pointer to framebuffer (`uint16_t *`)
        #               a1 - Pointer to the star (`skyline_star *`)
        #               t0 - temp register for star x, fbuf offset value, and final pixel pos
        #               t1 - temp register for star y
        #               t2 - temp register for color
        #               t3 - temp register for frambuffer's width and length
        #       
        #       Return Value:
        #               None
        #
        #       Side Effects:
        #               - Changes framebuffer memory
        #
        #       Notes:
        #               None
        # ============================
                .global draw_star
                .type draw_star, @function
        draw_star:
                mv t0, zero 
                mv t1, zero 
                mv t2, zero 
                mv t3, zero 
                lh t0, 8(a1) 
                lh t1, 10(a1) 
                lh t2, 12(a1)
                blt t0, zero, return_draw_star          #if 0> x pos, out of bound, return
                li t3, width_fbuf
                bge t0, t3, return_draw_star            #if t0>= 640, out of bound, return
                blt t1, zero, return_draw_star          #if 0> y pos, out of bound, return
                li t3, length_fbuf
                bge t1, t3, return_draw_star            #if t1>= 480, out of bound, return
                li t3, width_fbuf
                mul t1, t1, t3                          #frambuffer offset calculation
                add t0, t1, t0 
                slli t0, t0, 1 
                add t0, a0, t0                          # add to the framebuffer
                sh t2, 0(t0)                            #pixel value = target.color
        return_draw_star: ret


        # ============================
        # Function: add_window
        # 
        #       Summary:
        #               Adds a window to `skyline_windows` and updates `skyline_win_cnt`.
        #               We skip adding if window count exceed max window count.
        #               And we find the address for this window by multiplying struct window size with number of windows
        #
        #       Arguments:
        #               a0 - x position (uint16_t)
        #               a1 - y position (uint16_t)
        #               a2 - Width (uint8_t)
        #               a3 - Height (uint8_t)
        #               a4 - Color (uint16_t)
        #               t0 - address of skyline_win_cnt
        #               t1 - max_window_cnt
        #               t2 - skyline_windows_cnt
        #               t3 - size of window struct
        #               t4 - address of skyline_windows
        #
        #       Return Value:
        #               None
        #
        #       Side Effects:
        #               - Modifies `skyline_windows`
        #               - Increments `skyline_win_cnt`
        #
        #       Notes:
        #               None
        # ============================
                .global add_window
                .type add_window, @function
        add_window:
                mv t0, zero 
                mv t1, zero 
                mv t2, zero 
                mv t3, zero 
                mv t4, zero 
                la t0, skyline_win_cnt 
                lh t2, 0(t0)
                li t1, max_window_cnt
                bgeu t2, t1, return_add_window          #if excceeds max window count, return
                li t3, 8                                # 8 is the # of bytes of window struct
                mul t3, t2, t3                          #getting the correct pos for this window
                la t4, skyline_windows
                add t4, t4, t3                          #offsetting to the correct location for this window
                sh a0, 0(t4)
                sh a1, 2(t4)
                sb a2, 4(t4)
                sb a3, 5(t4)
                sh a4, 6(t4)
                addi t2, t2, 1                          # increment skyline_win_cnt
                sh t2, 0(t0)                            #store the count into the address of skyline_win_cnt
        return_add_window: ret


        # ============================
        # Function: remove_window
        # 
        #       Summary:
        #               Removes a window from `skyline_windows` and shift all windows forward to maintain the array to be continuous
        #               We also decrement windows count after removal
        #
        #       Arguments:
        #               a0 - x position of window to remove (uint16_t)
        #               a1 - y position of window to remove (uint16_t)
        #               t0 - address of skyline_win_cnt
        #               t1 - address of skyline_windows
        #               t2 - actual count for windows
        #               t3 - counter for the for loop of finding the correct window and counter for shifting everything up
        #               t4 - offset value and current window when shifting everything up
        #               t5 - size of window struct
        #               t6 - temp for x and y pos, later used as address of the next window when shifting everything up
        #               s1 - temp register used to shift every element up 
        #
        #       Return Value:
        #               None
        #
        #       Side Effects:
        #               - Modifies `skyline_windows`
        #               - Decrements `skyline_win_cnt`
        #
        #        Notes:
        #               - When shifting everything up, we evenually will clear the last window's address
        # ============================
                .global remove_window
                .type remove_window, @function
        remove_window:
                mv t0, zero 
                mv t1, zero  
                mv t2, zero 
                mv t3, zero
                mv t4, zero 
                mv t5, zero 
                mv t6, zero 
                addi sp, sp, -8                         # allocate space on stack to callee save s1
                sd s1, 0(sp)                            
                la t1, skyline_windows
                la t0, skyline_win_cnt
                lh t2, 0(t0)
                mv t3, zero
                li t5, 8                                #8 bytes for each window struct
        for_window: bgeu t3, t2, return_remove_window   # if counter >= max window, finished
                mul t4, t3, t5                          # cal offset
                add t4, t1, t4 
                lh t6, 0(t4)
                bne t6, a0, skip_remove_window          #if t6 = target.x, continue
                lh t6, 2(t4)   
                bne t6, a1, skip_remove_window          #if t6 = target.y, continue
        for_shift:      addi t3, t3, 1
                bgeu t3, t2, clean                      # check if we are the last window, if yes we clean up
                addi t6, t4, 8                          # get the next window's starting address
                lh s1, 0(t6)                            # shifting everything upwards
                sh s1, 0(t4)
                lh s1, 2(t6)
                sh s1, 2(t4)
                lb s1, 4(t6)
                sb s1, 4(t4)
                lb s1, 5(t6)
                sb s1, 5(t4)
                lh s1, 6(t6)
                sh s1, 6(t4)
                mv t4, t6                               # increment t4 to the next window
                j for_shift
        clean:  addi t3, t3, -1                         # minus one b/c we added one in for_shift which cause us to be one over the array
                mul t3, t3, t5
                add t3, t1, t3                          # cal offset address for last window and cleaning everything
                sh zero, 0(t3)
                sh zero, 2(t3)
                sb zero, 4(t3)
                sb zero, 5(t3)
                sh zero, 6(t3)
                addi t2, t2, -1                         # decrement skyline_windows_cnt
                sh t2, 0(t0)
                j return_remove_window
        skip_remove_window:   add t3, t3, 1
                j for_window
        return_remove_window: ld s1, 0(sp)              # restore s1
                addi sp, sp, 8                          # restore stack
                ret


        # ============================
        # Function: draw_window
        # 
        #       Summary:
        #               Draws a window onto the framebuffer.
        #               We will draw parts of the window that is in the frambuffer even if some parts is not
        #               We do so by looping over each row and column pixel and check if it is in bound
        #
        #       Arguments:
        #               a0 - Pointer to framebuffer (`uint16_t *`)
        #               a1 - Pointer to window (`skyline_window *`)
        #               t0 - stores x pos
        #               t1 - stores y pos
        #               t2 - stores width of window
        #               t3 - stores Height of window
        #               t4 - stores color of window
        #               t5 - width_fbuf
        #               t6 - length_fbuf
        #               s1 - counter for row loop
        #               s2 - temp register for x + row counter
        #               s3 - counter for column loop
        #               s4 - temp register for y + column counter, and for calculating offset and final pixel position
        #
        #       Return Value:
        #               None
        #
        #       Side Effects:
        #               - Modifies framebuffer memory
        #
        #       Notes:
        #               - None
        # ============================
                .global draw_window
                .type draw_window, @function
        draw_window:
                lh t0, 0(a1) 
                lh t1, 2(a1) 
                lb t2, 4(a1) 
                lb t3, 5(a1) 
                lh t4, 6(a1)
                li t5, width_fbuf
                li t6, length_fbuf
                bge zero, t2, return_draw_window        # check if w is negative, return if true
                bge zero, t3, return_draw_window        # check if h is negative, return if true
                addi sp, sp, -32                        # reserve place on stack to callee save s registers
                sd s4, 0(sp)
                sd s3, 8(sp)
                sd s1, 24(sp)
                sd s2, 16(sp)
                mv s2, zero 
                mv s1, zero 
                mv s3, zero 
                mv s4, zero
        row_loop_dw:    bgeu s1, t2, return_draw_window # if counter >= width, restore and return
                add s2, s1, t0                          # calcuate the new x pos 
                bge s2, t5, end_row_dw                  # check for out of bound
                blt s2, zero, end_row_dw 
        col_loop_dw:       bgeu s3, t3, end_row_dw      # if col counter >= column, end column loop
                add s4, t1, s3                          # calculate the new y pos
                bge s4, t6, end_col_dw                  #check for out of bound
                blt s4, zero, end_col_dw 
                mul s4, s4, t5                          # cal offset of screen buffer
                add s4, s4, s2 
                slli s4, s4, 1 
                add s4, a0, s4
                sh t4, 0(s4)                            #pixel value = target.color
        end_col_dw:        mv s4, zero                  # cleaning offset for next loop
                addi s3, s3, 1                          # increment the col counter
                j col_loop_dw 
        end_row_dw:        mv s2, zero                  
                mv s3, zero                             # cleaning col loop counter
                addi s1,s1,1                            # increment the row counter
                j row_loop_dw   
        return_draw_window:         ld s4, 0(sp)        # store the s registers back to their oringial value
                ld s3, 8(sp)
                ld s2, 16(sp)
                ld s1, 24(sp)
                addi sp, sp, 32
                ret

                
        # ============================
        # Function: start_beacon, void start_beacon (const uint16_t * img,uint16_t x,uint16_t y,uint8_t dia,uint16_t period,uint16_t ontime);
        # 
        #       Summary:
        #               Initializes a beacon with the given properties and stores it in `skyline_beacon`.
        #
        #       Arguments:
        #               a0 - Pointer to beacon image data (`uint16_t *`)
        #               a1 - x position (uint16_t)
        #               a2 - y position (uint16_t)
        #               a3 - Diameter (uint8_t)
        #               a4 - Period (uint16_t)
        #               a5 - On-time (uint16_t)
        #               t0 - skyline_beacon address
        #       
        #       Return Value:
        #               None
        #
        #       Side Effects:
        #               - Modifies `skyline_beacon`
        #
        #       Notes:
        #               None
        # ============================
                .global start_beacon
                .type start_beacon, @function
        start_beacon:
                la t0, skyline_beacon                   
                sd a0, 0(t0)                    
                sh a1, 8(t0) 
                sh a2, 10(t0) 
                sb a3, 12(t0) 
                sh a4, 14(t0) 
                sh a5, 16(t0) 
                ret


        # ============================
        # Function: draw_beacon, void draw_beacon (uint16_t * fbuf,uint64_t t,const struct skyline_beacon * bcn);
        # 
        #       Summary:
        #               Draws a beacon onto the framebuffer if it is in the supposed "on-time" period. 
        #               We still draws the beacon even if some parts is out of bound, we will just draw the parts that is in the bound
        #               We draw by looping over the row and column of the beacon and check if out of bound for each pixel
        #               We Grab the color information from the img pointer by calculating the corresponding offset
        #       Arguments:
        #               a0 - Pointer to framebuffer (`uint16_t *`)
        #               a1 - Current time (uint64_t)
        #               a2 - Pointer to beacon (`skyline_beacon *`)
        #               t0 - stores img pointer
        #               t1 - stores star x pos
        #               t2 - stores star y pos
        #               t3 - stores star dia
        #               t4 - stores star period
        #               t5 - stores star ontime
        #               t6 - temp register to determine if beacon on or off
        #               s1 - counter for row loop
        #               s2 - temp register for x pos + dia
        #               s3 - counter for column loop
        #               s4 - temp register for y pos + dia
        #               s5 - width_fbuf
        #               s6 - length_fbuf
        #               s7 - temp register for img pointer offset and stores pixel color
        #       
        #       Return Value:
        #               None
        #
        #       Side Effects:
        #               - Modifies framebuffer memory
        #
        #       Notes:
        #               - If `current_time % period >= ontime`, the beacon is off.
        #               - Handles cases where the beacon is partially out of bounds.
        # ============================
                .global draw_beacon
                .type draw_beacon, @function
        draw_beacon:
                ld t0, 0(a2)
                lh t1, 8(a2)
                lh t2, 10(a2)
                lb t3, 12(a2)
                lh t4, 14(a2)
                lh t5, 16(a2)
                mv t6, zero 
                remu t6, a1, t4                 # t % bcn->period
                bge t6, t5, return_db_final     #if t6 >= ontime, return with no restoring
                addi sp, sp, -56                # reserve place on stack to callee save s registers
                sd s6, 8(sp)
                sd s5, 16(sp) 
                sd s4, 24(sp) 
                sd s3, 32(sp) 
                sd s1, 48(sp) 
                sd s2, 40(sp) 
                sd s7, 0(sp) 
                mv s2, zero 
                mv s1, zero 
                mv s3, zero 
                mv s4, zero
                mv s7, zero
                li s5, width_fbuf
                li s6, length_fbuf
        row_loop_db:    bgeu s1, t3, return_db  # if counter >= dia, return and restore
                add s2, s1, t1                  # calcuate the new x pos 
                bge s2, s5, end_row_db         # check for out of bound
                blt s2, zero, end_row_db 
        col_loop_db:    bgeu s3, t3, end_row_db # if counter >= dia, end inner for loop
                add s4, t2, s3                  # calculate the new y pos
                bge s4, s6, end_col_db         #check for out of bound
                blt s4, zero, end_col_db 
                mul s7, s3, t3                  # *img offset
                add s7, s7, s1 
                slli s7, s7, 1 
                add s7, t0, s7 
                lh s7, 0(s7)                    # store the color information
                mul s4, s4, s5                  # fbuf offset
                add s4, s4, s2 
                slli s4, s4, 1 
                add s4, a0, s4
                sh s7, 0(s4)                    #pixel value = target.color
        end_col_db:     mv s4, zero             # cleaning temp for next loop
                mv s7, zero
                addi s3, s3, 1                  # increment col counter
                j col_loop_db 
        end_row_db:     mv s2, zero             # cleaning temp for outer loop
                mv s3, zero                     # clean col loop counter
                addi s1,s1,1                    # increment the row counter
                j row_loop_db
        return_db: ld s7, 0(sp)                 # restore registers and stack
                ld s6, 8(sp)
                ld s5, 16(sp)
                ld s4, 24(sp)
                ld s3, 32(sp)
                ld s2, 40(sp)
                ld s1, 48(sp)
                addi sp, sp, 56
        return_db_final:        ret

        .end
