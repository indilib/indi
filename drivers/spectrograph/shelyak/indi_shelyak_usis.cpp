/**
 * @file indi_shelyak_usis.cpp
 * @author Etienne Cochard
 * @copyright (c) 2022 R-libre ingenierie, all rights reserved.
 *
 * @description shelyak universal usis driver
 **/

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

#define PORT_SETTING _text_settings[0]
#define PORT_LINE _text_line[0]
#define DEFAULT_NAME "Shelyak Usis driver"

double _atof( const char* in )
{
	char* e;
	return strtof( in, &e );
}


#define _ERROR(fmt, ...) DEBUGFDEVICE( DEFAULT_NAME, INDI::Logger::DBG_ERROR, (fmt), __VA_ARGS__)



/**
 * read the given filename 
 * return null on error or the file content (zero term)
 */

char* readFile( const char* name ) {
	FILE* f = fopen( name, "rt" );
	if( !f ) {
		return NULL;
	}

	fseek( f, 0, SEEK_END );
	size_t size = ftell( f );
	fseek( f, 0, SEEK_SET );

	char* json = (char*)malloc( size+1 );
	if( fread( json, 1, size, f )!=size ) {
		return NULL;
	}

	fclose( f );

	json[size] = 0;
	return json;
}


/**
 * seach a board by it's name
 */

json_value* json_config = NULL;


json_value* get_item( json_value* object, const char* name ) {

	if( object->type==json_object ) {
		
		int length = object->u.object.length;
        for (int x = 0; x < length; x++) {
			json_object_entry& item = object->u.object.values[x];
			if( strcmp(item.name,name)==0 ) {
				return item.value;
			}
		}
	}

	return NULL;
}

/**
 * 
 */

json_value* findBoard( json_value* json, const char* sigName ) {

	size_t sigLen = strlen( sigName );

	json_value* boards = get_item( json, "boards" );
	if( !boards || boards->type!=json_array ) {
		_ERROR( "Cannot find boards definition in json.", "" );
		return NULL;
	}

	int length = boards->u.array.length;
    for ( int x = 0; x < length; x++) {
		json_value* item = boards->u.array.values[x];
		if( item->type==json_object ) {
			json_value* sig = get_item( item, "signature" );
			if( sig && sig->type==json_string && strncmp(sig->u.string.ptr,sigName,sigLen)==0 ) {
				return item;
			}
		}
	}

	return NULL;
}






// :: Driver ::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

ShelyakUsis::ShelyakUsis( )
{
	_props = nullptr;
	_serialPort = -1;
	setVersion( SHELYAK_USIS_VERSION_MAJOR, SHELYAK_USIS_VERSION_MINOR );
}

ShelyakUsis::~ShelyakUsis( )
{
	if ( _serialPort != -1 ) {
		tty_disconnect( _serialPort );
	}
}

/* Returns the name of the device. */
const char* ShelyakUsis::getDefaultName( )
{
	return DEFAULT_NAME;
}


/* */
bool ShelyakUsis::Connect( )
{
	int rc;
	if ( ( rc = tty_connect( PORT_SETTING.text, 2400, 8, 0, 1, &_serialPort ) ) != TTY_OK ) {
		char errMsg[MAXRBUF];
		tty_error_msg( rc, errMsg, MAXRBUF );
		LOGF_ERROR( "Failed to connect to port %s. Error: %s", PORT_SETTING.text, errMsg );
		return false;
	}

	LOGF_INFO( "%s is online.", getDeviceName( ) );
	return true;
}

bool ShelyakUsis::Disconnect( )
{
	if ( _serialPort != -1 ) {
		tty_disconnect( _serialPort );
		LOGF_INFO( "%s is offline.", getDeviceName( ) );
	}

	return true;
}

