#pragma once

#include "macros.h"

// Fake marlin config, good enough for use xy_pos_t from types.h
#define Z_DRIVER_TYPE

#define HOTENDS         1
// 8 + NoTool, which is the WIDEST the representation allows rather than any one printer's count:
// connect's slot_mask is a uint8_t carrying one bit per virtual tool, and its own
// static_assert(8 * sizeof slot_mask >= VirtualToolIndex::count) leaves exactly zero headroom at
// eight. The tests want the demanding end of that, not a mid-range machine - an off-by-one or a mask
// that overflows shows up here and nowhere else. Was 6 (five tools, the XL's count).
#define EXTRUDERS       9
#define SWITCH_ENABLED_ 1
#define SINGLENOZZLE

#define LOGICAL_AXES 4
#define NUM_AXES     4
#define E_STEPPERS   6
