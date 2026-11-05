/* GEOS-U64: minimal HTTP/REST client, see rest.h */

#include <geos.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "ultnet.h"
#include "rest.h"

char rest_host[16];
char rest_hosts[REST_MAX_HOSTS][16];
char rest_herr[REST_MAX_HOSTS][24];
unsigned char rest_nhosts;
char rest_pass[24];
char rest_resp[REST_RESP_SIZE];
char rest_error[40];
char *rest_body;
unsigned rest_status;

static char request[320];
static unsigned char ipbuf[12];

#ifdef REST_FAKE
/* test builds for VICE (no Ultimate): canned answers in firmware format */
static const char fake_item[] =
    "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n\r\n"
    "{\"Test\" : {\"Item\" : {\"current\" : \"%s\", \"values\" : "
    "[ \"Off\", \"TurboEnable Bit\", \" 48\", \" 64\", \"Enabled\" ], "
    "\"default\" : \"Off\" } }, \"errors\" : [ ] }";
static const char fake_cat[] =
    "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n\r\n"
    "{\"Cat\" : {\"Turbo Control\" : \"%s\", \"CPU Speed\" : \" 48\", "
    "\"Badline Timing\" : \"Disabled\", \"RAM Expansion Unit\" : \"Enabled\", "
    "\"REU Size\" : \"512 KB\", \"Command Interface\" : \"Enabled\", "
    "\"Fast Reset\" : \"Disabled\", \"Mouse Mode\" : \"Mouse\", "
    "\"Joystick Swapper\" : \"Normal\", \"Drive Type\" : \"1541\", "
    "\"Drive\" : \"Enabled\", \"Drive Bus ID\" : 9 }, \"errors\" : [ ] }";
static char fake_cur[24] = "Off";
static unsigned fake_calls;     /* requests so far, shown by the test */

unsigned char rest_init(void)
{
    strcpy(rest_host, "10.0.0.64");
    return REST_OK;
}

unsigned char rest_probe(void)
{
#ifdef REST_FAKE_FAIL           /* test the "not reachable" box */
    strcpy(rest_hosts[0], "127.0.0.1");
    strcpy(rest_herr[0], "refused (111)");
    strcpy(rest_hosts[1], "192.168.0.69");
    strcpy(rest_herr[1], "timeout (110)");
    strcpy(rest_hosts[2], "192.168.2.64");
    strcpy(rest_herr[2], "no route (113)");
    rest_nhosts = 3;
    return REST_NO_CONNECT;
#else
    return REST_OK;
#endif
}

unsigned char __fastcall__ rest_call(const char *method, const char *path)
{
    char *v = strstr(path, "?value=");
    char *o = fake_cur;
    static char hex[3];

    if (method[0] == 'P' && v) {
        for (v += 7; *v; ++v) {         /* URL-decode like the server */
            if (*v == '%' && v[1] && v[2]) {
                hex[0] = v[1];
                hex[1] = v[2];
                *o++ = (char)strtoul(hex, NULL, 16);
                v += 2;
            } else {
                *o++ = *v;
            }
        }
        *o = 0;
    }
    ++fake_calls;
    /* "/v1/configs/<cat>" without an item: the whole category */
    sprintf(rest_resp, strchr(path + 12, '/') ? fake_item : fake_cat,
            fake_cur);
    sprintf(rest_host, "fake, %u calls", fake_calls);
    rest_status = 200;
    rest_body = strstr(rest_resp, "\r\n\r\n") + 4;
    return REST_OK;
}
#else
unsigned char rest_init(void)
{
    unsigned r;
    unsigned char ifs, i;

    if (memcmp(ULT_SIGNATURE, "UNET", 4) != 0)
        return REST_NO_KERNAL;
    r = ultcall(0, UltDetect);
    if (ULT_ERR(r))
        return REST_NO_UCI;
    /* Addresses to try, in this order:
       - 127.0.0.1: lwIP passes loopback traffic through any interface
         that is up, without needing a link (LWIP_NETIF_LOOPBACK)
       - the interfaces, last first: an own interface address only
         routes when that interface has a link, and Ethernet (interface
         0) keeps its static address without a cable (EHOSTUNREACH) */
    ifs = ULT_VAL(r);
    strcpy(rest_hosts[0], "127.0.0.1");
    rest_nhosts = 1;
    for (i = ifs; i-- > 0 && rest_nhosts < REST_MAX_HOSTS; ) {
        memset(ipbuf, 0, sizeof ipbuf);
        R0 = (unsigned)ipbuf;
        r = ultcall(i, NetGetIP);
        if (!ULT_ERR(r) && ipbuf[0])
            sprintf(rest_hosts[rest_nhosts++], "%u.%u.%u.%u", ipbuf[0],
                    ipbuf[1], ipbuf[2], ipbuf[3]);
    }
    if (rest_nhosts == 1)
        return REST_NO_IP;
    strcpy(rest_host, rest_hosts[0]);
    return REST_OK;
}

