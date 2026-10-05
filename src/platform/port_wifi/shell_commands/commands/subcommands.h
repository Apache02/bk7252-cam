#pragma once

// Every subcommand takes the name `wifi` was reached by, so its usage line can
// name the whole command rather than assume what the parent is called. Then its
// own argc and argv, with the subcommand's name as argv[0].

int wifi_sub_scan(const char *command, int argc, const char *argv[]);
int wifi_sub_join(const char *command, int argc, const char *argv[]);
int wifi_sub_disconnect(const char *command, int argc, const char *argv[]);
int wifi_sub_sta(const char *command, int argc, const char *argv[]);
int wifi_sub_ap(const char *command, int argc, const char *argv[]);
int wifi_sub_stat(const char *command, int argc, const char *argv[]);
int wifi_sub_ip(const char *command, int argc, const char *argv[]);
