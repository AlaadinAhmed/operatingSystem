#include "disk/disk.h"
#include "drivers/font.h"
#include "drivers/sse.h"
#include "drivers/vga.h"
#include "fs/lwext4_adapter.h"
#include "print/print.h"
#include "shell/shell.h"
#include "stb_truetype.h"
#include "system/system.h"
#include <cstdint>
#include <ext4.h>

extern "C" void main() {
  System system;
  system.Initialize();
  system.Run();
  system.Shutdown();
  return;
}
