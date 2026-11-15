#include <3ds.h>
#include <curl/curl.h>
#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <stdint.h>
#include <string.h>
#include <sys/stat.h>
#include <math.h>

#define MINIMP3_IMPLEMENTATION
#define MINIMP3_ONLY_MP3
#define MINIMP3_NO_SIMD
#include "minimp3.h"
#include "shield_language_choice.h"
#include "ui.h"

#define NET_CAP 65536
#define AUDIO_BUFS 4
#define PCM_SAMPLES 1152
#define SNAPSHOT 16384
#define LANGUAGE_PATH "sdmc:/3ds/ShieldRadio/language.dat"
#define GUIDE_PATH "sdmc:/3ds/ShieldRadio/guide_seen.dat"

enum {
    COUNTRY_FRANCE,
    COUNTRY_BELGIUM,
    COUNTRY_GERMANY,
    COUNTRY_SWITZERLAND,
    COUNTRY_SPAIN,
    COUNTRY_ITALY,
    COUNTRY_UK,
    COUNTRY_USA,
    COUNTRY_COUNT
};

/* A separate list section, while still part of the French catalogue. */
#define GROUP_AUTOROUTE 8

typedef struct {
    unsigned country;
    const char *name;
    const char *description_fr;
    const char *description_en;
    const char *url;
} Station;

/* Keep the station table grouped by country: this also makes navigation and
   the compact country headers deterministic on the Old model. */
static const Station stations[] = {
    {COUNTRY_FRANCE, "Rire & Chansons", "Humour et chansons", "Comedy and music", "http://streaming.nrjaudio.fm/ou8o8xgk7oiu"},
    {COUNTRY_FRANCE, "NRJ", "Les hits en direct", "Today's hits", "http://streaming.nrjaudio.fm/oumvmk8fnozc"},
    {COUNTRY_FRANCE, "Nostalgie", "Les plus grands tubes", "Classic hits", "http://streaming.nrjaudio.fm/oug7girb92oc"},
    {COUNTRY_FRANCE, "France Inter", "Radio generaliste", "News, culture and talk", "http://icecast.radiofrance.fr/franceinter-midfi.mp3"},
    {COUNTRY_FRANCE, "Fun Radio", "Le son dancefloor", "Dance and electronic", "http://icecast.funradio.fr/fun-1-44-128"},
    {COUNTRY_FRANCE, "franceinfo", "L'actualite en direct", "Live news", "http://icecast.radiofrance.fr/franceinfo-midfi.mp3"},

    {GROUP_AUTOROUTE, "Sanef 107.7 IDF", "Trafic routier en Ile-de-France", "Road traffic in the Paris region", "http://sanef.ice.infomaniak.ch/sanef1077-idf.mp3"},
    {GROUP_AUTOROUTE, "Sanef 107.7 Régions", "Reseau Sanef/Sapn : Nord, Est, Normandie", "Sanef/Sapn network: North, East, Normandy", "http://sanef.ice.infomaniak.ch/sanef1077-regions.mp3"},

    {COUNTRY_BELGIUM, "La Premiere", "Information et culture", "News and culture", "http://radio.rtbf.be/lapremiere/mp3-160/me"},
    {COUNTRY_BELGIUM, "Classic 21", "Rock et classiques", "Rock and classics", "http://radio.rtbf.be/c21/mp3-160/me"},

    {COUNTRY_GERMANY, "Deutschlandfunk", "Actualite et debats", "News and discussion", "http://st01.sslstream.dlf.de/dlf/01/128/mp3/stream.mp3"},
    {COUNTRY_GERMANY, "DLF Kultur", "Culture et musique", "Culture and music", "https://st02.sslstream.dlf.de/dlf/02/128/mp3/stream.mp3?aggregator=web"},

    {COUNTRY_SWITZERLAND, "Radio Swiss Classic", "Musique classique", "Classical music", "http://stream.srg-ssr.ch/srgssr/rsc_de/mp3/128"},
    {COUNTRY_SWITZERLAND, "Radio Swiss Jazz", "Jazz en continu", "Jazz all day", "http://stream.srg-ssr.ch/m/rsj/mp3_128"},

    {COUNTRY_SPAIN, "Cadena SER", "Actualite et radio generaliste", "News and talk", "http://playerservices.streamtheworld.com/api/livestream-redirect/CADENASER.mp3"},
    {COUNTRY_SPAIN, "LOS40", "Hits et musique pop", "Pop and current hits", "http://playerservices.streamtheworld.com/api/livestream-redirect/LOS40.mp3"},

    {COUNTRY_ITALY, "Rai Radio 1", "Actualite et emissions", "News and programmes", "http://icestreaming.rai.it/1.mp3"},
    {COUNTRY_ITALY, "Rai Radio 2", "Musique et divertissement", "Music and entertainment", "http://icestreaming.rai.it/2.mp3"},

    {COUNTRY_UK, "Capital", "Hits et pop", "Pop and current hits", "http://media-ice.musicradio.com/CapitalMP3"},
    {COUNTRY_UK, "Classic FM", "Musique classique", "Classical music", "http://media-ice.musicradio.com/ClassicFMMP3"},

    {COUNTRY_USA, "IPR News", "Actualite en continu", "Live news", "http://news-stream.iowapublicradio.org/News.mp3"},
    {COUNTRY_USA, "IPR Classical", "Musique classique", "Classical music", "http://classical-stream.iowapublicradio.org/Classical.mp3"},
};
#define STATION_COUNT (sizeof(stations) / sizeof(stations[0]))

