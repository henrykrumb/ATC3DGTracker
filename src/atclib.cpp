// PointATC3DG.cpp
//
// Linux USB driver for Ascension 3dg
//

#include "atclib.h"

#include <usb.h>

#include <cstdio>
#include <cstdarg>
#include <algorithm>

const int DELAY = 500;

// Commands
#define POINT                       0x42
#define RUN                         0x46
#define SLEEP                       0x47
#define EXAMINE_VALUE               0x4F
#define CHANGE_VALUE                0x50
#define POS_ANG                     0x59
#define POS_MAT                     0x5A
#define POS_QUAT                    0x5D
#define RESET                       0x62
#define METAL                       0x73

// Examine options
#define BIRD_STATUS                 0x00 // 2 bytes
#define BIRD_POSITION_SCALING       0x03 // 2 bytes
#define MEASUREMENT_RATE            0x07 // 2 bytes
#define BIRD_ERROR_CODE             0x0A // 1 byte
#define SYSTEM_MODEL_IDENT          0x0F // 10 bytes
#define BIRD_SERIAL_NUMBER          0x19 // 2 bytes
#define SENSOR_SERIAL_NUMBER        0x1A // 2 bytes
#define TRANSMITTER_SERIAL_NUMBER   0x1B // 2 bytes
#define SUDDEN_OUTPUT_CHANGE_LOCK   0x0E // 1 byte
#define FBB_AUTO_CONFIGURATION      0x32

// Conversions
const double WTF = 1.0/32768; // word to float
const double ANGK = 180 * WTF;
const double INCH_TO_METER  = 25.4 / 1000.0;
const double POSK36 = 36 * WTF * INCH_TO_METER;
const double POSK72 = 72 * WTF * INCH_TO_METER;


void PointATC3DG::write(unsigned int bytes) {
    ret = usb_bulk_write( handle, BIRD_EP_OUT, dataout, bytes, DELAY);
}


void PointATC3DG::read(unsigned int bytes) {
    do {
        ret = usb_bulk_read( handle, BIRD_EP_IN, datain, bytes, DELAY);      
    } while (ret == 0);
}


PointATC3DG::PointATC3DG(unsigned int productId, unsigned int vendorId)
	: vendorId(vendorId),
	productId(productId),
	isOk(true)
{
    usb_init();

    dev = find_device( vendorId, productId);
    if(!dev) {
        error( 1, "finding device on USB bus. Turn it on?");
        return;
    }

    handle = usb_open( dev);
    if (!handle) {
        error(2, "claiming USB device -> %s", usb_strerror());
        return;
    }

    ret = usb_set_configuration( handle, 1);
    if (ret < 0) {
        error(ret, "setting configuration on USB device."
                    " Check device permissions? -> %s", usb_strerror());
        return;
    }
    ret = usb_claim_interface( handle, 0);
    if (ret < 0) {
        error(ret, "claiming USB interface on device -> %s", usb_strerror());
        return;
    }
    ret = usb_set_altinterface( handle, 0);
    if (ret < 0) {
        error(ret, "setting alternate interface -> %s", usb_strerror());
        return;
    }
    ret = usb_clear_halt( handle, BIRD_EP_IN);
    if (ret < 0) {
        error(ret, "clearing halt on EP_IN -> %s", usb_strerror());
        return;
    }
    ret = usb_bulk_read(handle, BIRD_EP_IN, datain, 32, DELAY);

//    check_bird_errors();

    dataout[0] = CHANGE_VALUE;
    dataout[1] = FBB_AUTO_CONFIGURATION;
    dataout[2] = 0x01;
    write(3);
    if (ret < 0) {
        error(ret, "sending FBB_AUTO_CONFIGURATION -> %s", usb_strerror());
        return;
    }
    usleep( 600000); // delay 600 ms after auto-configuration

    dataout[0] = EXAMINE_VALUE;
    dataout[1] = BIRD_POSITION_SCALING;
    write(2);
    read(2);
    if (ret < 0) {
        error(ret, "querying scaling factor -> %s", usb_strerror());
        return;
    }
    posk = POSK36;
    if (datain[0] == 1) {
        posk = POSK72;
	}

    check_bird_errors();
}


