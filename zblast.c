/* ZBLAST v1.0 - shoot the thingies for Linux + VGA.
 * Copyright (C) 1993, 1994 Russell Marks.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include <vga.h>
#include <rawkey.h>
#include <sys/soundcard.h>

#include "font.h"
#include "levels.h"


int blastertoggle=0;
int soundfd=-1;

/* virtual sound channels, used for mixing.
 * each can play at most one sample at a time.
 */
#define PSHOT_CHANNEL	0	/* player shot */
#define SMALLHIT_CHANNEL 1	/* small baddie being hit */
#define BIGHIT_CHANNEL	2	/* big baddie being hit */
#define BSHOT_CHANNEL	3	/* baddie shot channel */
#define EFFECT_CHANNEL  4	/* effects like level end noises */

#define NUM_CHANNELS	5

struct channel_tag {
  struct sample_tag *sample;	/* pointer to sample struct, NULL if none */
  int offset;			/* position in sample */
  } channel[NUM_CHANNELS];


/* sample filenames */
char samplename[][16]={
  "pshot.raw",
  "type1.raw",
  "type2.raw",
  "type3.raw",
  "type4.raw",
  "type5.raw",
  "type10.raw",
  "type11.raw",
  "bshot.raw",
  "phit.raw",
  "lvlend.raw"};

/* sample offsets in sample[] */
#define PSHOT_SAMPLE	0	/* player shooting */
#define TYPE1_SAMPLE	1	/* noises each type of baddie makes */
#define TYPE2_SAMPLE	2	/* ...when hit */
#define TYPE3_SAMPLE	3
#define TYPE4_SAMPLE	4
#define TYPE5_SAMPLE	5
#define TYPE10_SAMPLE	6
#define TYPE11_SAMPLE	7
#define BSHOT_SAMPLE	8	/* baddie shooting */
#define PHIT_SAMPLE	9	/* player hit */
#define LVLEND_SAMPLE   10
#define NUM_SAMPLES	11

/* for in-memory samples */
struct sample_tag {
  unsigned char *data;		/* pointer to sample, NULL if none */
  int length;			/* length of sample */
  } sample[NUM_SAMPLES];


#define GRIDDEPTH  100
#define SHOTNUM     32
#define BADDIENUM   40
#define STARNUM     10
#define SHIELDLIFE  30
#define DEBRISNUM   32


/* array is used like an array of [GRIDDEPTH][-32 to 32] :
 * that is, we add 32 when to x index referencing it.
 */
struct grid
  {
  int x,y;
  } gridxy[GRIDDEPTH][65];


struct {
  char x,y;   /* -32<=x<=32 and 0<=y<=GRIDDEPTH so signed char is fine */
  char active;
  } shot[SHOTNUM];

struct {
  char x,y;
  char dx,dy;
  char active;
  int freq;
  } debris[DEBRISNUM];

struct {
  char x,y;
  char col;   /* colour */
  char speed;
  } star[STARNUM];

struct {
  char x,y;
  char which;   /* which type of baddie  0 for inactive */
  char dx,dy;
  char hits;    /* hits left till death */
  char hitnow;  /* have been hit recently */
  } baddie[BADDIENUM];

#define sgnval(a) ((a)>0)?1:(((a)<0)?-1:0)

int movx=0,movy=0,gtmpx,gtmpy,textx=0,texty=0;

#define lowrand()	(rand()%32768)
#define gridpset(ix,iy) vga_drawpixel(gridxy[iy][ix].x,gridxy[iy][ix].y)
#define gridmoveto(ix,iy) movx=gridxy[iy][ix].x,movy=gridxy[iy][ix].y
#define gridlineto(ix,iy) vga_drawlinechk(movx,movy,\
			    gtmpx=gridxy[iy][ix].x,gtmpy=gridxy[iy][ix].y),\
                            movx=gtmpx,movy=gtmpy
#define _settextposition(y,x) textx=(x)*8,texty=(y)*13



#define drawshot(nval,clr) { \
  vga_setcolor(clr); \
  gridmoveto(shot[nval].x+32,shot[nval].y); \
  gridlineto(shot[nval].x+32,shot[nval].y+1); \
  }


int lives,lostlife,pdc,sound=1;
long score;
int tweenwave,paused;
int usedebris=1,usestars=1;
int count,donetrace,endtrace;


main()
{
int x,y;

initialise();

titlescreen();


scan_keyboard();
while(is_key_pressed(ESCAPE_KEY)==0)
  {
  scan_keyboard();
  if(is_key_pressed(scancode_trans(32)))
    {
    playgame();
    titlescreen();
    while(is_key_pressed(ESCAPE_KEY)) scan_keyboard();
    }
  }

uninitialise();
}


