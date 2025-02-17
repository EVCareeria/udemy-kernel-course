#ifndef PROCESS_H
#define PROCESS_H
#include <stdint.h>
#include "config.h"

struct process 
{
    uint16_t id;                // The process id
    char filename[PEACHOS_MAX_PATH];

    struct task* task;          // The main process task

    void* allocations[PEACHOS_MAX_PROGRAM_ALLOCATIONS];     // Memory (malloc) allocations of the process

    void* ptr;                  // The physical pointer to the process memory

    void* stack;                // The physical pointer to the stack memory

    uint32_t size;              // The size of the data pointed to by "ptr"

    struct keyboard_buffer
    {
        char buffer[PEACHOS_KEYBOARD_BUFFER_SIZE];
        int tail;
        int head;
    } keyboard;
};

int process_load_for_slot(const char* filename, struct process** process, int process_slot);
int process_load(const char* filename, struct process** process);
struct process* process_current();
struct process* process_get(int process_id);

#endif