PointATC3DG::~PointATC3DG()
{
    if (dev && handle) {
        dataout[0] = SLEEP;
        write(1);
        usb_close(handle);
    }
}


bool PointATC3DG::operator!() const
{
    return !isOk;
}


bool PointATC3DG::ok() const
{
    return isOk;
}


int PointATC3DG::setSuddenOutputChangeLock( int iSensorId )
{
    dataout[0] = 0xf1 + iSensorId;
    dataout[1] = CHANGE_VALUE;
    dataout[2] = SUDDEN_OUTPUT_CHANGE_LOCK;
    dataout[3] = 0x01;
    write(4);

    return check_bird_errors();
}


int PointATC3DG::setSensorQuaternion( int iSensorId )
{
    dataout[0] = 0xf1 + iSensorId;
    dataout[1] = POS_QUAT;
    write(2);

    return check_bird_errors();
}


int PointATC3DG::setSensorRotMat(int iSensorId)
{
    dataout[0] = 0xf1 + iSensorId;
    dataout[1] = POS_MAT;
    write(2);
    
    return check_bird_errors();
}


int PointATC3DG::setSensorTopHemisphere(int iSensorId)
{
    return check_bird_errors();
}


int PointATC3DG::setSensorHemisphere(int iSensorId, char cSphereId)
{
    return check_bird_errors();
}


int PointATC3DG::setMeasurementRate(double dRate)
{
    short sRate = (short) (dRate * 256);
    dataout[0] = CHANGE_VALUE;
    dataout[1] = MEASUREMENT_RATE;
    dataout[2] = (char) (sRate & 0xff);
    dataout[3] = (char) (sRate >> 8);
    write(4);

    return check_bird_errors();
}


int PointATC3DG::getNumberOfSensors(void)
{
    int nSensors = 0;
    for (char i = 0x00 ; i < 0x04 ; ++i) {
        dataout[0] = 0xf1 + i;
        dataout[1] = EXAMINE_VALUE;
        dataout[2] = SENSOR_SERIAL_NUMBER;
        write(3);
        read(2);
        if (ret < 0) {
            error( ret, "getting serial number for sensor %d -> %s",
                        i, usb_strerror());
        }
        else if (datain[0] || datain[1]) {
			++nSensors;
		}
    }
    return nSensors;
}

int PointATC3DG::getCoordinatesAngles( int iSensorId,
                     double& dX, double& dY, double& dZ,
                     double& dAzimuth, double& dElevation, double& dRoll )
{
    short nX, nY, nZ, nAzimuth, nElevation, nRoll;

    dataout[0] = 0xf1 + iSensorId;
    dataout[1] = POINT;
    write(2);

    read(12);

    if (ret == 12) {
        nX = ( (datain[1] << 7) | (datain[0] & 0x7f) ) << 2;
        nY = ( (datain[3] << 7) | (datain[2] & 0x7f) ) << 2;
        nZ = ( (datain[5] << 7) | (datain[4] & 0x7f) ) << 2;

        nAzimuth = ( (datain[7] << 7) | (datain[6] & 0x7f) ) << 2;
        nElevation = ( (datain[9] << 7) | (datain[8] & 0x7f) ) << 2;
        nRoll = ( (datain[11] << 7) | (datain[10] & 0x7f) ) << 2;

        dX = nX * posk;
        dY = nY * posk;
        dZ = nZ * posk;

        dAzimuth = nAzimuth * ANGK;
        dElevation = nElevation * ANGK;
        dRoll = nRoll * ANGK;

        return check_bird_errors();
    }
    error(ret, "reading point data -> %s", usb_strerror());
    return ret;
}