titlescreen()
{
int x,y;

vga_clear();

for(x=-5;x<=5;x++)
  {
  vga_setcolor(2+(x&1));
  vgadrawtext(90+x,0,17,"Z B L A S T");
  }

vga_setcolor(0);
x=1;
for(y=30;y<80;y+=x)
  {
  vga_drawline(90,y,600,y);
  x++;
  }

vga_setcolor(14);
for(x=0;x<2;x++)
  vgadrawtext(200+x,100,2,"(C) 1994 Russell Marks for improbabledesigns");

vga_setcolor(10);
vgadrawtext(30,160,6,"Hit SPACE to play game or ESC to quit");

vga_setcolor(2);
_settextposition(24,3);
_outtext(
 "in-game - cursors move - space-fire - s-toggle sound - enter-PDC - p-pause");

for(x=0;x<BADDIENUM;x++)
  baddie[x].which=0;
}



playgame()
{
int quit,dead,wavenum,xpos,ypos,oldx,oldy,wasfire,waspdc,shield,oldsound;
int f;

lives=startlives;
quit=dead=wasfire=waspdc=paused=0;
wavenum=startlevel;
tweenwave=0;
pdc=5;
xpos=0; ypos=90;
shield=SHIELDLIFE;
score=0;

wipeshots();
vga_clear();
getnewwave(wavenum);
donetrace=0;

/* turn off any old sounds */
for(f=0;f<NUM_CHANNELS;f++) channel[f].sample=NULL;

while((!quit)&&(!dead))
  {
  scan_keyboard();
  oldx=xpos; oldy=ypos;
  if(is_key_pressed(scancode_trans('p')))		/* pause */
    {
    paused=!paused;
    if(paused==0)
      {
      sound=oldsound;
      showpaused(0);
      }
    else
      {
      oldsound=sound;
      sound=0;
      }
    while(is_key_pressed(scancode_trans('p'))) scan_keyboard();
    }
  if(!paused)
    {
    if(is_key_pressed(scancode_trans('s')))		/* sound toggle */
      {
      sound=!sound;
      while(is_key_pressed(scancode_trans('s'))) scan_keyboard();
      }
    if(is_key_pressed(ESCAPE_KEY))   quit=1;                /* exit (esc) */
    if(is_key_pressed(CURSOR_LEFT))  if(xpos>-28) xpos-=2;        /* left */
    if(is_key_pressed(CURSOR_RIGHT)) if(xpos< 28) xpos+=2;       /* right */
    if(is_key_pressed(CURSOR_UP))    if(ypos> 80) ypos-=2;          /* up */
    if(is_key_pressed(CURSOR_DOWN))  if(ypos< 96) ypos+=2;        /* down */
    if((is_key_pressed(ENTER_KEY))&&(waspdc==0)&&(pdc>0))
      {
      waspdc=1;
      pdc--;
      firemany(ypos);
      showstatus();
      }
    else
      if(is_key_pressed(ENTER_KEY)==0) waspdc=0;
      
    if(is_key_pressed(scancode_trans(32)) && wasfire==0)
      {
      wasfire=1;
      addnewshot(xpos-3,ypos-2);                   /* fire */
      addnewshot(xpos+3,ypos-2);
      if(sound)
        queuesam(PSHOT_CHANNEL,PSHOT_SAMPLE);
      }
    else
      if(is_key_pressed(scancode_trans(32))==0) wasfire=0;

    if(shield)
      shield--;
    else
      if(deadyet(xpos,ypos))
        {
        lives--;
        lostlife++;
        showstatus();
        if(lives<0)
          dead=1;
        else
          {
          shield=SHIELDLIFE;
          if(sound) queuesam(BIGHIT_CHANNEL,PHIT_SAMPLE);
          }
        }
    }
  vga_waitretrace();

  drawship(oldx,oldy,0);
  drawship(xpos,ypos,(shield>0)?9:10);

  drawframe();

  if(paused)
    showpaused(1);
  else
    {
    if(tweenwave)
      {
      tweenwave--;
      showtweenwave(wavenum,(tweenwave<10)?0:1);
      if(tweenwave==0) lostlife=0;
      }
    else
      {
      if((count=countbaddies())==0)
        {
        wavenum++;
        if(getnewwave(wavenum)==-1)
          quit=1;
        if(sound)
          queuesam(EFFECT_CHANNEL,LVLEND_SAMPLE);
        }
      }
    }
  
  /* play some sound */
  if(sound) playchunk();
  }
}


firemany(y)
int y;
{
int f;

for(f=0;f<10;f++)
  {
  addnewshot(f*3,89+f);
  addnewshot(-(f*3),89+f);
  }
}


