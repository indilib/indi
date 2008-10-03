
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
 
#include "ApogeeNet.h"
#include "ApogeeNetErr.h"

#ifdef LINUX
#include "stdafx.h"
#endif

int main( int argc, char* argv[] )
{
     unsigned long subnet;
     unsigned short count;
     char cameras[1024];

     subnet = 256*256*256*192 + 256*256*168 + 255;
     ApnNetDiscovery(subnet, &count, cameras);
						 
     printf ("%d %s\n", count, cameras);
     exit(0);
}