static u8 *soc_memory;
static u8 net_data[NET_CAP];
static size_t net_read, net_write, net_count;
static LightLock net_lock;
static volatile bool net_stop;
static volatile int net_status; /* 0 stopped, 1 connecting, 2 receiving, 3 retrying */
static volatile int net_retries;
static volatile int net_last_curl_code;
static volatile long net_last_http_code;
static Thread net_thread;
static const char *net_url;
static bool ndm_initialized, ndm_exclusive, ndm_locked;
static unsigned ndm_retry_frames;
static aptHookCookie apt_cookie;
static volatile bool wake_pending;
static mp3dec_t decoder;
static u8 snapshot[SNAPSHOT];
static mp3d_sample_t decoded[MINIMP3_MAX_SAMPLES_PER_FRAME];
static s16 *pcm;
static ndspWaveBuf waves[AUDIO_BUFS];
static bool queued[AUDIO_BUFS];
static int sample_rate;
static int bitrate_average;
static unsigned bitrate_samples;
static unsigned low_buffer_frames;
static bool buffering_alert;
static const int bass_presets_db[] = {0, 6, 9, 16, 20};
static unsigned bass_preset;
static bool bass_enabled;

static bool set_bass_preset(unsigned preset) {
    if (preset >= sizeof(bass_presets_db) / sizeof(bass_presets_db[0])) return false;
    float mix[12] = {1.0f, 1.0f};
    if (bass_presets_db[preset] == 0) {
        ndspChnIirBiquadSetEnable(0, false);
        ndspChnSetMix(0, mix);
        return true;
    }
    const float boost_db = (float)bass_presets_db[preset];
    const float gain = powf(10.0f, boost_db / 40.0f);
    const float w0 = 2.0f * 3.14159265f * 120.0f / NDSP_SAMPLE_RATE;
    const float alpha = sinf(w0) / (2.0f * 0.7071f);
    const float cosine = cosf(w0);
    /* libctru's peaking helper currently uses the same coefficients for the
       numerator and denominator, giving unity gain. Supply the correct
       peaking EQ coefficients directly to the DSP instead. */
    const float a0 = 1.0f + alpha / gain;
    const float a1 = -2.0f * cosine;
    const float a2 = 1.0f - alpha / gain;
    const float b0 = 1.0f + alpha * gain;
    const float b1 = -2.0f * cosine;
    const float b2 = 1.0f - alpha * gain;
    if (!ndspChnIirBiquadSetParamsCustomFilter(0, a0, a1, a2, b0, b1, b2)) {
        ndspChnIirBiquadSetEnable(0, false);
        ndspChnSetMix(0, mix);
        return false;
    }
    ndspChnSetMix(0, mix);
    ndspChnIirBiquadSetEnable(0, true);
    return true;
}

static void on_apt_event(APT_HookType hook, void *unused) {
    (void)unused;
    if (hook == APTHOOK_ONWAKEUP || hook == APTHOOK_ONRESTORE) wake_pending = true;
}

static size_t ring_count(void) {
    LightLock_Lock(&net_lock);
    size_t n = net_count;
    LightLock_Unlock(&net_lock);
    return n;
}