int countbaddies()
{
int f,t;

for(f=0,t=0;f<BADDIENUM;f++)
  {
  if(baddie[f].which>0) t++;
  }
return(t);
}


/* returns -1 if no more waves */
int getnewwave(wavenum)
int wavenum;
{
if(createwave(wavenum)==-1)
  {
  /* wow, finished! */
  return(-1);
  }

tweenwave=60;   /* end wave */
if(wavenum>1)
  {
  doendwave(wavenum);
  if(((wavenum-2)%5)==4)
    {
    tweenwave=200;   /* end level */
    doendlevel(wavenum);
    }
  }

showstatus();
count=countbaddies();

return(0);
}


showpaused(showthem)
int showthem;
{
vga_setcolor(showthem?14:0);
centretext(12," --- PAUSED --- ");
}


showtweenwave(wavenum,showthem)
int wavenum,showthem;
{
char buf[80];

if(wavenum>1)
  {
  if(lostlife==0)
    {
    vga_setcolor(showthem?14:0);
    centretext(15,"No hits sustained from wave - energy increased 10");
    }

  if(lostlife<10)
    {
    vga_setcolor(showthem?3:0);
    centretext(18,"Low energy loss bonus - extra PDC");
    }

  if(((wavenum-2)%5)==4)
    {
    vga_setcolor(showthem?15:0);
    sprintf(buf,"Level %d Cleared",(wavenum-1)/5);
    centretext(8,buf);
    vga_setcolor(showthem?14:0);
    centretext(21,"Level Complete Bonus - energy increased 10");
    }
  }

vga_setcolor(showthem?13:0);
sprintf(buf,"Prepare for Wave %d",wavenum);
centretext(12,buf);
}


doendwave(wavenum)
int wavenum;
{
if(lostlife==0) lives+=10;
if(lostlife<10) pdc++;
}


doendlevel(wavenum)
int wavenum;
{
lives+=10;
}


centretext(y,str)
int y;
char *str;
{
_settextposition(y,(81-strlen(str))>>1);
_outtext(str);
}


showstatus()
{
char buf[80];
int f;

vga_setcolor(0);
for(f=0;f<16;f++) vga_drawline(568,f,639,f);
vga_setcolor(12);
sprintf(buf,"%3.3d",lives);
_settextposition(0,71);
_outtext(buf);
vga_setcolor(12);
_settextposition(0,76);
sprintf(buf,"%3.3d",pdc);
_outtext(buf);
showscore();
}


showscore()
{
char buf[80];
int f;

sprintf(buf,"%8.8ld",score);
_settextposition(0,0);
vga_setcolor(0);
for(f=0;f<16;f++) vga_drawline(0,f,72,f);
vga_setcolor(12);
_outtext(buf);
}


drawframe()
{
drawstars();
drawshots();
if(sound) speakeroff();
vga_waitretrace();
if(usedebris)  drawdebris();
if(!tweenwave) drawbaddies();
}


/* notice that this routine also moves stars */
drawstars()
{
int f;

if(!usestars) return(0);
for(f=0;f<STARNUM;f++)
  {
  vga_setcolor(0);
  vga_drawpixel(gridxy[star[f].y][32+star[f].x].x,
            gridxy[star[f].y][32+star[f].x].y);
  star[f].y+=star[f].speed;
  if(star[f].y>=GRIDDEPTH)
    {
    star[f].y=0;
    star[f].x=(lowrand()%65)-32;
    }
  vga_setcolor(star[f].col);
  vga_drawpixel(gridxy[star[f].y][32+star[f].x].x,
            gridxy[star[f].y][32+star[f].x].y);

  }
}


