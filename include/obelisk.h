#ifndef OBELISK_H
#define OBELISK_H

/*
 * Obelisk master header file.
 * Includes all public runtime, compilation, server, and protocol definitions.
 */

#include "obelisk/vm.h"
#include "obelisk/ir.h"
#include "obelisk/optimizer.h"
#include "obelisk/jit.h"
#include "obelisk/aot.h"
#include "obelisk/ring_buffer.h"
#include "obelisk/protocol.h"
#include "obelisk/server.h"

#define OBELISK_VERSION_MAJOR 1
#define OBELISK_VERSION_MINOR 0
#define OBELISK_VERSION_PATCH 0
#define OBELISK_VERSION_STRING "1.0.0"

#endif /* OBELISK_H */