/* Initialize and setup all properties on startup. */
bool ShelyakUsis::initProperties( )
{
	INDI::DefaultDevice::initProperties( );




	
	//--------------------------------------------------------------------------------
	// Options
	//--------------------------------------------------------------------------------

	// setup the text input for the serial port
	IUFillText( &PORT_SETTING, "PORT", "Port", "/dev/ttyACM0" );
	IUFillTextVector( &PORT_LINE, &PORT_SETTING, 1, getDeviceName( ), "DEVICE_PORT", "Ports", COMMUNICATION_TAB, IP_RW, 60, IPS_IDLE );


	// read the usis.json file
	const char* fname = INSTALL_FOLDER"/bin/shelyak_boards.json";
		
	char* json = readFile( fname );
	if( !json ) {
		LOGF_ERROR( "Failed to read file %s.", fname );
	}
	else {
		json_config = json_parse(json,strlen(json) );
		if (json_config == NULL) {
			LOGF_ERROR( "Bad json content, file %s.", fname );
		}
		
		free( json );
	}

	//--------------------------------------------------------------------------------
	// Spectrograph Settings
	//--------------------------------------------------------------------------------

	// IUFillNumber(&_settings_num[0], "GRATING", "Grating [lines/mm]", "%.2f", 0, 1000, 0, 79);
	// IUFillNumber(&_settings_num[1], "INCIDENCE_ANGLE_ALPHA", "Incidence angle alpha [degrees]", "%.2f", 0, 90, 0, 62.2);
	// IUFillNumberVector(&SettingsNP, SettingsN, 5, getDeviceName(), "SPECTROGRAPH_SETTINGS", "Settings", "Spectrograph settings", IP_RW, 60, IPS_IDLE);

	setDriverInterface( SPECTROGRAPH_INTERFACE );
	return true;
}

void ShelyakUsis::ISGetProperties( const char* dev )
{
	INDI::DefaultDevice::ISGetProperties( dev );
	defineProperty( &PORT_LINE );
	loadConfig( true, PORT_SETTING.name );
}

void ShelyakUsis::genCatProps( const char* catName, json_value* categ ) {

	//we scan different properties
	
	int len = categ->u.object.length;
	for( int x=0; x<len; x++ ) {

		json_object_entry& item = categ->u.object.values[x];

		json_value* type = get_item( item.value, "type" );
		if( !type || type->type!=json_string ) {
			LOGF_ERROR( "expected property type", "" );
			continue;
		}

		json_value* cmd = get_item( item.value, "command" );
		if( !cmd || cmd->type!=json_string ) {
			LOGF_ERROR( "expected property command", "" );
			continue;
		}

		json_value* actions = get_item( item.value, "actions" );
		if( !actions || actions->type!=json_array ) {
			LOGF_ERROR( "expected property actions", "" );
			continue;
		}

		// we create a internal property
		UsisProperty* p = new UsisProperty;
		p->type = _undefined;
		p->actions = 0;
		p->next = _props;
		_props = p;

		strncpy( p->title, item.name, MAX_NAME_LENGTH + 1 );
		strncpy( p->name, cmd->u.string.ptr, MAX_NAME_LENGTH + 1 );
		
		// allowed actions
		for( unsigned a=0; a<actions->u.array.length; a++ ) {
			json_value* action = actions->u.array.values[a];
			if( action->type==json_string ) {

				if( strcmp(action->u.string.ptr,"SET")==0 ) {
					p->actions |= ACTION_SET;
				}
				else if( strcmp(action->u.string.ptr,"CALIB")==0 ) {
					p->actions |= ACTION_CALIB;
				}
				else if( strcmp(action->u.string.ptr,"STOP")==0 ) {
					p->actions |= ACTION_STOP;
				}
				else {
					LOGF_ERROR( "Unknown action: %s", action->u.string.ptr );
				}
			}
		}

		// switch property type...

		if ( strcmp( type->u.string.ptr, "string" ) == 0 ) {
			p->type = _text;
		}
		else if ( strcmp( type->u.string.ptr, "enum" ) == 0 ) {
			p->type = _enum;
			p->actions |= ACTION_SET;
			p->enm._evals = nullptr;
		}
		else if ( strcmp( type->u.string.ptr, "number" ) == 0 ) {
			p->type = _number;
			p->num.minVal = -9999.0;
			p->num.maxVal = +9999.0;
			p->num.prec = 0.01;

			// fix min / max
			json_value* min_ = get_item( item.value, "min" );
			if( min_ ) {
				if( min_->type==json_integer ) {
					p->num.minVal = min_->u.integer;
				} 
				else if( min_->type==json_double ) {
					p->num.minVal = min_->u.dbl;
				}
			}

			json_value* max_ = get_item( item.value, "max" );
			if( max_ ) {
				if( max_->type==json_integer ) {
					p->num.maxVal = max_->u.integer;
				} 
				else if( max_->type==json_double ) {
					p->num.maxVal = max_->u.dbl;
				}
			}

			json_value* prec_ = get_item( item.value, "prec" );
			if( prec_ ) {
				if( prec_->type==json_integer ) {
					p->num.prec = prec_->u.integer;
				} 
				else if( prec_->type==json_double ) {
					p->num.prec = prec_->u.dbl;
				}
			}
		}
		else {
			LOGF_ERROR( "bad property type %s", type->u.string.ptr );
			p->type = _text;
		}

		UsisResponse rsp;
		sendCmd( &rsp, "GET;%s;VALUE", p->name );

		if ( p->type == _text ) {
			strncpy( p->text.value, rsp.parts[4], sizeof(p->text.value) );
		}
		else if ( p->type == _enum ) {
			strncpy( p->enm.value, rsp.parts[4], sizeof(p->enm.value) );
		}
		else if ( p->type == _number ) {
			p->num.value = _atof( rsp.parts[4] );
		}

		createProperty( catName, p );
	}
}

