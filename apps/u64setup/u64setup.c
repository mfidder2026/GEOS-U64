/* U64 Setup - change the C64 Ultimate's own settings from GEOS.

   Uses the Ultimate REST API (/v1/configs) through the GEOS-U64
   kernal network API. Turbo Control only offers the modes that are
   safe for GEOS disk access. */

#include <geos.h>
#include <string.h>
#include "../common/rest.h"
#include "../common/settab.h"
#include "../common/ultnet.h"
#include <stdio.h>

#define U64  "U64 Specific Settings"
#define C64  "C64 and Cartridge Settings"
#define DRVA "Drive A Settings"
#define DRVB "Drive B Settings"

#define EN_DIS "Disabled|Enabled"

/* choices: offline value lists, from the firmware's config definitions */
static const struct setting tab[] = {
    { "Turbo Control",       U64,  "Turbo Control", "Off|TurboEnable Bit",
      "Off|TurboEnable Bit", 0 },
    { "CPU Speed (MHz)",     U64,  "CPU Speed", NULL,
      "1|2|3|4|6|8|10|12|14|16|20|24|32|40|48|64", 0 },
    { "Badline Timing",      U64,  "Badline Timing", NULL, EN_DIS, 0 },
    { "RAM Expansion (REU)", C64,  "RAM Expansion Unit", NULL,
      "Disabled|Enabled|GeoRAM Mode", SF_REBOOT },
    { "REU / GeoRAM Size",   C64,  "REU Size", NULL,
      "128 KB|256 KB|512 KB|1 MB|2 MB|4 MB|8 MB|16 MB", SF_REBOOT },
    { "Command Interface",   C64,  "Command Interface", "Enabled", NULL, 0 },
    { "Fast Reset",          C64,  "Fast Reset", NULL, EN_DIS, 0 },
    { "USB Mouse Mode",      U64,  "Mouse Mode", NULL,
      "Cursor|Mouse|Mouse + Cursor|Mouse + Wheel", 0 },
    { "Joystick Swapper",    U64,  "Joystick Swapper", NULL,
      "Normal|Swapped", 0 },
    { "Drive A Type",        DRVA, "Drive Type", NULL, "1541|1571|1581",
      SF_REBOOT },
    { "Drive B",             DRVB, "Drive", NULL, EN_DIS, SF_REBOOT },
    { "Drive B Type",        DRVB, "Drive Type", NULL, "1541|1571|1581",
      SF_REBOOT },
    { "Drive B Bus ID",      DRVB, "Drive Bus ID", NULL, "8|9|10|11",
      SF_REBOOT },
};

/* offline: what the C64 itself can see. $D031 (U64 turbo register)
   reads $FF unless Turbo Control is "TurboEnable Bit" or "U64 Turbo
   Registers"; $D030 only reads back in "TurboEnable Bit" mode. */
static void offline_values(char values[][24])
{
    static const char *const mhz[16] = {
        "1", "2", "3", "4", "6", "8", "10", "12",
        "14", "16", "20", "24", "32", "40", "48", "64" };
    unsigned char d030, d031, banks = *(unsigned char *)0x88c3;

    ultcall(0, UltDetect);
    d030 = ((unsigned char *)R0)[0];
    d031 = ((unsigned char *)R0)[1];
    if (d031 == 0xff) {
        strcpy(values[0], "Off or Manual");
    } else {
        strcpy(values[0], d030 == 0xff ? "U64 Turbo Registers" :
                                         "TurboEnable Bit");
        strcpy(values[1], mhz[d031 & 15]);    /* U64E2/C64U table */
        strcpy(values[2], (d031 & 0x80) ? "Disabled" : "Enabled");
    }
    strcpy(values[3], banks ? "Enabled" : "Disabled or GeoRAM");
    if (banks)
        sprintf(values[4], banks >= 8 ? "512 KB or more" : "%u KB",
                banks * 64);
    strcpy(values[5], "Enabled");
}