static size_t on_audio(char *ptr, size_t size, size_t nmemb, void *unused) {
    (void)unused;
    if (size && nmemb > SIZE_MAX / size) return 0;
    size_t n = size * nmemb;
    size_t offset = 0;
    if (n) {
        net_status = 2;
        net_retries = 0;
    }
    while (offset < n && !net_stop) {
        LightLock_Lock(&net_lock);
        size_t space = NET_CAP - net_count;
        size_t count = n - offset;
        if (count > space) count = space;
        size_t until_end = NET_CAP - net_write;
        if (count > until_end) count = until_end;
        if (count) {
            memcpy(net_data + net_write, ptr + offset, count);
            net_write = (net_write + count) % NET_CAP;
            net_count += count;
            offset += count;
        }
        LightLock_Unlock(&net_lock);
        if (!count) svcSleepThread(2000000);
    }
    return net_stop ? 0 : n;
}

static int on_progress(void *unused, curl_off_t a, curl_off_t b, curl_off_t c, curl_off_t d) {
    (void)unused; (void)a; (void)b; (void)c; (void)d;
    return net_stop ? 1 : 0;
}

static void network_worker(void *unused) {
    (void)unused;
    while (!net_stop) {
        CURL *curl = curl_easy_init();
        if (curl) {
            net_status = net_retries ? 3 : 1;
            curl_easy_setopt(curl, CURLOPT_URL, net_url);
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, on_audio);
            curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, on_progress);
            curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
            curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
            curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 8L);
            curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
            curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
            curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, 1L);
            curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, 12L);
            curl_easy_setopt(curl, CURLOPT_USERAGENT, "ShieldRadio/2");
            curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_1);
            CURLcode result = curl_easy_perform(curl);
            long http_code = 0;
            (void)curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
            net_last_curl_code = (int)result;
            net_last_http_code = http_code;
            curl_easy_cleanup(curl);
        } else {
            net_last_curl_code = (int)CURLE_FAILED_INIT;
            net_last_http_code = 0;
        }
        if (!net_stop) {
            ++net_retries;
            net_status = 3;
            for (int i = 0; i < 10 && !net_stop; ++i) svcSleepThread(100000000);
        }
    }
    net_status = 0;
}

static bool lock_infrastructure(void) {
    if (ndm_locked) return true;
    if (!ndm_initialized) {
        if (R_FAILED(ndmuInit())) return false;
        ndm_initialized = true;
    }
    if (R_FAILED(NDMU_EnterExclusiveState(NDM_EXCLUSIVE_STATE_INFRASTRUCTURE))) goto fail;
    ndm_exclusive = true;
    if (R_FAILED(NDMU_LockState())) goto fail;
    ndm_locked = true;
    return true;
fail:
    if (ndm_exclusive) { NDMU_LeaveExclusiveState(); ndm_exclusive = false; }
    ndmuExit();
    ndm_initialized = false;
    return false;
}

static void unlock_infrastructure(void) {
    if (ndm_locked) { NDMU_UnlockState(); ndm_locked = false; }
    if (ndm_exclusive) { NDMU_LeaveExclusiveState(); ndm_exclusive = false; }
    if (ndm_initialized) { ndmuExit(); ndm_initialized = false; }
}

static void stop_radio(bool allow_sleep) {
    net_stop = true;
    if (net_thread) {
        threadJoin(net_thread, U64_MAX);
        threadFree(net_thread);
        net_thread = NULL;
    }
    ndspChnWaveBufClear(0);
    memset(queued, 0, sizeof(queued));
    LightLock_Lock(&net_lock);
    net_read = net_write = net_count = 0;
    LightLock_Unlock(&net_lock);
    mp3dec_init(&decoder);
    sample_rate = 0;
    bitrate_average = 0;
    bitrate_samples = 0;
    low_buffer_frames = 0;
    buffering_alert = false;
    net_status = 0;
    net_retries = 0;
    net_last_curl_code = 0;
    net_last_http_code = 0;
    if (allow_sleep) {
        aptSetSleepAllowed(true);
        unlock_infrastructure();
    }
}

static void start_radio(size_t index) {
    if (index >= STATION_COUNT) return;
    /* Closing the lid during setup must not enter real sleep while streaming. */
    aptSetSleepAllowed(false);
    stop_radio(false);
    net_url = stations[index].url;
    net_stop = false;
    net_status = 1;
    net_retries = 0;
    ndm_retry_frames = 0;
    (void)lock_infrastructure();
    net_thread = threadCreate(network_worker, NULL, 16 * 1024, 0x20, -2, false);
    if (!net_thread) {
        net_stop = true;
        net_status = 0;
        aptSetSleepAllowed(true);
        unlock_infrastructure();
    }
}

