/* GEOS-U64: a screen of Ultimate settings, one per line.
   Clicking a line selects the next value (enumerations, numbers) or
   opens a text input box (strings). Values go through the REST API. */

#ifndef SETTAB_H
#define SETTAB_H

#define SF_STRING   0x01    /* free text, edited in a dialog box */
#define SF_REBOOT   0x02    /* active after "save" + "reboot" (shows *) */
#define SF_SECRET   0x04    /* do not display the value */

struct setting {
    const char *label;
    const char *cat;        /* REST category, e.g. "U64 Specific Settings" */
    const char *item;       /* REST item, e.g. "CPU Speed" */
    const char *allowed;    /* NULL = every value, else "A|B|C" */
    const char *choices;    /* offline (no REST): values to cycle through,
                               "A|B|C"; NULL = not changeable offline */
    unsigned char flags;
};

/* offline: optional callback that fills in the values the C64 can see
   itself (turbo registers, REU size); values[i] has room for 24 chars */
extern void (*settab_offline_values)(char values[][24]);

#define SETTAB_MAX 13

/* sets up the screen and the menu; returns after the first load,
   the application then calls MainLoop() */
void __fastcall__ settab_start(const char *title,
                               const struct setting *tab,
                               unsigned char count,
                               struct menu *menu);

/* one setting of a preset; value NULL = the item's highest choice */
struct preset_item {
    const char *cat;
    const char *item;
    const char *value;
};

/* applies a preset with a progress bar, then reloads the screen */
void __fastcall__ settab_apply(const char *name,
                               const struct preset_item *p, unsigned char n);

/* status text with a progress bar done/total */
void __fastcall__ settab_progress(const char *text, unsigned char done,
                                  unsigned char total);

/* menu actions shared by the applications */
void settab_host(void);
void settab_reload(void);
void settab_save(void);
void settab_reboot(void);
void settab_password(void);
void settab_quit(void);

/* status line at the bottom of the screen */
void __fastcall__ settab_message(const char *text);

/* PUT one value without the table (presets); returns REST_OK or error */
unsigned char __fastcall__ settab_put(const char *cat, const char *item,
                                      const char *value);

#endif
