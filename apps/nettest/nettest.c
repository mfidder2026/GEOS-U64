/* NetTest - checks the GEOS-U64 network API on a C64 Ultimate.

   1. kernal signature "UNET" at $FE7A
   2. Ultimate command interface + turbo registers
   3. IP address of the first interface that has one
   4. HTTP/1.0 request to example.com, shows the status line */

#include <geos.h>
#include <string.h>
#include <stdio.h>
#include "../common/ultnet.h"

#define HOST "example.com"

static char line1[48], line2[48];
static unsigned char ipbuf[12];
static char rxbuf[256];
static const char request[] =
    "GET / HTTP/1.0\r\nHost: " HOST "\r\nConnection: close\r\n\r\n";

static const char *status(void)
{
    ultcall(0, UltGetStatus);
    return (const char *)R0;
}

static void ok(void)
{
    DlgBoxOk(line1, line2);
}

void main(void)
{
    unsigned r, n, tries;
    unsigned char ifs, i, sock, *turbo;
    char *eol;

    if (memcmp(ULT_SIGNATURE, "UNET", 4) != 0) {
        DlgBoxOk(CBOLDON "Not the GEOS-U64 kernal.", "Boot GEOS from the U64 disk.");
        return;
    }

    r = ultcall(0, UltDetect);
    turbo = (unsigned char *)R0;
    if (turbo[1] == 0xFF)
        strcpy(line2, "Turbo: no registers (see guide)");
    else
        sprintf(line2, "Turbo: %s, speed %u%s", (turbo[0] & 1) ? "on" : "off",
                turbo[1] & 15, (turbo[1] & 0x80) ? ", no badlines" : "");
    if (ULT_ERR(r)) {
        strcpy(line1, CBOLDON "No Ultimate command interface");
        ok();
        return;
    }
    ifs = ULT_VAL(r);
    sprintf(line1, CBOLDON "UCI ok, %u network interface(s)", ifs);
    ok();

    /* every interface (Ethernet, WiFi): IP and gateway */
    n = 0;
    for (i = 0; i < ifs; ++i) {
        R0 = (unsigned)ipbuf;
        memset(ipbuf, 0, sizeof ipbuf);
        r = ultcall(i, NetGetIP);
        if (ULT_ERR(r))
            continue;
        sprintf(line1, CBOLDON "Interface %u: IP %u.%u.%u.%u", i, ipbuf[0],
                ipbuf[1], ipbuf[2], ipbuf[3]);
        sprintf(line2, "Gateway %u.%u.%u.%u", ipbuf[8], ipbuf[9],
                ipbuf[10], ipbuf[11]);
        ok();
        n += ipbuf[0] != 0;
    }
    if (!n) {
        strcpy(line1, CBOLDON "No IP address");
        strcpy(line2, "Check LAN/WiFi in the Ultimate menu");
        ok();
        return;
    }

    /* the Ultimate's own web service (used by U64 Setup and Network) */
    R0 = (unsigned)"127.0.0.1";
    R1 = 80;
    r = ultcall(0, NetTCPConnect);
    if (ULT_ERR(r)) {
        strcpy(line1, CBOLDON "Web service 127.0.0.1:80 not reachable");
        strncpy(line2, status(), 40);
    } else {
        ultcall(ULT_VAL(r), NetClose);
        strcpy(line1, CBOLDON "Web service 127.0.0.1:80 OK");
        strcpy(line2, "U64 Setup and Network can connect");
    }
    ok();

    R0 = (unsigned)HOST;
    R1 = 80;
    r = ultcall(0, NetTCPConnect);
    if (ULT_ERR(r)) {
        strcpy(line1, CBOLDON "Connect to " HOST " failed");
        strncpy(line2, status(), 40);
        ok();
        return;
    }
    sock = ULT_VAL(r);

    R0 = (unsigned)request;
    R1 = sizeof request - 1;
    r = ultcall(sock, NetWrite);
    if (ULT_ERR(r)) {
        strcpy(line1, CBOLDON "Write failed");
        strncpy(line2, status(), 40);
        ok();
        ultcall(sock, NetClose);
        return;
    }

    n = NET_NO_DATA;
    for (tries = 0; tries < 5000 && n == NET_NO_DATA; ++tries) {
        R0 = (unsigned)rxbuf;
        R1 = sizeof rxbuf - 1;
        r = ultcall(sock, NetRead);
        n = R1;
        if (ULT_ERR(r))
            break;
    }
    ultcall(sock, NetClose);

    if (n == NET_NO_DATA || n == 0) {
        strcpy(line1, CBOLDON "No answer from " HOST);
        strcpy(line2, "");
    } else {
        rxbuf[n] = 0;
        if ((eol = strchr(rxbuf, '\r')) != NULL)
            *eol = 0;
        rxbuf[40] = 0;
        strcpy(line1, CBOLDON "HTTP answer:");
        strcpy(line2, rxbuf);
    }
    ok();
}
