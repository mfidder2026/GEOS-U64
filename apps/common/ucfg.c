/* GEOS-U64: settings through a config file, see ucfg.h */

#include <geos.h>
#include <string.h>
#include "ultnet.h"
#include "ucfg.h"

#define TARGET_DOS1     0x01
#define TARGET_CONTROL  0x04
#define DOS_OPEN_FILE   0x02
#define DOS_CLOSE_FILE  0x03
#define DOS_WRITE_DATA  0x05
#define DOS_CHANGE_DIR  0x11
#define CTRL_REBOOT     0x06
#define CTRL_LOAD_CONFIG 0x50
#define FA_CREATE_WRITE 0x0E    /* FA_WRITE | FA_CREATE_NEW | FA_CREATE_ALWAYS */
#define CHUNK           480     /* file bytes per WRITE_DATA command */

char ucfg_msg[48];

static char text[900];          /* the config file */
static unsigned len;
static const char *lastcat;
static unsigned char cmd[4 + CHUNK];
static char reply[96];

unsigned char ucfg_available(void)
{
    return memcmp(ULT_SIGNATURE, "UNET", 4) == 0 && ULT_VERSION >= 2;
}

void ucfg_begin(void)
{
    len = 0;
    text[0] = 0;
    lastcat = NULL;
}

static void __fastcall__ put(const char *s)
{
    unsigned n = strlen(s);

    if (len + n < sizeof text) {
        memcpy(text + len, s, n);
        len += n;
    }
}

void __fastcall__ ucfg_add(const char *cat, const char *item,
                           const char *value)
{
    if (!lastcat || strcmp(cat, lastcat) != 0) {
        put("[");
        put(cat);
        put("]\n");
        lastcat = cat;
    }
    put(item);
    put("=");
    put(value);
    put("\n");
}

/* send cmd[0..n-1]; returns 0 if the Ultimate answered "00,OK" */
static unsigned char __fastcall__ send(unsigned n)
{
    unsigned r;

    R0 = (unsigned)cmd;
    R1 = n;
    R2 = (unsigned)reply;
    R3 = sizeof reply - 1;
    r = ultcall(0, UltCommand);
    reply[R1 & 0xff] = 0;
    if (!ULT_ERR(r))
        return 0;
    ultcall(0, UltGetStatus);
    strncpy(ucfg_msg, (const char *)R0, sizeof ucfg_msg - 1);
    return 1;
}

static unsigned __fastcall__ build(unsigned char target, unsigned char c,
                                   const char *arg)
{
    unsigned n = strlen(arg);

    cmd[0] = target;
    cmd[1] = c;
    memcpy(cmd + 2, arg, n);
    return n + 2;
}

unsigned char ucfg_apply(void)
{
    unsigned off, n;
    char *nl;

    ucfg_msg[0] = 0;
    if (!ucfg_available()) {
        strcpy(ucfg_msg, "Kernal without UltCommand");
        return UCFG_NO_API;
    }
    /* /temp/uci_config.cfg is the file LOAD_CONFIG reads by default */
    if (send(build(TARGET_DOS1, DOS_CHANGE_DIR, "/temp")))
        return UCFG_FILE_ERR;
    n = build(TARGET_DOS1, DOS_OPEN_FILE, "?uci_config.cfg");
    cmd[2] = FA_CREATE_WRITE;
    if (send(n))
        return UCFG_FILE_ERR;
    for (off = 0; off < len; off += n) {
        n = len - off > CHUNK ? CHUNK : len - off;
        cmd[0] = TARGET_DOS1;
        cmd[1] = DOS_WRITE_DATA;
        cmd[2] = cmd[3] = 0;
        memcpy(cmd + 4, text + off, n);
        if (send(n + 4)) {
            build(TARGET_DOS1, DOS_CLOSE_FILE, "");
            send(2);
            return UCFG_FILE_ERR;
        }
    }
    if (send(build(TARGET_DOS1, DOS_CLOSE_FILE, "")))
        return UCFG_FILE_ERR;

    if (send(build(TARGET_CONTROL, CTRL_LOAD_CONFIG, ""))) {
        /* the reply holds the log of lines that could not be applied */
        if (reply[0]) {
            if ((nl = strchr(reply, '\n')) != NULL)
                *nl = 0;
            strncpy(ucfg_msg, reply, sizeof ucfg_msg - 1);
        }
        return UCFG_LOAD_ERR;
    }
    strcpy(ucfg_msg, "Set via config file");
    return UCFG_OK;
}

void ucfg_reboot(void)
{
    send(build(TARGET_CONTROL, CTRL_REBOOT, ""));
}
