/*******************************************************************************
  Copyright(c) 2022 Etienne Cochard. All rights reserved.

  This program is free software; you can redistribute it and/or modify it
  under the terms of the GNU General Public License as published by the Free
  Software Foundation; either version 2 of the License, or (at your option)
  any later version.

  This program is distributed in the hope that it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
  more details.

  You should have received a copy of the GNU Library General Public License
  along with this library; see the file COPYING.LIB.  If not, write to
  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
  Boston, MA 02110-1301, USA.

  The full GNU General Public License is included in this distribution in the
  file called LICENSE.
*******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <map>
#include <math.h>
#include <memory>

#include <indicom.h>

#include "version.h"
#include "indi_shelyak_usis.h"

double _atof( const char* in )
{
    char* e;
    return strtof( in, &e );
}

static std::unique_ptr<ShelyakDriver> usis(new ShelyakDriver());

// :: Driver ::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

ShelyakDriver::ShelyakDriver( )
{
    _guid = 1;
    _serialPort = -1;

    setVersion( SHELYAK_USIS_VERSION_MAJOR, SHELYAK_USIS_VERSION_MINOR );
}

ShelyakDriver::~ShelyakDriver( )
{
    if ( _serialPort != -1 )
    {
        clearProperties( );
        tty_disconnect( _serialPort );
    }
}

/**
 * read the configuration file
 */

bool ShelyakDriver::readConfig( )
{
    auto fname = getSharedFilePath("shelyak_boards.json");

    try
    {
        std::ifstream file( fname );
        file >> _config;
        return true;
    }
    catch( const std::ifstream::failure &e )
    {
        LOGF_ERROR( "File not found: %s", fname.c_str() );
        return false;
    }
    catch( json::exception & )
    {
        LOGF_ERROR( "Bad json file format: %s", fname.c_str() );
        return false;
    }
}

/**
 *
 */

bool ShelyakDriver::findBoard( const char* boardName, json* result )
{
    if( !_config.is_object() )
    {
        return false;
    }

    json boards = _config["boards"];
    if( !boards.is_array() )
    {
        return false;
    }

    for( json::iterator it = boards.begin(); it != boards.end(); ++it)
    {

        if( !it->is_object() )
        {
            continue;
        }

        json sig = (*it)["signature"];
        if( !sig.is_string() )
        {
            continue;
        }

        std::string s = sig;
        if( s == boardName )
        {
            *result = *it;
            return true;
        }
    }

    return false;
}

/* Returns the name of the device. */
const char* ShelyakDriver::getDefaultName( )
{
    return "Shelyak Usis";
}

/* */
bool ShelyakDriver::Connect( )
{
    int rc = TTY_OK;
    if ( ( rc = tty_connect( serialConnection->port(), 2400, 8, 0, 1, &_serialPort ) ) != TTY_OK )
    {
        char errMsg[MAXRBUF];
        tty_error_msg( rc, errMsg, MAXRBUF );
        LOGF_ERROR( "Failed to connect to port %s. Error: %s", serialConnection->port(), errMsg );
        return false;
    }

    LOGF_INFO( "%s is online.", getDeviceName( ) );
    return true;
}

bool ShelyakDriver::Disconnect( )
{
    if ( _serialPort != -1 )
    {
        tty_disconnect( _serialPort );
        LOGF_INFO( "%s is offline.", getDeviceName( ) );
    }

    clearProperties( );

    return true;
}

/* Initialize and setup all properties on startup. */
bool ShelyakDriver::initProperties( )
{
    INDI::DefaultDevice::initProperties( );

    //--------------------------------------------------------------------------------
    // Options
    //--------------------------------------------------------------------------------
    serialConnection = new Connection::Serial(this);
    serialConnection->setDefaultPort("/dev/ttyACM0");
    serialConnection->registerHandshake([&]()
    {
        return true;
    });
    registerConnection(serialConnection);

    // read the usis.json file
    readConfig();

    setDriverInterface( SPECTROGRAPH_INTERFACE );
    return true;
}

Action* ShelyakDriver::createAction( PropType type, const std::string &command )
{
    Action* act = new Action( _guid++, command, type );
    _actions.push_back( act );
    return act;
}

/**
 *
*/

void ShelyakDriver::scanProperties( )
{
    if( !_config.is_object() )
    {
        return;
    }

    UsisResponse rsp;

    // ask the board name
    if ( sendCmd( &rsp, "GET;DEVICE_NAME;VALUE" ) )
    {

        // try to find dthe device definition in the .json
        json device;
        if( !findBoard( rsp.parts[4], &device ) )
        {
            LOGF_ERROR( "unknown device: %s", rsp.parts[4] );
            fprintf( stderr, "device not found %s\n", rsp.parts[4] );
            return;
        }
        else
        {
            LOGF_DEBUG( "found device: %s", rsp.parts[4] );
            fprintf( stderr, "found device %s\n", rsp.parts[4] );

            // ok, found:
            //	enumerate categories and build properties

            json categs = device["categories"];
            if( categs.is_object() )
            {
                for (json::iterator it = categs.begin(); it != categs.end(); ++it)
                {
                    fprintf( stderr, "category %s\n", it.key().c_str() );
                    genCatProps( it.key().c_str(), it.value() );
                }
            }
        }
    }

}

