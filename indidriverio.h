
typedef struct driverio{
    struct userio userio;
    void * user;
    int * fds;
    int fdCount;
    char * outBuff;
    unsigned int outPos;
} driverio;

void driverio_init(struct driverio * dio);
void driverio_finish();
