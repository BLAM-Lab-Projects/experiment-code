#ifndef MOUSEINPUT_H
#define MOUSEINPUT_H
#pragma once

#include "SDL.h"
#include "InputFrame.h"

static GLfloat x;
static GLfloat y;

/*
struct MouseDATAFRAME
{
	int Ninput;

	double x;
	double y;
	double z;
	//double azimuth;
	//double elevation;
	//double roll;
	double anglematrix[3][3];

	double time;  //this is time according to the tracking system, arbitrary according to the system clock
	double etime;  //this is elapsed time from the start of data recording
	double quality;

};
*/

// Handles mouse actions
class MouseInput
{
public:
	MouseInput() { }
	~MouseInput() { }
	// Gets the most recent frame of data from the mouse
	static int GetFrame(TrackDATAFRAME DataMouseFrame[]);
	// Updates MouseInput with new position information.
	// event is an SDL_Event containing the updated information.
	static void ProcessEvent(SDL_Event event);
};

#endif
