#include <descriptortables.h>
#include <io.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

// Initialises the GDT and IDT, and defines the default ISR and IRQ handler.
// Based on code from Bran's kernel development tutorials.
// Rewritten for JamesM's kernel development tutorials.

constexpr uint8_t kDPLUser = 0x60;

constexpr size_t kNumGDTEntries = 6;
gdt_entry_t gdt_entries[kNumGDTEntries];
gdt_ptr_t gdt_ptr;
idt_entry_t idt_entries[256];
idt_ptr_t idt_ptr;

extern "C" {
// Lets us access our ASM functions from our C code.
extern void GDTFlush(uint32_t);
extern void IDTFlush(uint32_t);
extern void TSSFlush();

// These extern directives let us access the addresses of our ASM ISR handlers.
extern void isr0();
extern void isr1();
extern void isr2();
extern void isr3();
extern void isr4();
extern void isr5();
extern void isr6();
extern void isr7();
extern void isr8();
extern void isr9();
extern void isr10();
extern void isr11();
extern void isr12();
extern void isr13();
extern void isr14();
extern void isr15();
extern void isr16();
extern void isr17();
extern void isr18();
extern void isr19();
extern void isr20();
extern void isr21();
extern void isr22();
extern void isr23();
extern void isr24();
extern void isr25();
extern void isr26();
extern void isr27();
extern void isr28();
extern void isr29();
extern void isr30();
extern void isr31();

extern void irq0();
extern void irq1();
extern void irq2();
extern void irq3();
extern void irq4();
extern void irq5();
extern void irq6();
extern void irq7();
extern void irq8();
extern void irq9();
extern void irq10();
extern void irq11();
extern void irq12();
extern void irq13();
extern void irq14();
extern void irq15();
extern void isr128();
}

