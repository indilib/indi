#ifndef EQ500X_H
#define EQ500X_H

class EQ500X : public LX200Generic
{
public:
    EQ500X();
    const char *getDefautName();
protected:
    bool getEQ500XRA(double *);
    bool setEQ500XRA(double);
    bool getEQ500XDEC(double *);
    bool setEQ500XDEC(double);
    bool slewEQ500X();
    int sendCmd(const char *);
    virtual bool checkConnection();
    virtual bool ReadScopeStatus();
    virtual bool initProperties() override;
    virtual bool Goto(double, double) override;
    virtual void getBasicData() override;
    bool getEQ500XRA_snprint(char *buf, size_t buf_length, double value);
    bool getEQ500XDEC_snprint(char *buf, size_t buf_length, double value);
    bool updateLocation(double latitude, double longitude, double elevation);
private:
    double previousRA = {0}, previousDEC = {0};
    ln_lnlat_posn lnobserver { 0, 0 };
    ln_hrz_posn lnaltaz { 0, 0 };
    bool forceMeridianFlip { false };
};

#endif // EQ500X_H
