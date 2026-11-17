/* SPDX-License-Identifier: LGPL-2.1-or-later */
#include <indicom.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
static int one(int bounded,int mode) {
 int f[2],n=0;char b[64];if(socketpair(AF_UNIX,SOCK_STREAM,0,f))return 1;
 memset(b,'A',sizeof b);b[16]='#';
 if(mode==1)write(f[1],"AB",2);
 if(mode==2)write(f[1],"OK#",3);
 close(f[1]);
 int r=bounded?tty_nread_section(f[0],b,sizeof b,'#',1,&n):tty_read_section(f[0],b,'#',1,&n);close(f[0]);
 int want=mode==2?TTY_OK:TTY_READ_ERROR;int count=mode==2?3:mode==1?2:0;int pass=r==want&&n==count;
 printf("%s %s %s: result=%d bytes=%d expected=%d bytes=%d\n",pass?"PASS":"FAIL",bounded?"bounded":"unbounded",mode==0?"empty EOF":mode==1?"partial EOF":"valid delimiter",r,n,want,count);return !pass;
}
int main(void){int bad=0;for(int b=0;b<2;b++)for(int m=0;m<3;m++)bad+=one(b,m);return bad?1:0;}
