#include <iostream>
#include <algorithm>

#include "utils.h"
#include "atclib.h"


int main( int argc, char** argv )
{
    BirdProduct product = DRIVEBAY2;
    
    if (argc > 1)
    {
        std::string productString(argv[1]);
        std::transform(productString.begin(), productString.end(), productString.begin(), ::tolower);
        
        if (productString == "drivebay2")
            product = DRIVEBAY2;
        else if (productString == "medsafe")
            product = MEDSAFE;
        else if (productString == "trakstar")
            product = TRAKSTAR;
        else
            throw std::runtime_error("Unrecognized device identifier! (must be one of [drivebay2,medsafe,trakstar])");
        
    }
    
	PointATC3DG bird(product);

	if( !bird ) return -1;

	bird.setSuddenOutputChangeLock( 0 );

	double dX, dY, dZ, dAzimuth, dElevation, dRoll;
	
	int numsen=bird.getNumberOfSensors();
	std::cout << "nSensors: " << numsen << std::endl;

	int rec_count=0;	 
	time_t ta=time(NULL);
	while (!kbhit()) 
	{	
		rec_count++;
	//	usleep( 100000 );
		for( int i = 0; i <numsen  ; ++i ) 
		{
			bird.getCoordinatesAngles( i, dX, dY, dZ, dAzimuth, dElevation, dRoll );
			std::cout << "\rX: " << dX << ", \tY: " << dY << ", \tZ: " << dZ;
        		std::cout << ", \tA: " << dAzimuth << ", \tE: " << dElevation << ", \tR: " << dRoll << std::endl;
		}
	}
	time_t tb=time(NULL);
	std::cout << rec_count << " samples collected" << std::endl;
	std::cout << tb-ta << " seconds elapsed" << std::endl;
	std::cout << rec_count/(tb-ta) << " samples per second" << std::endl;
	return 0;
}