void ShelyakUsis::scanProperties( )
{
	if( !json_config ) {
		return;
	}

	UsisResponse rsp;

	// ask the board name
	if ( sendCmd( &rsp, "GET;DEVICE_NAME;VALUE" ) ) {
		
		// try to find dthe device definition in the .json
		json_value* device = findBoard( json_config, rsp.parts[4] );
		if( !device ) {
			LOGF_ERROR( "unknown device: %s", rsp.parts[4] );
		}
		else {
			LOGF_DEBUG( "found device: %s", rsp.parts[4] );

			// ok, found:
			//	enumerate categories and build properties

			json_value* categs = get_item( device, "categories" );
			if( categs && categs->type==json_object ) {

				int length = categs->u.object.length;
				for ( int x = 0; x < length; x++) {
					json_object_entry& item = categs->u.object.values[x];
					
					const char* catName = item.name;
					LOGF_DEBUG( "building category: %s", catName );

					json_value* categ = item.value;
					if( categ->type!=json_object )
						continue;

					genCatProps( catName, categ );
				}
			}
		}
	}



	/*
	UsisResponse rsp;

	if ( sendCmd( &rsp, "INFO;PROPERTY_COUNT;" ) ) {
		int nprops = atoi( rsp.parts[4] );
		// LOGF_INFO("driver has %d properties.", nprops );

		for ( int i = 0; i < nprops; i++ ) {
			// if the value is readonly, skip it.
			sendCmd( &rsp, "INFO;PROPERTY_ATTR_MODE;%d;0", i );
			if ( strcmp( rsp.parts[4], "RO" ) == 0 ) {
				continue;
			}

			UsisProperty* p = new UsisProperty;
			p->type = _undefined;
			p->next = _props;
			_props = p;

			sendCmd( &rsp, "INFO;PROPERTY_NAME;%d", i );
			strncpy( p->name, rsp.parts[4], MAX_NAME_LENGTH + 1 );

			sendCmd( &rsp, "INFO;PROPERTY_TYPE;%d", i );
			if ( strcmp( rsp.parts[4], "TEXT" ) == 0 ) {
				p->type = _text;
			}
			else if ( strcmp( rsp.parts[4], "ENUM" ) == 0 ) {
				p->type = _enum;
				p->enm._evals = nullptr;
			}
			else if ( strcmp( rsp.parts[4], "FLOAT" ) == 0 ) {
				p->type = _number;
				p->num.minVal = -9999.0;
				p->num.maxVal = +9999.0;
			}
			else {
				// unknown type
				delete p;
				continue;
			}

			if ( sendCmd( &rsp, "INFO;PROPERTY_ATTR_COUNT;%d", i ) ) {
				int nattr = atoi( rsp.parts[4] );
				// fprintf( stderr, "property %s has %d attrs.\n", p->name, nattr );
				// LOGF_INFO("property %s has %d attrs.", p->name, nattr );

				for ( int j = 0; j < nattr; j++ ) {
					sendCmd( &rsp, "INFO;PROPERTY_ATTR_NAME;%d;%d", i, j );

					if ( p->type == _number ) { // float

						if ( strcmp( rsp.parts[4], "MIN" ) == 0 ) {
							sendCmd( &rsp, "GET;%s;MIN", p->name );
							p->num.minVal = _atof( rsp.parts[4] );
						}
						else if ( strcmp( rsp.parts[4], "MAX" ) == 0 ) {
							sendCmd( &rsp, "GET;%s;MAX", p->name );
							p->num.maxVal = _atof( rsp.parts[4] );
						}
					}
					else if ( p->type == _enum ) {
						sendCmd( &rsp, "INFO;PROPERTY_ATTR_ENUM_COUNT;%d;%d", i, j );
						int nenum = atoi( rsp.parts[4] );

						
						

						for( int k=0; k<nenum; k++ ) {
							sendCmd( &rsp, "INFO;PROPERTY_ATTR_ENUM_VALUE;%d;%d", i, k );

							UsisEnum* e = new UsisEnum;
							strncpy( e->name, rsp.parts[4], sizeof(e->name) );
							e->value = k;
							e->next = p->enm._evals;
							p->enm._evals = e; 
						}
					}
				}
			}

			// get value
			sendCmd( &rsp, "GET;%s;VALUE", p->name );

			if ( p->type == _text ) {
				strncpy( p->text.value, rsp.parts[4], sizeof(p->text.value) );
			}
			else if ( p->type == _enum ) {
				strncpy( p->enm.value, rsp.parts[4], sizeof(p->enm.value) );
			}
			else if ( p->type == _number ) {
				p->num.value = _atof( rsp.parts[4] );
			}

			createProperty( p );
		}
	}
	*/
}