int PointATC3DG::getCoordinatesMatrix(int iSensorId,
                     double& dX, double& dY, double& dZ,
                     double* pMat )
{
    short sMat[9];
    int nX, nY, nZ;

    dataout[0] = 0xf1 + iSensorId;
    dataout[1] = POINT;
    write(2);

    read(24);

    if (ret == 24 ) {
        nX = ((datain[1] << 7) | (datain[0] & 0x7f)) << 2;
        nY = ((datain[3] << 7) | (datain[2] & 0x7f)) << 2;
        nZ = ((datain[5] << 7) | (datain[4] & 0x7f)) << 2;
    
        short *dataptr = sMat;
        for (int i = 7; i < 24 ; i += 2, ++dataptr) {
            *dataptr = ( (datain[i] << 7) | (datain[i-1] & 0x7f) ) << 2;
        }
    
        dX = nX * posk;
        dY = nY * posk;
        dZ = nZ * posk;
    
        for (int i = 0 ; i < 9 ; ++i) {
            pMat[i] = sMat[i] * WTF;
		}
    
        return check_bird_errors();
    }
    error(ret, "reading point data -> %s", usb_strerror());
    return ret;
}

int PointATC3DG::getCoordinatesQuaternion( int iSensorId,
                                           double& dX, double& dY, double& dZ,
                                           double* quat )
{
    short q[4];
    short nX, nY, nZ;
    
    dataout[0] = 0xf1 + iSensorId;
    dataout[1] = POINT;
    write(2);
    
    read(14);
    
    if (ret == 14) {
        nX = ( (datain[1] << 7) | (datain[0] & 0x7f) ) << 2;
        nY = ( (datain[3] << 7) | (datain[2] & 0x7f) ) << 2;
        nZ = ( (datain[5] << 7) | (datain[4] & 0x7f) ) << 2;
        
        short *dataptr = q;
        for (int i = 7 ; i < 14 ; i += 2, ++dataptr ) {
            *dataptr = ( (datain[i] << 7) | (datain[i-1] & 0x7f) ) << 2;
        }
        
        dX = nX * posk;
        dY = nY * posk;
        dZ = nZ * posk;
        
        for (int i = 0 ; i < 4 ; ++i) {
            quat[i] = q[i] * WTF;
		}
        
        return check_bird_errors();
    }
    error(ret, "reading point data -> %s", usb_strerror());
    return ret;
}

bool PointATC3DG::transmitterAttached()
{
    dataout[0] = EXAMINE_VALUE;
    dataout[1] = TRANSMITTER_SERIAL_NUMBER;
    write(2);
    read(2);
    if (ret < 0) {
        error(ret, "getting serial number for transmitter -> %s", usb_strerror());
    }
    else if (datain[0] || datain[1]) {
		return true;
	}
    return false;
}

bool PointATC3DG::sensorAttached(const int& iSensorId)
{
    dataout[0] = 0xf1 + iSensorId;
    dataout[1] = EXAMINE_VALUE;
    dataout[2] = SENSOR_SERIAL_NUMBER;
    write(3);
    read(2);
    if (ret < 0) {
        error(ret, "getting serial number for sensor %d -> %s",
                    iSensorId, usb_strerror());
    }
    else if (datain[0] || datain[1]) {
		return true;
	}
    return false;
}

struct usb_device* PointATC3DG::find_device( unsigned short iVendorId, unsigned short iProductId )
{
    struct usb_bus *bus;
    struct usb_device *dev;

    usb_find_busses();
    usb_find_devices();

    for (bus = usb_busses; bus; bus = bus->next) {
        for (dev = bus->devices ; dev ; dev = dev->next ) {
            if (dev->descriptor.idVendor == iVendorId &&
                dev->descriptor.idProduct == iProductId ) {
                return dev;
            }
        }
    }
    return NULL;
}

