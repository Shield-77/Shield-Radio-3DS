#include <3ds.h>
#include <citro2d.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "ui.h"

static C3D_RenderTarget *top, *bottom;
static C2D_TextBuf textbuf;

#define RGBA(r,g,b) ((u32)(r) | ((u32)(g) << 8) | ((u32)(b) << 16) | 0xff000000u)
static const u32 bg = RGBA(5, 14, 28);
static const u32 panel = RGBA(11, 26, 45);
static const u32 panel_light = RGBA(17, 43, 68);
static const u32 edge = RGBA(39, 69, 99);
static const u32 cyan = RGBA(47, 213, 245);
static const u32 white = RGBA(247, 249, 255);
static const u32 muted = RGBA(173, 190, 211);
static const u32 amber = RGBA(255, 190, 82);
static const u32 danger = RGBA(255, 58, 72);

/* Keep the six French badges exactly as drawn in v2.0.5. */
static const u32 french_badge_colors[] = {
    RGBA(224, 0, 35), RGBA(223, 1, 32), RGBA(20, 61, 151),
    RGBA(221, 0, 50), RGBA(220, 31, 139), RGBA(226, 41, 65)
};
static const char *french_badge_titles[] = {
    "RIRE", "NRJ", "NOST", "inter", "fun", "info"
};

/* Original wordmarks for the added stations; no external logo assets. */
static const char *badge_text[] = {
    "RIRE", "NRJ", "NOST", "INTER", "FUN", "INFO",
    "SANEF IDF", "SANEF REG", "RTBF", "C21", "DLF", "KULTUR",
    "SWISS", "JAZZ", "SER", "LOS40", "RAI 1", "RAI 2",
    "CAPITAL", "CLASSIC", "IPR NEWS", "IPR CLASSIC"
};
static const u32 badge_colors[] = {
    RGBA(205, 16, 45), RGBA(215, 20, 40), RGBA(35, 64, 145),
    RGBA(174, 25, 38), RGBA(210, 0, 93), RGBA(191, 35, 55),
    RGBA(27, 62, 125), RGBA(26, 59, 125), RGBA(20, 23, 29),
    RGBA(69, 37, 105), RGBA(21, 116, 139), RGBA(27, 65, 117),
    RGBA(28, 82, 140), RGBA(204, 15, 42), RGBA(20, 112, 74),
    RGBA(111, 30, 51), RGBA(27, 49, 105), RGBA(26, 35, 58),
    RGBA(32, 91, 131), RGBA(61, 70, 122), RGBA(26, 93, 145), RGBA(22, 74, 120)
};

static const char *countries_fr[] = {
    "FRANCE", "BELGIQUE", "ALLEMAGNE", "SUISSE",
    "ESPAGNE", "ITALIE", "ANGLETERRE", "ETATS-UNIS", "AUTOROUTES & TRAFIC"
};
static const char *countries_en[] = {
    "FRANCE", "BELGIUM", "GERMANY", "SWITZERLAND",
    "SPAIN", "ITALY", "UNITED KINGDOM", "UNITED STATES", "ROAD & TRAFFIC"
};

static void label(const char *s, float x, float y, float scale, u32 color) {
    C2D_Text t;
    C2D_TextParse(&t, textbuf, s);
    C2D_DrawText(&t, C2D_WithColor, x, y, 0.5f, scale, scale, color);
}

static void line(float x0, float y0, float x1, float y1, u32 color, float thick) {
    C2D_DrawLine(x0, y0, color, x1, y1, color, thick, 0.5f);
}

static void frame_begin(void) {
    C2D_TextBufClear(textbuf);
    C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
    C2D_TargetClear(top, bg);
    C2D_SceneBegin(top);
    C2D_DrawRectangle(3, 3, 0.5f, 394, 234, panel, panel, bg, bg);
}

