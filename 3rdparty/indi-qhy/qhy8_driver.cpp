/*
 * cam_qhy8l.cpp
 *
 *  Created on: 19.01.2012
 *      Author: gm
 */


#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <math.h>
#include <netinet/in.h>
#include <errno.h>
#include <utility>

#include "qhy8_driver.h"



// disables hardware access for QHY8L
//#define NO_QHY8L

#define SENDREGS  0xB5

const int QHY8Driver::m_binn_loading_time[QHY8L_SPEED_CNT][QHY8L_BINN_CNT] = {
											{9000, 4500, 2250, 2250 },
											{4500, 1125, 1200, 1200 },
											};
const int QHY8Driver::m_binn_loading_size[QHY8L_BINN_CNT] = { 13513728, 3379712, 1690624, 1690624 };

QHY8Driver::QHY8Driver()

{
    int ret = 0;//lusb::initialize();

	if( ret != 0 )
        IDLog( "QHY8Driver::QHY8Driver(): Could not initialize usb" );

	// load params
    //m_high_params.load();
}


QHY8Driver::~QHY8Driver()
{
    //stop();

    //lusb::release();

	// save params
    //m_high_params.save();
}



bool QHY8Driver::Connect()
{
    int ret=0;

     //  locate_device(QHY8L_VENDOR_ID, QHY8L_PRODUCT_ID)) == NULL )

	do
	{
		// set defaults
		qhy8l_params_t p;
        p.exposure	= (int)Exposition;
        p.binn		= Binning;
        p.gain		= gain;
        p.offset	= offset;
        p.speed		= readout_speed;
        p.amp		= 1;;
        p.pwm		= pwm;
		p.out_frame_width = 0;
		p.out_frame_height = 0;
		p.out_buffer_size = 0;

        ret = set_params( p.exposure,
				p.binn,
				p.gain,
				p.offset,
				p.speed,
				p.amp,
				p.pwm,
				&p.out_frame_width,
				&p.out_frame_height,
				NULL,
				&p.out_top_skip_pix );
		if( ret != EXIT_SUCCESS )
		{
            IDLog( "QHY8Driver::open_device(): set_params() failed" );
			break;
		}

		m_low_params  = p;
        //m_initialized = true;
        IDLog( "Connected to QHY8L camera" );
        return true;
	}while( 0 );

	close_device();	// error happened

    return false;
}


void QHY8Driver::Disconnect()
{
    /*if( m_handle )
	{
        usb_close( m_handle );
		m_handle = NULL;
	}

	m_initialized = false;
    IDLog( "Disconnected" );*/
}



/*bool QHY8Driver::set_params( cam_base_params *params )
{
 cam_qhy8l_params *h_params = NULL;
 qhy8l_params_t p;
 bool ret = false;

	h_params = dynamic_cast<cam_qhy8l_params *>(params);

	if( !m_initialized || !h_params )
		return false;

	p.exposure	= (int)h_params->Exposition;
	p.binn		= h_params->Binning;
	p.gain		= h_params->gain;
	p.offset	= h_params->offset;
	p.speed		= h_params->readout_speed;
	p.amp		= h_params->amp;
	p.pwm		= h_params->pwm;
	p.out_frame_width = 0;
	p.out_frame_height = 0;
	p.out_buffer_size = 0;

	ret = set_thread_task( MK_CMD("@SETPAR"), (char*)&p, sizeof(qhy8l_params_t), NULL, 0, true );
	if( !ret )
    {
        IDLog("QHY8Driver::set_params: set_thread_task error");
		return false;
	}

	m_high_params = *h_params;
	m_low_params  = p;

 return true;
}*/