void ShelyakUsis::createProperty( const char* catName, UsisProperty* prop )
{
	if ( prop->type == _text ) {
		IUFillText( &prop->text._val, prop->name, prop->name, prop->text.value );
		IUFillTextVector( &prop->text._vec, &prop->text._val, 1, getDeviceName( ), prop->name, prop->name, catName, IP_RW, 60, IPS_IDLE );
		
		defineProperty( &prop->text._vec );
	}
	else if( prop->type==_enum ) {
		ISwitch switches[32];
		UsisEnum* e = prop->enm._evals;
		int nsw = 0;
		
		while( nsw<32 && e ) {
			IUFillSwitch( &e->_val, e->name, e->name, ISS_ON );
			switches[nsw] = e->_val;
			nsw++;
			e = e->next;
		}

		IUFillSwitchVector( &prop->enm._vec, switches, nsw, getDeviceName( ), "value", prop->name, catName, IP_RW, ISR_1OFMANY, 60, IPS_IDLE );
		defineProperty( &prop->enm._vec );
	}
	else if ( prop->type == _number ) {
		IUFillNumber( &prop->num._val, prop->name, "value", "%.2f", prop->num.minVal, prop->num.maxVal, prop->num.prec, prop->num.value );
		IUFillNumberVector( &prop->num._vec, &prop->num._val, 1, getDeviceName( ), prop->name, prop->title, catName, IP_RW, 5, IPS_IDLE );
		defineProperty( &prop->num._vec );

		if( prop->actions&(ACTION_CALIB|ACTION_STOP) ) {

			ISwitch switches[2];
			IUFillSwitch( &switches[0], "STOP", "STOP", ISS_OFF );
			IUFillSwitch( &switches[1], "CALIB", "CALIB", ISS_OFF );

			IUFillSwitchVector( &prop->num._btn, switches, 2, getDeviceName( ), "actions", prop->name, catName, IP_RW, ISR_ATMOST1, 5, IPS_IDLE );
			defineProperty( &prop->num._btn );
		}
	}
}


void ShelyakUsis::releaseProperty( UsisProperty* p ) {

	if( p->type==_enum ) {
		UsisEnum* e = p->enm._evals;
		while( e ) {
			UsisEnum* n = e->next;
			deleteProperty( e->name );
			delete e;
			e = n;
		}
	}

	deleteProperty( p->name );
}



bool ShelyakUsis::sendCmd( UsisResponse* rsp, const char* text, ... )
{
	va_list lst;
	va_start( lst, text );

	bool rc = _send( text, lst ) && _receive( rsp );

	va_end( lst );
	return rc;
}

bool ShelyakUsis::_send( const char* text, va_list lst )
{
	char buf[MAX_FRAME_LENGTH+1];
	vsnprintf( buf, MAX_FRAME_LENGTH, text, lst );

	fprintf( stderr, "> sending %s\n", buf );
	LOGF_INFO("> sending %s", buf );

	strcat( buf, "\n" );

	int rc, nbytes_written;
	if ( ( rc = tty_write( _serialPort, buf, strlen( buf ), &nbytes_written ) ) != TTY_OK ) // send the bytes to the spectrograph
	{
		char errmsg[MAXRBUF];
		tty_error_msg( rc, errmsg, MAXRBUF );
		LOGF_ERROR( "error: %s.", errmsg );
		return false;
	}

	return true;
}

