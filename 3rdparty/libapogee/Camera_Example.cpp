// Example source code for implementing the CCameraIO object

#include "windows.h"
#include "stdio.h"

#include "CameraIO.h"
#include "CameraIO_ISA_9x.h"
#include "CameraIO_PPI_9x.h"

#include "CameraIO_ISA_NT.h"
#include "CameraIO_PPI_NT.h"

#include "CameraIO_PCI.h"


// Error codes returned from config_load
const long CCD_OPEN_NOERR = 0;		// No error detected
const long CCD_OPEN_CFGNAME = 1;	// No config file specified
const long CCD_OPEN_CFGDATA = 2;	// Config missing or missing required data
const long CCD_OPEN_LOOPTST = 3;	// Loopback test failed, no camera found
const long CCD_OPEN_ALLOC = 4;		// Memory alloc failed - system error
const long CCD_OPEN_NTIO = 5;		// NT I/O driver not present

CCameraIO* cam;		// the Camera interface object

// Function declarations for this file
int InitCam( char* cfgname );
long config_load( char* cfgname, short BaseAddress, short RegOffset );
bool CfgGet ( FILE* inifp,
		   char* inisect,
		   char* iniparm,
		   char* retbuff,
		   short bufflen,
		   short* parmlen);

unsigned short hextoi(char* instr);
void trimstr(char* s);

// Initializes the CameraIO object from an INI file specified by cfgname
int InitCam( char* cfgname )
{
	long ret = config_load( cfgname, -1, -1 );
	if ( ret == 0 )
	{
		// We can now access the cam objects members
		cam->Flush();	// Start the camera flushing
		return 0;
	}
	else
	{
		switch ( ret )
		{
		case CCD_OPEN_CFGNAME:
			// "No config file specified."
			break;
		case CCD_OPEN_CFGDATA:
			// "Config file missing or missing required data."
			break;
		case CCD_OPEN_LOOPTST:
			// "Loopback test failed, no camera found"
			break;
		case CCD_OPEN_ALLOC:
			// "Memory allocation failed - system error"
			break;
		case CCD_OPEN_NTIO:
			// "NT I/O driver not present"
			break;
		}
		return ret;
	}
}

// Convert a string to a decimal or hexadecimal integer
unsigned short hextoi(char *instr)
{
    unsigned short val, tot = 0;
	bool IsHEX = false;

	long n = strlen( instr );
	if ( n > 1 )
	{	// Look for hex format e.g. 8Fh, A3H or 0x5D
		if ( instr[ n - 1 ] == 'h' || instr[ n - 1 ] == 'H' )
			IsHEX = true;
		else if ( *instr == '0' && *(instr+1) == 'x' )
		{
			IsHEX = true;
			instr += 2;
		}
	}

	if ( IsHEX )
	{
		while (instr && *instr && isxdigit(*instr))
		{
			val = *instr++ - '0';
			if (9 < val)
				val -= 7;
			tot <<= 4;
			tot |= (val & 0x0f);
		}
	}
	else
		tot = atoi( instr );

	return tot;
}

// Trim trailing spaces from a string
void trimstr(char *s)
{
    char *p;

    p = s + (strlen(s) - 1);
    while (isspace(*p))
        p--;
    *(++p) = '\0';
}