/*
bool QHY8Driver::get_params( cam_base_params *params )
{
 cam_qhy8l_params *dst = dynamic_cast<cam_qhy8l_params *>(params);
 bool ret = false;

	if( !m_initialized || !dst )
		return false;

	dst->reset();

	// goto thread
	ret = set_thread_task( MK_CMD("@GETPAR"), NULL, 0, (char*)&m_low_params, sizeof(qhy8l_params_t), true );
	if( !ret )
	{
        IDLog("ccamera_qhy6::get_params: set_thread_task error");
		return false;
	}

	m_high_params.Exposition 	= (double)m_low_params.exposure;
	m_high_params.Binning    	= m_low_params.binn;
	m_high_params.gain		 	= m_low_params.gain;
	m_high_params.offset	 	= m_low_params.offset;
	m_high_params.readout_speed = m_low_params.speed;
	m_high_params.amp		 	= m_low_params.amp;

	*dst = m_high_params;

 return true;
}*/

/*
bool QHY8Driver::get_params_copy( cam_base_params *params ) const
{
 cam_qhy8l_params *dst = dynamic_cast<cam_qhy8l_params *>(params);

	if( !m_initialized || !dst )
		return false;

	*dst = m_high_params;

 return true;
}
*/


/*
const cam_base_params &QHY8Driver::get_params_ref( void ) const
{
 return m_high_params;
}
*/


bool QHY8Driver::exec_slow_ambiguous_synchronous_request( int req_num,
														const std::map< std::string, std::string > &params,
														std::map< std::string, std::string > *result )
{
	bool ret = false;

//	if( !m_initialized )
//		return false;

	switch( req_num )
	{
    case QHY8Driver::temp_voltage_req:
	{
		(void)params;
		struct info_s info_out = { 0, 0 };
//		ret = set_thread_task( MK_CMD("@GETTEMP"), NULL, 0, (char *)&info_out, sizeof(struct info_s), true );
		if( !ret )
		{
            IDLog("QHY8Driver::exec_slow_ambiguous_synchronous_request(): req_num = %d: set_thread_task error", req_num );
			return false;
		}
		if( result )
		{
			char buf[64];
			snprintf( buf, sizeof(buf)-1, "%f", info_out.temperature );
			result->insert( std::make_pair( "temperature", std::string( buf ) ) );

			snprintf( buf, sizeof(buf)-1, "%d", info_out.voltage );
			result->insert( std::make_pair( "voltage", std::string( buf ) ) );
		}
        IDLog( "QHY8Driver::exec_slow_ambiguous_synchronous_request(): req = @GETTEMP: temp = %.1f, volt = %d", info_out.temperature, info_out.voltage );
		break;
	}
	default:
        IDLog( "QHY8Driver::exec_slow_ambiguous_synchronous_request(): Unknown request nubler" );
		return false;

	}

	return true;
}


std::vector<float> QHY8Driver::get_debayer_settings( void ) const
{
	std::vector<float> settings;

    if( /*m_high_params.Binning ==*/ 1 )
	{
		settings.resize( 5 );

		// offset
		settings[0] = 0;
		settings[1] = 1;
		// correction coefficients
		settings[2] = 1.0/0.96;		// r
		settings[3] = 1.0;			// g
		settings[4] = 1.0/0.773;	// b
	}
    IDLog( "debayer settings.size = %d", settings.size() );

	return settings;
}


bool QHY8Driver::get_frame( void )
{
 /*
    frame_params_t frame_params;
 int len = 0;
 bool ret = false;

	if( !m_initialized )
		return false;

	ret = get_frame_params( &frame_params );

	if( !ret )
		return false;

	init_buffer( frame_params );	// addinf extra size to buffer tail

	len = frame_params.width * frame_params.height;

	ret = set_thread_task( MK_CMD("@IMG"), NULL, 0, (char*)m_buffer.data, len*pixel_base::bytes_per_pixel(frame_params.bpp), false );
	if( !ret )
	{
        IDLog("QHY8Driver::get_frame: set_thread_task error");
		return false;
	}

    */
 return true;
}


/*
bool QHY8Driver::get_frame_params( frame_params_t *params )
{
 frame_params_t frame_params;


	 if( params != NULL )
		 memset( params, 0, sizeof(frame_params_t) );

	 frame_params.width 	= m_low_params.out_frame_width;
	 frame_params.height	= m_low_params.out_frame_height;
	 frame_params.bpp   	= pixel_base::BPP_MONO_16;

	*params = frame_params;

 return true;
}
*/

