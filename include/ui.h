#pragma once
#include <stdbool.h>
#include <stddef.h>

bool ui_init(void);
void ui_draw_language_prompt(bool french, bool first_use);
void ui_draw_guide(bool french, unsigned page);
void ui_draw(const char *const *names, const char *const *descriptions,
             const unsigned *countries, size_t station_count,
             size_t selected, int active, int status,
             size_t buffered_bytes, unsigned bitrate_kbps,
             int wifi_state, bool lr_locked, bool french,
             int bass_boost_db, int battery_percent, unsigned frame,
             int curl_code, long http_code);
int ui_station_at_touch(int touch_y, const unsigned *countries,
                        size_t station_count, size_t selected);
void ui_exit(void);
