#pragma once

#include <stdint.h>

void main_task_start();

bool main_task_ready();

// Tells the main task that g_config has changed. Safe to call from any task.
void main_task_config_changed();