#if 0
int QHY8Driver::do_command( cam_task_t *task )
{
	ctimer progress_timer;
	int nsec = 0;
	qhy8l_params_t *p = NULL;
	unsigned long now;
	int ret = EXIT_SUCCESS;
	bool break_done = false;


	// check that we are not in main thread!
	if( strncmp( task->cmd, "@SETPAR", task->cmd_len ) == 0 )
	{
		if( task->param_len != sizeof(qhy8l_params_t) || !(p = reinterpret_cast<qhy8l_params_t *>(task->param)) )
		{
            IDLog( "QHY8Driver::do_command(): CMD = %.*s has inconsistent params.", task->cmd_len, task->cmd );
			return 1;
		}

		int res = set_params( p->exposure, p->binn, p->gain, p->offset, p->speed, 1/*p->amp*/, p->pwm, &p->out_frame_width, &p->out_frame_height, &p->out_buffer_size, &p->out_top_skip_pix );
		if( res != EXIT_SUCCESS )
		{
            IDLog( "QHY8Driver::do_command(): CMD = %.*s failed.", task->cmd_len, task->cmd );
			return 2;
		}
	}
	else
	if( strncmp( task->cmd, "@GETPAR", task->cmd_len ) == 0 )
	{
		if( task->out_len != sizeof(qhy8l_params_t) || !(p = reinterpret_cast<qhy8l_params_t *>(task->out)) )
		{
            IDLog( "QHY8Driver::do_command(): CMD = %.*s has inconsistent params.", task->cmd_len, task->cmd );
			return 3;
		}

		qhy8l_params_t lp;
		lp = m_low_params;		// emulation of getting parameters (already got by @SETPAR)

		memcpy( task->out, &lp, sizeof(qhy8l_params_t) );

        IDLog( "Config get_params OK" );
	}
	else
	if( strncmp( task->cmd, "@IMG", task->cmd_len ) == 0 )
	{
		uint16_t *raw = NULL;
		if( task->out_len != m_low_params.out_buffer_size || !(raw = reinterpret_cast<uint16_t *>(task->out)) )
		{
            IDLog( "QHY8Driver::do_command(): CMD = %.*s has inconsistent params.", task->cmd_len, task->cmd );
			return 4;
		}

        IDLog( "Start capture..." );

		unsigned char REG[2];
		REG[0] = 0;
		REG[1] = 100;
		int res = ctrl_msg( 0x40, 0xB3, 0, 0, REG, 1/*sizeof(REG)*/ );
		if( res != EXIT_SUCCESS )
		{
            IDLog( "QHY8Driver::do_command(): CMD = %.*s failed. Unable to start exposure", task->cmd_len, task->cmd );
			return 5;
		}

		// wait for exposition
		if( m_notify_wnd )
			QApplication::postEvent( m_notify_wnd, new cam_progress_Event(0) );

		progress_timer.start();

		// this camera can not break capturing! so...  we have to ignore m_frame_break besides retcode
		while( (now = progress_timer.gettime()) < (unsigned long)m_high_params.Exposition )
		{
			if( m_frame_break && now < (unsigned long)m_high_params.Exposition-100 )
			{
				//hardware BREAK
				for( int i = 0;i < ccamera_base::try_reset_cnt;i++ )
				{
					int res = system( "utils/qhy8lreset.sh" );
					if( res < 0 )
					{
                        IDLog( "Failed to reset camera by script... '%s'", strerror(errno) );
						break;
					}
					else
					{

						res = set_params( m_low_params.exposure,
								m_low_params.binn,
								m_low_params.gain,
								m_low_params.offset,
								m_low_params.speed,
								m_low_params.amp,
								m_low_params.pwm,
								NULL,
								NULL,
								NULL,
								NULL );
						if( res == EXIT_SUCCESS )
							break;
						else
                            IDLog( "QHY8Driver::do_command(): set_params() failed after reset. Trying #%d", i+1 );
					}
				}
				break_done = true;
				break;
			}
			if( (unsigned long)m_high_params.Exposition - now > progress_time_limit )
			{
				nsec++;
				// emit time event...
				if( m_notify_wnd && nsec % 4 == 0 )
					QApplication::postEvent( m_notify_wnd, new cam_progress_Event(now) );
				usleep( 50000 );
			}
			else
				usleep( 1000 );
		}

        IDLog( "waiting finished. time = %d", progress_timer.gettime() );

		// time has gone... read frame
		if( !break_done )
		{
			if( m_notify_wnd )
				QApplication::postEvent( m_notify_wnd, new cam_progress_Event(progress_timer.gettime()) );

#ifdef NO_QHY8L
			for( int i = 0;i < m_low_params.out_frame_width*m_low_params.out_frame_height;i++ )
			{
				raw[i] = rand() % 65535;
			}
			for( int i = 0;i < m_low_params.out_frame_height;i++ )
			{
				raw[i*m_low_params.out_frame_width+i] = 65535;
			}
#else
			int raw_buffer_size = m_low_params.out_buffer_size + m_low_params.out_top_skip_pix*4;
			unsigned char *raw_buffer = (unsigned char *)malloc( raw_buffer_size );
			if( !raw_buffer )
			{
                IDLog( "QHY8Driver::do_command(): CMD = %.*s failed. Unable to allocate raw data buffer", task->cmd_len, task->cmd );
				return 6;
			}

			unsigned long start_tm = progress_timer.gettime();
			int result = 0;
			int binn_idx = m_low_params.binn - 1;
			int speed_idx = m_low_params.speed == 1 ? 1 : 0;
            int res = usb_bulk_transfer( m_handle,
											0x82,
											raw_buffer,
											m_binn_loading_size[ binn_idx ],
											&result,
											m_binn_loading_time[ speed_idx ][ binn_idx ] );
            IDLog( "downloading time. time = %d", progress_timer.gettime() - start_tm );
			if( res < 0 || result != m_binn_loading_size[ binn_idx ] )
			{
				if( abs(m_binn_loading_size[ binn_idx ] - result) >  m_low_params.out_frame_width*4 )
				{
					free( raw_buffer );
                    IDLog( "QHY8Driver::do_command(): CMD = %.*s failed. usb_bulk_transfer() failed: res = %d, expected = %d bytes, got = %d bytes",
							task->cmd_len, task->cmd,
							res,
							m_binn_loading_size[ binn_idx ],
							result );
					return 7;
				}
				else
                    IDLog( "QHY8Driver::do_command(): CMD = %.*s Warning!!! res = %d, expected = %d bytes, got = %d bytes",
							task->cmd_len, task->cmd,
							res,
							m_binn_loading_size[ binn_idx ],
							result );
			}
#endif

			// swap
#define SWAP(a,b) { a ^= b; a ^= (b ^= a); }
			unsigned data_size = raw_buffer_size;
			if( htons(0x55aa) != 0x55aa )
				for( unsigned i = 0;i < data_size;i += 2 )
					SWAP( raw_buffer[i], raw_buffer[i+1] );
#undef SWAP

			// decode
			uint16_t *src1, *src2, *tgt;
			int line_sz = m_low_params.out_frame_width * sizeof( uint16_t );
			unsigned int t = 0;
			switch( m_low_params.binn )
			{
			case 1:  //1X1 binning
				t = m_low_params.out_frame_height >> 1;
				src1 = (uint16_t *)raw_buffer + m_low_params.out_top_skip_pix;
				src2 = (uint16_t *)raw_buffer + m_low_params.out_frame_width * t + m_low_params.out_top_skip_pix;
				tgt = raw;

				while( t-- )
				{
					memcpy( tgt, src1, line_sz );
					tgt += m_low_params.out_frame_width;
					memcpy( tgt, src2, line_sz );
					tgt += m_low_params.out_frame_width;


					src1 += m_low_params.out_frame_width;
					src2 += m_low_params.out_frame_width;
				}
				break;
			case 2:  //2X2 binning
				src1 = (uint16_t *)raw_buffer + m_low_params.out_top_skip_pix;
				memcpy( raw, src1, m_low_params.out_buffer_size );
				break;
			}
			free( raw_buffer );

			if( m_notify_wnd )
				QApplication::postEvent( m_notify_wnd, new cam_progress_Event(progress_timer.gettime()) );
		}
		if( m_frame_break )
			ret = ERRCAM_USER_BREAK;
		else
		{
            IDLog( "downloading finished. time = %d", progress_timer.gettime() );
            IDLog( "Capture done." );
		}

		get_info( NULL );

	}
	else
	if( strncmp( task->cmd, "@GETTEMP", task->cmd_len ) == 0 )
	{
		struct info_s *info_out = NULL;
		if( task->out_len != sizeof(struct info_s) || !(info_out = reinterpret_cast<struct info_s *>(task->out)) )
		{
            IDLog( "QHY8Driver::do_command(): CMD = %.*s has inconsistent params.", task->cmd_len, task->cmd );
			return 8;
		}
		ret = get_info( info_out );
	}

 return ret;
}
#endif