static bool decode_into(int slot) {
    LightLock_Lock(&net_lock);
    size_t n = net_count;
    if (n > SNAPSHOT) n = SNAPSHOT;
    for (size_t i = 0; i < n; ++i) snapshot[i] = net_data[(net_read + i) % NET_CAP];
    LightLock_Unlock(&net_lock);
    if (n < 4096) return false;

    mp3dec_frame_info_t info;
    int samples = mp3dec_decode_frame(&decoder, snapshot, (int)n, decoded, &info);
    if (info.frame_bytes > 0) {
        LightLock_Lock(&net_lock);
        size_t consumed = (size_t)info.frame_bytes;
        if (consumed > net_count) consumed = net_count;
        net_read = (net_read + consumed) % NET_CAP;
        net_count -= consumed;
        LightLock_Unlock(&net_lock);
    }
    if (samples <= 0 || info.channels < 1 || info.channels > 2 || samples > PCM_SAMPLES) return false;

    s16 *out = pcm + slot * PCM_SAMPLES * 2;
    for (int i = 0; i < samples; ++i) {
        out[2 * i] = decoded[i * info.channels];
        out[2 * i + 1] = decoded[i * info.channels + (info.channels == 2)];
    }
    if (sample_rate != info.hz) {
        sample_rate = info.hz;
        ndspChnSetRate(0, (float)sample_rate);
        /* Filter coefficients depend on the stream sample rate. */
        bass_enabled = set_bass_preset(bass_preset) && bass_presets_db[bass_preset] > 0;
    }
    if (info.bitrate_kbps > 0) {
        if (bitrate_samples < 32) {
            bitrate_average = (bitrate_average * (int)bitrate_samples + info.bitrate_kbps) /
                              (int)(bitrate_samples + 1);
            ++bitrate_samples;
        } else {
            bitrate_average = (bitrate_average * 15 + info.bitrate_kbps) / 16;
        }
    }
    DSP_FlushDataCache(out, samples * 2 * sizeof(s16));
    waves[slot].nsamples = samples;
    ndspChnWaveBufAdd(0, &waves[slot]);
    queued[slot] = true;
    return true;
}

static bool read_flag(const char *path) {
    FILE *file = fopen(path, "rb");
    if (!file) return false;
    int value = fgetc(file);
    fclose(file);
    return value == '1';
}

static void save_flag(const char *path) {
    (void)mkdir("sdmc:/3ds", 0777);
    (void)mkdir("sdmc:/3ds/ShieldRadio", 0777);
    FILE *file = fopen(path, "wb");
    if (!file) return;
    (void)fputc('1', file);
    (void)fclose(file);
}

static void choose_language(ShieldLanguageChoice *choice, bool first_use) {
    while (aptMainLoop()) {
        hidScanInput();
        ui_draw_language_prompt(choice->french, first_use);
        u32 down = hidKeysDown();
        if (down & KEY_B) shield_language_choice_toggle(choice);
        if (down & KEY_A) {
            shield_language_choice_accept(choice);
            return;
        }
        gspWaitForVBlank();
    }
}

static void show_guide(bool french) {
    unsigned page = 0;
    while (aptMainLoop()) {
        hidScanInput();
        ui_draw_guide(french, page);
        u32 down = hidKeysDown();
        if ((down & KEY_B) && page > 0) --page;
        if (down & KEY_A) {
            if (page < 4) ++page;
            else {
                save_flag(GUIDE_PATH);
                return;
            }
        }
        gspWaitForVBlank();
    }
}

static void show_startup_error(const char *title, const char *detail) {
    ui_exit();
    consoleInit(GFX_TOP, NULL);
    printf("%s\n\n%s\n\nSTART to quit", title, detail);
    while (aptMainLoop()) {
        hidScanInput();
        if (hidKeysDown() & KEY_START) break;
        gspWaitForVBlank();
    }
}