/* notice that this routine also moves baddies */
drawbaddies()
{
int f;

/* baddies being shot at was tested for in drawshots(), but
 * we'll still need to undraw the baddie if baddie[f].hits = 0.
 */


for(f=0;f<BADDIENUM;f++)
  {
  switch(baddie[f].which)
    {
    case 1:
    case 3:
    case 4:
      drawbaddie(baddie[f].x,baddie[f].y,baddie[f].which,0);
      if(baddie[f].hits<=0)
        {
        score+=(long)getscorefor(baddie[f].which);
        baddie[f].which=0;
        }
      else
        {
        if(!paused)
          {
          baddie[f].x+=baddie[f].dx;
          if((baddie[f].x>30)||(baddie[f].x<-30))
            {
            baddie[f].dx=-baddie[f].dx;
            baddie[f].x+=2*baddie[f].dx;
            }
          baddie[f].y+=baddie[f].dy;
          if((baddie[f].y>97)||(baddie[f].y<1))
            {
            baddie[f].dy=-baddie[f].dy;
            baddie[f].y+=2*baddie[f].dy;
            }
          if(baddie[f].which==4)
            {
            if(lowrand()<800)                                /* shoot? */
              addnewbaddie(baddie[f].x,baddie[f].y+2,2,1+(lowrand()&1),1,2);
            }
          else
            if(lowrand()<100+500*(baddie[f].which-1))         /* shoot? */
              addnewbaddie(baddie[f].x,baddie[f].y+2,0,1+(lowrand()&1),1,2);
          }
        if(baddie[f].hitnow)
          {
          drawbaddie(baddie[f].x,baddie[f].y,baddie[f].which,15);
          baddie[f].hitnow--;
          }
        else
          drawbaddie(baddie[f].x,baddie[f].y,baddie[f].which,
                   15-baddie[f].which);
        }
      break;

    case 2:
      drawbaddie(baddie[f].x,baddie[f].y,2,0);
      if((baddie[f].y>=97)||(baddie[f].hits==0))
        {
        score+=(long)getscorefor(baddie[f].which);
        baddie[f].which=0;
        }
      else
        {
        if(!paused)
          {
          baddie[f].x+=baddie[f].dx;
          if((baddie[f].x>32)||(baddie[f].x<-32))
            {
            baddie[f].dx=-baddie[f].dx;
            baddie[f].x+=2*baddie[f].dx;
            }
          baddie[f].y+=baddie[f].dy;
          }
        drawbaddie(baddie[f].x,baddie[f].y,2,15-(baddie[f].y%6));
        }
      break;

    case 10:
      drawbaddie(baddie[f].x,baddie[f].y,10,0);
      if(baddie[f].hits<=0)
        {
        score+=(long)getscorefor(baddie[f].which);
        baddie[f].which=0;
        }
      else
        {
        if(!paused)
          {
          baddie[f].x+=baddie[f].dx;
          if((baddie[f].x>24)||(baddie[f].x<-24))
            {
            baddie[f].dx=-baddie[f].dx;
            baddie[f].x+=2*baddie[f].dx;
            }
          baddie[f].y+=baddie[f].dy;
          if((baddie[f].y>72)||(baddie[f].y<8))
            {
            baddie[f].dy=-baddie[f].dy;
            baddie[f].y+=2*baddie[f].dy;
            }

          if(lowrand()<500)                /* shoot a type 3? */
            addnewbaddie(baddie[f].x,baddie[f].y+2,(lowrand()&1)?-1:1,
            				1+(lowrand()&1),3,3);   /* 3 hits */
          if(lowrand()<2000)                /* shoot? */
            {
            addnewbaddie(baddie[f].x-6,baddie[f].y+8,0,2,1,2);
            addnewbaddie(baddie[f].x+6,baddie[f].y+8,0,2,1,2);
            }
          }

        if(baddie[f].hitnow)
          {
          drawbaddie(baddie[f].x,baddie[f].y,10,15);
          baddie[f].hitnow--;
          }
        else
          drawbaddie(baddie[f].x,baddie[f].y,10,5);
        }
      break;

    case 11:
      drawbaddie(baddie[f].x,baddie[f].y,11,0);
      if(baddie[f].hits<=0)
        {
        score+=(long)getscorefor(baddie[f].which);
        baddie[f].which=0;
        }
      else
        {
        if(!paused)
          {
          baddie[f].x+=baddie[f].dx;
          if((baddie[f].x>8)||(baddie[f].x<-8))
            {
            baddie[f].dx=-baddie[f].dx;
            baddie[f].x+=2*baddie[f].dx;
            }
          baddie[f].y+=baddie[f].dy;
          if((baddie[f].y>64)||(baddie[f].y<16))
            {
            baddie[f].dy=-baddie[f].dy;
            baddie[f].y+=2*baddie[f].dy;
            }

          if(lowrand()<200)                /* shoot a type 10!? */
            addnewbaddie(baddie[f].x-((lowrand()&1)?-15:15),baddie[f].y+8,
                         (lowrand()&1)?-1:1,1,10,10);   /* 10 hits */

          if(lowrand()<2000)                /* shoot a type 1? */
            {
            addnewbaddie(baddie[f].x-24,baddie[f].y+12,0,2,1,2);
            addnewbaddie(baddie[f].x+24,baddie[f].y+12,0,2,1,2);
            }
          }
        if(baddie[f].hitnow)
          {
          drawbaddie(baddie[f].x,baddie[f].y,11,15);
          baddie[f].hitnow--;
          }
        else
          drawbaddie(baddie[f].x,baddie[f].y,11,5);
        }
      break;

    case 5:
      drawbaddie(baddie[f].x,baddie[f].y,5,0);
      if(baddie[f].hits<=0)
        {
        score+=(long)getscorefor(baddie[f].which);
        baddie[f].which=0;
        }
      else
        {
        if(!paused)
          {
          baddie[f].x+=baddie[f].dx;
          baddie[f].y+=baddie[f].dy;

          if(baddie[f].x<=-28)
            {
            baddie[f].x=-27;
            baddie[f].dy=baddie[f].dx;
            baddie[f].dx=0;
            }
          if(baddie[f].y<=4)
            {
            baddie[f].y=5;
            baddie[f].dx=-baddie[f].dy;
            baddie[f].dy=0;
            }
          if(baddie[f].x>=28)
            {
            baddie[f].x=27;
            baddie[f].dy=baddie[f].dx;
            baddie[f].dx=0;
            }
          if(baddie[f].y>=94)
            {
            baddie[f].y=93;
            baddie[f].dx=-baddie[f].dy;
            baddie[f].dy=0;
            }

          if(lowrand()<500)                /* shoot? */
            addnewbaddie(baddie[f].x,baddie[f].y+4,0,2,1,2);
          }

        if(baddie[f].hitnow)
          {
          drawbaddie(baddie[f].x,baddie[f].y,5,15);
          baddie[f].hitnow--;
          }
        else
          drawbaddie(baddie[f].x,baddie[f].y,5,4);
        }
      break;
    }
  }
}