/*int QHY8Driver::ctrl_msg( int request_type, int request, unsigned int value, unsigned int index, unsigned char *data, int len )
{
	int result;

#ifdef NO_QHY8L
	if( m_handle == NULL )
		return 0;
#endif

	assert( m_handle != NULL );

    result = usb_control_transfer( m_handle, request_type, request, value, index, data, len, 1000 );

	return result < 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}*/


unsigned char QHY8Driver::MSB(unsigned int i)
{
	unsigned int j;
	j = (i&~0x00ff)>>8;
	return j;
}


unsigned char QHY8Driver::LSB( unsigned int i )
{
	unsigned int j;
	j = i&~0xff00;
	return j;
}


int QHY8Driver::set_params( int exposuretime, int binn, int gain, int offset, int speed, int amp, int pwm, int *out_width, int *out_height, int *out_buffer_size, int *out_top_skip_pix )
{
	unsigned char REG[64];
	//unsigned long T = 0;  //total actual transfer data  (byte)
	int PatchNumber, time, Vbin, Hbin;
	int ret = EXIT_SUCCESS;
	unsigned char time_H,time_M,time_L;
	int width = 0;
	int height = 0;
	int LineSize = 0;
	int VerticalSize;
	int TopSkipPix = 0;

    IDLog( "Setting parameters..." );

	memset( REG , 0 , sizeof(REG) );
	time = exposuretime;

	ccdreg_t reg;
	memset( &reg, 0, sizeof(ccdreg_t) );

	switch( binn )
	{
	case 1:
		width = QHY8L_WIDTH_B1; height = QHY8L_HEIGHT_B1; Hbin = binn; Vbin = binn;
		LineSize = QHY8L_WIDTH_B1;	// 3328
		VerticalSize = QHY8L_HEIGHT_B1; // 2030
		TopSkipPix = 1200;
		//P_Size = 26624;
		break;
	case 2:
		width = QHY8L_WIDTH_B2; height = QHY8L_HEIGHT_B2; Hbin = binn; Vbin = binn;
		LineSize = QHY8L_WIDTH_B2;	// 1664
		VerticalSize = QHY8L_HEIGHT_B2; // 1015
		TopSkipPix = 1120;
		//P_Size = 26624;
		break;
	case 3:
	case 4:
		width = QHY8L_WIDTH_B4; height = QHY8L_HEIGHT_B4; Hbin = 2; Vbin = 4;
		LineSize = 1664;	// 1664
		VerticalSize = 508; // 508
		TopSkipPix = 0;
		//P_Size=1651*1024;
		break;

	default:
        IDLog( "Setting parameters... Unsupported binning value = %d", binn );
		return 2;
	}

	// Subframe not implemented as of yet
	int totalsize = width * 2 * height;

	if( out_width )
		*out_width = width;
	if( out_height )
		*out_height = height;
	if( out_buffer_size )
		*out_buffer_size = totalsize;
	if( out_top_skip_pix )
		*out_top_skip_pix = TopSkipPix;

	PatchNumber = (int)fmod( totalsize, 1024 );
    //IDLog( "PatchNumber: %d", PatchNumber );

	time_L = fmod( time, 256 );
	time_M = (time - time_L) / 256;
	time_H = (time - time_L - time_M * 256) / 65536;


	reg.Gain = gain;
	reg.Offset = offset;
	reg.SKIP_TOP = 0;
	reg.SKIP_BOTTOM = 0;
	reg.AMPVOLTAGE = amp;
	reg.Exptime = exposuretime;       //unit: ms
	reg.DownloadSpeed = speed;		// 0 - slow, 1 - fast
	reg.LiveVideo_BeginLine = 0;
	reg.AnitInterlace = 1;
	reg.MultiFieldBIN = 0;
	reg.TgateMode = 0;
	reg.ShortExposure = 0;
	reg.VSUB = 0;
	reg.TransferBIT = 0;
	reg.TopSkipNull = 100;
	reg.TopSkipPix = TopSkipPix;
	reg.MechanicalShutterMode = 0;
	reg.DownloadCloseTEC = 0;
	reg.SDRAM_MAXSIZE = 100;
	reg.ClockADJ = 0x0000;
	reg.HBIN = Hbin;	// binning 1 of 2 or 4
	reg.VBIN = Vbin;	// binning 1 of 2 or 4
	reg.LineSize = LineSize;	   // 3328
	reg.VerticalSize = VerticalSize; // 2030

	// fill registers
	REG[0] = reg.Gain;

	REG[1] = reg.Offset;                                          //Offset : range 0-255 default is 120

	REG[2] = time_H;                                          //unit is ms       24bit
	REG[3] = time_M;
	REG[4] = time_L;

	REG[5] = reg.HBIN;                                             // Horizonal BINNING    0 = 1= No bin
	REG[6] = reg.VBIN;                                             // Vertical Binning        0 =  1= No bin

	REG[7] = MSB( reg.LineSize );                                      // The readout X  Unit is pixel 16Bit
	REG[8] = LSB( reg.LineSize );

	REG[9] = MSB( reg.VerticalSize );                                    // The readout Y  unit is line 16Bit
	REG[10] = LSB( reg.VerticalSize );

	REG[11] = reg.SKIP_TOP;                                              // use for subframe    Skip lines on top 16Bit
	REG[12] = reg.SKIP_TOP;

	REG[13] = reg.SKIP_BOTTOM;                                              // use for subframe    Skip lines on Buttom 16Bit
	REG[14] = reg.SKIP_BOTTOM;                                              // VerticalSize + SKIP_TOP +  SKIP_BOTTOM  should be the actual CCD Y size

	REG[15] = MSB(reg.LiveVideo_BeginLine );                                              // LiveVideo no use for QHY8-9-11   16Bit set to 0
	REG[16] = LSB(reg.LiveVideo_BeginLine );

	REG[17] = MSB( PatchNumber );                               // PatchNumber 16Bit
	REG[18] = LSB( PatchNumber );

	REG[19] = MSB(reg.AnitInterlace );                     // AnitInterlace no use for QHY8-9-11  16Bit set to 0
	REG[20] = LSB(reg.AnitInterlace );

	REG[22] = reg.MultiFieldBIN ;                         // MultiFieldBIN no use for QHY9  set to 0

	REG[29] = MSB(reg.ClockADJ );                          // ClockADJ no use for QHY9-11  16Bit set to 0
	REG[30] = LSB(reg.ClockADJ );

	REG[32] = reg.AMPVOLTAGE;                              // 1: anti-amp light mode

	REG[33] = reg.DownloadSpeed;                         // 0: low speed     1: high speed

	REG[35] = reg.TgateMode;                           // TgateMode if set to 1 , the camera will exposure forever, till the ForceStop command coming
	REG[36] = reg.ShortExposure;                       // ShortExposure
	REG[37] = reg.VSUB;                                // no use for QHY8-9-11   set to 0
	REG[38] = reg.CLAMP;                                // Unknown reg.CLAMP

	REG[42] = reg.TransferBIT;

	REG[46] = reg.TopSkipNull;                                             // TopSkipNull unit is line.

	REG[47] = MSB(reg.TopSkipPix );                     // TopSkipPix no use for QHY9-11 16Bit set to 0
	REG[48] = LSB(reg.TopSkipPix );

	REG[51] = reg.MechanicalShutterMode;                 // QHY9 0: programme control mechanical shutter automaticly   1: programme will not control shutter.
	REG[52] = reg.DownloadCloseTEC ;                     // DownloadCloseTEC no use for QHY9   set to 0

	REG[53] = (reg.WindowHeater&~0xf0)*16+(reg.MotorHeating&~0xf0);
	REG[57] = reg.ADCSEL;

	REG[58] = reg.SDRAM_MAXSIZE ;                      // SDRAM_MAXSIZE no use for QHY8-9-11   set to 0
	REG[63] = reg.Trig;                                // Unknown reg.Trig

#ifdef NO_QHY8L
	if( m_handle == NULL )
		return EXIT_SUCCESS;
#endif

	//0x40 - QHYCCD_REQUEST_WRITE
	//0xC0 - QHYCCD_REQUEST_READ
	// set main params
//	ret = ctrl_msg( 0x40, SENDREGS, 0, 0, REG, sizeof(REG) );
	if( ret != EXIT_SUCCESS )
	{
        IDLog( "QHY8Driver::set_params(): ctrl_msg() failed" );
		return ret;
	}

	// set TEC
	ret = set_dc201( pwm );
	if( ret != EXIT_SUCCESS )
	{
        IDLog( "QHY8Driver::set_params(): set_dc201() failed" );
		return ret;
	}

	// get and dump info
	ret = get_info( NULL );
	if( ret != EXIT_SUCCESS )
	{
        IDLog( "QHY8Driver::set_params(): get_info() failed" );
		return ret;
	}

    IDLog( "Done." );

	return ret;
}