/**
 *
*/

void ShelyakDriver::genCatProps( const char* catName, json &category )
{

    //we scan different properties
    if( !category.is_array() )
    {
        return;
    }

    for ( auto it = category.begin(); it != category.end(); ++it)
    {

        json item = *it;
        if( !item.is_object( ) )
        {
            continue;
        }

        json jname = item["name"];
        if( !jname.is_string( ) )
        {
            LOGF_ERROR( "expected property name", "" );
            continue;
        }

        json jtype = item["type"];
        if( !jtype.is_string() )
        {
            LOGF_ERROR( "expected property type", "" );
            continue;
        }

        json jcmd = item["command"];
        if( !jcmd.is_string() )
        {
            LOGF_ERROR( "expected property command", "" );
            continue;
        }

        // we create a internal action

        Action* act;

        std::string name = jname;
        std::string type = jtype;
        std::string cmd = jcmd;

        fprintf( stderr, "        name: %s\n", name.c_str() );

        // ------------------------------------------------------
        if ( type == "string" )
        {

            fprintf( stderr, "        type: string\n" );

            act = createAction( _text, type.c_str() );

            TextValue &text = act->text;
            IUFillText( &text._val, act->uid, cmd.c_str(), "" );
            IUFillTextVector( &text._vec, &text._val, 1, getDeviceName( ), act->uid, name.c_str(), catName, IP_RW, 60, IPS_OK );

            defineProperty( &act->text._vec );
        }
        // ------------------------------------------------------
        else if ( type == "enum" )
        {

            fprintf( stderr, "        type: enum\n" );

            json values = item["values"];
            if( !values.is_array() )
            {
                LOGF_ERROR( "expected enum values", "" );
                continue;
            }

            act = createAction( _enum, cmd.c_str() );
            EnumValue &enm = act->enm;

            ISwitch* sws = enm._vals;
            unsigned nsw = 0;

            // allowed values
            for (json::iterator it = values.begin(); it != values.end() && nsw < MAX_ENUMS; ++it, ++nsw)
            {
                json jvalue = (*it);

                if( jvalue.is_string() )
                {
                    std::string value = jvalue;

                    Action* sub_act = createAction( _eitm, cmd.c_str() );
                    sub_act->itm.parent = act;
                    sub_act->itm.index = nsw;
                    sub_act->itm.val = value;
                    IUFillSwitch( &sws[nsw], sub_act->uid, value.c_str(), ISS_OFF );
                }
            }

            IUFillSwitchVector( &enm._vec, sws, nsw, getDeviceName( ), act->uid, name.c_str(), catName, IP_RW, ISR_1OFMANY, 60,
                                IPS_OK );
            defineProperty( &enm._vec );
        }
        // ------------------------------------------------------
        else if ( type == "number" )
        {

            fprintf( stderr, "        type: number\n" );

            double minVal = -9999.0;
            double maxVal = +9999.0;
            double precVal = 0.01;

            // fix min / max
            json min = item["min"];
            if( min.is_number() )
            {
                minVal = min;
                fprintf( stderr, "            min: %lf\n", minVal );
            }

            json max = item["max"];
            if( max.is_number() )
            {
                maxVal = max;
                fprintf( stderr, "            max: %lf\n", maxVal );
            }

            json prec = item["prec"];
            if( prec.is_number() )
            {
                precVal = prec;
                fprintf( stderr, "            prec: %lf\n", precVal );
            }

            act = createAction( _number, cmd );

            NumValue  &num = act->num;
            IUFillNumber( &num._val, act->uid, name.c_str(), "%.2f", minVal, maxVal, precVal, 0 );
            IUFillNumberVector( &num._vec, &num._val, 1, getDeviceName( ), act->uid, name.c_str(), catName, IP_RW, 5, IPS_OK );

            defineProperty( &num._vec );


            json actions = item["actions"];
            if( actions.is_array() )
            {
                fprintf( stderr, "        actions:\n" );

                uint32_t flags = 0;
                for (json::iterator it = actions.begin(); it != actions.end(); ++it)
                {

                    json value = (*it);
                    if( value.is_string() )
                    {
                        std::string n = value;
                        if( n == "STOP" )
                        {
                            flags |= ACTION_STOP;
                            fprintf( stderr, "            stop\n" );
                        }
                        else if( n == "CALIB" )
                        {
                            flags |= ACTION_CALIB;
                            fprintf( stderr, "            calib\n" );
                        }
                    }
                }

                if( flags )
                {

                    act = createAction( _enum, cmd );
                    EnumValue &enm = act->enm;

                    ISwitch* sws = enm._vals;
                    unsigned nsw = 0;

                    if( flags & ACTION_STOP )
                    {
                        Action* sub_act = createAction( _ecmd, "" );
                        sub_act->cmd.cmd = "STOP";
                        sub_act->cmd.name = cmd;
                        sub_act->cmd.parent = act;
                        IUFillSwitch( &sws[nsw++], sub_act->uid, "STOP", ISS_OFF );
                    }

                    if( flags & ACTION_CALIB )
                    {
                        Action* sub_act = createAction( _ecmd, "" );
                        sub_act->cmd.cmd = "CALIB";
                        sub_act->cmd.name = cmd;
                        sub_act->cmd.parent = act;
                        IUFillSwitch( &sws[nsw++], sub_act->uid, "CALIB", ISS_OFF );
                    }

                    IUFillSwitchVector( &enm._vec, sws, nsw, getDeviceName( ), act->uid, " ", catName, IP_RW, ISR_ATMOST1, 60, IPS_IDLE );
                    defineProperty( &enm._vec );
                }
            }
        }
        else
        {
            LOGF_ERROR( "bad property type %s", type.c_str() );
        }
    }
}

