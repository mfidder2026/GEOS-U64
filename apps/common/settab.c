/* GEOS-U64: settings screen engine, see settab.h */

#include <geos.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "rest.h"
#include "settab.h"
#include "ucfg.h"

#define ROW0        44      /* baseline of the first line */
#define ROWH        11
#define VALX        184
#define VALLEN      24
#define BARL        212     /* progress bar in the status line */
#define BARR        306

static const char *title;
static const struct setting *tab;
static unsigned char count;
static unsigned char connected;
static unsigned char offline;   /* no REST: write through a config file */
void (*settab_offline_values)(char values[][24]);
static char values[SETTAB_MAX][VALLEN];
static char path[200];
static char cur[40], tmp[40], first[40], next[40];
static char line[48];

static struct window box;

static void __fastcall__ fill(char top, char bot, unsigned left,
                              unsigned right, char pattern)
{
    box.top = top;
    box.bot = bot;
    box.left = left;
    box.right = right;
    InitDrawWindow(&box);
    SetPattern(pattern);
    Rectangle();
}

void __fastcall__ settab_message(const char *text)
{
    fill(186, 197, 8, 311, 0);
    PutString((char *)text, 195, 12);
}

/* status text plus a bar that is done/total full */
void __fastcall__ settab_progress(const char *text, unsigned char done,
                                  unsigned char total)
{
    settab_message(text);
    fill(188, 195, BARL, BARR, 0);
    FrameRectangle(0xff);
    if (done)
        fill(189, 194, BARL + 1,
             BARL + 1 + (unsigned)(BARR - BARL - 2) * done / total, 1);
}

static void draw_title(void)
{
    fill(21, 33, 8, 311, 0);
    sprintf(line, CBOLDON "%s" CPLAINTEXT "  Ultimate %s", title,
            connected ? rest_host :
            offline ? "offline (config file)" : "not connected");
    PutString(line, 30, 12);
}

static void __fastcall__ draw_row(unsigned char i)
{
    char y = ROW0 + i * ROWH;

    fill(y - 8, y + 2, VALX, 311, 0);
    if (tab[i].flags & SF_SECRET)
        PutString(values[i][0] ? "********" : "(none)", y, VALX);
    else
        PutString(values[i], y, VALX);
}

static const char *__fastcall__ first_error(void)
{
    char *p = json_find("errors");

    if (p && *p == '[' && json_value(p + 1, line, sizeof line) && line[0])
        return line;
    if (rest_error[0])
        return rest_error;
    switch (rest_status) {
    case 403: return "Wrong network password";
    case 404: return "Not available on this Ultimate";
    case 0:   return "Web Remote Control not reachable";
    }
    sprintf(line, "HTTP error %u", rest_status);
    return line;
}

static void __fastcall__ load_row(unsigned char i)
{
    char *p;

    rest_config_path(path, tab[i].cat, tab[i].item, NULL);
    if (rest_call("GET", path) == REST_OK &&
        (p = json_find("current")) != NULL)
        json_value(p, values[i], VALLEN);
    else
        strcpy(values[i], rest_status == 404 ? "n/a" : "?");
}

static unsigned char __fastcall__ is_allowed(const char *allowed,
                                             const char *v)
{
    unsigned char n = strlen(v);
    const char *p = allowed;

    if (!allowed)
        return 1;
    while (*p) {
        if (strncmp(p, v, n) == 0 && (p[n] == '|' || p[n] == 0))
            return 1;
        while (*p && *p != '|')
            ++p;
        if (*p)
            ++p;
    }
    return 0;
}

unsigned char __fastcall__ settab_put(const char *cat, const char *item,
                                      const char *value)
{
    rest_config_path(path, cat, item, value);
    return rest_call("PUT", path);
}

static unsigned char not_connected(void)
{
    if (connected || offline)
        return 0;
    settab_message("Not connected: see file / ultimate IP");
    return 1;
}

/* offline: the choice after cur in "A|B|C" (the first if cur is not in
   the list) into next; 0 if there are no choices */