int QHY8Driver::get_info( struct info_s *info_out, bool dump )
{
	short dc = 0;
	int ret = EXIT_SUCCESS;

	ret = get_dc201( &dc );
	if( ret != EXIT_SUCCESS )
	{
        IDLog( "QHY8Driver::get_info(): get_dc201() failed" );
		return ret;
	}
	double temp = mv_to_degree( 1.024 * (float)dc );
	if( info_out )
	{
		info_out->temperature = temp;
		info_out->voltage 	  = (int)dc;
	}
	if( dump )
        IDLog( "temperature = %.1f deg. voltage = %d mV", temp, dc );

	return ret;
}


int QHY8Driver::get_dc201( short *dc )
{
	unsigned char REG[4] = {0x0, 0x0, 0x0, 0x0};

    int ret = 0; /*ctrl_msg( 0x81, 0xC5, 0, 0, REG, sizeof(REG) );*/
	if( ret != EXIT_SUCCESS )
		return ret;

	*dc = short(REG[1]) * 256 + short(REG[2]);

	return ret;
}


double QHY8Driver::r_to_degree( double R ) const
{
	double  T;
	double LNR;

	if( R > 400 )
		R = 400;
	if( R < 1 )
		R = 1;

	LNR = log( R );

	T = 1 / ( 0.002679+0.000291*LNR + LNR*LNR*LNR*4.28e-7  );

	T = T - 273.15;

	return T;
}


double QHY8Driver::mv_to_degree( double v ) const
{
	double R;
	double T;

	R = 33/(v/1000 + 1.625)-10;
	T = r_to_degree( R );

	return T;
}


int QHY8Driver::set_dc201( int pwm )
{
	unsigned char REG[3] = {0, 0, 0};

	pwm = pwm < 0 ? 0 : pwm;
	pwm = pwm > 255 ? 255 : pwm;

	if( pwm == 0 )	// TEC off
	{
		REG[0] = 0x01;
		REG[1] = 0x0;
		REG[2] = 0x01;
	}
	else			// TEC manual
	{
		REG[0] = 0x1;
		REG[1] = (unsigned char)pwm;
		REG[2] = 0x85;
	}

	int result = 0;
	int res = 0;

    /*
    res = usb_bulk_transfer( m_handle, 0x01, REG, sizeof(REG), &result, 1000 );
	usleep( 100000 );
    res = usb_bulk_transfer( m_handle, 0x01, REG, sizeof(REG), &result, 1000 );
	usleep( 100000 );
    IDLog( "set_dc201():" );
    */

	return res;
}
