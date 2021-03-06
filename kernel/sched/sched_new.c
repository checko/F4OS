#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <list.h>
#include <mm/mm.h>
#include <dev/resource.h>
#include <kernel/fault.h>

#include <kernel/sched.h>
#include "sched_internals.h"

static task_ctrl *create_task(void (*fptr)(void), uint8_t priority, uint32_t period) __attribute__((section(".kernel")));
static int register_task(task_ctrl *task_ptr, int periodic) __attribute__((section(".kernel")));

struct list runnable_task_list = INIT_LIST(runnable_task_list);
struct list periodic_task_list = INIT_LIST(periodic_task_list);

DEFINE_INSERT_TASK_FUNC(runnable_task_list);
DEFINE_INSERT_TASK_FUNC(periodic_task_list);

volatile uint32_t total_tasks = 0;

task_t *new_task(void (*fptr)(void), uint8_t priority, uint32_t period) {
    task_ctrl *task = create_task(fptr, priority, period);
    if (task == NULL) {
        goto fail;
    }

    int ret = register_task(task, period);
    if (ret != 0) {
        goto fail2;
    }

    total_tasks += 1;

    return get_task_t(task);

fail2:
    free(task->stack_limit);
    kfree(task);
fail:
    panic_print("Could not allocate task with function pointer 0x%x", fptr);
}

static task_ctrl *create_task(void (*fptr)(void), uint8_t priority, uint32_t period) {
    task_ctrl *task;
    uint32_t *memory;
    static uint32_t pid_source = 1;
    task = (task_ctrl *) kmalloc(sizeof(task_ctrl));
    if (task == NULL) {
        return NULL;
    }

    memory = (uint32_t *) malloc(STKSIZE*4);
    if (memory == NULL) {
        kfree(task);
        return NULL;
    }

    task->stack_limit       = memory;
    task->stack_base        = memory + STKSIZE;
    task->stack_top         = memory + STKSIZE;
    task->fptr              = fptr;
    task->priority          = priority;
    task->running           = 0;
    task->abort             = 0;

    task->period            = period;
    task->ticks_until_wake  = period;
    task->pid               = pid_source++;

    list_init(&task->runnable_task_list);
    list_init(&task->periodic_task_list);
    list_init(&task->free_task_list);

    generic_task_setup(get_task_t(task));

    return task;
}

static int register_task(task_ctrl *task, int periodic) {
    /* When task switching, we cannot safely modify the task lists
     * ourselves, instead we must ask the OS to do so for us. */
    if (task_switching) {
        SVC_ARG2(SVC_REGISTER_TASK, task, periodic);
    }
    else {
        svc_register_task(task, periodic);
    }

    return 0;
}

void svc_register_task(task_ctrl *task, int periodic) {
    insert_task(runnable_task_list, task);

    if (periodic) {
        insert_task(periodic_task_list, task);
    }
}

void create_context(task_ctrl* task, void (*lptr)(void)) {
    asm volatile("stmdb   %[stack]!, {%[zero]}  /* Empty */                     \n\
                  stmdb   %[stack]!, {%[zero]}  /* FPSCR */                     \n\
                  stmdb   %[stack]!, {%[zero]}  /* S15 */                       \n\
                  stmdb   %[stack]!, {%[zero]}  /* S14 */                       \n\
                  stmdb   %[stack]!, {%[zero]}  /* S13 */                       \n\
                  stmdb   %[stack]!, {%[zero]}  /* S12 */                       \n\
                  stmdb   %[stack]!, {%[zero]}  /* S11 */                       \n\
                  stmdb   %[stack]!, {%[zero]}  /* S10 */                       \n\
                  stmdb   %[stack]!, {%[zero]}  /* S9 */                        \n\
                  stmdb   %[stack]!, {%[zero]}  /* S8 */                        \n\
                  stmdb   %[stack]!, {%[zero]}  /* S7 */                        \n\
                  stmdb   %[stack]!, {%[zero]}  /* S6 */                        \n\
                  stmdb   %[stack]!, {%[zero]}  /* S5 */                        \n\
                  stmdb   %[stack]!, {%[zero]}  /* S4 */                        \n\
                  stmdb   %[stack]!, {%[zero]}  /* S3 */                        \n\
                  stmdb   %[stack]!, {%[zero]}  /* S2 */                        \n\
                  stmdb   %[stack]!, {%[zero]}  /* S1 */                        \n\
                  stmdb   %[stack]!, {%[zero]}  /* S0 */                        \n\
                  stmdb   %[stack]!, {%[psr]}   /* xPSR */                      \n\
                  stmdb   %[stack]!, {%[pc]}    /* PC */                        \n\
                  stmdb   %[stack]!, {%[lr]}    /* LR */                        \n\
                  stmdb   %[stack]!, {%[zero]}  /* R12 */                       \n\
                  stmdb   %[stack]!, {%[zero]}  /* R3 */                        \n\
                  stmdb   %[stack]!, {%[zero]}  /* R2 */                        \n\
                  stmdb   %[stack]!, {%[zero]}  /* R1 */                        \n\
                  stmdb   %[stack]!, {%[zero]}  /* R0 */                        \n\
                  stmdb   %[stack]!, {%[zero]}  /* R11 */                       \n\
                  stmdb   %[stack]!, {%[zero]}  /* R10 */                       \n\
                  stmdb   %[stack]!, {%[zero]}  /* R9 */                        \n\
                  stmdb   %[stack]!, {%[zero]}  /* R8 */                        \n\
                  stmdb   %[stack]!, {%[frame]} /* R7 - Frame Pointer*/         \n\
                  stmdb   %[stack]!, {%[zero]}  /* R6 */                        \n\
                  stmdb   %[stack]!, {%[zero]}  /* R5 */                        \n\
                  stmdb   %[stack]!, {%[zero]}  /* R4 */                        \n\
                  stmdb   %[stack]!, {%[zero]}  /* S31 */                       \n\
                  stmdb   %[stack]!, {%[zero]}  /* S30 */                       \n\
                  stmdb   %[stack]!, {%[zero]}  /* S29 */                       \n\
                  stmdb   %[stack]!, {%[zero]}  /* S28 */                       \n\
                  stmdb   %[stack]!, {%[zero]}  /* S27 */                       \n\
                  stmdb   %[stack]!, {%[zero]}  /* S26 */                       \n\
                  stmdb   %[stack]!, {%[zero]}  /* S25 */                       \n\
                  stmdb   %[stack]!, {%[zero]}  /* S24 */                       \n\
                  stmdb   %[stack]!, {%[zero]}  /* S23 */                       \n\
                  stmdb   %[stack]!, {%[zero]}  /* S22 */                       \n\
                  stmdb   %[stack]!, {%[zero]}  /* S21 */                       \n\
                  stmdb   %[stack]!, {%[zero]}  /* S20 */                       \n\
                  stmdb   %[stack]!, {%[zero]}  /* S19 */                       \n\
                  stmdb   %[stack]!, {%[zero]}  /* S18 */                       \n\
                  stmdb   %[stack]!, {%[zero]}  /* S17 */                       \n\
                  stmdb   %[stack]!, {%[zero]}  /* S16 */"
                  /* Output */
                  :[stack] "+r" (task->stack_top)
                  /* Input */
                  :[pc] "r" (task->fptr), [lr] "r" (lptr), [frame] "r" (task->stack_limit),
                   [zero] "r" (0), [psr] "r" (0x01000000) /* Set the Thumb bit */
                  /* Clobber */
                  :);
}