static void draw_brand_header(bool french) {
    label("(( ))", 13, 11, .62f, cyan);
    label("SHIELD", 63, 8, .90f, white);
    label("RADIO", 170, 8, .90f, cyan);
    label(french ? "RADIO INTERNET" : "INTERNET RADIO", 64, 34, .37f, muted);
    line(13, 47, 387, 47, edge, 1);
}

static void draw_wordmark(size_t index, float x, float y, float w, float h) {
    if (index < 6) {
        u32 color = french_badge_colors[index];
        u32 paper = index == 2 ? color : white;
        C2D_DrawRectSolid(x, y, 0.5f, w, h, paper);
        if (index == 3 || index == 4 || index == 5)
            C2D_DrawRectSolid(x, y + h * .63f, 0.5f, w, h * .37f, color);
        if (w > 70 && index == 0) {
            label("RIRE", x + 8, y + 21, 1.22f, color);
            label("& CHANSONS", x + 8, y + 68, .43f, RGBA(20, 20, 20));
            return;
        }
        float scale = w > 70 ? .79f : .36f;
        if (index == 2) scale = w > 70 ? .45f : .27f;
        if (index == 3 || index == 4 || index == 5)
            scale = w > 70 ? .70f : .34f;
        label(french_badge_titles[index],
              x + (w > 70 ? 7 : 3), y + h * .21f,
              scale, index == 2 ? white : color);
        return;
    }
    u32 color = badge_colors[index % (sizeof(badge_colors) / sizeof(badge_colors[0]))];
    C2D_DrawRectSolid(x, y, 0.5f, w, h, color);
    C2D_DrawRectangle(x + 2, y + 2, 0.5f, w - 4, h - 4,
                      color, color, panel, panel);
    float scale = w > 70 ? .52f : .34f;
    if (index == 0 && w > 70) scale = .48f;
    if (index == 9 && w > 70) scale = .40f;
    if (index >= 18 && w > 70) scale = .38f;
    label(badge_text[index % (sizeof(badge_text) / sizeof(badge_text[0]))],
          x + (w > 70 ? 7 : 3), y + h * .35f, scale, white);
}

/* Tiny native car mark; keeps the road-radio section recognizable without a logo asset. */
static void draw_car_mark(float x, float y) {
    C2D_DrawRectSolid(x + 2, y + 4, .5f, 18, 4, cyan);
    C2D_DrawRectSolid(x + 6, y + 1, .5f, 11, 4, cyan);
    C2D_DrawRectSolid(x + 4, y + 7, .5f, 4, 3, panel);
    C2D_DrawRectSolid(x + 15, y + 7, .5f, 4, 3, panel);
}

static void draw_bottom_frame(void) {
    C2D_TargetClear(bottom, bg);
    C2D_SceneBegin(bottom);
    C2D_DrawRectangle(3, 3, .5f, 314, 234, panel, panel, bg, bg);
}

bool ui_init(void) {
    if (!C3D_Init(C3D_DEFAULT_CMDBUF_SIZE)) return false;
    if (!C2D_Init(C2D_DEFAULT_MAX_OBJECTS)) { C3D_Fini(); return false; }
    C2D_Prepare();
    top = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
    bottom = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);
    textbuf = C2D_TextBufNew(4096);
    return top && bottom && textbuf;
}

void ui_draw_language_prompt(bool french, bool first_use) {
    frame_begin();
    draw_brand_header(french);
    label(french ? "CHOIX DE LANGUE" : "LANGUAGE", 24, 62, .78f, cyan);
    label(french ? "Shield Radio est disponible en francais et en anglais."
                 : "Shield Radio is available in French and English.",
          24, 103, .53f, white);
    label(french ? "La langue selectionnee est-elle correcte ?"
                 : "Is the selected language correct?",
          24, 132, .54f, muted);
    label(french ? "Langue actuelle : Francais"
                 : "Current language: English",
          24, 159, .60f, white);
    label(french ? "B : English" : "B : Francais", 24, 195, .55f, cyan);
    label(french ? "A : Confirmer" : "A : Confirm", 24, 214, .55f, white);
    (void)first_use;

    draw_bottom_frame();
    label(french ? "LANGUE" : "LANGUAGE", 18, 12, .66f, cyan);
    line(15, 39, 305, 39, edge, 1);
    label(french ? "Francais" : "English", 34, 70, .76f, white);
    label(french ? "B pour choisir English" : "B to choose Francais", 22, 119, .48f, muted);
    label(french ? "A pour continuer" : "A to continue", 22, 143, .48f, muted);
    label(french ? "Le choix reste modifiable au prochain demarrage."
                 : "You can change this again at the next startup.",
          18, 194, .39f, muted);
    C3D_FrameEnd(0);
}