deadyet(x1,y1)
int x1,y1;
{
int f,x2,y2;

/* one way to be generous and make it quicker:
 * only 'midpoints' (that is, baddie[f].x and .y) in a box from
 * x-2,y-2 to x+2,y+1.
 */

x2=x1+2; y2=y1+1;
x1-=2;   y1-=2;

for(f=0;f<BADDIENUM;f++)
  {
  if(baddie[f].which>0)
    if((baddie[f].x>=x1)&&(baddie[f].x<=x2)&&
       (baddie[f].y>=y1)&&(baddie[f].y<=y2))
      return(1);   /* you only need to be hit once to die...! */
  }
return(0);
}


seeifshothitbaddie()
{
register int g;
int f;
int x1,y1,x2,y2;
int die;
long oldscore;

oldscore=score;
for(f=0;f<BADDIENUM;f++)
  {
  if(baddie[f].which!=0)
    {
    switch(baddie[f].which)
      {
      case 1:
      case 3:
      case 4:
        /* we treat types 1,3,4 as a box from x-2,y-1 to x+2,y+2 */
        x1=baddie[f].x; y1=baddie[f].y;
        x2=x1+2;        y2=y1+2;
        x1-=2;          y1--;
        break;

      case 5:
        x1=baddie[f].x; y1=baddie[f].y;
        x2=x1+4;        y2=y1+4;
        x1-=4;          y1-=4;
        break;

      case 2:
        /* you can't hit a normal baddie shot */
        x1=999;
        break;

      case 10:
        /* we treat type 10 as a box from x-8,y-6 to x+8,y+2 */
        x1=baddie[f].x; y1=baddie[f].y;
        x2=x1+8;        y2=y1+2;
        x1-=8;          y1-=6;
        break;

      case 11:
        /* type 11 is treated as a box from x-18,y-12 to x+18,y+4 */
        x1=baddie[f].x; y1=baddie[f].y;
        x2=x1+18;       y2=y1+4;
        x1-=18;         y1-=12;
        break;
      }

    if(x1!=999)
      {
      die=0;
      for(g=0;(g<SHOTNUM)&&(!die);g++)
        {
        if(shot[g].active)
          {
          if((shot[g].x>=x1)&&(shot[g].x<=x2)&&
             (shot[g].y>=y1)&&(shot[g].y<=y2))
            {
            drawshot(g,0);
            shot[g].active=0;
            baddie[f].hits--;
            baddie[f].hitnow=3;
            die=1;
            score++;
            if(baddie[f].hits==0) makedebrisfor(f);
            }
          }
        }
      }
    if(sound)
      {
      if(baddie[f].which!=0 && baddie[f].hitnow==3)
        switch(baddie[f].which)
          {
          case  1: queuesam(SMALLHIT_CHANNEL,TYPE1_SAMPLE); break;
          case  2: queuesam(SMALLHIT_CHANNEL,TYPE2_SAMPLE); break;
          case  3: queuesam(SMALLHIT_CHANNEL,TYPE3_SAMPLE); break;
          case  4: queuesam(SMALLHIT_CHANNEL,TYPE4_SAMPLE); break;
          case  5: queuesam(SMALLHIT_CHANNEL,TYPE5_SAMPLE); break;
          case 10: queuesam(BIGHIT_CHANNEL,  TYPE10_SAMPLE); break;
          case 11: queuesam(BIGHIT_CHANNEL,  TYPE11_SAMPLE); break;
          }
      }
    }
  }
if(score!=oldscore) showscore();
}


