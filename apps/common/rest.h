/* GEOS-U64: minimal HTTP/REST client for the Ultimate's own web API
   (/v1/configs, /v1/machine, ...), over the kernal network API.
   The Ultimate answers on its own IP address (lwIP loopback); the
   "Web Remote Control Service" must be enabled in Network Settings. */

#ifndef REST_H
#define REST_H

#define REST_RESP_SIZE 3072

#define REST_MAX_HOSTS 4

extern char rest_host[16];      /* dotted IP of this Ultimate */
extern char rest_hosts[REST_MAX_HOSTS][16];  /* 127.0.0.1 + interfaces */
extern char rest_herr[REST_MAX_HOSTS][24];   /* rest_probe: why each failed */
extern unsigned char rest_nhosts;
extern char rest_pass[24];      /* Network Password, "" if none */
extern char rest_error[40];     /* why the last call failed, or "" */
extern char rest_resp[REST_RESP_SIZE];
extern char *rest_body;         /* body of the last response */
extern unsigned rest_status;    /* HTTP status of the last response */

/* errors of rest_init / rest_call */
#define REST_OK         0
#define REST_NO_KERNAL  1       /* not the GEOS-U64 kernal */
#define REST_NO_UCI     2       /* command interface disabled */
#define REST_NO_IP      3       /* no network interface with an IP */
#define REST_NO_CONNECT 4       /* web service not reachable */
#define REST_HTTP_ERROR 5       /* status != 200, see rest_status */

unsigned char rest_init(void);
/* tries every interface address until the web service answers; sets
   rest_host. REST_OK or REST_NO_CONNECT (see rest_error) */
unsigned char rest_probe(void);
unsigned char __fastcall__ rest_call(const char *method, const char *path);

/* build "/v1/configs/<cat>[/<item>[?value=<value>]]" URL-encoded into
   buf; item NULL = the whole category */
void __fastcall__ rest_config_path(char *buf, const char *cat,
                                   const char *item, const char *value);

/* tiny JSON helpers working on rest_body */
char * __fastcall__ json_find(const char *key);
char * __fastcall__ json_value(char *p, char *out, unsigned char max);

#endif
