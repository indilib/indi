#include <stdio.h>
#include <stdlib.h>

#include "testclient.h"

int main()
{
    bool rc;
    testclient *client;

    client=new testclient();

    client->setServer("localhost",7624);
    rc=client->connectServer();
    sleep(1);  //  liet it connect
    if(rc) client->dotest();
    printf("program exit\n");

    return 0;
}