/* short reason for a failed probe of one address */
static void __fastcall__ probe_error(char *out)
{
    char *p = strrchr(rest_error, ':');
    int e;

    if (rest_status) {
        sprintf(out, "HTTP %u", rest_status);
    } else if (strstr(rest_error, "CONNECT") && p) {
        /* errno numbers of the firmware's C library (newlib) */
        e = atoi(p + 1);
        sprintf(out, e == 118 ? "no route (118)" :
                     e == 113 ? "aborted (113)" :
                     e == 111 ? "refused (111)" :
                     e == 104 ? "reset (104)" :
                     e == 116 ? "timeout (116)" : "connect error %d", e);
    } else {
        strncpy(out, rest_error[0] ? rest_error : "no answer", 22);
        out[22] = 0;
    }
}

unsigned char rest_probe(void)
{
    unsigned char i;

    for (i = 0; i < rest_nhosts; ++i) {
        strcpy(rest_host, rest_hosts[i]);
        if (rest_call("GET", "/v1/version") == REST_OK || rest_status == 403)
            return REST_OK;     /* 403: answers, but wants a password */
        probe_error(rest_herr[i]);
    }
    return REST_NO_CONNECT;
}

static void __fastcall__ set_error(const char *text)
{
    if (!text) {                /* the Ultimate's status of the last call */
        ultcall(0, UltGetStatus);
        text = (const char *)R0;
    }
    strncpy(rest_error, text, sizeof rest_error - 1);
}

/* value of the Content-Length header, or $FFFF if there is none */
static unsigned __fastcall__ content_length(const char *end)
{
    const char *p;

    for (p = rest_resp; p < end; ++p)
        if ((*p == 'C' || *p == 'c') && strnicmp(p, "content-length:", 15) == 0)
            return atoi(p + 15);
    return 0xFFFF;
}

static unsigned char __fastcall__ send_all(unsigned char sock, unsigned len)
{
    R0 = (unsigned)request;
    R1 = len;
    return ULT_ERR(ultcall(sock, NetWrite));
}

