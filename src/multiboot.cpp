#include <Multiboot.h>
#include <kstring.h>

toy::Unique<Multiboot> Multiboot::DeepCopy() const {
  toy::Unique<Multiboot> mb(new Multiboot);
  memcpy(mb.get(), this, sizeof(Multiboot));

  // Create new modules also.
  for (ModuleInfo *info = getModuleBegin(), end = getModuleEnd(); info < end;
       ++info) {}

  return mb;
}
