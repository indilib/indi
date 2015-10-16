#include <math.h>
#include "lx200gemini.h"

LX200Gemini::LX200Gemini()
{
    setVersion(1, 0);

    IUFillNumber(&SlewAccuracyN[0], "SlewRA",  "RA (arcmin)", "%10.6m",  0., 60., 1., 3.0);
    IUFillNumber(&SlewAccuracyN[1], "SlewDEC", "Dec (arcmin)", "%10.6m", 0., 60., 1., 3.0);
    IUFillNumberVector(&SlewAccuracyNP, SlewAccuracyN, NARRAY(SlewAccuracyN), getDeviceName(), "Slew Accuracy", "", OPTIONS_TAB, IP_RW, 0, IPS_IDLE);
}

bool LX200Gemini::updateProperties()
{
    LX200Generic::updateProperties();

    if (isConnected())
    {
        defineNumber(&SlewAccuracyNP);

        // Delete unsupported properties
        deleteProperty(AlignmentSP.name);
    }
    else
    {
        deleteProperty(SlewAccuracyNP.name);
    }

    return true;
}

bool LX200Gemini::ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n)
{
    if(strcmp(dev,getDeviceName())==0)
    {
        if (!strcmp(name, SlewAccuracyNP.name))
        {
            if (IUUpdateNumber(&SlewAccuracyNP, values, names, n) < 0)
                return false;

            SlewAccuracyNP.s = IPS_OK;

            if (SlewAccuracyN[0].value < 3 || SlewAccuracyN[1].value < 3)
                IDSetNumber(&SlewAccuracyNP, "Warning: Setting the slew accuracy too low may result in a dead lock");

            IDSetNumber(&SlewAccuracyNP, NULL);
            return true;
        }
    }

    return LX200Generic::ISNewNumber(dev, name, values, names, n);
}


const char * LX200Gemini::getDefaultName()
{
    return (char *)"Losmandy Gemini";
}


bool LX200Gemini::isSlewComplete()
{
    const double dx = targetRA - currentRA;
    const double dy = targetDEC - currentDEC;
    return fabs(dx) <= (SlewAccuracyN[0].value/(900.0)) && fabs(dy) <= (SlewAccuracyN[1].value/60.0);
}

bool LX200Gemini::saveConfigItems(FILE *fp)
{
    INDI::Telescope::saveConfigItems(fp);

    IUSaveConfigNumber(fp, &SlewAccuracyNP);

    return true;
}
