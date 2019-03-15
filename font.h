/* ZBLAST v1.0 - shoot the thingies for Linux + VGA.
 * Copyright (C) 1993, 1994 Russell Marks. See README for license details.
 *
 * font.h - external prototypes for font.c
 */
 
#define NO_CLIP_FONT  0x7FFFFFFF

extern int vgadrawtext(int,int,int,char *);
extern int vgatextsize(int,char *);
extern int set_max_text_width(int);
