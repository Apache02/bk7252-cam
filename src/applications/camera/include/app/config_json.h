#pragma once

#include <stddef.h>

#include "config.h"

// Writes the config as one JSON object with a key per field; the mac is "aa:bb:cc:dd:ee:ff".
// Returns the length, or 0 when buf is too small.
size_t config_to_json(const Config &config, char *buf, size_t size);

struct ScanResult;

// Writes a scan as {"status": "ok", "networks": [{"ssid", "rssi", "channel", "security"}, ...]}.
// Hidden networks are left out, and a network heard on several BSSIDs is listed once, at its
// strongest signal, strongest first. Stops adding networks when buf runs out of room.
// Returns the length, or 0 when buf is too small for even an empty list.
size_t scan_to_json(const ScanResult &scan, char *buf, size_t size);

// Reads a JSON object into config. A field that is absent keeps its value, an unknown key is
// ignored. Returns false and leaves config unchanged when the text is not valid or a value is
// out of range.
bool config_from_json(const char *json, size_t length, Config &config);
