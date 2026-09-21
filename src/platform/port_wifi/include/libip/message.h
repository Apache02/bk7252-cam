#pragma once

// Every message the archive's tasks can send or receive, in one include.
//
// The catalog itself lives one file per task under libip/tasks/, in the
// archive's own order: each task opens its enum with KE_FIRST_MSG(its task),
// so an id is its position in that list and never a hand-written number.
// docs/wifi_rw.md walks through what each one does.
//
// Two tasks the archive's source knows about have no header here: TDLS and
// MESH were both compiled out of this build and have no slot in ke_task_id.

// In task-id order, which is also the order the ids themselves run in.
#include "libip/tasks/mm.h"    // 0
#include "libip/tasks/scan.h"  // 1
#include "libip/tasks/scanu.h" // 2
#include "libip/tasks/me.h"    // 3
#include "libip/tasks/sm.h"    // 4
#include "libip/tasks/apm.h"   // 5
#include "libip/tasks/bam.h"   // 6
#include "libip/tasks/rxu.h"   // 7
