/* ZBLAST v1.0 - shoot the thingies for Linux + VGA.
 * Copyright (C) 1993, 1994 Russell Marks. See README for license details.
 */

#include <stdlib.h>
#include <math.h>

extern int addnewbaddie(int,int,int,int,int,int);

/* change these to test new levels */
int startlives=20;    /* normally 20 */
int startlevel=1;     /* normally 1  */


int createwave(wavenum)
int wavenum;
{
int f,g;
double a;

switch(wavenum)
  {
  case 1:
    for(f=0;f<5;f++)
      addnewbaddie(-30+f*2,2+rand()%30,1+rand()%2,1+rand()%2,2,1);
    for(f=0;f<3;f++)
      addnewbaddie(-30+f*2,2+rand()%30,1+rand()%2,1+rand()%2,2,3);
    break;

  case 2:
    for(f=0;f<5;f++)
      addnewbaddie(-30+f*2,2+rand()%30,1+rand()%2,1+rand()%2,4,3);
    break;

  case 3:
    for(f=0;f<10;f++)
      addnewbaddie(-30+f*6,50,0,-2,5,3);
    for(f=0;f<20;f++)
      addnewbaddie(-20+f*2,10,0,1,1,2);
    break;

  case 4:
    for(f=0;f<100;f+=10)
      {
      addnewbaddie(-30,2+f,0,1,4,1);
      addnewbaddie( 30,2+f,0,1,4,1);
      }
    for(f=0;f<5;f++)
      {
      addnewbaddie(f*3,20-f,0,1,6,1);
      if(f>0) addnewbaddie(-(f*3),20-f,0,1,6,1);
      }
    break;

  case 5:
    addnewbaddie(0,70,2,0,40,10);
    addnewbaddie(0,50,1,0,40,10);
    break;

  case 6:
    for(f=0;f<4;f++)
      for(g=0;g<4;g++)
        {
        addnewbaddie(-19+10*f,10+g*10,1+(g%2),0,8,5);
        }
    for(f=0;f<5;f++)
      {
      addnewbaddie(f*3,20-f,0,1,10,1);
      if(f>0) addnewbaddie(-(f*3),20-f,0,1,10,1);
      }
    break;

  case 7:
    addnewbaddie(0,50,1,0,50,10);
    break;

  case 8:
    for(f=0;f<30;f++)
      addnewbaddie(-30+f*2,20,1+rand()%2,1+rand()%2,1,3);
    break;

  case 9:
    addnewbaddie(-10,10,-1,1,20,4);
    addnewbaddie( 10,10,-1,1,20,4);
    break;

  case 10:
    addnewbaddie(0,24,1,1,60,11);
    addnewbaddie(0,50,2,2,50,10);
    break;

  case 11:
    for(f=0;f<2;f++)
      addnewbaddie(f,70-f*16,3,-1,20,10);
    break;

  case 12:
    for(f=0;f<4;f++)
      addnewbaddie(-24+f*16,10,0,1,10,10);
    break;

  case 13:
    for(f=0;f<20;f++)
      addnewbaddie(f,70-f*3,3,-1,4,1);
    break;

  case 14:
    for(f=0;f<10;f++)
      {
      addnewbaddie(f*3,f*5,0,-1,8,1+2*(f%2));
      if(f>0) addnewbaddie(-(f*3),f*5,0,-1,8,1+2*(f%2));
      }
    break;

  case 15:
    for(f=0;f<10;f++)
      {
      addnewbaddie(f,20+f*5,0,-1,8,1+9*(f%2));
      if(f>0) addnewbaddie(-f,20+f*5,0,-1,8,1+9*(f%2));
      }
    break;

  case 16:
    for(f=0;f<4;f++)
      for(g=0;g<4;g++)
        {
        addnewbaddie(-19+10*f,10+g*10,2*(g%2),2-2*(g%2),8,5);
        }
    addnewbaddie(0,70,2,0,40,10);
    addnewbaddie(0,50,1,0,40,10);
    break;

  case 17:
    addnewbaddie(0,8,1,0,50,10);
    for(f=0;f<15;f++)
      addnewbaddie(-30+f*4,50,1,0,10,1);
    break;

  case 18:
    for(f=0;f<5;f++)
      {
      addnewbaddie(f*3,20-f,0,1,10,4);
      if(f>0) addnewbaddie(-(f*3),20-f,0,1,10,4);
      }
    break;

  case 19:
    addnewbaddie(0,24,1,1,100,11);
    addnewbaddie(0,56,-1,1,60,11);
    break;

  case 20:
    addnewbaddie(-10,30,-2,1,20,4);
    addnewbaddie( 10,30,-2,1,20,4);
    addnewbaddie(-10,10,-1,1,20,4);
    addnewbaddie(0,56,-1,1,60,11);
    addnewbaddie(0,30,1,0,30,10);
    for(f=0;f<5;f++)
      {
      addnewbaddie(f*3,20-f,0,1,10,1);
      if(f>0) addnewbaddie(-(f*3),20-f,0,1,10,1);
      }
    break;

  case 21:
    for(f=0;f<4;f++)
      {
      addnewbaddie(f*2,20+f*5,0,-1,8,10+(f%2));
      if(f>0) addnewbaddie(-(f*2),20+f*5,0,-1,8,10+(f%2));
      }
    break;

  case 22:
    for(a=-3.14;a<3.14;a+=(3.14/7.0))
      {
      addnewbaddie((int)(25.0*sin(a)),(int)(40+30.0*cos(a)),0,0,6,1);
      addnewbaddie((int)(20.0*sin(a)),(int)(40+10.0*cos(a)),0,0,2,3);
      }
    addnewbaddie(0,40,0,0,100,11);
    break;

  case 23:
    for(f=0;f<4;f++)
      {
      addnewbaddie(0,20+f,1,0,50,11);
      addnewbaddie(f*4,20+f*4,2,0,10,5);
      addnewbaddie(f*4,80-f*4,2,0,10,5);
      }
    break;

  case 24:
    addnewbaddie(0,40,1,0,100,11);
    for(a=-3.14;a<3.14;a+=(3.14/5.0))
      {
      addnewbaddie((int)(25.0*sin(a)),(int)(40+30.0*cos(a)),2,0,15,1);
      }
    for(f=0;f<10;f++)
      {
      addnewbaddie(-20+f*4,8+f*8,2,0,10,5);
      addnewbaddie(20-f*4,80-(f*8),-2,0,10,5);
      }
    break;

  case 25:
    for(f=0;f<12;f++)
      addnewbaddie(-12+f,10+f*3,2,0,50,10);
    break;

  case 26:
    for(a=-3.14;a<3.14;a+=(3.14/3.0))
      {
      addnewbaddie((int)(3.0*sin(a)),(int)(40+20.0*cos(a)),0,0,50,11);
      }
    for(f=0;f<12;f++)
      {
      addnewbaddie(-24+f*4,8+f*7,2,0,10,5);
      addnewbaddie(24-f*4,90-(f*7),-2,0,10,5);
      }
    break;

  default:
    return(-1);
  }
return(0);
}