static unsigned char __fastcall__ next_choice(const char *list,
                                              const char *cur)
{
    const char *p = list, *start;
    unsigned char n, found = 0;

    next[0] = 0;
    if (!list)
        return 0;
    while (*p) {
        start = p;
        while (*p && *p != '|')
            ++p;
        n = p - start;
        if (found || !next[0]) {
            memcpy(next, start, n);
            next[n] = 0;
            if (found)
                return 1;
        }
        if (strlen(cur) == n && strncmp(start, cur, n) == 0)
            found = 1;
        if (*p)
            ++p;
    }
    /* cur was the last one or unknown: wrap to the first */
    p = list;
    for (n = 0; p[n] && p[n] != '|'; ++n)
        ;
    memcpy(next, list, n);
    next[n] = 0;
    return 1;
}

static void __fastcall__ offline_change(unsigned char i)
{
    const struct setting *s = &tab[i];

    if (s->flags & SF_STRING) {
        strcpy(next, (s->flags & SF_SECRET) ? "" : values[i]);
        if (DlgBoxGetString(next, 31, CBOLDON "New value for:",
                            s->label) == CANCEL)
            return;
    } else if (!next_choice(s->choices, values[i])) {
        settab_message("Not changeable offline");
        return;
    }
    settab_progress("Writing config file...", 1, 2);
    ucfg_begin();
    ucfg_add(s->cat, s->item, next);
    if (ucfg_apply() != UCFG_OK) {
        settab_message(ucfg_msg);
        return;
    }
    strncpy(values[i], next, VALLEN - 1);
    draw_row(i);
    settab_message((s->flags & SF_REBOOT) ?
                   "Set via config file; * = after reboot" :
                   "Set via config file");
}

static void offline_load(void)
{
    unsigned char i;

    for (i = 0; i < count; ++i)
        strcpy(values[i], "-");
    if (settab_offline_values)
        settab_offline_values(values);
    for (i = 0; i < count; ++i)
        draw_row(i);
    settab_message("Offline: click to set values via config file");
}

static void __fastcall__ change_row(unsigned char i)
{
    const struct setting *s = &tab[i];
    char *p;
    int v, max;

    if (not_connected())
        return;
    if (offline) {
        offline_change(i);
        return;
    }
    if (s->flags & SF_STRING) {
        strcpy(next, (s->flags & SF_SECRET) ? "" : values[i]);
        if (DlgBoxGetString(next, 31, CBOLDON "New value for:",
                            s->label) == CANCEL)
            return;
    } else {
        settab_progress("Reading choices...", 0, 3);
        rest_config_path(path, s->cat, s->item, NULL);
        if (rest_call("GET", path) != REST_OK) {
            settab_message(first_error());
            return;
        }
        p = json_find("current");
        json_value(p ? p : "", cur, sizeof cur);
        next[0] = 0;
        if ((p = json_find("values")) != NULL && *p == '[') {
            /* enumeration: the allowed value after the current one */
            unsigned char found = 0;
            first[0] = 0;
            ++p;
            while ((p = json_value(p, tmp, sizeof tmp)) != NULL) {
                if (!is_allowed(s->allowed, tmp))
                    continue;
                if (!first[0])
                    strcpy(first, tmp);
                if (found) {
                    strcpy(next, tmp);
                    break;
                }
                if (strcmp(tmp, cur) == 0)
                    found = 1;
            }
            if (!next[0])
                strcpy(next, first);
        } else if ((p = json_find("max")) != NULL) {
            /* number: count up, wrap to min */
            json_value(p, tmp, sizeof tmp);
            max = atoi(tmp);
            v = atoi(cur) + 1;
            if (v > max) {
                p = json_find("min");
                json_value(p ? p : "0", tmp, sizeof tmp);
                v = atoi(tmp);
            }
            sprintf(next, "%d", v);
        }
        if (!next[0]) {
            settab_message("This setting cannot be changed here");
            return;
        }
    }
    settab_progress("Writing...", 1, 3);
    if (settab_put(s->cat, s->item, next) != REST_OK) {
        settab_message(first_error());
        return;
    }
    settab_progress("Reading back...", 2, 3);
    load_row(i);
    draw_row(i);
    settab_message((s->flags & SF_REBOOT) ?
                   "Changed. * = active after save + reboot" :
                   "Changed (until reboot; use file/save)");
}

static void on_click(void)
{
    char y = mousePos.y;
    unsigned char i;

    if (mouseData & 0x80)       /* button released */
        return;
    if (y < ROW0 - 9)
        return;
    i = (y - (ROW0 - 9)) / ROWH;
    if (i < count)
        change_row(i);
}

