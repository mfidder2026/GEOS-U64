/* GEOS-U64: change Ultimate settings without the network.
   Writes /temp/uci_config.cfg ("[Category]" / "Item=Value" lines) through
   the command interface's DOS target and applies it with the control
   target's LOAD_CONFIG command. Needs kernal API version 2 (UltCommand)
   and firmware with LOAD_CONFIG. Settings cannot be read this way. */

#ifndef UCFG_H
#define UCFG_H

#define UCFG_OK         0
#define UCFG_NO_API     1   /* kernal without UltCommand */
#define UCFG_FILE_ERR   2   /* writing the file failed, see ucfg_msg */
#define UCFG_LOAD_ERR   3   /* LOAD_CONFIG failed, see ucfg_msg */

extern char ucfg_msg[48];   /* status or first log line of the last apply */

unsigned char ucfg_available(void);
void ucfg_begin(void);
void __fastcall__ ucfg_add(const char *cat, const char *item,
                           const char *value);
unsigned char ucfg_apply(void);

/* control target REBOOT (works offline too) */
void ucfg_reboot(void);

#endif
