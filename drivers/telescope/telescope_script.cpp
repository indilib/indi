/*******************************************************************************
 Copyright(c) 2016 CloudMakers, s. r. o.. All rights reserved.

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Library General Public
 License version 2 as published by the Free Software Foundation.
 .
 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Library General Public License for more details.
 .
 You should have received a copy of the GNU Library General Public License
 along with this library; see the file COPYING.LIB.  If not, write to
 the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 Boston, MA 02110-1301, USA.
 *******************************************************************************/

#include "telescope_script.h"

#include <cstring>
#include <memory>

#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>

#define MAXARGS 20

typedef enum
{
    SCRIPT_CONNECT = 1,
    SCRIPT_DISCONNECT,
    SCRIPT_STATUS,
    SCRIPT_GOTO,
    SCRIPT_SYNC,
    SCRIPT_PARK,
    SCRIPT_UNPARK,
    SCRIPT_MOVE_NORTH,
    SCRIPT_MOVE_EAST,
    SCRIPT_MOVE_SOUTH,
    SCRIPT_MOVE_WEST,
    SCRIPT_ABORT,
    SCRIPT_COUNT
} scripts;

static std::unique_ptr<ScopeScript> scope_script(new ScopeScript());

ScopeScript::ScopeScript()
{
    SetTelescopeCapability(TELESCOPE_CAN_PARK | TELESCOPE_CAN_SYNC | TELESCOPE_CAN_GOTO | TELESCOPE_CAN_ABORT, 4);
}

const char *ScopeScript::getDefaultName()
{
    return (const char *)"Telescope Scripting Gateway";
}

bool ScopeScript::initProperties()
{
    INDI::Telescope::initProperties();

#if defined(__APPLE__)
    IUFillText(&ScriptsT[0], "FOLDER", "Folder", "/usr/local/share/indi/scripts");
#else
    IUFillText(&ScriptsT[0], "FOLDER", "Folder", "/usr/share/indi/scripts");
#endif
    IUFillText(&ScriptsT[SCRIPT_CONNECT], "SCRIPT_CONNECT", "Connect script", "connect.py");
    IUFillText(&ScriptsT[SCRIPT_DISCONNECT], "SCRIPT_DISCONNECT", "Disconnect script", "disconnect.py");
    IUFillText(&ScriptsT[SCRIPT_STATUS], "SCRIPT_STATUS", "Get status script", "status.py");
    IUFillText(&ScriptsT[SCRIPT_GOTO], "SCRIPT_GOTO", "Goto script", "goto.py");
    IUFillText(&ScriptsT[SCRIPT_SYNC], "SCRIPT_SYNC", "Sync script", "sync.py");
    IUFillText(&ScriptsT[SCRIPT_PARK], "SCRIPT_PARK", "Park script", "park.py");
    IUFillText(&ScriptsT[SCRIPT_UNPARK], "SCRIPT_UNPARK", "Unpark script", "unpark.py");
    IUFillText(&ScriptsT[SCRIPT_MOVE_NORTH], "SCRIPT_MOVE_NORTH", "Move north script", "move_north.py");
    IUFillText(&ScriptsT[SCRIPT_MOVE_EAST], "SCRIPT_MOVE_EAST", "Move east script", "move_east.py");
    IUFillText(&ScriptsT[SCRIPT_MOVE_SOUTH], "SCRIPT_MOVE_SOUTH", "Move south script", "move_south.py");
    IUFillText(&ScriptsT[SCRIPT_MOVE_WEST], "SCRIPT_MOVE_WEST", "Move west script", "move_west.py");
    IUFillText(&ScriptsT[SCRIPT_ABORT], "SCRIPT_ABORT", "Abort motion script", "abort.py");
    IUFillTextVector(&ScriptsTP, ScriptsT, SCRIPT_COUNT, getDefaultName(), "SCRIPTS", "Scripts", OPTIONS_TAB, IP_RW, 60,
                     IPS_IDLE);

    addDebugControl();
    setDriverInterface(getDriverInterface());
    return true;
}

bool ScopeScript::saveConfigItems(FILE *fp)
{
    INDI::Telescope::saveConfigItems(fp);
    IUSaveConfigText(fp, &ScriptsTP);
    return true;
}

void ScopeScript::ISGetProperties(const char *dev)
{
    INDI::Telescope::ISGetProperties(dev);
    defineProperty(&ScriptsTP);
}

bool ScopeScript::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    if (strcmp(dev, getDeviceName()) == 0 && strcmp(name, ScriptsTP.name) == 0)
    {
        IUUpdateText(&ScriptsTP, texts, names, n);
        IDSetText(&ScriptsTP, nullptr);
        return true;
    }
    return Telescope::ISNewText(dev, name, texts, names, n);
}