static void draw_screen(void)
{
    unsigned char i;

    fill(15, 199, 0, 319, 0);
    fill(16, 198, 4, 315, 9);           /* light frame pattern */
    fill(20, 182, 8, 311, 0);
    fill(186, 197, 8, 311, 0);
    draw_title();
    for (i = 0; i < count; ++i) {
        sprintf(line, "%s%s", tab[i].label,
                (tab[i].flags & SF_REBOOT) ? " *" : "");
        PutString(line, ROW0 + i * ROWH, 16);
        draw_row(i);
    }
}

/* one request per category: GET /v1/configs/<cat> returns the current
   value of every item in it */
void settab_reload(void)
{
    static unsigned char done[SETTAB_MAX];
    unsigned char i, j, ok, cats, step;
    char *p;

    GotoFirstMenu();
    if (not_connected())
        return;
    if (offline) {
        offline_load();
        return;
    }
    memset(done, 0, sizeof done);
    for (cats = 0, i = 0; i < count; ++i) {
        for (j = 0; j < i && strcmp(tab[j].cat, tab[i].cat) != 0; ++j)
            ;
        cats += (j == i);
    }
    for (step = 0, i = 0; i < count; ++i) {
        if (done[i])
            continue;
        settab_progress("Reading settings...", step++, cats);
        rest_config_path(path, tab[i].cat, NULL, NULL);
        ok = rest_call("GET", path) == REST_OK;
        for (j = i; j < count; ++j) {
            if (done[j] || strcmp(tab[j].cat, tab[i].cat) != 0)
                continue;
            done[j] = 1;
            if (ok && (p = json_find(tab[j].item)) != NULL)
                json_value(p, values[j], VALLEN);
            else
                strcpy(values[j], ok || rest_status == 404 ? "n/a" : "?");
            draw_row(j);
        }
        if (!ok && rest_status == 0) {          /* connection lost */
            settab_message(first_error());
            return;
        }
    }
    settab_message("Click a line to change it.");
}

/* offline preset: all entries in one config file; value NULL = the last
   choice of the matching screen line */
static void __fastcall__ offline_apply(const char *name,
                                       const struct preset_item *p,
                                       unsigned char n)
{
    unsigned char i, j;
    const char *v, *c;

    settab_progress(name, 1, 3);
    ucfg_begin();
    for (i = 0; i < n; ++i) {
        v = p[i].value;
        for (j = 0; j < count; ++j)
            if (strcmp(tab[j].cat, p[i].cat) == 0 &&
                strcmp(tab[j].item, p[i].item) == 0)
                break;
        if (!v && j < count && (c = tab[j].choices) != NULL) {
            v = strrchr(c, '|');        /* last choice */
            v = v ? v + 1 : c;
        }
        if (!v)
            continue;
        ucfg_add(p[i].cat, p[i].item, v);
        if (j < count) {
            strncpy(values[j], v, VALLEN - 1);
            draw_row(j);
        }
    }
    settab_progress(name, 2, 3);
    settab_message(ucfg_apply() == UCFG_OK ?
                   "Preset set via config file. Reboot for *" : ucfg_msg);
}

/* PUT every entry; value NULL = the last (highest) choice of the item */
void __fastcall__ settab_apply(const char *name,
                               const struct preset_item *p, unsigned char n)
{
    unsigned char i, errors = 0;
    char *q;

    if (not_connected())
        return;
    if (offline) {
        offline_apply(name, p, n);
        return;
    }
    for (i = 0; i < n; ++i, ++p) {
        settab_progress(name, i, n);
        if (p->value) {
            strcpy(next, p->value);
        } else {
            next[0] = 0;
            rest_config_path(path, p->cat, p->item, NULL);
            if (rest_call("GET", path) == REST_OK &&
                (q = json_find("values")) != NULL && *q == '[')
                for (++q; (q = json_value(q, tmp, sizeof tmp)) != NULL; )
                    strcpy(next, tmp);
            if (!next[0]) {
                ++errors;
                continue;
            }
        }
        if (settab_put(p->cat, p->item, next) != REST_OK)
            ++errors;
    }
    settab_reload();
    if (errors) {
        sprintf(line, "Preset set, %u setting(s) failed", errors);
        settab_message(line);
    } else {
        settab_message("Preset set. Use file/save, then reboot.");
    }
}

