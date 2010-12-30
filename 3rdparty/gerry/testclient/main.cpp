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
    sleep(5);  //  liet it connect
    //if(rc) client->dotest();
    //printf("program exit\n");

    //  Disconnect
    printf("Calling disconnect\n");
    client->disconnectServer();
    //  since brain damaged function dont return success/failure indications
    //  lets just wait and see if we get the print
    sleep(5);
    if(client->Connected) {
        printf("No Disconnect messages came to the client\n");
    }

    return 0;
}
