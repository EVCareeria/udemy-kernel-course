#include "kernel.h"
#include <stdint.h>
#include <stddef.h>
#include "idt/idt.h"
#include "fs/file.h"
#include "io/io.h"
#include "memory/heap/kheap.h"
#include "memory/paging/paging.h"
#include "string/string.h"
#include "disk/disk.h"
#include "isr80h/isr80h.h"
#include "fs/pparser.h"
#include "disk/streamer.h"
#include "gdt/gdt.h"
#include "config.h"
#include "memory/memory.h"
#include "task/tss.h"
#include "task/task.h"
#include "task/process.h"
#include "status.h"
#include "keyboard/keyboard.h"

uint16_t* video_mem = 0;
uint16_t terminal_row = 0;
uint16_t terminal_col = 0;

uint16_t terminal_make_char(char c, char colour)
{
    return (colour << 8) | c;
}

void terminal_putchar(int x, int y, char c, char colour)
{
    video_mem[(y * VGA_WIDTH) + x] = terminal_make_char(c, colour);
}

void terminal_writechar(char c, char colour)
{
    if (c == '\n')
    {
        terminal_row += 1;
        terminal_col = 0;
        return;
    }

terminal_putchar(terminal_col, terminal_row, c, colour);
    terminal_col += 1;
    if (terminal_col >= VGA_WIDTH)
    {
        terminal_col = 0;
        terminal_row += 1;
    }
}

void terminal_initialize()
{
    video_mem = (uint16_t*)(0xB8000);
    for (int y =0; y < VGA_HEIGHT; y++)
    {
        for (int x = 0; x < VGA_WIDTH; x++)
         {
            terminal_putchar(x, y, ' ', 0);
         }
    }
}

void print(const char* str)
{
    size_t len = strlen(str);
    for(int i = 0; i < len; i++)
    {
        terminal_writechar(str[i], 15);
    }
}

static struct paging_4gb_chunk* kernel_chunk = 0;

void panic(const char* msg)
{
    print(msg);
    while(1) {}
}
void kernel_page()
{
    kernel_registers();
    paging_switch(kernel_chunk);
}

struct tss tss;
struct gdt gdt_real[PEACHOS_TOTAL_GDT_SEGMENTS];
struct gdt_structured gdt_structured[PEACHOS_TOTAL_GDT_SEGMENTS] = {
    {.base = 0x00, .limit = 0x00, .type = 0x00},     // NULL segment
    {.base = 0x00, .limit = 0xffffffff, .type = 0x9a},  // Kernel code segment
    {.base = 0x00, .limit = 0xffffffff, .type = 0x92},   // Kernel Data segment
    {.base = 0x00, .limit = 0xffffffff, .type = 0xf8},  // User code segment
    {.base = 0x00, .limit = 0xffffffff, .type = 0xf2},  // User data segment
    {.base = (uint32_t)&tss, .limit = sizeof(tss), .type = 0xE9}     // TSS segment
};

// For testing interrupt handler
void pic_timer_callback(struct interrupt_frame* frame)
{
    print("Timer activated\n");
}
void kernel_main()
{
    terminal_initialize();
    memset(gdt_real, 0x00, sizeof(gdt_real));
    gdt_structured_to_gdt(gdt_real, gdt_structured, PEACHOS_TOTAL_GDT_SEGMENTS);

    gdt_load(gdt_real, sizeof(gdt_real));

    // Initialize the heap
    kheap_init();

    // Initialize filesystems
    fs_init();

    // Search and initialize the disk
    disk_search_and_init();

    // Initialize the interrupt descriptor table
    idt_init();

    // Setup tss
    memset(&tss, 0x00, sizeof(tss));
    tss.esp0 = 0x600000;
    tss.ss0 = KERNEL_DATA_SELECTOR;

    tss_load(0x28);

    // Setup paging
    kernel_chunk = paging_new_4gb(PAGING_IS_WRITABLE | PAGING_IS_PRESENT | PAGING_ACCESS_FROM_ALL);

    // Switch to kernel paging chunk
    paging_switch(kernel_chunk);

    // Enable paging
    enable_paging();

    // Registering the kernel commands
    isr80h_register_commands();

    keyboard_init();

    // Testing purposes
    idt_register_interrupt_callback(0x20, pic_timer_callback);

    struct process* process = 0;
    int res = process_load("0:/blank.bin", &process);
    if (res != PEACHOS_ALL_OK) {
        panic("Failed to load blank.bin\n");
    }

    task_run_first_ever_task();

    // Enable the system interrupts
    // enable_interrupts();
    
    while(1) {}
}
