#ifndef INPUTFRAME_H
#define INPUTFRAME_H
#pragma once

#include "SDL_opengl.h"


// Data type used to return input data
struct TrackDATAFRAME
{
	
	//int Ninput;
	int ValidInput;	//[BIRDCOUNT+1];

	double x;		//[BIRDCOUNT+1];
	double y;		//[BIRDCOUNT+1];
	double z;		//[BIRDCOUNT+1];
	//double azimuth[BIRDCOUNT+1];
	//double elevation[BIRDCOUNT+1];
	//double roll[BIRDCOUNT+1];
	double anglematrix[3][3]; //[BIRDCOUNT+1]
	double theta;

	double time;	//[BIRDCOUNT+1];  //this is time according to the tracking system, arbitrary according to the system clock
	double etime;	//[BIRDCOUNT+1];  //this is elapsed time from the start of data recording
	double quality;	//[BIRDCOUNT+1];

	float vel;		//[BIRDCOUNT+1];

};


#endif
