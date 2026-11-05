/* GEOS-U64: C interface to the Ultimate network API in the kernal.
   See ULTIMATE.md for the register conventions of every call. */

#ifndef ULTNET_H
#define ULTNET_H

#define R0 (*(unsigned *)0x02)
#define R1 (*(unsigned *)0x04)
#define R2 (*(unsigned *)0x06)
#define R3 (*(unsigned *)0x08)

#define ULT_SIGNATURE   ((const char *)0xFE7A)  /* "UNET", version */

#define UltDetect       0xFE50
#define NetGetIP        0xFE53
#define NetTCPConnect   0xFE56
#define NetUDPConnect   0xFE59
#define NetClose        0xFE5C
#define NetRead         0xFE5F
#define NetWrite        0xFE62
#define NetListen       0xFE65
#define NetListenStop   0xFE68
#define NetListenState  0xFE6B
#define NetListenSocket 0xFE6E
#define UltSetSpeed     0xFE71
#define UltGetStatus    0xFE74
#define UltCommand      0xFE77  /* API version 2 */
#define ULT_VERSION     (ULT_SIGNATURE[4])

#define NET_NO_DATA     0xFFFF  /* NetRead: nothing received yet */

unsigned __fastcall__ ultcall(unsigned char a, unsigned addr);

#define ULT_ERR(r)      ((unsigned char)((r) >> 8))
#define ULT_VAL(r)      ((unsigned char)(r))

#endif
