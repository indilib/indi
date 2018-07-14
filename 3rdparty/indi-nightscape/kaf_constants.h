#ifndef KAF_CONSTANTS_H
#define KAF_CONSTANTS_H

#define KAF8300_MAX_X 3448
#define KAF8300_MAX_Y 2574
//Trailing 2 dummy 1 CTE 3 dummy 6 dark dummy 39 dark 8 dar dummy 4 blue 16 active buf
#define KAF8300_PREAMBLE (2 + 1 + 3 + 6 + 39 +8 + 4+  + 16) 
//Leading 16 active 4 blue 5 dark dummy 5 dummy 1 active 8 dummy 4 virtual dummy
#define KAF8300_POSTAMBLE (4 + 8+ 1 + 5 + 5 + 4 + 16) 
#define KAF8300_ACTIVE_X 3326 

//trailing 6 dark dummy 12 dark 8 dark dumy 4 blue 16 active buf 
#define KAF8300_Y_PREAMBLE (6 + 12 + 8 + 4 + 16)
//Leading 1 active cte 3 dark dummy 4 blue 16 active
#define KAF8300_Y_POSTAMBLE (1+3+4+16)

#define IMG_Y 2506
#define IMG_Y_HALF 1254

#define CMD_SIZE 16

#endif