/* "not reachable" box with the result for each address tried */
typedef void dlgBoxStr;
static char hostln[3][40];
static const dlgBoxStr probeBox = {
    DB_DEFPOS(1),
    DB_TXTSTR(TXT_LN_X, TXT_LN_1_Y, CBOLDON "Web service not found:"),
    DB_TXTSTR(TXT_LN_X, TXT_LN_2_Y, hostln[0]),
    DB_TXTSTR(TXT_LN_X, TXT_LN_3_Y, hostln[1]),
    DB_TXTSTR(TXT_LN_X, TXT_LN_4_Y, hostln[2]),
    DB_ICON(OK, DBI_X_0, DBI_Y_2),
    DB_END
};

static void do_connect(void)
{
    unsigned char i;

    connected = 0;
    offline = 0;
    draw_title();
    settab_progress("Connecting to the Ultimate...", 0, 1);
    if (rest_probe() == REST_OK) {
        connected = 1;
        draw_title();
        settab_reload();
        return;
    }
    draw_title();
    for (i = 0; i < 3; ++i) {
        if (i < rest_nhosts)
            sprintf(hostln[i], "%s: %s", rest_hosts[i], rest_herr[i]);
        else
            hostln[i][0] = 0;
    }
    DoDlgBox(&probeBox);
    if (ucfg_available()) {
        /* fallback: settings can still be written via a config file */
        offline = 1;
        draw_title();
        DlgBoxOk(CBOLDON "Offline mode",
                 "Values unknown; changes via config file");
        offline_load();
    } else {
        settab_message("Enable Web Remote Control or set file/ultimate IP");
    }
}

void settab_host(void)
{
    GotoFirstMenu();
    strcpy(next, rest_host);
    if (DlgBoxGetString(next, 15, CBOLDON "IP address of the Ultimate",
                        "(see its menu: Network / Ethernet)") == CANCEL)
        return;
    strcpy(rest_hosts[0], next);
    rest_nhosts = 1;
    do_connect();
}

void settab_save(void)
{
    GotoFirstMenu();
    if (not_connected())
        return;
    if (offline) {
        settab_message("Offline: save in the Ultimate menu");
        return;
    }
    settab_progress("Saving to flash...", 0, 1);
    settab_message(rest_call("PUT", "/v1/configs:save_to_flash") == REST_OK ?
                   "Saved. Settings stay after power off." :
                   first_error());
}

void settab_reboot(void)
{
    GotoFirstMenu();
    if (not_connected())
        return;
    if (DlgBoxYesNo(CBOLDON "Reboot the Ultimate now?",
                    "GEOS ends; unsaved files are lost.") != YES)
        return;
    if (offline)
        ucfg_reboot();
    else
        rest_call("PUT", "/v1/machine:reboot");
}

void settab_password(void)
{
    GotoFirstMenu();
    DlgBoxGetString(rest_pass, sizeof rest_pass - 1,
                    CBOLDON "Ultimate Network Password", "(empty if none)");
    do_connect();
}

void settab_quit(void)
{
    EnterDeskTop();
}

void __fastcall__ settab_start(const char *t, const struct setting *s,
                               unsigned char n, struct menu *menu)
{
    unsigned char e;

    title = t;
    tab = s;
    count = n;
    /* draw into the background buffer too, so dialog boxes and menus are
       restored correctly when they close */
    dispBufferOn = ST_WR_FORE | ST_WR_BACK;

    e = rest_init();
    if (e != REST_OK) {
        DlgBoxOk(CBOLDON "No Ultimate network",
                 e == REST_NO_KERNAL ? "Boot the GEOS-U64 kernal." :
                 e == REST_NO_UCI ? "Enable the Command Interface." :
                 "Connect LAN or WiFi first.");
        EnterDeskTop();
    }
    draw_screen();
    DoMenu(menu);
    otherPressVec = on_click;
    do_connect();
#ifdef REST_FAKE
    change_row(0);              /* test: Off -> TurboEnable Bit */
    settab_message(rest_host);  /* request count of the fake server */
#endif
}
