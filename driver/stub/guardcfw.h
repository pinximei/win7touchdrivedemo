// Empty stub. Win22621 SDK (which we use to target Win7) does not ship
// guardcfw.h, but v143 toolchain transitively expects it from <ntdef.h>
// when targeting kernel-mode. Provide a no-op header so compilation
// proceeds; CFG is disabled for the actual build (Win7 kernel cannot
// honor it).
#pragma once
