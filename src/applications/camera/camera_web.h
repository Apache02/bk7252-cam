#pragma once

// Serves a page at "/" and an MJPEG stream of the camera at "/stream". One stream
// at a time; a second client asking for it gets 503, and the rest of the site
// stays reachable meanwhile.
//
// "GET /config" returns g_config as JSON. "POST /config" takes the same object, applies it to
// g_config, stores it and wakes the main task; fields that are absent keep their value.
void camera_web_start();