/**
 *
*/

void ShelyakDriver::__update( void* ptr )
{
    ((ShelyakDriver*)ptr)->update( );
}

void ShelyakDriver::update( )
{

    if ( isConnected( ) )
    {
        //fprintf( stderr, "updating --------------------------------------\n" );

        for( auto it = _actions.cbegin(); it != _actions.cend(); ++it )
        {

            Action* p = (*it);
            if( !(p->type & 0x10) )
            {
                continue;
            }

            //fprintf( stderr, "test: %s\n", p->name );

            UsisResponse rsp;
            if( sendCmd( &rsp, "GET;%s;VALUE", p->name.c_str() ) )
            {

                if ( p->type == _text )
                {
                    p->text._vec.s = strcmp( rsp.parts[3], "BUSY") == 0 ? IPS_BUSY : IPS_OK;

                    p->text._vec.tp->text = rsp.parts[4];
                    IDSetText( &p->text._vec, NULL );
                }
                else if( p->type == _enum )
                {
                    p->enm._vec.s = strcmp( rsp.parts[3], "BUSY") == 0 ? IPS_BUSY : IPS_OK;

                    ISwitch* values = p->enm._vals;
                    for( int i = 0; i < p->enm._vec.nsp; i++ )
                    {
                        values[i].s = ISS_OFF;
                    }

                    for( auto it2 = _actions.cbegin(); it2 != _actions.cend(); ++it2 )
                    {
                        Action* c = (*it2);
                        if( c->type == _eitm && c->itm.parent == p && c->itm.val == rsp.parts[4] )
                        {
                            values[c->itm.index].s = ISS_ON;
                            IDSetSwitch( &p->enm._vec, NULL );
                            break;
                        }
                    }
                }
                else if ( p->type == _number )
                {
                    p->num._vec.s = strcmp( rsp.parts[3], "BUSY") == 0 ? IPS_BUSY : IPS_OK;

                    double value = _atof( rsp.parts[4] );
                    p->num._vec.np->value = value;
                    IDSetNumber( &p->num._vec, NULL );
                }
            }
        }

        IEAddTimer( 1000, __update, this );
    }
}




// :: SERIAL ::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::


bool ShelyakDriver::sendCmd( UsisResponse* rsp, const char* text, ... )
{
    va_list lst;
    va_start( lst, text );

    bool rc = _send( text, lst ) && _receive( rsp );

    va_end( lst );
    return rc;
}

bool ShelyakDriver::_send( const char* text, va_list lst )
{
    char buf[MAX_FRAME_LENGTH + 1];
    vsnprintf( buf, MAX_FRAME_LENGTH, text, lst );

    //fprintf( stderr, "> sending %s\n", buf );
    //LOGF_INFO("> sending %s", buf );

    strcat( buf, "\n" );

    int rc, nbytes_written;
    if ( ( rc = tty_write( _serialPort, buf, strlen( buf ),
                           &nbytes_written ) ) != TTY_OK ) // send the bytes to the spectrograph
    {
        char errmsg[MAXRBUF];
        tty_error_msg( rc, errmsg, MAXRBUF );
        LOGF_ERROR("> sending %s", buf );
        LOGF_ERROR( "error: %s.", errmsg );
        return false;
    }

    return true;
}