unsigned char __fastcall__ rest_call(const char *method, const char *path)
{
    unsigned r, n, len, idle, need;
    unsigned char sock, closed;
    char *hdr;

    rest_status = 0;
    rest_body = rest_resp;
    rest_resp[0] = 0;
    rest_error[0] = 0;

    R0 = (unsigned)rest_host;
    R1 = 80;
    r = ultcall(0, NetTCPConnect);
    if (ULT_ERR(r)) {
        set_error(NULL);
        return REST_NO_CONNECT;
    }
    sock = ULT_VAL(r);

    len = sprintf(request, "%s %s HTTP/1.0\r\nHost: %s\r\n", method, path,
                  rest_host);
    if (rest_pass[0])
        len += sprintf(request + len, "X-Password: %s\r\n", rest_pass);
    strcpy(request + len, "Content-Length: 0\r\n\r\n");
    len += 21;
    if (send_all(sock, len)) {
        set_error(NULL);
        ultcall(sock, NetClose);
        return REST_NO_CONNECT;
    }

    /* read until Content-Length is complete or the server closes; every
       empty poll waits 40 ms: ~6 s for the first byte, ~1 s later on */
    len = 0;
    idle = 0;
    closed = 0;
    hdr = NULL;
    need = 0xFFFF;
    while (len < REST_RESP_SIZE - 1 && len < need) {
        R0 = (unsigned)(rest_resp + len);
        n = REST_RESP_SIZE - 1 - len;
        R1 = n > 1472 ? 1472 : n;
        r = ultcall(sock, NetRead);
        if (ULT_ERR(r)) {
            set_error(NULL);
            break;
        }
        n = R1;
        if (n == 0) {
            closed = 1;
            break;
        }
        if (n == NET_NO_DATA) {
            if (++idle > (len ? 25 : 150)) {
                if (!len)
                    set_error("No answer from the web service");
                break;
            }
            continue;
        }
        idle = 0;
        len += n;
        rest_resp[len] = 0;
        if (!hdr && (hdr = strstr(rest_resp, "\r\n\r\n")) != NULL) {
            need = content_length(hdr);
            if (need != 0xFFFF)
                need += hdr + 4 - rest_resp;
        }
    }
    if (!closed)
        ultcall(sock, NetClose);
    rest_resp[len] = 0;

    if (memcmp(rest_resp, "HTTP/", 5) == 0)
        rest_status = atoi(rest_resp + 9);
    rest_body = strstr(rest_resp, "\r\n\r\n");
    rest_body = rest_body ? rest_body + 4 : rest_resp + len;
    return rest_status == 200 ? REST_OK : REST_HTTP_ERROR;
}
#endif

static char *__fastcall__ url_encode(char *out, const char *s)
{
    static const char hex[] = "0123456789ABCDEF";
    unsigned char c;

    while ((c = *s++) != 0) {
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.') {
            *out++ = c;
        } else {
            *out++ = '%';
            *out++ = hex[c >> 4];
            *out++ = hex[c & 15];
        }
    }
    *out = 0;
    return out;
}

void __fastcall__ rest_config_path(char *buf, const char *cat,
                                   const char *item, const char *value)
{
    strcpy(buf, "/v1/configs/");
    buf = url_encode(buf + 12, cat);
    if (!item)
        return;
    *buf++ = '/';
    buf = url_encode(buf, item);
    if (value) {
        strcpy(buf, "?value=");
        url_encode(buf + 7, value);
    }
}

/* returns a pointer just behind "key": (and spaces), or NULL */
char *__fastcall__ json_find(const char *key)
{
    char *p = rest_body;
    unsigned char kl = strlen(key);

    while ((p = strchr(p, '"')) != NULL) {
        ++p;
        if (strncmp(p, key, kl) == 0 && p[kl] == '"') {
            p += kl + 1;
            while (*p == ' ' || *p == ':')
                ++p;
            return p;
        }
    }
    return NULL;
}

/* copies a JSON string or number at p into out; returns the position
   after it (behind a following comma), or NULL at the end of a list */
char *__fastcall__ json_value(char *p, char *out, unsigned char max)
{
    unsigned char n = 0;

    while (*p == ' ' || *p == '\r' || *p == '\n' || *p == '\t')
        ++p;
    if (*p == ']' || *p == '}' || *p == 0) {
        *out = 0;
        return NULL;
    }
    if (*p == '"') {
        ++p;
        while (*p && *p != '"') {
            if (*p == '\\' && p[1])
                ++p;
            if (n < max - 1)
                out[n++] = *p;
            ++p;
        }
        if (*p)
            ++p;
    } else {
        while (*p && *p != ',' && *p != '}' && *p != ']' && *p != ' ' &&
               *p != '\r' && *p != '\n') {
            if (n < max - 1)
                out[n++] = *p;
            ++p;
        }
    }
    out[n] = 0;
    while (*p == ' ' || *p == '\r' || *p == '\n' || *p == '\t')
        ++p;
    if (*p == ',')
        ++p;
    return p;
}
