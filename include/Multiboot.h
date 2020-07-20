#ifndef MULTIBOOT_H_
#define MULTIBOOT_H_

#include <kassert.h>
#include <kstdint.h>

// See https://www.gnu.org/software/grub/manual/multiboot/multiboot.html for
// information on individual fields.
struct Multiboot {
  uint32_t flags;
  uint32_t mem_lower;
  uint32_t mem_upper;
  uint32_t boot_device;
  uint32_t cmdline;
  uint32_t mods_count;
  uint32_t mods_addr;
  uint32_t num;
  uint32_t size;
  uint32_t addr;
  uint32_t shndx;
  uint32_t mmap_length;
  uint32_t mmap_addr;
  uint32_t drives_length;
  uint32_t drives_addr;
  uint32_t config_table;
  uint32_t boot_loader_name;
  uint32_t apm_table;
  uint32_t vbe_control_info;
  uint32_t vbe_mode_info;
  uint16_t vbe_mode;
  uint16_t vbe_interface_seg;
  uint16_t vbe_interface_off;
  uint16_t vbe_interface_len;

  uint64_t framebuffer_addr;
  uint32_t framebuffer_pitch;
  uint32_t framebuffer_width;
  uint32_t framebuffer_height;
  uint8_t framebuffer_bpp;
  uint8_t framebuffer_type;

  struct ModuleInfo {
    uint32_t mod_start;
    uint32_t mod_end;
    uint32_t cmdline;  // Module command line
    uint32_t padding;  // Must be 0

    size_t getModuleSize() const { return mod_end - mod_start; }
    void *getModuleStart() const { return reinterpret_cast<void *>(mod_start); }
  };
  static_assert(sizeof(ModuleInfo) == 16, "ModuleInfo size changed!");

  // NOTE: These getModule* functions will return jiberish if there are no
  // modules.
  ModuleInfo *getModuleBegin() const {
    assert(mods_count && "No modules were provided");
    return reinterpret_cast<ModuleInfo *>(mods_addr);
  }

  ModuleInfo *getModuleEnd() const { return getModuleBegin() + mods_count; }

} __attribute__((packed));
static_assert(sizeof(Multiboot) == 110,
              "Multiboot size changed! Make sure this change was necessary "
              "then update the size checked in this assertion.");

#endif
