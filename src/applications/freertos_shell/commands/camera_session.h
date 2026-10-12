#pragma once

// Gives a shell command the camera. It is started on demand and stopped after 10 s without a command
// using it. Lives for the length of one command; a second command waits until the first one is done.
class CameraSession {
public:
    CameraSession();
    ~CameraSession();

    CameraSession(const CameraSession &)            = delete;
    CameraSession &operator=(const CameraSession &) = delete;

    // Starts the camera if it is stopped. Returns 0 or a negative errno, as camera_start() does.
    int ensure_started();
};