int PointATC3DG::check_bird_errors( void )
{
    bool fatal = false;
    dataout[0] = EXAMINE_VALUE;
    dataout[1] = BIRD_ERROR_CODE;
    write(2);
    read(1);

    if (datain[0] == 0) {
		return 0;
	}

    switch (datain[0]) {
        case 1:
			fprintf( stderr, "FATAL(1): System Ram Failure");
			fatal = true;
			break;
        case 2:
			fprintf( stderr, "FATAL(2): Non-Volatile Storage Write Failure");
			fatal = true;
			break;
        case 3:
			fprintf( stderr, "WARNING(3): PCB Configuration Data Corrupt");
			break;
        case 4:
			fprintf( stderr, "WARNING(4): Bird Transmitter Calibration Data Corrupt or Not Connected");
			break;
        case 5:
			fprintf( stderr, "WARNING(5): Bird Sensor Calibration Data Corrupt or Not Connected");
			break;
        case 6:
			fprintf( stderr, "WARNING(6): Invalid RS232 Command");
			break;
        case 7:
			fprintf( stderr, "WARNING(7): Not an FBB Master");
			break;
		case 8:
			fprintf( stderr, "WARNING(8): No Birds Accessible in Device List");
			break;
        case 9:
			fprintf( stderr, "WARNING(9): Bird is Not Initialized");
			break;
        case 10:
			fprintf( stderr, "WARNING(10): FBB Serial Port Receive Error - Intra Bird Bus");
			break;
        case 11:
			fprintf( stderr, "WARNING(11): RS232 Serial Port Receive Error");
			break;
        case 12:
			fprintf( stderr, "WARNING(12): FBB Serial Port Receive Error");
			break;
        case 13:
			fprintf( stderr, "WARNING(13): No FBB Command Response");
			break;
        case 14:
			fprintf( stderr, "WARNING(14): Invalid FBB Host Command");
			break;
        case 15:
			fprintf( stderr, "FATAL(15): FBB Run Time Error");
			fatal = true;
			break;
        case 16:
			fprintf( stderr, "FATAL(16): Invalid CPU Speed");
			fatal = true;
			break;
        case 17:
			fprintf( stderr, "WARNING(17): No FBB Data");
			break;
        case 18:
			fprintf( stderr, "WARNING(18): Illegal Baud Rate");
			break;
        case 19:
			fprintf( stderr, "WARNING(19): Slave Acknowledge Error");
			break;
        case 20: case 21: case 22: case 23:
        case 24: case 25: case 26: case 27:
			std::cerr << "FATAL(" << datain[0] << "): Intel 80186 CPU Errors";
			fatal = true;
			break;
        case 28:    fprintf( stderr, "WARNING(28): CRT Synchronization"); break;
        case 29:    fprintf( stderr, "WARNING(29): Transmitter Not Accessible"); break;
        case 30:    fprintf( stderr, "WARNING(30): Extended Range Transmitter Not Attached"); break;
        case 32:    fprintf( stderr, "WARNING(32): Sensor Saturated"); break;
        case 33:    fprintf( stderr, "WARNING(33): Slave Configuration"); break;
        case 34:    fprintf( stderr, "WARNING(34): Watch Dog Timer"); break;
        case 35:    fprintf( stderr, "WARNING(35): Over Temperature"); break;
		default:    std::cerr << "WARNING(" << datain[0] << ": Unknown Error Code"; break;
    }
	std::cerr << std::endl;

    if (fatal) {
        isOk = false;
        exit( datain[0]);
    }

    return datain[0];
}

void PointATC3DG::error( int val, const char* msg, ... )
{
    va_list ap;
    va_start( ap, msg);
    fprintf( stderr, "error(%d): ", val);
    vfprintf( stderr, msg, ap);
    fprintf( stderr, "\n");
    va_end( ap);
    isOk = false;
}