bool ShelyakDriver::_receive( UsisResponse* rsp )
{
    int rc, nread;
    if ( ( rc = tty_nread_section( _serialPort, rsp->buffer, sizeof( rsp->buffer ), '\n', 100,
                                   &nread ) ) != TTY_OK ) // send the bytes to the spectrograph
    {
        char errmsg[MAXRBUF];
        tty_error_msg( rc, errmsg, MAXRBUF );
        LOGF_ERROR( "error: %s.", errmsg );
        return false;
    }

    rsp->buffer[nread] = 0;

    fprintf( stderr, "< received %s\n", rsp->buffer );
    //LOGF_INFO("< received %s", rsp->buffer );

    char* p = rsp->buffer;
    char* e = p + nread;

    // trim '\n' & ' '
    while ( e >= p && ( e[-1] == '\n' || e[-1] == ' ' ) )
    {
        e--;
        *e = 0;
    }

    rsp->pcount = 0;
    rsp->parts[rsp->pcount++] = p;

    while ( p < e && rsp->pcount < 5 )
    {
        if ( *p == ';' )
        {
            *p = 0;
            rsp->parts[rsp->pcount++] = p + 1;
        }

        p++;
    }

    if ( strcmp( rsp->parts[0], "M00" ) != 0 )
    {
        LOGF_ERROR( "response error: %s", rsp->parts[1] );
        return false;
    }

    return true;
}


// :: PROPERTIES ::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::


bool ShelyakDriver::updateProperties( )
{
    INDI::DefaultDevice::updateProperties( );
    if ( isConnected( ) )
    {
        //fprintf( stderr, "update request --------------------------------------\n" );
        scanProperties( );
        update( );
    }
    else
    {
        clearProperties( );
    }

    return true;
}

void ShelyakDriver::clearProperties( )
{
    //fprintf( stderr, "clear properties --------------------------------------\n" );
    for( auto it = _actions.cbegin(); it != _actions.cend(); ++it )
    {
        Action* p = *it;
        deleteProperty( p->uid );
    }

    _actions.clear();
}

/**
 * Handle a request to change a switch.
 **/

bool ShelyakDriver::ISNewSwitch( const char* dev, const char* name, ISState* states, char* names[], int n )
{
    if ( dev && strcmp( dev, getDeviceName( ) ) == 0 )
    {

        for( auto it = _actions.cbegin(); it != _actions.cend(); ++it )
        {
            Action* p = *it;

            if( p->type == _eitm && strcmp( p->uid, names[0] ) == 0 )
            {
                UsisResponse rsp;
                sendCmd( &rsp, "SET;%s;VALUE;%s", p->name.c_str(), p->itm.val.c_str() );
                return true;
            }
            else if( p->type == _ecmd && strcmp( p->uid, names[0] ) == 0 )
            {
                UsisResponse rsp;
                sendCmd( &rsp, "%s;%s;", p->cmd.cmd.c_str(), p->cmd.name.c_str() );

                EnumValue &enm = p->cmd.parent->enm;
                IUResetSwitch( &enm._vec );
                enm._vec.s = IPS_IDLE;
                IDSetSwitch(&enm._vec, nullptr);
                return true;
            }
        }
    }

    return INDI::DefaultDevice::ISNewSwitch( dev, name, states, names, n ); // send it to the parent classes
}

/**
 * Handle a request to change text.
 **/

bool ShelyakDriver::ISNewText( const char* dev, const char* name, char* texts[], char* names[], int n )
{
    if ( dev && strcmp( dev, getDeviceName( ) ) == 0 )
    {
        for( auto it = _actions.cbegin(); it != _actions.cend(); ++it )
        {
            Action* p = *it;
            if ( p->type == _text && strcmp( p->uid, names[0] ) == 0 )
            {
                UsisResponse rsp;
                sendCmd( &rsp, "SET;%s;VALUE;%s", p->name.c_str(), texts[0] );
                return true;
            }
        }
    }

    return INDI::DefaultDevice::ISNewText( dev, name, texts, names, n );
}

/**
 *
 */

bool ShelyakDriver::ISNewNumber( const char* dev, const char* name, double values[], char* names[], int n )
{
    if ( dev && strcmp( dev, getDeviceName( ) ) == 0 && n == 1 )
    {

        for( auto it = _actions.cbegin(); it != _actions.cend(); ++it )
        {
            Action* p = *it;
            if ( p->type == _number && strcmp( p->uid, names[0] ) == 0 )
            {
                UsisResponse rsp;
                sendCmd( &rsp, "SET;%s;VALUE;%lf", p->name.c_str(), values[0] );
                return true;
            }
        }
    }

    return INDI::DefaultDevice::ISNewNumber( dev, name, values, names, n );
}



