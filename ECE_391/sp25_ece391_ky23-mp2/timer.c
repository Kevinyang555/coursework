// timer.c
//
// Copyright (c) 2024-2025 University of Illinois
// SPDX-License-identifier: NCSA
//

#ifdef TIMER_TRACE
#define TRACE
#endif

#ifdef TIMER_DEBUG
#define DEBUG
#endif

#include "timer.h"
#include "thread.h"
#include "riscv.h"
#include "assert.h"
#include "intr.h"
#include "conf.h"
#include "see.h" // for set_stcmp

// EXPORTED GLOBAL VARIABLE DEFINITIONS
// 

char timer_initialized = 0;

// INTERNVAL GLOBAL VARIABLE DEFINITIONS
//

static struct alarm * sleep_list;

// INTERNAL FUNCTION DECLARATIONS
//

// EXPORTED FUNCTION DEFINITIONS
//
extern void enable_timer_interrupt(void);
extern void disable_timer_interrupt(void);


void timer_init(void) {
    set_stcmp(UINT64_MAX);
    timer_initialized = 1;
}



//============================
//Function: alarm_init
//      
//       Summary:
//              This function initalize components of alarm struct
//
//       Arguments:
//              -The new alarm struct
//              -Optional Name
//
//       Return Value:
//               NONE
//
//       Side Effects:
//              -We set name and next alarm 
//              -We initalize twake to current so we can make it future time in alarm sleep
//              -We initalize condition struct
//
//       Notes:
//              -If passed in alarm does not exist we return
// ============================
void alarm_init(struct alarm * al, const char * name) {
    // FIXME your code goes here
    if(al == NULL){
        return;
    }
    al->cond.name = name;
    al->next = NULL;
    al->twake = rdtime();
    condition_init(&al->cond, "alarm conditon");

}

//============================
//Function: alarm_sleep
//      
//       Summary:
//              This function set the wake up time an alarm to the corresponding tcnt and add it into the correct position in sleep list
//
//       Arguments:
//              -The alarm struct
//              -How long into the future we want to wake our alarm
//
//       Return Value:
//               NONE
//
//       Side Effects:
//              -We set twake
//              -We will modify mtimecmp according to where we place the new alarm
//              -We puts the new alarm to sleep with condition wait
//              -We will enable timer interrupt and disable all interrupt when condition wait and restore afterwards
//
//       Notes:
//              -If twake time is in the past, we ignore this alarm and return
// ============================
void alarm_sleep(struct alarm * al, unsigned long long tcnt) {
    unsigned long long now;
    //struct alarm * prev;
    int pie;

    now = rdtime();

    // If the tcnt is so large it wraps around, set it to UINT64_MAX

    if (UINT64_MAX - al->twake < tcnt)
        al->twake = UINT64_MAX;
    else
        al->twake += tcnt;
    
    // If the wake-up time has already passed, return

    if (al->twake < now)
        return;

    // FIXME your code goes here
    disable_timer_interrupt();//disable timer interrupt while changing the sleep list
    if(sleep_list == NULL){ //if nothing on the list, new alarm becomes the head
        sleep_list = al;
        al->next = NULL;
        set_stcmp(sleep_list->twake);//set mtimecmp since we are earliest to be wake up
    }else{
        if(sleep_list->twake>al->twake){//if our wake up time is sooner than the first element of the list
            al->next = sleep_list;
            sleep_list = al;
            set_stcmp(sleep_list->twake);//set mtimecmp since we are earliest to be wake up 
        }else{
            struct alarm *list = sleep_list;
            while(list->next != NULL &&list->next->twake <=al->twake) { //find the correct spot in the list
                
                list = list->next;
            }
            al->next = list->next;
            list->next = al;

        }
    }
    pie = disable_interrupts(); //disable interrupts before condition wait
    enable_timer_interrupt(); //enable timer interrupts
    condition_wait(&al->cond);
    restore_interrupts(pie);
    

}

// Resets the alarm so that the next sleep increment is relative to the time
// alarm_reset is called.

void alarm_reset(struct alarm * al) {
    al->twake = rdtime();
}

void alarm_sleep_sec(struct alarm * al, unsigned int sec) {
    alarm_sleep(al, sec * TIMER_FREQ);
}

void alarm_sleep_ms(struct alarm * al, unsigned long ms) {
    alarm_sleep(al, ms * (TIMER_FREQ / 1000));
}

void alarm_sleep_us(struct alarm * al, unsigned long us) {
    alarm_sleep(al, us * (TIMER_FREQ / 1000 / 1000));
}

void sleep_sec(unsigned int sec) {
    sleep_ms(1000UL * sec);
}

void sleep_ms(unsigned long ms) {
    sleep_us(1000UL * ms);
}

void sleep_us(unsigned long us) {
    struct alarm al;

    alarm_init(&al, "sleep");
    alarm_sleep_us(&al, us);
}

// handle_timer_interrupt() is dispatched from intr_handler in intr.c


//============================
//Function: handle_timer_interrupt
//      
//       Summary:
//              -This function wakes up alarms whose twake is pasted
//
//       Arguments:
//              -NONE
//
//       Return Value:
//               NONE
//
//       Side Effects:
//              -We will delete the woke up alarm from the sleep list
//              -We will modify mtimecmp to the new sleep list head
//              -We broadcast alarms to wake up
//              -We will disable all interrupt when changing the sleep list
//
//       Notes:
//              -If nothing on the sleep list at the end we disable timer interrupt
// ============================

void handle_timer_interrupt(void) {
    struct alarm * head = sleep_list;
    struct alarm * next;
    uint64_t now;

    now = rdtime();

    trace("[%lu] %s()", now, __func__);
    debug("[%lu] mtcmp = %lu", now, rdtime());

    // FIXME your code goes here
    while(head != NULL &&head->twake <=now){
        next = head->next;
        condition_broadcast(&head->cond);
        head = next;
    }
    int pie = disable_interrupts();
    sleep_list = head;
    restore_interrupts(pie);
    if(sleep_list != NULL){
        set_stcmp(sleep_list->twake);
    }else{
        set_stcmp(UINT64_MAX);
        disable_timer_interrupt();
    }
}