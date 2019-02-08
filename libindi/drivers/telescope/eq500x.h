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
    virtual bool ReadScopeStatus();
    virtual bool initProperties() override;
    virtual bool Goto(double, double) override;
    virtual void getBasicData() override;
    bool getEQ500XRA_snprint(char *buf, size_t buf_length, double value);
    bool getEQ500XDEC_snprint(char *buf, size_t buf_length, double value);
};

#endif // EQ500X_H
