/*
 * cam_qhy8l.h
 *
 *  Created on: 19.01.2012
 *      Author: gm
 */

#ifndef CAM_QHY8L_H_
#define CAM_QHY8L_H_


#include <libindi/indiusbdevice.h>
#include <map>
#include <vector>
#include <string>

//#include "camera.h"


#define QHY8L_VENDOR_ID   0x1618
#define QHY8L_PRODUCT_ID  0x6005

#define QHY8L_MATRIX_WIDTH	 3328
#define QHY8L_MATRIX_HEIGHT  2030




#define QHY8L_WIDTH_B1	QHY8L_MATRIX_WIDTH
#define QHY8L_HEIGHT_B1	QHY8L_MATRIX_HEIGHT

#define QHY8L_WIDTH_B2	1664
#define QHY8L_HEIGHT_B2	1015

#define QHY8L_WIDTH_B4	832
#define QHY8L_HEIGHT_B4	508

#define QHY8L_BINN_CNT 4
#define QHY8L_SPEED_CNT 2


/*
class cam_qhy8l_params : public cam_base_params
{
public:
	cam_qhy8l_params() : cam_base_params( 9000 )
	{
		reset();
	}
	virtual ~cam_qhy8l_params(){}
	virtual void reset()
	{
		cam_base_params::reset();

		gain = 20;
		offset = 120;
		readout_speed = 0;
		amp = 1;
		pwm = 128;
	}
	virtual void load( void )
	{
		QSettings settings( "GM_software", QString("ccd_cam_") + camera_model::to_string( m_model ) );

		this->cam_base_params::load( &settings );

		// camera dependent params
		settings.beginGroup("hw_dependent");
			gain = settings.value( "gain", 20 ).toInt();
			offset = settings.value( "offset", 120 ).toInt();
			readout_speed = settings.value( "readout_speed", 0 ).toInt();
			amp = settings.value( "amp", 1 ).toInt();
			pwm = settings.value( "pwm", 128 ).toInt();
		settings.endGroup();
	}
	virtual void save( void ) const
	{
		QSettings settings( "GM_software", QString("ccd_cam_") + camera_model::to_string( m_model ) );

		this->cam_base_params::save( &settings );

		// camera dependent params
		settings.beginGroup("hw_dependent");
			settings.setValue( "gain", gain );
			settings.setValue( "offset", offset );
			settings.setValue( "readout_speed", readout_speed );
			settings.setValue( "amp", amp );
			settings.setValue( "pwm", pwm );
		settings.endGroup();
	}
	// device-dependent
	int offset;
	int gain;
	int readout_speed;
	int amp;
	int pwm;

private:
	static const camera_model::model m_model = camera_model::qhy8l;
};
*/


typedef struct
{
	int exposure;
	int binn;
	int gain;
	int offset;
	int speed;
	int amp;
	int pwm;
	int out_frame_width;
	int out_frame_height;
	int out_buffer_size;
	int out_top_skip_pix;	// dependent on binning number of pixels to skip from the buffer beginning
}qhy8l_params_t;


class QHY8Driver
{
public:
    QHY8Driver();
    virtual ~QHY8Driver();


	// Operations
	virtual bool get_frame( void );
//	virtual cam_base_params *alloc_params_object( void ) const;			// returns default params. object
//	virtual bool set_params( cam_base_params *params );	// sends params into camera
//	virtual bool get_params( cam_base_params *params );		// returns params from camera
//	virtual bool get_params_copy( cam_base_params *params ) const; // returns last params without cam. access
//	virtual const cam_base_params &get_params_ref( void ) const;	// returns reference to params.
	virtual bool exec_slow_ambiguous_synchronous_request( int req_num,
												const std::map< std::string, std::string > &params,
												std::map< std::string, std::string > *result );
	virtual std::vector<float> get_debayer_settings( void ) const;

	enum ASR_numbers	//ambiguous synchronous request
	{
		temp_voltage_req = 1
	};
protected:
//	virtual bool get_frame_params( frame_params_t *params );

private:
	typedef struct ccdreg
	{
	        char* devname;
	        unsigned char Gain;
	        unsigned char Offset;
	        unsigned long Exptime;
	        unsigned char HBIN;
	        unsigned char VBIN;
	        unsigned short LineSize;
	        unsigned short VerticalSize;
	        unsigned short SKIP_TOP;
	        unsigned short SKIP_BOTTOM;
	        unsigned short LiveVideo_BeginLine;
	        unsigned short AnitInterlace;
	        unsigned char MultiFieldBIN;
	        unsigned char AMPVOLTAGE;
	        unsigned char DownloadSpeed;
	        unsigned char TgateMode;
	        unsigned char ShortExposure;
	        unsigned char VSUB;
	        unsigned char CLAMP;
	        unsigned char TransferBIT;
	        unsigned char TopSkipNull;
	        unsigned short TopSkipPix;
	        unsigned char MechanicalShutterMode;
	        unsigned char DownloadCloseTEC;
	        unsigned char SDRAM_MAXSIZE;
	        unsigned short ClockADJ;
	        unsigned char Trig;
	        unsigned char MotorHeating;   //0,1,2
	        unsigned char WindowHeater;   //0-15
	        unsigned char ADCSEL;
	} ccdreg_t;

	struct info_s
	{
		double temperature;
		int voltage;
	};

	virtual int open_device( void );
	virtual void close_device( void );

//	virtual int do_command( cam_task_t *task );

    //usb_device_handle *locate_device( unsigned int vid, unsigned int pid );
    //int ctrl_msg( int request_type, int request, unsigned int value, unsigned int index, unsigned char *data, int len );

	int set_params( int exposuretime, int binn, int gain, int offset, int speed, int amp, int pwm, int *out_width, int *out_height, int *out_buffer_size, int *out_top_skip_pix );
	int get_info( struct info_s *info_out, bool dump = true );

	unsigned char MSB( unsigned int i );
	unsigned char LSB( unsigned int i );

	int get_dc201( short *dc );
	double r_to_degree( double R ) const;
	double mv_to_degree( double v ) const;
	int set_dc201( int pwm );

//	cam_qhy8l_params m_high_params;	// human-readable params
	qhy8l_params_t   m_low_params;

    struct usb_device_handle *m_handle;

	static const int m_binn_loading_time[QHY8L_SPEED_CNT][QHY8L_BINN_CNT];
	static const int m_binn_loading_size[QHY8L_BINN_CNT];
};

#endif /* CAM_QHY8L_H_ */