bool ScopeScript::RunScript(int script, ...)
{
    char tmp[256];
    strncpy(tmp, ScriptsT[script].text, sizeof(tmp));

    char **args = (char **)malloc(MAXARGS * sizeof(char *));
    int arg     = 1;
    char *p     = tmp;
    args[0] = p;
    while (arg < MAXARGS)
    {
        char *pp = strstr(p, " ");
        if (pp == nullptr)
            break;
        *pp++       = 0;
        args[arg++] = pp;
        p           = pp;
    }
    va_list ap;
    va_start(ap, script);
    while (arg < MAXARGS)
    {
        char *pp    = va_arg(ap, char *);
        args[arg++] = pp;
        if (pp == nullptr)
            break;
    }
    va_end(ap);
    char path[1024];
    snprintf(path, sizeof(path), "%s/%s", ScriptsT[0].text, tmp);

    if (access(path, F_OK | X_OK) != 0)
    {
        LOGF_ERROR("Cannot use script [%s], %s", path, strerror(errno));
        return false;
    }
    if (isDebug())
    {
        char dbg[8 * 1024];
        snprintf(dbg, sizeof(dbg), "execvp('%s'", path);
        for (int i = 0; args[i]; i++)
        {
            strcat(dbg, ", '");
            strcat(dbg, args[i]);
            strcat(dbg, "'");
        }
        strcat(dbg, ", nullptr)");
        LOG_DEBUG(dbg);
    }

    int pid = fork();
    if (pid == -1)
    {
        LOG_ERROR("Fork failed");
        return false;
    }
    else if (pid == 0)
    {
        execvp(path, args);
        LOG_ERROR("Failed to execute script");
        exit(1);
    }
    else
    {
        int status;
        waitpid(pid, &status, 0);
        LOGF_DEBUG("Script %s returned %d", ScriptsT[script].text, status);
        return status == 0;
    }
}

bool ScopeScript::Handshake()
{
    return true;
}

bool ScopeScript::Connect()
{
    if (isConnected())
        return true;

    bool status = RunScript(SCRIPT_CONNECT, nullptr);
    if (status)
    {
        LOG_INFO("Successfully connected");
        ReadScopeStatus();
        SetTimer(getCurrentPollingPeriod());
    }
    else
    {
        LOG_ERROR("Failed to connect");
    }
    return status;
}

bool ScopeScript::Disconnect()
{
    bool status = RunScript(SCRIPT_DISCONNECT, nullptr);
    if (status)
    {
        LOG_INFO("Successfully disconnected");
    }
    else
    {
        LOG_WARN("Failed to disconnect");
    }
    return status;
}

bool ScopeScript::ReadScopeStatus()
{
    if (!isConnected())
        return false;
    char tmpfile[] = "/tmp/indi_telescope_script_status_XXXXXX";
    int fd = mkstemp(tmpfile);
    if (fd == -1)
    {
        LOGF_ERROR("Temp file %s creation for status script failed, %s", tmpfile, strerror(errno));
        unlink(tmpfile);
        return false;
    }
    close(fd);
    bool status = RunScript(SCRIPT_STATUS, tmpfile, nullptr);
    if (status)
    {
        int parked = 0;
        float ra = 0, dec = 0;
        FILE *file = fopen(tmpfile, "r");
        int ret = 0;

        ret = fscanf(file, "%d %f %f", &parked, &ra, &dec);
        fclose(file);
        unlink(tmpfile);
        if (parked != 0)
        {
            if (!isParked())
            {
                SetParked(true);
                LOG_INFO("Park successfully executed");
            }
        }
        else
        {
            if (isParked())
            {
                SetParked(false);
                LOG_INFO("Unpark successfully executed");
            }
        }
        NewRaDec(ra, dec);
    }
    else
    {
        LOG_ERROR("Failed to read status");
    }
    return status;
}

bool ScopeScript::Goto(double ra, double dec)
{
    char _ra[16], _dec[16];
    snprintf(_ra, 16, "%f", ra);
    snprintf(_dec, 16, "%f", dec);
    bool status = RunScript(SCRIPT_GOTO, _ra, _dec, nullptr);
    if (status)
    {
        LOG_INFO("Goto successfully executed");
        TrackState = SCOPE_SLEWING;
    }
    else
    {
        LOG_ERROR("Goto failed");
    }
    return status;
}

bool ScopeScript::Sync(double ra, double dec)
{
    char _ra[16], _dec[16];
    snprintf(_ra, 16, "%f", ra);
    snprintf(_dec, 16, "%f", dec);
    bool status = RunScript(SCRIPT_SYNC, _ra, _dec, nullptr);
    if (status)
    {
        LOG_INFO("Sync successfully executed");
    }
    else
    {
        LOG_ERROR("Failed to sync");
    }
    return status;
}

bool ScopeScript::Park()
{
    bool status = RunScript(SCRIPT_PARK, nullptr);
    if (!status)
    {
        LOG_ERROR("Failed to park");
    }
    return status;
}

bool ScopeScript::UnPark()
{
    bool status = RunScript(SCRIPT_UNPARK, nullptr);
    if (!status)
    {
        LOG_ERROR("Failed to unpark");
    }
    return status;
}

bool ScopeScript::MoveNS(INDI_DIR_NS dir, TelescopeMotionCommand command)
{
    char _rate[] = { (char)('0' + IUFindOnSwitchIndex(&SlewRateSP)), 0 };
    bool status  = RunScript(command == MOTION_STOP ? SCRIPT_ABORT :
                             dir == DIRECTION_NORTH ? SCRIPT_MOVE_NORTH : SCRIPT_MOVE_SOUTH,
                             _rate, nullptr, nullptr);
    return status;
}

bool ScopeScript::MoveWE(INDI_DIR_WE dir, TelescopeMotionCommand command)
{
    char _rate[] = { (char)('0' + IUFindOnSwitchIndex(&SlewRateSP)), 0 };
    bool status =
        RunScript(command == MOTION_STOP ? SCRIPT_ABORT : dir == DIRECTION_WEST ? SCRIPT_MOVE_WEST : SCRIPT_MOVE_EAST,
                  _rate, nullptr, nullptr);
    return status;
}

bool ScopeScript::Abort()
{
    bool status = RunScript(SCRIPT_ABORT, nullptr);
    if (status)
    {
        LOG_INFO("Successfully aborted");
    }
    else
    {
        LOG_ERROR("Failed to abort");
    }
    return status;
}