drawshots()
{
int f,shotfin;

if((!tweenwave)&&(!paused)) seeifshothitbaddie();
for(f=0;f<SHOTNUM;f++)
  {
  if(shot[f].active)
    {
    drawshot(f,0);
    if(!paused) shot[f].y-=2;
    if(shot[f].y<0) shotfin=1; else shotfin=0;
    if(shotfin)
      shot[f].active=0;
    else
      drawshot(f,12);
    }
  }
}


drawdebris()
{
int f;

for(f=0;f<DEBRISNUM;f++)
  {
  if(debris[f].active)
    {
    drawdebrisfor(f,0);

    if(!paused)
      {
      /* move */
      debris[f].x+=debris[f].dx;
      debris[f].y+=debris[f].dy;
      if(debris[f].freq>0) debris[f].freq-=50;
      }

    if((debris[f].x<-32)||(debris[f].x>32)||
       (debris[f].y<  0)||(debris[f].y>98))
      debris[f].active=0;
    else
      drawdebrisfor(f,2);
    }
  }
}


drawdebrisfor(index,col)
int index,col;
{
int x,y;

vga_setcolor(col);
x=debris[index].x; y=debris[index].y;
gridmoveto(32+x,y);
gridlineto(32+x-(sgnval(debris[index].dx)),
              y-(sgnval(debris[index].dy)));
}


makedebrisfor(index)
int index;
{
int x,y,dx,dy,mdx,mdy,pdx,pdy;

 x=baddie[index].x;   y=baddie[index].y;
dx=baddie[index].dx; dy=baddie[index].dy;
mdx=dx-2; if(mdx==0) mdx=-1;
mdy=dy-2; if(mdy==0) mdy=-1;
pdx=dx+2; if(pdx==0) pdx= 1;
pdy=dy+2; if(pdy==0) pdy= 1;

switch(baddie[index].which)
  {
  case 1: case 3: case 4:
    addnewdebris(x-1,y-1,mdx,mdy);
    addnewdebris(x+1,y-1,pdx,mdy);
    addnewdebris(x-1,y+1,mdx,pdy);
    addnewdebris(x+1,y+1,pdx,pdy);
    break;

  case 5:
    addnewdebris(x-2,y-2,mdx,mdy);
    addnewdebris(x+2,y-2,pdx,mdy);
    addnewdebris(x-2,y+2,mdx,pdy);
    addnewdebris(x+2,y+2,pdx,pdy);

    addnewdebris(x  ,y-2,  0,mdy);
    addnewdebris(x-2,y  ,mdx,  0);
    addnewdebris(x+2,y  ,pdx,  0);
    addnewdebris(x  ,y+2,  0,pdy);
    break;

  case 10: case 11:
    addnewdebris(x-4,y-4,mdx,mdy);
    addnewdebris(x+4,y-4,pdx,mdy);
    addnewdebris(x-4,y+4,mdx,pdy);
    addnewdebris(x+4,y+4,pdx,pdy);

    addnewdebris(x  ,y-4,  0,mdy);
    addnewdebris(x-4,y  ,mdx,  0);
    addnewdebris(x+4,y  ,pdx,  0);
    addnewdebris(x  ,y+4,  0,pdy);
    break;
  }
}


addnewshot(x,y)
int x,y;
{
int n;

if((n=findavailableshot())!=-1)
  {
  shot[n].x=x;
  shot[n].y=y;
  shot[n].active=1;
  }
}


addnewdebris(x,y,dx,dy)
int x,y,dx,dy;
{
int n;

if((n=findavailabledebris())!=-1)
  {
  debris[n].x=x;
  debris[n].y=y;
  debris[n].dx=dx;
  debris[n].dy=dy;
  debris[n].active=1;
  debris[n].freq=2000+lowrand()%500;
  }
}


addnewbaddie(x,y,dx,dy,hits,which)
int x,y,dx,dy,hits,which;
{
int n;

if((n=findavailablebaddie())!=-1)
  {
  baddie[n].x=x;
  baddie[n].y=y;
  baddie[n].dx=dx;
  baddie[n].dy=dy;
  baddie[n].hits=hits;
  baddie[n].which=which;
  baddie[n].hitnow=0;
  if(sound)
    queuesam(BSHOT_CHANNEL,BSHOT_SAMPLE);
  }
}