bool ShelyakUsis::_receive( UsisResponse* rsp )
{
	int rc, nread;
	if ( ( rc = tty_nread_section( _serialPort, rsp->buffer, sizeof( rsp->buffer ), '\n', 100, &nread ) ) != TTY_OK ) // send the bytes to the spectrograph
	{
		char errmsg[MAXRBUF];
		tty_error_msg( rc, errmsg, MAXRBUF );
		LOGF_ERROR( "error: %s.", errmsg );
		return false;
	}

	rsp->buffer[nread] = 0;

	fprintf( stderr, "< received %s\n", rsp->buffer );
	LOGF_INFO("< received %s", rsp->buffer );

	char* p = rsp->buffer;
	char* e = p + nread;

	// trim '\n' & ' '
	while ( e >= p && ( e[-1] == '\n' || e[-1] == ' ' ) ) {
		e--;
		*e = 0;
	}

	rsp->pcount = 0;
	rsp->parts[rsp->pcount++] = p;

	while ( p < e && rsp->pcount < 5 ) {
		if ( *p == ';' ) {
			*p = 0;
			rsp->parts[rsp->pcount++] = p + 1;
		}

		p++;
	}

	if ( strcmp( rsp->parts[0], "M00" ) != 0 ) {
		return false;
	}

	return true;
}

/**
 *
 */

bool ShelyakUsis::updateProperties( )
{
	INDI::DefaultDevice::updateProperties( );
	if ( isConnected( ) ) {
		scanProperties( );
	}
	else {
		UsisProperty* p = _props;
		while ( p ) {
			UsisProperty* n = p->next;
			releaseProperty( p );
			delete p;
			p = n;
		}

		_props = nullptr;
	}

	return true;
}

/**
 * Handle a request to change a switch. 
 **/

bool ShelyakUsis::ISNewSwitch( const char* dev, const char* name, ISState* states, char* names[], int n )
{
	//for( int i=0; i<n; i++ ) {
	//	fprintf( stderr, "set switch: %s = %d\n", names[i], states[i] ) ;
	//}

	if ( n==1 && dev && strcmp( dev, getDeviceName( ) ) == 0 ) {
		for ( UsisProperty* p = _props; p; p = p->next ) {
			if ( p->type == _enum && strcmp( p->name, name ) == 0 ) {

				for( UsisEnum* e = p->enm._evals; e; e=e->next ) {

					if( strcmp( e->name, names[0] )==0 ) {
						UsisResponse rsp;
						sendCmd( &rsp, "SET;%s;VALUE;%s", name, names[0] );
						return true;
					}
				}
			}
		}
	}

	return INDI::DefaultDevice::ISNewSwitch( dev, name, states, names, n ); // send it to the parent classes
}

/**
 * Handle a request to change text.
 **/

bool ShelyakUsis::ISNewText( const char* dev, const char* name, char* texts[], char* names[], int n )
{
	if ( dev && strcmp( dev, getDeviceName( ) ) == 0 ) {
		if ( strcmp( PORT_SETTING.name, name ) == 0 ) {
			IUUpdateText( &PORT_LINE, texts, names, n );
			PORT_LINE.s = IPS_OK;
			IDSetText( &PORT_LINE, nullptr );
			return true;
		}
	}

	return INDI::DefaultDevice::ISNewText( dev, name, texts, names, n );
}

/**
 *
 */

bool ShelyakUsis::ISNewNumber( const char* dev, const char* name, double values[], char* names[], int n )
{
	if ( dev && strcmp( dev, getDeviceName( ) ) == 0 && n == 1 ) {

		for ( UsisProperty* p = _props; p; p = p->next ) {
			if ( p->type == _number && strcmp( p->name, names[0] ) == 0 ) {
				UsisResponse rsp;
				sendCmd( &rsp, "SET;%s;VALUE;%lf", names[0], values[0] );
				return true;
			}
		}
	}

	return INDI::DefaultDevice::ISNewNumber( dev, name, values, names, n );
}


ShelyakUsis* usis = new ShelyakUsis( );


