#include "config.h"
#include "JoystickInput.h"
#include <iostream>


/*
bool JoystickInput::InitJoystick()
{
	if (SDL_Init(SDL_INIT_JOYSTICK ) < 0)
    {
        fprintf(stderr, "Couldn't initialize Joystick: %s\n", SDL_GetError());
        return(false);
    }

	std::cerr << SDL_NumJoysticks() << " joysticks were found." << std::endl;

	//by default we will use the first joystick found, which is index 0.
	SDL_JoystickEventState(SDL_ENABLE);
    joystick = SDL_JoystickOpen(0);
	if (joystick == NULL)
		std::cerr << "Joystick could not be opened." << std::endl;
	else
		std::cerr << "Joystick has " << SDL_JoystickNumAxes(joystick) << " axes." << std::endl;

	return(true);
}
*/

int JoystickInput::GetFrame(TrackDATAFRAME dataframe[],int input)
{
	dataframe[input].x = jx;
	dataframe[input].y = jy;

	/*
	if (x > deadzone)  //positive and larger than deadzone
		x_norm = (x-deadzone)/(32767.0f-deadzone);
	else if (x < -(deadzone+1))  //negative and smaller than deadzone
		x_norm = (x+(deadzone+1))/(32768.0f-(deadzone+1));
	else  //within the deadzone; this is treated as a zero value
		x_norm = 0.0f;
	
	if (y > deadzone)  //positive and larger than deadzone
		y_norm = (y-deadzone)/(32767.0f-deadzone);  
	else if (y < -(deadzone+1))  //negative and smaller than deadzone
		y_norm = (y+(deadzone+1))/(32768.0f-(deadzone+1));
	else  //within the deadzone; this is treated as a zero value
		y_norm = 0.0f;
	
	y_norm = -y_norm;  //reverse the sign of y to get a standard angular position
	//x_norm =  (x > 0 ? (x-deadzone)/(32767-deadzone) : (x+(deadzone+1))/(32768-(deadzone+1)) );
	//y_norm = -(y > 0 ? (y-deadzone)/(32767-deadzone) : (y+(deadzone+1))/(32768-(deadzone+1)) );  //reverse the sign of y to get a standard angular position


	//we transform the rectangular joystick coordinates into circular polar coordinates by first performing a spatial transformation to warp the hand space.
	//this is done according to the proof here: http://mathproofs.blogspot.com/2005/07/mapping-square-to-circle.html
	x_trans = x_norm*sqrt(1-(y_norm*y_norm)/2);
	y_trans = y_norm*sqrt(1-(x_norm*x_norm)/2);
	//note this is not quite ideal because the rectangular "dead zone" extracted from the middle of the square is not transformed into a circle, 
	//  but more of a squarish-thing-with-curved-edges.  but for now, it's good enough.

	//input.z = sqrt(x_norm*x_norm + y_norm*y_norm); //z reports the normalized radial position of the joystick (minus the deadzone) in polar coordinates
	input.z = sqrt(x_trans*x_trans + y_trans*y_trans); //z reports the normalized, transformed radial position of the joystick (minus the square deadzone) in polar coordinates

	//input.pitch = 0.0f;
	//input.roll = 0.0f;
	input.theta = atan2f(y_trans,x_trans); //theta reports the angular position of the joystick (outside the deadzone) in polar coordinates
	*/

	dataframe[input].z = jradius;
	dataframe[input].theta = jangle;

	dataframe[input].time = SDL_GetTicks();

	return(1);
}

void JoystickInput::ProcessEvent(SDL_Event event, SDL_Joystick *joystick)
{
	float x_norm, y_norm, x_trans,y_trans;
	float deadzone = 3200.0f;  //dead zone around the center of the joystick that must be exceeded to register a real "extent".  


	//the joystick is 16 bit signed, so it ranges from from -32767 to 32768.

	if (event.type == SDL_JOYAXISMOTION)
	{
		//update the current joystick status
		SDL_JoystickUpdate();

        /* 
			Read in the axis motion data:
			
			In this function, no manipulation of the input will be performed, we just read in the raw values as is.
			Scaling, dead zones, etc. will be handled in the GetFrame() call.

			Left-right movement: left = -32768, right = 32767
			Up-Down movement: up = -32768, down = 32767 

		*/

		jx = SDL_JoystickGetAxis(joystick, 0);  //axis0 is left/right
		jy = SDL_JoystickGetAxis(joystick, 1);  //axis1 is up/down

		if (jx > deadzone)  //positive and larger than deadzone
			x_norm = (jx-deadzone)/(32767.0f-deadzone);
		else if (jx < -(deadzone+1))  //negative and smaller than deadzone
			x_norm = (jx+(deadzone+1))/(32768.0f-(deadzone+1));
		else  //within the deadzone; this is treated as a zero value
			x_norm = 0.0f;
	
		if (jy > deadzone)  //positive and larger than deadzone
			y_norm = (jy-deadzone)/(32767.0f-deadzone);  
		else if (jy < -(deadzone+1))  //negative and smaller than deadzone
			y_norm = (jy+(deadzone+1))/(32768.0f-(deadzone+1));
		else  //within the deadzone; this is treated as a zero value
			y_norm = 0.0f;
	
		y_norm = -y_norm;  //reverse the sign of y to get a standard angular position
		//x_norm =  (x > 0 ? (x-deadzone)/(32767-deadzone) : (x+(deadzone+1))/(32768-(deadzone+1)) );
		//y_norm = -(y > 0 ? (y-deadzone)/(32767-deadzone) : (y+(deadzone+1))/(32768-(deadzone+1)) );  //reverse the sign of y to get a standard angular position


		//we transform the rectangular joystick coordinates into circular polar coordinates by first performing a spatial transformation to warp the hand space.
		//this is done according to the proof here: http://mathproofs.blogspot.com/2005/07/mapping-square-to-circle.html
		x_trans = x_norm*sqrt(1-(y_norm*y_norm)/2);
		y_trans = y_norm*sqrt(1-(x_norm*x_norm)/2);
		//note this is not quite ideal because the rectangular "dead zone" extracted from the middle of the square is not transformed into a circle, 
		//  but more of a squarish-thing-with-curved-edges.  but for now, it's good enough.

		//save the normalized positions of the two axes
		jx = x_norm;
		jy = y_norm;

		//calculate the normalized radius position
		//input.z = sqrt(x_norm*x_norm + y_norm*y_norm); //z reports the normalized radial position of the joystick (minus the deadzone) in polar coordinates
		jradius = sqrt(x_trans*x_trans + y_trans*y_trans); //z reports the normalized, transformed radial position of the joystick (minus the square deadzone) in polar coordinates

		//calculate the angular position
		jangle = atan2f(y_trans,x_trans); //theta reports the angular position of the joystick (outside the deadzone) in polar coordinates

		//input.pitch = 0.0f;
		//input.roll = 0.0f;


	}// end event is axismotion
	
}


void JoystickInput::CloseJoystick(SDL_Joystick *joystick)
{
	SDL_JoystickClose(joystick);
}