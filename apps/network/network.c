/* Network - network configuration of the C64 Ultimate from GEOS.

   Ethernet (DHCP or static), host name, password, services and
   internet time, through the Ultimate REST API. "Web Remote Control
   Service" is not offered as "Disabled": this program needs it. */

#include <geos.h>
#include <string.h>
#include <stdio.h>
#include "../common/ultnet.h"
#include "../common/rest.h"
#include "../common/settab.h"

#define ETH "Ethernet Settings"
#define NET "Network Settings"

#define EN_DIS "Disabled|Enabled"

/* choices: offline value lists, from the firmware's config definitions */
static const struct setting tab[] = {
    { "Use DHCP",             ETH, "Use DHCP",       NULL, EN_DIS, 0 },
    { "Static IP",            ETH, "Static IP",      NULL, NULL, SF_STRING },
    { "Static Netmask",       ETH, "Static Netmask", NULL, NULL, SF_STRING },
    { "Static Gateway",       ETH, "Static Gateway", NULL, NULL, SF_STRING },
    { "Static DNS",           ETH, "Static DNS",     NULL, NULL, SF_STRING },
    { "Host Name",            NET, "Host Name",      NULL, NULL, SF_STRING },
    { "Network Password",     NET, "Network Password", NULL, NULL,
      SF_STRING | SF_SECRET },
    { "Web Remote Control",   NET, "Web Remote Control Service",
      "Enabled", "Enabled", 0 },
    { "FTP File Service",     NET, "FTP File Service", NULL, EN_DIS, 0 },
    { "Telnet Remote Menu",   NET, "Telnet Remote Menu Service", NULL,
      EN_DIS, 0 },
    { "Internet Time (SNTP)", NET, "SNTP Enable",    NULL, EN_DIS, 0 },
    { "Time Zone",            NET, "TimeZone",       NULL, NULL, 0 },
    { "Time Server",          NET, "Time Server 1",  NULL, NULL, SF_STRING },
};

static void about(void);
static void test_internet(void);

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

static struct menu testMenu = {
    { 15, 15 + 14, 48, 150 }, 1 | VERTICAL,
    { { "internet", MENU_ACTION, test_internet } }
};

static struct menu mainMenu = {
    { 0, 14, 0, 80 }, 3 | HORIZONTAL,
    {
        { "geos", SUB_MENU, &geosMenu },
        { "file", SUB_MENU, &fileMenu },
        { "test", SUB_MENU, &testMenu },
    }
};

static void about(void)
{
    GotoFirstMenu();
    DlgBoxOk(CBOLDON "Network V1.0" CPLAINTEXT,
             "C64 Ultimate network setup for GEOS-U64");
}

/* opens a TCP connection to a well known host and port */
static void test_internet(void)
{
    static char msg[40];
    unsigned r;

    GotoFirstMenu();
    settab_message("Connecting to example.com:80 ...");
    R0 = (unsigned)"example.com";
    R1 = 80;
    r = ultcall(0, NetTCPConnect);
    if (ULT_ERR(r)) {
        ultcall(0, UltGetStatus);
        strncpy(msg, (const char *)R0, sizeof msg - 1);
        settab_message("No internet connection");
        DlgBoxOk(CBOLDON "No internet connection", msg);
        return;
    }
    ultcall(ULT_VAL(r), NetClose);
    settab_message("Internet OK (DNS + TCP to example.com)");
}

void main(void)
{
    settab_start("Network", tab, sizeof tab / sizeof tab[0], &mainMenu);
    MainLoop();
}