void ui_draw_guide(bool french, unsigned page) {
    if (page > 4) page = 4;
    frame_begin();
    draw_brand_header(french);
    char page_text[20];
    snprintf(page_text, sizeof(page_text), "%u/5", page + 1);
    label(french ? "GUIDE RAPIDE" : "QUICK GUIDE", 24, 59, .76f, cyan);
    label(page_text, 344, 62, .48f, muted);

    if (page == 0) {
        label(french ? "1. CHOISIR UNE STATION" : "1. PICK A STATION", 24, 96, .57f, white);
        label(french ? "Les radios sont regroupees par pays."
                     : "Stations are grouped by country.",
              24, 123, .50f, muted);
        label(french ? "Fleches haut/bas ou toucher une radio."
                     : "Use Up/Down or tap a station.",
              24, 148, .50f, muted);
        label(french ? "Un toucher lance directement la radio."
                     : "A tap starts that station right away.",
              24, 173, .50f, muted);
    } else if (page == 1) {
        label(french ? "2. ECOUTER" : "2. LISTEN", 24, 96, .57f, white);
        label(french ? "A demarre la station choisie."
                     : "A starts the selected station.",
              24, 123, .50f, muted);
        label(french ? "A une seconde fois l'arrete."
                     : "Press A again to stop it.",
              24, 148, .50f, muted);
        label(french ? "Une connexion Wi-Fi est necessaire."
                     : "A Wi-Fi connection is required.",
              24, 173, .50f, muted);
    } else if (page == 2) {
        label(french ? "3. COMMANDES ET FLUX" : "3. CONTROLS AND STREAM", 24, 96, .57f, white);
        label(french ? "L/R : station suivante ou precedente."
                     : "L/R: previous or next station.",
              24, 123, .50f, muted);
        label(french ? "X verrouille ou deverrouille L/R."
                     : "X locks or unlocks L/R.",
              24, 148, .50f, muted);
        label(french ? "Mise en tampon ou reconnexion : le flux reprend seul."
                     : "Buffering or reconnecting: the stream retries automatically.",
              24, 173, .43f, muted);
    } else if (page == 3) {
        label(french ? "4. ETATS DU FLUX" : "4. STREAM STATUS",
              24, 96, .57f, white);
        label(french ? "MISE EN TAMPON : reserve audio trop basse."
                     : "BUFFERING: audio reserve is running low.",
              24, 123, .46f, muted);
        label(french ? "RECONNEXION : le flux a ete interrompu."
                     : "RECONNECTING: the stream was interrupted.",
              24, 148, .46f, muted);
        label(french ? "CURL / HTTP : codes utiles pour le diagnostic."
                     : "CURL / HTTP: codes that help diagnose errors.",
              24, 173, .43f, muted);
        label(french ? "Pub, serveur ou reseau peuvent provoquer une pause."
                     : "Ads, server or network issues may cause a pause.",
              24, 197, .40f, muted);
    } else {
        label(french ? "5. UN PROJET FAIT SUR MON TEMPS LIBRE"
                     : "5. A PROJECT BUILT IN MY FREE TIME",
              24, 96, .48f, white);
        label(french ? "Je suis un createur solo benevole : les idees et les essais"
                     : "This is a volunteer solo project: I provide the ideas and",
              24, 123, .43f, muted);
        label(french ? "viennent de moi, Codex m'aide a ecrire le code."
                     : "testing, while Codex helps me write the code.",
              24, 145, .43f, muted);
        label(french ? "Le developpement avance sur mon temps libre."
                     : "Development happens in my spare time.",
              24, 171, .43f, muted);
        label(french ? "La traduction anglaise peut contenir des erreurs."
                     : "The English text may contain mistakes.",
              24, 194, .43f, muted);
    }

    draw_bottom_frame();
    label(french ? "A : Continuer" : "A: Continue", 18, 18, .51f, white);
    label(french ? "B : Page precedente" : "B: Previous page", 18, 42, .46f, muted);
    line(15, 70, 305, 70, edge, 1);
    if (page == 0) {
        label(french ? "Formats : flux MP3 directs."
                     : "Format: direct MP3 streams.",
              20, 92, .46f, cyan);
        label(french ? "Les flux peuvent parfois ralentir."
                     : "Streams can slow down sometimes.",
              20, 119, .46f, muted);
        label(french ? "Un message indique la mise en tampon"
                     : "A status message shows buffering",
              20, 146, .46f, muted);
        label(french ? "ou la reconnexion automatique."
                     : "or automatic reconnection.",
              20, 168, .46f, muted);
    } else if (page == 1) {
        label(french ? "Les radios necessitent Internet."
                     : "Radio stations need internet access.",
              20, 96, .46f, cyan);
        label(french ? "B arrete la lecture."
                     : "B stops playback.",
              20, 124, .46f, muted);
        label(french ? "START quitte l'application."
                     : "START exits the app.",
              20, 152, .46f, muted);
    } else if (page == 2) {
        label(french ? "Y : basses Off, +6, +9, +16 ou +20 dB."
                     : "Y: bass Off, +6, +9, +16 or +20 dB.",
              20, 96, .42f, cyan);
        label(french ? "X verrouille L/R pour eviter les erreurs."
                     : "X locks L/R to prevent accidental skips.",
              20, 122, .42f, muted);
        label(french ? "A passe a la page suivante."
                     : "A goes to the next page.",
              20, 157, .42f, muted);
    } else if (page == 3) {
        label(french ? "Wi-Fi ON ne garantit pas le flux audio."
                     : "Wi-Fi ON does not guarantee audio.",
              20, 92, .43f, cyan);
        label(french ? "CURL 28 : delai d'attente depasse."
                     : "CURL 28: connection timed out.",
              20, 119, .43f, muted);
        label(french ? "HTTP 4xx/5xx : reponse en erreur."
                     : "HTTP 4xx/5xx: error response.",
              20, 146, .43f, muted);
        label(french ? "Si cela persiste, essayez une autre radio."
                     : "If it persists, try another station.",
              20, 173, .43f, muted);
    } else {
        label(french ? "Merci d'essayer Shield Radio !"
                     : "Thanks for trying Shield Radio!",
              20, 108, .50f, cyan);
        label(french ? "A : J'ai compris" : "A: Got it", 20, 152, .54f, white);
    }
    char bottom_page[24];
    snprintf(bottom_page, sizeof(bottom_page), "%u / 5", page + 1);
    label(bottom_page, 269, 216, .38f, muted);
    C3D_FrameEnd(0);
}