//-------------------------------------------------------------
// CfgGet
//
// Retrieve a parameter from an INI file. Returns a status code
// and the paramter string in retbuff.
//-------------------------------------------------------------
bool CfgGet ( FILE* inifp,
               char  *inisect,
               char  *iniparm,
               char  *retbuff,
               short bufflen,
               short *parmlen)
{
    short gotsect;
    char  tbuf[256];
    char  *ss, *eq, *ps, *vs, *ptr;

	rewind( inifp );

    // find the target section

    gotsect = 0;
    while (fgets(tbuf,256,inifp) != NULL) {
        if ((ss = strchr(tbuf,'[')) != NULL) {
            if (strnicmp(ss+1,inisect,strlen(inisect)) == 0) {
                gotsect = 1;
                break;
                }
            }
        }

    if (!gotsect) {                             // section not found
        return false;
        }

    while (fgets(tbuf,256,inifp) != NULL) {     // find parameter in sect

        if ((ptr = strrchr(tbuf,'\n')) != NULL) // remove newline if there
            *ptr = '\0';

        ps = tbuf+strspn(tbuf," \t");           // find the first non-blank

        if (*ps == ';')                         // Skip line if comment
            continue;

        if (*ps == '[') {                       // Start of next section
            return false;
            }

        eq = strchr(ps,'=');                    // Find '=' sign in string

        if (eq)
            vs = eq + 1 + strspn(eq+1," \t");   // Find start of value str
        else
            continue;

        // found the target parameter

        if (strnicmp(ps,iniparm,strlen(iniparm)) == 0) {

            if ((ptr = strchr(vs,';')) != NULL) // cut off an EOL comment
                *ptr = '\0';

            if (short(strlen(vs)) > bufflen - 1) {// not enough buffer space
                strncpy(retbuff,vs,bufflen - 1);
                retbuff[bufflen - 1] = '\0';    // put EOL in string
                *parmlen = bufflen;
                return true;
                }
            else {
                strcpy(retbuff,vs);             // got it
                trimstr(retbuff);               // trim any trailing blanks
                *parmlen = strlen(retbuff);
                return true;
                }
            }
        }

    return false;                         // parameter not found
}