initialise()
{
int f;

vga_disabledriverreport();
vga_init();
vga_setmode(G640x350x16);
init3dgrid();
wipeshots();
for(f=0;f<BADDIENUM;f++)
  baddie[f].which=0;
for(f=0;f<STARNUM;f++)
  {
  star[f].x=(lowrand()%65)-32;
  star[f].y=lowrand()%GRIDDEPTH;
  switch(star[f].speed=(1+(lowrand()%3)))
    {
    case 1:
      star[f].col=3; break;
    case 2:
      star[f].col=3; break;
    case 3:
      star[f].col=7; break;
    }
  }
  
rawmode_init();

for(f=0;f<NUM_SAMPLES;f++) sample[f].data=NULL;

#ifdef SNDCTL_DSP_SETFRAGMENT		/* voxware 3 only */
if((soundfd=open("/dev/dsp",O_WRONLY))<0)
  soundfd=-1;
else
  {
  int frag,siz;
  FILE *in;
  char buf[256];

#define SOUND_BUFSIZ	256
  frag=0x00020008;	/* 2 fragments of 256 bytes - ~1/30th sec each */
  ioctl(soundfd,SNDCTL_DSP_SETFRAGMENT,&frag);
  /* want 8kHz mono 8-bit also, but these are all default anyway */
  
  /* load in the samples */
  for(f=0;f<NUM_SAMPLES;f++)
    {
    sprintf(buf,"%s/%s",SOUNDSDIR,samplename[f]);
    if((in=fopen(buf,"rb"))!=NULL)
      {
      fseek(in,0,SEEK_END);
      sample[f].length=ftell(in);
      if((sample[f].data=(unsigned char *)malloc(sample[f].length))==NULL)
        break;
      rewind(in);
      fread(sample[f].data,1,sample[f].length,in);
      fclose(in);
      }
    }
  }
#endif
}


/* also wipes debris */
wipeshots()
{
int f;

for(f=0;f<SHOTNUM;f++)
  {
  shot[f].active=0;
  debris[f].active=0;
  }
}


uninitialise()
{
rawmode_exit();
vga_setmode(TEXT);
if(soundfd!=-1) close(soundfd);
}


/* return first available shot as index into shot[], -1 on error */
int findavailableshot()
{
int f;

for(f=0;f<SHOTNUM;f++)
  if(shot[f].active==0) return(f);
return(-1);
}


int findavailabledebris()
{
int f;

for(f=0;f<DEBRISNUM;f++)
  if(debris[f].active==0) return(f);
return(-1);
}


int findavailablebaddie()
{
int f;

for(f=0;f<BADDIENUM;f++)
  if(baddie[f].which==0) return(f);
return(-1);
}


drawbaddie(x,y,which,col)
register int x,y;
int which,col;
{
x+=32;
vga_setcolor(col);
switch(which)
  {
  case 1:
  case 3:
  case 4:
    gridmoveto(x-2,y+1);
    gridlineto(x-2,y-1);
    gridlineto(x+2,y-1);
    gridlineto(x+2,y+1);
    gridlineto(x-2,y+1);
    gridlineto(x-1,y+2);

    gridmoveto(x+2,y+1);
    gridlineto(x+1,y+2);
    break;

  case 2:
    /* a line, for better visibility. notice only the lower end of the
     * the line is dangerous! */
    gridmoveto(x,y);
    gridlineto(x,y-1);
    break;

  case 5:
    gridmoveto(x-4,y-4);
    gridlineto(x+4,y-4);
    gridlineto(x+4,y+4);
    gridlineto(x-4,y+4);
    gridlineto(x-4,y-4);
    break;

  case 10:
    gridmoveto(x-6,y+8);
    gridlineto(x-4,y  );
    gridlineto(x+4,y  );
    gridlineto(x+6,y+8);
    gridlineto(x+8,y-4);
    gridlineto(x+6,y-4);
    gridlineto(x+2,y-8);
    gridlineto(x-2,y-8);
    gridlineto(x-6,y-4);
    gridlineto(x-8,y-4);
    gridlineto(x-6,y+8);
    break;

  case 11:
    gridmoveto(x   ,y+16);
    gridlineto(x+ 6,y+12);
    gridlineto(x+ 3,y+12);
    gridlineto(x+12,y   );
    gridlineto(x+18,y   );
    gridlineto(x+24,y+12);
    gridlineto(x+18,y-16);
    gridlineto(x+12,y- 8);
    gridlineto(x-12,y- 8);
    gridlineto(x-18,y-16);
    gridlineto(x-24,y+12);
    gridlineto(x-18,y   );
    gridlineto(x-12,y   );
    gridlineto(x- 3,y+12);
    gridlineto(x- 6,y+12);
    gridlineto(x   ,y+16);
    break;
  }
}

