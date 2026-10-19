#pragma once

// Each takes its own argc and argv, with the subcommand's name as argv[0].

int wifi_sub_scan(int argc, const char *argv[]);
int wifi_sub_join(int argc, const char *argv[]);
int wifi_sub_sta(int argc, const char *argv[]);
int wifi_sub_ap(int argc, const char *argv[]);
int wifi_sub_info(int argc, const char *argv[]);
