/* gen11.c - host helper: print every state at distance 11 as a 14-digit
 * input string, using the same reference model as verify.c. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cube.h"
static uint32_t ref_rank(const state_t *s){uint32_t p=0,o=0;for(int i=0;i<CUBIES;i++){uint32_t m=0;for(int j=i+1;j<CUBIES;j++)m+=s->p[j]<s->p[i];p=p*(uint32_t)(CUBIES-i)+m;}for(int i=0;i<6;i++)o=o*3+s->o[i];return p*ORIS+o;}
static void ref_unrank(uint32_t r,state_t*s){uint8_t av[CUBIES]={0,1,2,3,4,5,6};uint32_t p=r/ORIS,o=r%ORIS,f=720;unsigned sum=0;for(int i=0;i<CUBIES;i++){uint32_t q=p/f;p%=f;s->p[i]=av[q];for(uint32_t j=q;j+1<(uint32_t)(CUBIES-i);j++)av[j]=av[j+1];if(i<5)f/=(uint32_t)(6-i);}for(int i=5;i>=0;i--){s->o[i]=(uint8_t)(o%3);sum+=s->o[i];o/=3;}s->o[6]=(uint8_t)((3-sum%3)%3);}
int main(void){uint8_t*d=malloc(STATES);uint32_t*q=malloc(4u*STATES),h=0,t=1;memset(d,0xFF,STATES);d[0]=0;q[0]=0;
 while(h<t){uint32_t r=q[h++];state_t s;ref_unrank(r,&s);for(int f=0;f<3;f++){state_t u=s;for(int k=0;k<3;k++){u=quarter_turn(u,f);uint32_t r2=ref_rank(&u);if(d[r2]==0xFF){d[r2]=d[r]+1;q[t++]=r2;}}}}
 for(uint32_t r=0;r<STATES;r++)if(d[r]==11){state_t s;ref_unrank(r,&s);for(int i=0;i<7;i++)putchar('1'+s.p[i]);for(int i=0;i<7;i++)putchar('1'+s.o[i]);putchar('\n');}
 return 0;}
