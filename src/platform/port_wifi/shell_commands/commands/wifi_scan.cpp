#include "subcommands.h"
#include "shell/Table.h"

#include "wifi/connect.h"
#include "wifi/core.h"
#include "wifi/net.h"
#include "wifi/scan.h"

#include <stdio.h>
#include <string.h>


#undef count_of
#define count_of(x) (sizeof(x) / sizeof(x[0]))


static const char *security_name(StationSecurity security) {
    switch (security) {
        case Open:
            return "open";
        case Wep:
            return "WEP";
        case Wpa:
            return "WPA";
        case Wpa2:
            return "WPA2";
        case Wpa3:
            return "WPA3";
    }
    return "unknown";
}

static const char *scan_status_name(ScanStatus status) {
    switch (status) {
        case Ok:
            return "ok";
        case Partial:
            return "partial";
        case Timeout:
            return "timeout";
    }
    return "unknown";
}

// clang-format off
static const Table::ColumnDef scan_print_table_def[] = {
    {"BSSID", 20, "%s", Table::Align::Left},
    {"CH", 4, "%u", Table::Align::Right},
    {"RSSI", 8, "%d dBm", Table::Align::Right},
    {"BCN", 4, "%u", Table::Align::Right},
    {"Security", 8, "%s", Table::Align::Right},
    {"SSID", 32, "%s", Table::Align::Left},
};

// TIME is the channel time the LMAC was asked for; BUSY is how much of it the
// channel was occupied.
static const Table::ColumnDef survey_print_table_def[] = {
    {"FREQ", 6, "%u", Table::Align::Right},
    {"NOISE", 6, "%d", Table::Align::Right},
    {"TIME", 8, "%lu ms", Table::Align::Right},
    {"BUSY", 8, "%lu ms", Table::Align::Right},
    {"BUSY%", 6, "%u%%", Table::Align::Right},
};
// clang-format on

static void sprint_bssid(char *str, const uint8_t bssid[6]) {
    sprintf(str, "%02x:%02x:%02x:%02x:%02x:%02x", bssid[0], bssid[1], bssid[2], bssid[3], bssid[4], bssid[5]);
}

static void print_scan_summary(const ScanResult *r) {
    printf("scan: %u %s (", r->station_count, r->station_count == 1 ? "AP" : "APs");
    printf("%u indications", r->indications);
    printf(", %s", scan_status_name(r->status));
    if (r->dropped) printf(", %u dropped", r->dropped);
    printf(")\r\n");
}

static void print_stations(const ScanResult *r) {
    auto table = new Table(scan_print_table_def, count_of(scan_print_table_def));
    table->printHeader();

    for (const ScanStation *s = r->stations; s; s = s->next) {
        auto *row = table->createRow();

        char str_bssid[20];
        sprint_bssid(str_bssid, s->bssid);
        row->set("BSSID", str_bssid);
        row->set("CH", s->channel);
        row->set("RSSI", s->rssi);
        row->set("BCN", s->beacon_int);
        row->set("Security", security_name(s->security));

        if (s->ssid_len) {
            // ssid_len reaches SCAN_SSID_MAX, and sprintf still adds a NUL after
            // those 32 characters.
            char str_ssid[SCAN_SSID_MAX + 1];
            sprintf(str_ssid, "%.*s", (int)s->ssid_len, s->ssid);
            row->set("SSID", str_ssid);
        } else {
            row->set("SSID", "<hidden>");
        }

        table->printRow(row);
        delete row;
    }

    delete table;
}

static void print_channels(const ChannelSurvey *survey) {
    if (!survey) return;

    // Busy time is the useful number: a channel with real traffic on it that
    // yielded no beacons points at the receiver, not at an empty band.
    printf("channel survey:\r\n");

    auto table = new Table(survey_print_table_def, count_of(survey_print_table_def));
    table->printHeader();

    for (const ChannelSurvey *c = survey; c; c = c->next) {
        auto *row = table->createRow();

        row->set("FREQ", c->freq);
        row->set("NOISE", c->noise_dbm);
        row->set("TIME", static_cast<unsigned long>(c->time_ms));
        row->set("BUSY", static_cast<unsigned long>(c->busy_ms));
        row->set("BUSY%", c->time_ms ? static_cast<unsigned>((c->busy_ms * 100u) / c->time_ms) : 0u);

        table->printRow(row);
        delete row;
    }

    delete table;
}

static void print_usage(const char *command, const char *name) {
    printf("usage: %s %s [--long] [--survey]\r\n", command, name);
    printf("  --long    stay 110 ms on each channel instead of 90 ms — beacons\r\n");
    printf("            come about every 100 ms, so this finds networks the\r\n");
    printf("            shorter sweep can miss\r\n");
    printf("  --survey  also collect and show per-channel noise and busy time\r\n");
    printf("\r\n  The radio leaves its current channel for the whole sweep, so a\r\n");
    printf("  live link can miss beacons while this runs.\r\n");
}

// wifi scan — starts a sweep, waits for it, prints what came back.
int wifi_sub_scan(const char *command, int argc, const char *argv[]) {
    ScanConfig config = {};

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0) {
            print_usage(command, argv[0]);
            return 0;
        }

        if (strcmp(argv[i], "--long") == 0) {
            config.longer_channel_time = true;
        } else if (strcmp(argv[i], "--survey") == 0) {
            config.collect_survey = true;
        } else {
            printf("[wifi] unknown option: %s\r\n", argv[i]);
            print_usage(command, argv[0]);
            return -1;
        }
    }

    Shared<Promise<ScanResult>> promise = wifi_scan(&config);
    if (promise.empty()) {
        printf("[wifi] scan not started\r\n");
        return -1;
    }

    // Blocks until the sweep ends or the session gives up on it; either way a
    // result comes back, and it is ours from here on.
    Owned<ScanResult> result = promise->await();
    if (result.empty()) {
        printf("[wifi] scan produced no result\r\n");
        return -1;
    }

    printf("\r\n");
    print_scan_summary(result.get());
    printf("\r\n");
    print_stations(result.get());
    printf("\r\n");

    // Empty unless --survey asked for it, and print_channels() says nothing then.
    print_channels(result->survey);

    return 0;
}
