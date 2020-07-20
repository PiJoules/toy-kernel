#ifndef SCHEDULER_H
#define SCHEDULER_H

// TODO: Merge this and kthread.h.

#include <isr.h>
#include <kthread.h>

void init_scheduler();
void thread_is_ready(thread_t *t);
void schedule(const registers_t *regs);
void DestroyScheduler();

#endif
