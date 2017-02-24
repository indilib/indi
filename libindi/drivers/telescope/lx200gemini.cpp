#include <math.h>
#include <termio.h>
#include "lx200gemini.h"
#include "lx200driver.h"

#define GEMINI_TIMEOUT	5

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
    // The gemini can tell us when the slew has stopped
    int error_type;
    int nbytes_write = 4;
    if ( (error_type = tty_write_string(PortFD, ":Gv#", &nbytes_write)) != TTY_OK)
        return error_type > 0;

    int nbytes_read = 255;
    char temp_string[256];
    error_type = tty_read_section(PortFD, temp_string, '#', GEMINI_TIMEOUT, &nbytes_read);

    tcflush(PortFD, TCIFLUSH);

    if (nbytes_read < 1)
        return error_type > 0;

    // Allowed values back from the gemini are
    // N - Stopped
    // T - Tracking
    // G - Guiding
    // C - Centering
    // S - Slewing

    // The slew has stopped when the mount is tracking.  It is possible
    // that the mount my be stopped (Terrestrial mode), but then it
    // isn't really ready to use for astronomy either
    bool isComplete = (temp_string[0] == 'T');

    DEBUGF(INDI::Logger::DBG_DEBUG, "isSlewComplete? %s", isComplete ? "true" : "false");

    return isComplete;
}

bool LX200Gemini::saveConfigItems(FILE *fp)
{
    INDI::Telescope::saveConfigItems(fp);

    IUSaveConfigNumber(fp, &SlewAccuracyNP);

    return true;
}

