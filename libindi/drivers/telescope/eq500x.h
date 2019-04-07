#ifndef EQ500X_H
#define EQ500X_H

class EQ500X : public LX200Generic
{
public:
    class MechanicalPoint
    {
    public:
        //MechanicalPoint();
    public:
        double RAm() const { return _RAm; }
        double DECm() const { return _DECm; }
        double RAm(double const);
        double DECm(double const);
    public:
        bool isFlipped() const { return _isFlipped; }
        bool setFlipped(bool const);
    public:
        bool parseStringRA(char const *, size_t);
        bool parseStringDEC(char const *, size_t);
        char const * toStringRA(char *, size_t) const;
        char const * toStringDEC(char *, size_t) const;
    public:
        double operator -(MechanicalPoint &b) const;
        double RA_degrees_to(MechanicalPoint &b) const;
        double DEC_degrees_to(MechanicalPoint &b) const;
    protected:
        bool _isFlipped {false};
        double _RAm {0.0}, _DECm {90.0};
    };
public:
    EQ500X();
    const char *getDefautName();
protected:
    bool getCurrentPosition(MechanicalPoint&);
    bool setTargetPosition(MechanicalPoint const&);
    bool gotoTargetPosition();
    bool slewEQ500X();
protected:
    int sendCmd(char const *);
    int getReply(char *, size_t const);
protected:
    virtual bool checkConnection();
    virtual bool ReadScopeStatus();
    virtual bool initProperties() override;
    virtual bool Goto(double, double) override;
    virtual void getBasicData() override;
    bool updateLocation(double latitude, double longitude, double elevation);
private:
    void runTestSuite();
private:
    MechanicalPoint currentPosition, targetPosition;
    double previousRA = {0}, previousDEC = {0};
    ln_lnlat_posn lnobserver { 0, 0 };
    ln_hrz_posn lnaltaz { 0, 0 };
};

#endif // EQ500X_H