static void about(void);
static void preset_geos(void);
static void preset_c64(void);

static struct menu geosMenu = {
    { 15, 15 + 14, 0, 70 }, 1 | VERTICAL,
    { { "about", MENU_ACTION, about } }
};

static struct menu fileMenu = {
    { 15, 15 + 6 * 14, 26, 130 }, 6 | VERTICAL,
    {
        { "reload",          MENU_ACTION, settab_reload },
        { "save to flash",   MENU_ACTION, settab_save },
        { "ultimate IP",     MENU_ACTION, settab_host },
        { "password",        MENU_ACTION, settab_password },
        { "reboot ultimate", MENU_ACTION, settab_reboot },
        { "quit",            MENU_ACTION, settab_quit },
    }
};

static struct menu presetMenu = {
    { 15, 15 + 2 * 14, 48, 180 }, 2 | VERTICAL,
    {
        { "GEOS optimal",        MENU_ACTION, preset_geos },
        { "Commodore 64 default", MENU_ACTION, preset_c64 },
    }
};

static struct menu mainMenu = {
    { 0, 14, 0, 96 }, 3 | HORIZONTAL,
    {
        { "geos",    SUB_MENU, &geosMenu },
        { "file",    SUB_MENU, &fileMenu },
        { "presets", SUB_MENU, &presetMenu },
    }
};

static void about(void)
{
    GotoFirstMenu();
    DlgBoxOk(CBOLDON "U64 Setup V1.1" CPLAINTEXT,
             "C64 Ultimate settings for GEOS-U64");
}

/* what GEOS-U64 needs, plus the fastest CPU speed of this model */
static const struct preset_item geos[] = {
    { U64,  "Turbo Control",      "TurboEnable Bit" },
    { U64,  "CPU Speed",          NULL },           /* highest */
    { U64,  "Badline Timing",     "Disabled" },
    { C64,  "Command Interface",  "Enabled" },
    { C64,  "RAM Expansion Unit", "Enabled" },
    { C64,  "REU Size",           "512 KB" },
    { U64,  "Mouse Mode",         "Mouse" },
    { DRVA, "Drive Type",         "1541" },
};

/* a plain Commodore 64: 1 MHz, badlines, no REU, standard drives. The
   Command Interface stays on, or this program could not talk to the
   Ultimate any more. */
static const struct preset_item c64[] = {
    { U64,  "Turbo Control",      "Off" },
    { U64,  "CPU Speed",          " 1" },
    { U64,  "Badline Timing",     "Enabled" },
    { C64,  "RAM Expansion Unit", "Disabled" },
    { C64,  "Fast Reset",         "Disabled" },
    { U64,  "Joystick Swapper",   "Normal" },
    { DRVA, "Drive Type",         "1541" },
    { DRVB, "Drive",              "Disabled" },
};

static void preset_geos(void)
{
    GotoFirstMenu();
    if (DlgBoxYesNo(CBOLDON "Apply the GEOS optimal settings?",
                    "Turbo, REU 512 KB, 1351 mouse") != YES)
        return;
    settab_apply("Applying GEOS optimal...", geos,
                 sizeof geos / sizeof geos[0]);
}

static void preset_c64(void)
{
    GotoFirstMenu();
    if (DlgBoxYesNo(CBOLDON "Apply the Commodore 64 defaults?",
                    "1 MHz, no REU (after reboot)") != YES)
        return;
    settab_apply("Applying C64 default...", c64, sizeof c64 / sizeof c64[0]);
}

void main(void)
{
    settab_offline_values = offline_values;
    settab_start("U64 Setup", tab, sizeof tab / sizeof tab[0], &mainMenu);
#ifdef REST_FAKE
    settab_apply("Applying C64 default...", c64, sizeof c64 / sizeof c64[0]);
#endif
    MainLoop();
}