drawship(x,y,col)
int x,y,col;
{
int ox,oy;
int xs,ys;

x+=32;
vga_setcolor(col);
gridmoveto(x+1,y  );
gridlineto(x  ,y-2);
gridlineto(x-1,y  );
gridlineto(x-3,y+1);
gridlineto(x+3,y+1);
gridlineto(x+1,y  );

gridmoveto(x+3,y+1);
gridlineto(x+3,y-2);

gridmoveto(x-3,y+1); 
gridlineto(x-3,y-2);
}


init3dgrid()
{
double xmax,ymax,xmul,ymul;
int nx,ny;
struct grid tmp;

vga_setcolor(1);
xmul=1.5; ymul=0.6;
xmax=639; ymax=349;
for(ny=0;ny<GRIDDEPTH;ny++)
  {
  for(nx=-32;nx<=32;nx++)
    {
    tmp.x=(int)(320.0+(double)nx*xmul);
    tmp.y=(int)(10.0+(double)ny*ymul);
    gridxy[ny][nx+32]=tmp;
    }
  xmul*=1.021;
  ymul*=1.018;
  }
}


speakeron()
{
#if 0
outp(0x61,inp(0x61)|3);
#endif
}


speakeroff()
{
#if 0
outp(0x61,inp(0x61)&0xFC);
#endif
}


speakerfreq(freq)
int freq;
{
#if 0
long lf,c;
int a;

if(freq==0)
  speakeroff();
else
  {
#ifdef BLASTER
  while((inp(0x22C)&0x80)==0x80);
  outp(0x22C,0x10);
  while((inp(0x22C)&0x80)==0x80);
#if 0
  if(blastertoggle)
    outp(0x22C,(freq/50)&255);
  else
    outp(0x22C,freq&255);
#else
  outp(0x22C,(freq/10)&255);
#endif
  blastertoggle=(blastertoggle)?0:1;
#else
  lf=(long)freq;
  c=1193280L/lf;
  a=(int)c;
  outp(0x43,0xB6);
  outp(0x42,a&255);
  outp(0x42,a>>8);
  speakeron();
#endif
  }
#endif
}


waveendsound()
{
#if 0
int f,g,h;

if(sound) speakeron();

f=8000;
g=0x0FB5;

for(h=0;h<200;h++)
  {
  if(f<=30)
    speakerfreq(0);
  else
    {
    if(!sound)
      waithoriz(80);
    else
      {
      waithoriz(25); speakerfreq(f);
      waithoriz(25); speakerfreq(f>>2);
      waithoriz(25); speakerfreq(g);
      waithoriz(25); speakerfreq(g^f);
      }
    if(((8000-f)>>4)>0) waithoriz((8000-f)>>4);
    f-=80;
    g=g^f;
    g+=f;
    }
  }
if(sound) speakeroff();
#endif
}


/* well, ok, no sound. */
levelendsound()
{
int h;

for(h=0;h<200;h++)
  vga_waitretrace();
}


waithoriz(num)
int num;
{
#if 0
int f;

for(f=0;f<num;f++)
  {
  while((inp(0x3DA)&1)!=0);
  while((inp(0x3DA)&1)==0);
  }
#endif
}


int getscorefor(which)
int which;
{
switch(which)   /* he's a poet and he doesn't know it */
  {
  case 1:
    return(10);
  case 3:
    return(20);
  case 4:
    return(50);
  case 5:
    return(20);
  case 10:
    return(200);
  case 11:
    return(500);
  default:
    return(0);
  }
}


_outtext(str)
char *str;
{
vgadrawtext(textx,texty,3,str);
}


/* check edges and draw line if ok.
 * only bother checking horiz. ones.
 */
vga_drawlinechk(a,b,c,d)
int a,b,c,d;
{
if(a<0 || a>639 || c<0 || c>639) return(0);
vga_drawline(a,b,c,d);
}


/* mix and play a chunk of sound to /dev/dsp. */
playchunk()
{
int f,g,v;
unsigned char c;
struct channel_tag *cptr;
static unsigned char soundbuf[SOUND_BUFSIZ];

for(f=0;f<SOUND_BUFSIZ;f++)
  {
  v=0;
  for(g=0,cptr=&(channel[0]);g<NUM_CHANNELS;g++,cptr++)
    if(cptr->sample!=NULL)
      {
      v+=(int)cptr->sample->data[cptr->offset++];
      if(cptr->offset>=cptr->sample->length)
        cptr->sample=NULL;
      }
      
  v/=NUM_CHANNELS;
  soundbuf[f]=(unsigned char)v;
  }
write(soundfd,soundbuf,SOUND_BUFSIZ);
}


/* setup a new sample to be played on a given channel. */
queuesam(chan,sam)
int chan,sam;
{
channel[chan].sample=&(sample[sam]);
channel[chan].offset=0;
}