// Initializes internal variables to their default value and reads the parameters in the
// specified INI file. Then initializes the camera using current settings. If BaseAddress
// or RegOffset parameters are specified (not equal to -1) then one or both of these
// values are used for the m_BaseAddress and m_RegisterOffset properties overriding those
// settings in the INI file.
long config_load( char* cfgname, short BaseAddress, short RegOffset )
{
    short plen;
    char retbuf[256];

    if ((strlen(cfgname) == 0) || (cfgname[0] == '\0')) return CCD_OPEN_CFGNAME;

    // attempt to open INI file
    FILE* inifp = NULL;

	if ((inifp = fopen(cfgname,"r")) == NULL) return CCD_OPEN_CFGDATA;

	// Check whether we are on an NT platform
	OSVERSIONINFO VersionInfo;
	memset(&VersionInfo, 0, sizeof(OSVERSIONINFO));
	VersionInfo.dwOSVersionInfoSize = sizeof (OSVERSIONINFO);
	GetVersionEx ( &VersionInfo );
	bool IsNT = VersionInfo.dwPlatformId != VER_PLATFORM_WIN32_WINDOWS;

    // System
    if (CfgGet (inifp, "system", "interface", retbuf, sizeof(retbuf), &plen))
	{
		// Assume cam is currently null
		if ( stricmp( "isa", retbuf ) == 0 )
		{
			if ( IsNT )
				cam = new CCameraIO_ISA_NT;
			else
				cam = new CCameraIO_ISA_9x;
		}
		else if ( stricmp( "ppi", retbuf ) == 0 )
		{
			if ( IsNT )
				cam = new CCameraIO_PPI_NT;
			else
				cam = new CCameraIO_PPI_9x;
		}
		else if ( stricmp( "pci", retbuf ) == 0 )
		{
			cam = new CCameraIO_PCI;
		}

		if ( cam == NULL )
		{
			fclose( inifp );
			return CCD_OPEN_ALLOC;
		}
	}
    else
	{
		fclose( inifp );
		return CCD_OPEN_CFGDATA;
	}

	/////////////////////////////////////////////////////////////////////////////////
	// Settings which are stored in a class member (not in firmware) are already set
	// to a default value in the constructor. Settings accessed by get/put functions
	// must be set to a default value in this routine, after the base address and
	// communication protocal is setup.

	/////////////////////////////////////////////////////////////////////////////////
	// These settings must done first since they affect communication with the camera

		if ( BaseAddress == -1 )
		{
			if (CfgGet (inifp, "system", "base", retbuf, sizeof(retbuf), &plen))
				cam->m_BaseAddress = hextoi(retbuf) & 0xFFF;
			else
			{
				fclose( inifp );
				delete cam;
				cam = NULL;
				return CCD_OPEN_CFGDATA;           // base address MUST be defined
			}
		}
		else
			cam->m_BaseAddress = BaseAddress & 0xFFF;

		if ( RegOffset == -1 )
		{
			if (CfgGet (inifp, "system", "reg_offset", retbuf, sizeof(retbuf), &plen))
			{
				unsigned short val = hextoi(retbuf);
				if ( val >= 0x0 && val <= 0xF0 ) cam->m_RegisterOffset = val & 0xF0;
			}
		}
		else
		{
			if ( RegOffset >= 0x0 && RegOffset <= 0xF0 ) cam->m_RegisterOffset = RegOffset & 0xF0;
		}

		/////////////////////////////////////////////////////////////////////////////////
		// Necessary geometry settings

		if (CfgGet (inifp, "geometry", "rows", retbuf, sizeof(retbuf), &plen))
		{
			short val = hextoi(retbuf);
			if ( val >= 1 && val <= MAXTOTALROWS ) cam->m_Rows = val;
		}
		else
		{
			fclose( inifp );
			delete cam;
			cam = NULL;
			return CCD_OPEN_CFGDATA;           // rows MUST be defined
		}

		if (CfgGet (inifp, "geometry", "columns", retbuf, sizeof(retbuf), &plen))
		{
			short val = hextoi(retbuf);
			if ( val >= 1 && val <= MAXTOTALCOLUMNS ) cam->m_Columns = val;
		}
		else
		{
			fclose( inifp );
			delete cam;
			cam = NULL;
			return CCD_OPEN_CFGDATA;           // columns MUST be defined
		}

		/////////////////////////////////////////////////////////////////////////////////

		if (CfgGet (inifp, "system", "pp_repeat", retbuf, sizeof(retbuf), &plen))
		{
			short val = hextoi( retbuf );
			if ( val > 0 && val <= 1000 ) cam->m_PPRepeat = val;
		}

		/////////////////////////////////////////////////////////////////////////////////
		// First actual communication with camera if in PPI mode
		if ( !cam->InitDriver() )
		{
			delete cam;
			cam = NULL;
			fclose( inifp );
			if ( IsNT )
				return CCD_OPEN_NTIO;
			else
				return CCD_OPEN_LOOPTST;
		}
		/////////////////////////////////////////////////////////////////////////////////
		// First actual communication with camera if in ISA mode
		cam->Reset();	// Read in command register to set shadow register known state
		/////////////////////////////////////////////////////////////////////////////////

		if (CfgGet (inifp, "system", "cable", retbuf, sizeof(retbuf), &plen))
		{
			if (!stricmp("LONG",retbuf))
				cam->write_LongCable( true );
			else if ( !stricmp("SHORT",retbuf) )
				cam->write_LongCable( false );
		}
		else
			cam->write_LongCable( false );	// default

		if ( !cam->read_Present() )
		{
			delete cam;
			cam = NULL;
			fclose( inifp );

			return CCD_OPEN_LOOPTST;
		}
	/////////////////////////////////////////////////////////////////////////////////
	// Set default setting and read other settings from ini file

	cam->write_UseTrigger( false );
	cam->write_ForceShutterOpen( false );

	if (CfgGet (inifp, "system", "high_priority", retbuf, sizeof(retbuf), &plen))
	{
		if (!stricmp("ON",retbuf) || !stricmp("TRUE",retbuf) || !stricmp("1",retbuf))
		{
			cam->m_HighPriority = true;
		}
		  else if (!stricmp("OFF",retbuf) || !stricmp("FALSE",retbuf) || !stricmp("0",retbuf))
		{
			cam->m_HighPriority = false;
		}
	}

	if (CfgGet (inifp, "system", "data_bits", retbuf, sizeof(retbuf), &plen))
	{
		short val = hextoi( retbuf );
		if ( val >= 8 && val <= 18 ) cam->m_DataBits = val;
	}

	if (CfgGet (inifp, "system", "sensor", retbuf, sizeof(retbuf), &plen))
	{
		if ( stricmp( "ccd", retbuf ) == 0 )
		{
			cam->m_SensorType = Camera_SensorType_CCD;
		}
		else if ( stricmp( "cmos", retbuf ) == 0 )
		{
			cam->m_SensorType = Camera_SensorType_CMOS;
		}
	}

    if (CfgGet (inifp, "system", "mode", retbuf, sizeof(retbuf), &plen))
	{
        unsigned short val = hextoi(retbuf) & 0xF;
        cam->write_Mode( val );
    }
	else
		cam->write_Mode( 0 );			// default

    if (CfgGet (inifp, "system", "test", retbuf, sizeof(retbuf), &plen))
	{
        unsigned short val = hextoi(retbuf) & 0xF;
        cam->write_TestBits( val );
    }
	else
		cam->write_TestBits( 0 );		//default

    if (CfgGet (inifp, "system", "test2", retbuf, sizeof(retbuf), &plen))
	{
        unsigned short val = hextoi(retbuf) & 0xF;
        cam->write_Test2Bits( val );
    }
	else
		cam->write_Test2Bits( 0 );	// default

	cam->write_FastReadout( false );	//default

    if (CfgGet (inifp, "system", "shutter_speed", retbuf, sizeof(retbuf), &plen))
	{
        if (!stricmp("normal",retbuf))
		{
			cam->m_FastShutter = false;
			cam->m_MaxExposure = 10485.75;
			cam->m_MinExposure = 0.01;
		}
		else if (!stricmp("fast",retbuf))
		{
			cam->m_FastShutter = true;
			cam->m_MaxExposure = 1048.575;
			cam->m_MinExposure = 0.001;
		}
		else if ( !stricmp("dual",retbuf))
		{
			cam->m_FastShutter = true;
			cam->m_MaxExposure = 10485.75;
			cam->m_MinExposure = 0.001;
		}
    }

    if (CfgGet (inifp, "system", "shutter_bits", retbuf, sizeof(retbuf), &plen))
	{
		unsigned short val = hextoi(retbuf);
		cam->m_FastShutterBits_Mode = val & 0x0F;
		cam->m_FastShutterBits_Test = ( val & 0xF0 ) >> 4;
	}

    if (CfgGet (inifp, "system", "maxbinx", retbuf, sizeof(retbuf), &plen))
	{
        short val = hextoi(retbuf);
        if ( val >= 1 && val <= MAXHBIN ) cam->m_MaxBinX = val;
	}

    if (CfgGet (inifp, "system", "maxbiny", retbuf, sizeof(retbuf), &plen))
	{
        short val = hextoi(retbuf);
        if ( val >= 1 && val <= MAXVBIN ) cam->m_MaxBinY = val;
	}

    if (CfgGet (inifp, "system", "guider_relays", retbuf, sizeof(retbuf), &plen))
	{
		if (!stricmp("ON",retbuf) || !stricmp("TRUE",retbuf) || !stricmp("1",retbuf))
		{
			cam->m_GuiderRelays = true;
		}
		  else if (!stricmp("OFF",retbuf) || !stricmp("FALSE",retbuf) || !stricmp("0",retbuf))
		{
			cam->m_GuiderRelays = false;
		}
	}

    if (CfgGet (inifp, "system", "timeout", retbuf, sizeof(retbuf), &plen))
	{
        double val = atof(retbuf);
		if ( val >= 0.0 && val <= 10000.0 ) cam->m_Timeout = val;
    }

	/////////////////////////////////////////////////////////////////////////////////
    // Geometry

    if (CfgGet (inifp, "geometry", "bic", retbuf, sizeof(retbuf), &plen))
	{
        short val = hextoi(retbuf);
        if ( val >= 1 && val <= MAXCOLUMNS ) cam->m_BIC = val;
	}

    if (CfgGet (inifp, "geometry", "bir", retbuf, sizeof(retbuf), &plen))
	{
        short val = hextoi(retbuf);
        if ( val >= 1 && val <= MAXROWS ) cam->m_BIR = val;
	}

	if (CfgGet (inifp, "geometry", "skipc", retbuf, sizeof(retbuf), &plen))
	{
        short val = hextoi(retbuf);
        if ( val >= 0 && val <= MAXCOLUMNS ) cam->m_SkipC = val;
	}

	if (CfgGet (inifp, "geometry", "skipr", retbuf, sizeof(retbuf), &plen))
	{
        short val = hextoi(retbuf);
        if ( val >= 0 && val <= MAXROWS ) cam->m_SkipR = val;
	}

    if (CfgGet (inifp, "geometry", "imgcols", retbuf, sizeof(retbuf), &plen))
	{
        short val = hextoi(retbuf);
        if ( val >= 1 && val <= MAXTOTALCOLUMNS ) cam->m_ImgColumns = val;
	}
	else
		cam->m_ImgColumns = cam->m_Columns - cam->m_BIC - cam->m_SkipC;

    if (CfgGet (inifp, "geometry", "imgrows", retbuf, sizeof(retbuf), &plen))
	{
        short val = hextoi(retbuf);
        if ( val >= 1 && val <= MAXTOTALROWS ) cam->m_ImgRows = val;
	}
	else
		cam->m_ImgRows = cam->m_Rows - cam->m_BIR - cam->m_SkipR;

    if (CfgGet (inifp, "geometry", "hflush", retbuf, sizeof(retbuf), &plen))
	{
        short val = hextoi(retbuf);
        if ( val >= 1 && val <= MAXHBIN ) cam->m_HFlush = val;
	}

    if (CfgGet (inifp, "geometry", "vflush", retbuf, sizeof(retbuf), &plen))
	{
        short val = hextoi(retbuf);
        if ( val >= 1 && val <= MAXVBIN ) cam->m_VFlush = val;
	}

	// Default to full frame
	cam->m_NumX = cam->m_ImgColumns;
	cam->m_NumY = cam->m_ImgRows;

	/////////////////////////////////////////////////////////////////////////////////
	// Temperature

    if (CfgGet (inifp, "temp", "control", retbuf, sizeof(retbuf), &plen))
	{
		if (!stricmp("ON",retbuf) || !stricmp("TRUE",retbuf) || !stricmp("1",retbuf))
		{
			cam->m_TempControl = true;
		}
		  else if (!stricmp("OFF",retbuf) || !stricmp("FALSE",retbuf) || !stricmp("0",retbuf))
		{
			cam->m_TempControl = false;
		}
    }

    if (CfgGet (inifp, "temp", "cal", retbuf, sizeof(retbuf), &plen))
	{
        short val = hextoi(retbuf);
		if ( val >= 1 && val <= 255 ) cam->m_TempCalibration = val;
    }

    if (CfgGet (inifp, "temp", "scale", retbuf, sizeof(retbuf), &plen))
	{
        double val = atof(retbuf);
		if ( val >= 1.0 && val <= 10.0 ) cam->m_TempScale = val;
    }

    if (CfgGet (inifp, "temp", "target", retbuf, sizeof(retbuf), &plen))
	{
        double val = atof(retbuf);
        if ( val >= -60.0 && val <= 40.0 )
			cam->write_CoolerSetPoint( val );
		else
			cam->write_CoolerSetPoint( -10.0 );
    }
	else
		cam->write_CoolerSetPoint( -10.0 );	//default

	/////////////////////////////////////////////////////////////////////////////////
	// CCD

	if (CfgGet (inifp, "ccd", "sensor", retbuf, sizeof(retbuf), &plen))
	{
		if ( plen > 256 ) plen = 256;
		memcpy( cam->m_Sensor, retbuf, plen );
    }

	if (CfgGet (inifp, "ccd", "color", retbuf, sizeof(retbuf), &plen))
	{
		if (!stricmp("ON",retbuf) || !stricmp("TRUE",retbuf) || !stricmp("1",retbuf))
		{
			cam->m_Color = true;
		}
		  else if (!stricmp("OFF",retbuf) || !stricmp("FALSE",retbuf) || !stricmp("0",retbuf))
		{
			cam->m_Color = false;
		}
    }

    if (CfgGet (inifp, "ccd", "noise", retbuf, sizeof(retbuf), &plen))
	{
		cam->m_Noise = atof( retbuf );
    }

	if (CfgGet (inifp, "ccd", "gain", retbuf, sizeof(retbuf), &plen))
	{
		cam->m_Gain = atof( retbuf );
    }

    if (CfgGet (inifp, "ccd", "pixelxsize", retbuf, sizeof(retbuf), &plen))
	{
		cam->m_PixelXSize = atof( retbuf );
    }

    if (CfgGet (inifp, "ccd", "pixelysize", retbuf, sizeof(retbuf), &plen))
	{
		cam->m_PixelYSize = atof( retbuf );
    }

	fclose( inifp );
    return CCD_OPEN_NOERR;
}

