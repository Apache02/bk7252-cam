#pragma once

// Serves a page at "/" and an MJPEG stream of the camera at "/stream". One stream
// at a time; a second client asking for it gets 503, and the rest of the site
// stays reachable meanwhile.
void camera_web_start();