int main(void) {
    gfxInitDefault();
    bool force_language = false;
    bool force_guide = false;
    /* Give the launcher time to finish its handoff; X held here reopens the
       language picker on later launches without consuming an in-app control. */
    for (int i = 0; i < 25; ++i) {
        if (!aptMainLoop()) { gfxExit(); return 0; }
        hidScanInput();
        if (hidKeysHeld() & KEY_X) force_language = true;
        if (hidKeysHeld() & KEY_SELECT) force_guide = true;
        svcSleepThread(20000000);
    }

    if (!ui_init()) { gfxExit(); return 1; }
    bool cfgu_ready = R_SUCCEEDED(cfguInit());
    ShieldLanguageChoice language;
    shield_language_choice_load(&language, LANGUAGE_PATH, cfgu_ready);
    bool language_prompted = language.needs_confirmation || force_language;
    if (language_prompted) choose_language(&language, language.needs_confirmation);
    if (cfgu_ready) cfguExit();
    if (!aptMainLoop()) { ui_exit(); gfxExit(); return 0; }

    if (force_guide || language_prompted || !read_flag(GUIDE_PATH))
        show_guide(language.french);

    LightLock_Init(&net_lock);
    soc_memory = (u8 *)memalign(0x1000, 0x100000);
    if (!soc_memory || R_FAILED(socInit((u32 *)soc_memory, 0x100000))) {
        if (soc_memory) free(soc_memory);
        show_startup_error("Shield Radio", "Wi-Fi/network service could not start.");
        gfxExit();
        return 1;
    }
    if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK) {
        socExit(); free(soc_memory);
        show_startup_error("Shield Radio", "The network library could not start.");
        gfxExit();
        return 1;
    }
    if (R_FAILED(ndspInit())) {
        curl_global_cleanup(); socExit(); free(soc_memory);
        show_startup_error("Shield Radio", "The audio service could not start.");
        gfxExit();
        return 1;
    }
    pcm = (s16 *)linearAlloc(AUDIO_BUFS * PCM_SAMPLES * 2 * sizeof(s16));
    if (!pcm) {
        ndspExit(); curl_global_cleanup(); socExit(); free(soc_memory);
        show_startup_error("Shield Radio", "Not enough memory for audio buffers.");
        gfxExit();
        return 1;
    }

    memset(waves, 0, sizeof(waves));
    for (int i = 0; i < AUDIO_BUFS; ++i) waves[i].data_vaddr = pcm + i * PCM_SAMPLES * 2;
    ndspSetOutputMode(NDSP_OUTPUT_STEREO);
    ndspChnSetInterp(0, NDSP_INTERP_LINEAR);
    ndspChnSetFormat(0, NDSP_FORMAT_STEREO_PCM16);
    bass_preset = 0;
    bass_enabled = set_bass_preset(bass_preset) && bass_presets_db[bass_preset] > 0;
    mp3dec_init(&decoder);
    bool lid_state_available = R_SUCCEEDED(ptmuInit());
    bool battery_service_ready = R_SUCCEEDED(mcuHwcInit());
    int battery_percent = -1;
    if (battery_service_ready) {
        u8 battery_level = 0;
        if (R_SUCCEEDED(MCUHWC_GetBatteryLevel(&battery_level)) && battery_level <= 100)
            battery_percent = battery_level;
    }
    u64 next_battery_check = osGetTime() + 60000;

    aptSetSleepAllowed(false);
    aptHook(&apt_cookie, on_apt_event, NULL);
    size_t selected = 0;
    start_radio(selected);
    bool playing = net_thread != NULL;
    int active = playing ? (int)selected : -1;
    bool lr_locked = false;
    u64 next_ui_ms = 0;
    const char *names[STATION_COUNT], *descriptions_fr[STATION_COUNT], *descriptions_en[STATION_COUNT];
    unsigned countries[STATION_COUNT];
    for (size_t i = 0; i < STATION_COUNT; ++i) {
        names[i] = stations[i].name;
        descriptions_fr[i] = stations[i].description_fr;
        descriptions_en[i] = stations[i].description_en;
        countries[i] = stations[i].country;
    }

    while (aptMainLoop()) {
        if (wake_pending) {
            wake_pending = false;
            if (playing && active >= 0) {
                stop_radio(false);
                unlock_infrastructure();
                start_radio((size_t)active);
                playing = net_thread != NULL;
                if (!playing) active = -1;
            }
        }
        if (playing && !ndm_locked && ++ndm_retry_frames >= 120) {
            ndm_retry_frames = 0;
            (void)lock_infrastructure();
        }
        hidScanInput();
        u32 down = hidKeysDown();
        if (down & KEY_START) break;

        if (down & KEY_X) lr_locked = !lr_locked;
        if (down & KEY_Y) {
            unsigned next_preset = (bass_preset + 1) %
                (sizeof(bass_presets_db) / sizeof(bass_presets_db[0]));
            if (set_bass_preset(next_preset)) {
                bass_preset = next_preset;
                bass_enabled = bass_presets_db[bass_preset] > 0;
            }
        }
        if (down & KEY_UP) selected = (selected + STATION_COUNT - 1) % STATION_COUNT;
        if (down & KEY_DOWN) selected = (selected + 1) % STATION_COUNT;
        if (!lr_locked && (down & KEY_L)) {
            selected = (selected + STATION_COUNT - 1) % STATION_COUNT;
            start_radio(selected); playing = net_thread != NULL; active = playing ? (int)selected : -1;
        } else if (!lr_locked && (down & KEY_R)) {
            selected = (selected + 1) % STATION_COUNT;
            start_radio(selected); playing = net_thread != NULL; active = playing ? (int)selected : -1;
        }
        /* A toggles the selected station; B always stops playback. */
        if (down & KEY_A) {
            if (playing && active == (int)selected) {
                stop_radio(true); playing = false; active = -1;
            } else {
                start_radio(selected); playing = net_thread != NULL; active = playing ? (int)selected : -1;
            }
        }
        if ((down & KEY_B) && playing) {
            stop_radio(true); playing = false; active = -1;
        }
        if (down & KEY_TOUCH) {
            touchPosition touch;
            hidTouchRead(&touch);
            int tapped = ui_station_at_touch(touch.py, countries, STATION_COUNT, selected);
            if (tapped >= 0 && (size_t)tapped < STATION_COUNT) {
                selected = (size_t)tapped;
                start_radio(selected); playing = net_thread != NULL; active = playing ? (int)selected : -1;
            }
        }
        if (playing) {
            for (int i = 0; i < AUDIO_BUFS; ++i) {
                if (!queued[i] || waves[i].status == NDSP_WBUF_DONE) {
                    queued[i] = false;
                    for (int attempt = 0; attempt < 3 && !queued[i]; ++attempt) (void)decode_into(i);
                }
            }
        }

        u64 now_ms = osGetTime();
        u8 shell_open = 1;
        if (lid_state_available) PTMU_GetShellState(&shell_open);
        if (shell_open && now_ms >= next_ui_ms) {
            int display_status = net_status;
            size_t buffered = ring_count();
            unsigned nominal_kbps = bitrate_average > 0 ? (unsigned)bitrate_average : 128;
            float buffer_seconds = ((float)buffered * 8.0f) / ((float)nominal_kbps * 1000.0f);
            if (display_status == 1 && net_retries > 0) display_status = 3;
            if (display_status == 2) {
                if (buffer_seconds < 0.8f) {
                    if (low_buffer_frames < 12) ++low_buffer_frames;
                    if (buffer_seconds < 0.25f || low_buffer_frames >= 12)
                        buffering_alert = true;
                } else if (buffer_seconds > 1.6f) {
                    low_buffer_frames = 0;
                    buffering_alert = false;
                }
                if (buffering_alert) display_status = 4;
            } else if (display_status == 1 || display_status == 3 || display_status == 0) {
                low_buffer_frames = 0;
                buffering_alert = false;
            }
            int wifi_state = osGetWifiStrength() > 0 ? 1 : 0;
            if (now_ms >= next_battery_check) {
                if (battery_service_ready) {
                    u8 battery_level = 0;
                    if (R_SUCCEEDED(MCUHWC_GetBatteryLevel(&battery_level)) && battery_level <= 100)
                        battery_percent = battery_level;
                }
                next_battery_check = now_ms + 60000;
            }
            ui_draw(names,
                    language.french ? descriptions_fr : descriptions_en,
                    countries, STATION_COUNT, selected, active, display_status,
                    buffered, nominal_kbps, wifi_state, lr_locked,
                    language.french, bass_enabled ? bass_presets_db[bass_preset] : 0,
                    battery_percent, (unsigned)((now_ms / 95u) * 5u),
                    net_last_curl_code, net_last_http_code);
            next_ui_ms = osGetTime() + 33;
        } else {
            svcSleepThread(shell_open ? 2000000 : 10000000);
        }
    }

    stop_radio(true);
    aptUnhook(&apt_cookie);
    if (lid_state_available) ptmuExit();
    if (battery_service_ready) mcuHwcExit();
    linearFree(pcm);
    ndspExit();
    curl_global_cleanup();
    socExit();
    free(soc_memory);
    ui_exit();
    gfxExit();
    return 0;
}