namespace {

constexpr uint8_t kPicMasterCmd = 0x20;
constexpr uint8_t kPIcMasterData = 0x21;
constexpr uint8_t kPicSlaveCmd = 0xA0;
constexpr uint8_t kPicSlaveData = 0xA1;

// A struct describing a Task State Segment.
struct tss_entry_t {
  uint32_t prev_tss;  // The previous TSS - if we used hardware task switching
                      // this would form a linked list.
  uint32_t esp0;  // The stack pointer to load when we change to kernel mode.
  uint32_t ss0;   // The stack segment to load when we change to kernel mode.
  uint32_t esp1;  // Unused...
  uint32_t ss1;
  uint32_t esp2;
  uint32_t ss2;
  uint32_t cr3;
  uint32_t eip;
  uint32_t eflags;
  uint32_t eax;
  uint32_t ecx;
  uint32_t edx;
  uint32_t ebx;
  uint32_t esp;
  uint32_t ebp;
  uint32_t esi;
  uint32_t edi;
  uint32_t es;   // The value to load into ES when we change to kernel mode.
  uint32_t cs;   // The value to load into CS when we change to kernel mode.
  uint32_t ss;   // The value to load into SS when we change to kernel mode.
  uint32_t ds;   // The value to load into DS when we change to kernel mode.
  uint32_t fs;   // The value to load into FS when we change to kernel mode.
  uint32_t gs;   // The value to load into GS when we change to kernel mode.
  uint32_t ldt;  // Unused...
  uint16_t trap;
  uint16_t iomap_base;
} __attribute__((packed));
static_assert(sizeof(tss_entry_t) == 104, "");

tss_entry_t tss_entry;

// Set the value of one GDT entry.
void GDTSetGate(int32_t num, uint32_t base, uint32_t limit, uint8_t access,
                uint8_t gran) {
  gdt_entries[num].base_low = (base & 0xFFFF);
  gdt_entries[num].base_middle = (base >> 16) & 0xFF;
  gdt_entries[num].base_high = (base >> 24) & 0xFF;

  gdt_entries[num].limit_low = (limit & 0xFFFF);
  gdt_entries[num].granularity = (limit >> 16) & 0x0F;

  gdt_entries[num].granularity |= gran & 0xF0;
  gdt_entries[num].access = access;
}

void WriteTSS(int32_t num, uint16_t ss0, uint32_t esp0) {
  // Firstly, let's compute the base and limit of our entry into the GDT.
  uint32_t base = reinterpret_cast<uint32_t>(&tss_entry);
  uint32_t limit = sizeof(tss_entry);

  // Ensure the descriptor is initially zero.
  // Note that this sets the I/O privilege level in `eflags` to zero also,
  // preventing I/O instructions from being used outside of ring 0.
  memset(&tss_entry, 0, sizeof(tss_entry));

  tss_entry.ss0 = ss0;    // Set the kernel stack segment.
  tss_entry.esp0 = esp0;  // Set the kernel stack pointer.

  // Disable usage of the I/O privilege bitmap which can allow some ports to be
  // accessible outside of ring 0. We do this by setting the I/O map base to be
  // the same as the TSS limit.
  // https://cs.nyu.edu/~mwalfish/classes/15fa/ref/i386/s08_03.htm
  tss_entry.iomap_base = sizeof(tss_entry);

  // Here we set the cs, ss, ds, es, fs and gs entries in the TSS. These specify
  // what segments should be loaded when the processor switches to kernel mode.
  // Therefore they are just our normal kernel code/data segments - 0x08 and
  // 0x10 respectively, but with the last two bits set, making 0x0b and 0x13.
  // The setting of these bits sets the RPL (requested privilege level) to 3,
  // meaning that this TSS can be used to switch to kernel mode from ring 3.
  tss_entry.cs = 0x0b;
  tss_entry.ss = tss_entry.ds = tss_entry.es = tss_entry.fs = tss_entry.gs =
      0x13;

  // Now, add our TSS descriptor's address to the GDT.
  GDTSetGate(num, base, limit, 0xE9, 0x00);
}

// Internal function prototypes.
void InitGDT() {
  gdt_ptr.limit = (sizeof(gdt_entry_t) * kNumGDTEntries) - 1;
  gdt_ptr.base = reinterpret_cast<uint32_t>(&gdt_entries);

  GDTSetGate(0, 0, 0, 0, 0);                 // Null segment (0x00)
  GDTSetGate(1, 0, 0xFFFFFFFF, 0x9A, 0xCF);  // Code segment (0x08)
  GDTSetGate(2, 0, 0xFFFFFFFF, 0x92, 0xCF);  // Data segment (0x10)
  GDTSetGate(3, 0, 0xFFFFFFFF, 0xFA, 0xCF);  // User mode code segment (0x18)
  GDTSetGate(4, 0, 0xFFFFFFFF, 0xF2, 0xCF);  // User mode data segment (0x20)
  WriteTSS(5, 0x10, 0);

  GDTFlush(reinterpret_cast<uint32_t>(&gdt_ptr));
  TSSFlush();
}

void IDTSetGate(uint8_t num, uint32_t base, uint16_t sel, uint8_t flags) {
  idt_entries[num].base_lo = base & 0xFFFF;
  idt_entries[num].base_hi = (base >> 16) & 0xFFFF;

  idt_entries[num].sel = sel;
  idt_entries[num].always0 = 0;
  idt_entries[num].flags = flags;
}

void InitIDT() {
  idt_ptr.limit = sizeof(idt_entry_t) * 256 - 1;
  idt_ptr.base = reinterpret_cast<uint32_t>(&idt_entries);

  memset(&idt_entries, 0, sizeof(idt_entry_t) * 256);

  Write8(kPicMasterCmd, 0x11);
  Write8(kPicSlaveCmd, 0x11);
  Write8(kPIcMasterData, 0x20);
  Write8(kPicSlaveData, 0x28);
  Write8(kPIcMasterData, 0x04);
  Write8(kPicSlaveData, 0x02);
  Write8(kPIcMasterData, 0x01);
  Write8(kPicSlaveData, 0x01);
  Write8(kPIcMasterData, 0x00);
  Write8(kPicSlaveData, 0x00);

  // ISRs (raised by CPU)
  IDTSetGate(0, reinterpret_cast<uint32_t>(isr0), 0x08, 0x8E);
  IDTSetGate(1, reinterpret_cast<uint32_t>(isr1), 0x08, 0x8E);
  IDTSetGate(2, reinterpret_cast<uint32_t>(isr2), 0x08, 0x8E);
  IDTSetGate(3, reinterpret_cast<uint32_t>(isr3), 0x08, 0x8E);
  IDTSetGate(4, reinterpret_cast<uint32_t>(isr4), 0x08, 0x8E);
  IDTSetGate(5, reinterpret_cast<uint32_t>(isr5), 0x08, 0x8E);
  IDTSetGate(6, reinterpret_cast<uint32_t>(isr6), 0x08, 0x8E);
  IDTSetGate(7, reinterpret_cast<uint32_t>(isr7), 0x08, 0x8E);
  IDTSetGate(8, reinterpret_cast<uint32_t>(isr8), 0x08, 0x8e);
  IDTSetGate(9, reinterpret_cast<uint32_t>(isr9), 0x08, 0x8e);
  IDTSetGate(10, reinterpret_cast<uint32_t>(isr10), 0x08, 0x8E);
  IDTSetGate(11, reinterpret_cast<uint32_t>(isr11), 0x08, 0x8E);
  IDTSetGate(12, reinterpret_cast<uint32_t>(isr12), 0x08, 0x8E);
  IDTSetGate(13, reinterpret_cast<uint32_t>(isr13), 0x08, 0x8E);
  IDTSetGate(14, reinterpret_cast<uint32_t>(isr14), 0x08, 0x8E);
  IDTSetGate(15, reinterpret_cast<uint32_t>(isr15), 0x08, 0x8E);
  IDTSetGate(16, reinterpret_cast<uint32_t>(isr16), 0x08, 0x8E);
  IDTSetGate(17, reinterpret_cast<uint32_t>(isr17), 0x08, 0x8E);
  IDTSetGate(18, reinterpret_cast<uint32_t>(isr18), 0x08, 0x8e);
  IDTSetGate(19, reinterpret_cast<uint32_t>(isr19), 0x08, 0x8e);
  IDTSetGate(20, reinterpret_cast<uint32_t>(isr20), 0x08, 0x8E);
  IDTSetGate(21, reinterpret_cast<uint32_t>(isr21), 0x08, 0x8E);
  IDTSetGate(22, reinterpret_cast<uint32_t>(isr22), 0x08, 0x8E);
  IDTSetGate(23, reinterpret_cast<uint32_t>(isr23), 0x08, 0x8E);
  IDTSetGate(24, reinterpret_cast<uint32_t>(isr24), 0x08, 0x8E);
  IDTSetGate(25, reinterpret_cast<uint32_t>(isr25), 0x08, 0x8E);
  IDTSetGate(26, reinterpret_cast<uint32_t>(isr26), 0x08, 0x8E);
  IDTSetGate(27, reinterpret_cast<uint32_t>(isr27), 0x08, 0x8E);
  IDTSetGate(28, reinterpret_cast<uint32_t>(isr28), 0x08, 0x8e);
  IDTSetGate(29, reinterpret_cast<uint32_t>(isr29), 0x08, 0x8e);
  IDTSetGate(39, reinterpret_cast<uint32_t>(isr30), 0x08, 0x8E);
  IDTSetGate(31, reinterpret_cast<uint32_t>(isr31), 0x08, 0x8E);

  // IRQs (raised by external hardware)
  IDTSetGate(32, reinterpret_cast<uint32_t>(irq0), 0x08, 0x8E);
  IDTSetGate(33, reinterpret_cast<uint32_t>(irq1), 0x08, 0x8E);
  IDTSetGate(34, reinterpret_cast<uint32_t>(irq2), 0x08, 0x8E);
  IDTSetGate(35, reinterpret_cast<uint32_t>(irq3), 0x08, 0x8E);
  IDTSetGate(36, reinterpret_cast<uint32_t>(irq4), 0x08, 0x8E);
  IDTSetGate(37, reinterpret_cast<uint32_t>(irq5), 0x08, 0x8E);
  IDTSetGate(38, reinterpret_cast<uint32_t>(irq6), 0x08, 0x8E);
  IDTSetGate(39, reinterpret_cast<uint32_t>(irq7), 0x08, 0x8E);
  IDTSetGate(40, reinterpret_cast<uint32_t>(irq8), 0x08, 0x8E);
  IDTSetGate(41, reinterpret_cast<uint32_t>(irq9), 0x08, 0x8E);
  IDTSetGate(42, reinterpret_cast<uint32_t>(irq10), 0x08, 0x8E);
  IDTSetGate(43, reinterpret_cast<uint32_t>(irq11), 0x08, 0x8E);
  IDTSetGate(44, reinterpret_cast<uint32_t>(irq12), 0x08, 0x8E);
  IDTSetGate(45, reinterpret_cast<uint32_t>(irq13), 0x08, 0x8E);
  IDTSetGate(46, reinterpret_cast<uint32_t>(irq14), 0x08, 0x8E);
  IDTSetGate(47, reinterpret_cast<uint32_t>(irq15), 0x08, 0x8E);

  // Set the interrupt gate privilege for 0x80 to 3 so usermode can access it.
  IDTSetGate(128, reinterpret_cast<uint32_t>(isr128), 0x08, 0x8E | kDPLUser);

  IDTFlush(reinterpret_cast<uint32_t>(&idt_ptr));
}

}  // namespace

// Initialisation routine - zeroes all the interrupt service routines,
// initialises the GDT and IDT.
void InitDescriptorTables() {
  InitGDT();
  InitIDT();
}

void set_kernel_stack(uint32_t stack) { tss_entry.esp0 = stack; }
