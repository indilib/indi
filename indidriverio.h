
/* A driverio struct is valid only for sending one xml message */
typedef struct driverio{
    struct userio userio;
    void * user;
    void ** joins;
    size_t * joinSizes;
    int joinCount;
    int locked;
    char * outBuff;
    unsigned int outPos;
} driverio;

void driverio_init(driverio * dio);
void driverio_finish(driverio * dio);