static size_t row_for_station(const unsigned *countries, size_t count, size_t station) {
    size_t row = 0;
    for (size_t i = 0; i < count; ++i) {
        if (i == 0 || countries[i] != countries[i - 1]) ++row;
        if (i == station) return row;
        ++row;
    }
    return 0;
}

static int station_for_row(const unsigned *countries, size_t count, size_t target) {
    size_t row = 0;
    for (size_t i = 0; i < count; ++i) {
        if (i == 0 || countries[i] != countries[i - 1]) {
            if (row == target) return -1; /* country heading is not a station */
            ++row;
        }
        if (row == target) return (int)i;
        ++row;
    }
    return -1;
}

int ui_station_at_touch(int touch_y, const unsigned *countries,
                        size_t station_count, size_t selected) {
    if (!countries || station_count == 0 || touch_y < 43 || touch_y >= 198)
        return -1;
    if (selected >= station_count) selected = 0;
    size_t selected_row = row_for_station(countries, station_count, selected);
    size_t first_row = selected_row >= 5 ? selected_row - 4 : 0;
    size_t row = first_row + (size_t)(touch_y - 43) / 31;
    if ((size_t)(touch_y - 43) / 31 >= 5) return -1;
    return station_for_row(countries, station_count, row);
}

void ui_draw(const char *const *names, const char *const *descriptions,
             const unsigned *countries, size_t station_count,
             size_t selected, int active, int status,
             size_t buffered_bytes, unsigned bitrate_kbps,
             int wifi_state, bool lr_locked, bool french,
              int bass_boost_db, int battery_percent, unsigned frame,
              int curl_code, long http_code) {
    if (!names || !descriptions || !countries || station_count == 0) return;
    if (selected >= station_count) selected = 0;
    int display = active >= 0 && (size_t)active < station_count ? active : (int)selected;
    unsigned country = countries[display] < 9 ? countries[display] : 0;
    float buffer_seconds = (float)buffered_bytes * 8.0f /
                           ((float)(bitrate_kbps ? bitrate_kbps : 128) * 1000.0f);
    char meta[80], clock_text[16];
    if (status == 3)
        snprintf(meta, sizeof(meta), "CURL %d  HTTP %ld  |  Buffer: %.1f s",
                 curl_code, http_code, buffer_seconds);
    else if (bitrate_kbps)
        snprintf(meta, sizeof(meta), "MP3 %u kbps  |  Buffer: %.1f s", bitrate_kbps, buffer_seconds);
    else
        snprintf(meta, sizeof(meta), "MP3 stream  |  Buffer: %.1f s", buffer_seconds);
    time_t now = time(NULL);
    struct tm *local = localtime(&now);
    if (local) strftime(clock_text, sizeof(clock_text), "%H:%M", local);
    else snprintf(clock_text, sizeof(clock_text), "--:--");

    frame_begin();
    draw_brand_header(french);
    char battery_text[8];
    if (battery_percent < 0 || battery_percent > 100)
        snprintf(battery_text, sizeof(battery_text), "--%%");
    else
        snprintf(battery_text, sizeof(battery_text), "%d%%", (battery_percent / 5) * 5);
    u32 battery_color = battery_percent >= 0 && battery_percent <= 20 ? danger : white;
    label(battery_text, 306, 14, .43f, battery_color);
    label(clock_text, 352, 14, .38f, white);

    draw_wordmark((size_t)display, 22, 62, 105, 98);
    float name_scale = strlen(names[display]) > 17 ? .59f :
                       strlen(names[display]) > 12 ? .70f : .82f;
    label(names[display], 144, 64, name_scale, white);
    if (country == 8) {
        draw_car_mark(145, 90);
        label(french ? countries_fr[country] : countries_en[country],
              172, 92, .36f, cyan);
    } else {
        label(country < 9 ? (french ? countries_fr[country] : countries_en[country]) : "",
              145, 92, .43f, cyan);
    }

    const char *state;
    u32 state_color = cyan;
    float state_scale = .48f;
    switch (status) {
        case 1: state = french ? "CONNEXION" : "CONNECTING"; break;
        case 2: state = french ? "EN DIRECT" : "LIVE"; break;
        case 3:
            state = french ? "RECONNEXION" : "RECONNECTING";
            state_color = amber; state_scale = .42f; break;
        case 4:
            state = french ? "MISE EN TAMPON" : "BUFFERING";
            state_color = amber; state_scale = .40f; break;
        default: state = french ? "ARRETEE" : "STOPPED"; state_color = muted; break;
    }
    C2D_DrawRectSolid(145, 112, .5f, 132, 24, state_color);
    label(state, 151, 115, state_scale, bg);
    label(meta, 144, 146, .48f, muted);
    label(descriptions[display], 22, 175, .47f, muted);

    for (int i = 0; i < 29; ++i) {
        unsigned seed = (unsigned)(i * 37 + frame / 5 * (i % 4 + 1));
        int bars = active < 0 ? 1 : 1 + (int)((seed ^ (seed >> 3)) % 7);
        for (int b = 0; b < bars; ++b)
            C2D_DrawRectSolid(22 + i * 12, 221 - b * 4, .5f, 8, 2,
                              i < 13 ? cyan : RGBA(34, 93, 126));
    }
    line(13, 227, 387, 227, edge, 1);

    draw_bottom_frame();
    label(french ? ":::  Stations de radio" : ":::  Radio stations",
          14, 8, .58f, white);
    label(wifi_state ? "Wi-Fi ON" : "Wi-Fi OFF", 251, 12, .39f,
          wifi_state ? cyan : muted);
    line(14, 38, 306, 38, edge, 1);

    size_t selected_row = row_for_station(countries, station_count, selected);
    size_t first_row = selected_row >= 5 ? selected_row - 4 : 0;
    for (size_t slot = 0; slot < 5; ++slot) {
        size_t row = first_row + slot;
        int station = station_for_row(countries, station_count, row);
        float y = 42 + slot * 31;
        if (station < 0) {
            unsigned country_id = 0;
            size_t scan_row = 0;
            bool found = false;
            for (size_t i = 0; i < station_count; ++i) {
                if (i == 0 || countries[i] != countries[i - 1]) {
                    if (scan_row == row) { country_id = countries[i]; found = true; break; }
                    ++scan_row;
                }
                ++scan_row;
            }
            if (found) {
                C2D_DrawRectSolid(14, y + 2, .5f, 292, 26, panel_light);
                const char *country_name = country_id < 9
                    ? (french ? countries_fr[country_id] : countries_en[country_id]) : "";
                if (country_id == 8) {
                    draw_car_mark(20, y + 8);
                    label(country_name, 48, y + 7, .40f, cyan);
                } else {
                    label(country_name, 23, y + 7, .44f, cyan);
                }
            }
            continue;
        }
        bool is_selected = (size_t)station == selected;
        C2D_DrawRectSolid(13, y, .5f, 294, 29, is_selected ? cyan : edge);
        C2D_DrawRectangle(14, y + 1, .5f, 292, 27,
                          is_selected ? panel_light : bg,
                          is_selected ? panel_light : bg, panel, panel);
        if (station < 6)
            draw_wordmark((size_t)station, 22, y + 3, 30, 25);
        else
            draw_wordmark((size_t)station, 19, y + 3, 40, 23);
        float row_scale = strlen(names[station]) > 18 ? .42f : .48f;
        label(names[station], 67, y + 7, row_scale, white);
        if (station == active) label("LIVE", 270, y + 7, .36f, cyan);
    }

    line(14, 201, 306, 201, edge, 1);
    label(french ? "A: Lire/Arret" : "A: Play/Stop", 18, 205, .40f, white);
    label(french ? "B: Arret" : "B: Stop", 112, 205, .36f, muted);
    label(lr_locked ? "X: Unlock L/R" : "X: Lock L/R", 196, 205, .37f, cyan);
    char bass_label[32];
    if (bass_boost_db > 0)
        snprintf(bass_label, sizeof(bass_label), "Y: Bass +%d dB", bass_boost_db);
    else
        snprintf(bass_label, sizeof(bass_label), "Y: Bass Off");
    label(bass_label, 18, 222, .36f, cyan);
    label(lr_locked ? "L/R locked" : "L/R: Prev/Next", 133, 222, .34f, muted);
    label(french ? "START: Quitter" : "START: Exit", 230, 222, .34f, white);
    C3D_FrameEnd(0);
}

void ui_exit(void) {
    if (textbuf) C2D_TextBufDelete(textbuf);
    C2D_Fini();
    C3D_Fini();
}
