#ifndef HANDCURSOR_H
#define HANDCURSOR_H
#pragma once

#include "Circle.h"
#include "SDL.h"
#include "SDL_opengl.h"
#include "Object2D.h"
#define NHIST 5

// object to keep track of hand position/velocity and display cursor
class HandCursor
{
private:
	GLfloat xpos;  //this is the true hand position, sans the cursor gain/rotation manipulations
	GLfloat ypos;
	GLfloat xhist[NHIST]; // x velocity history
	GLfloat yhist[NHIST]; // y velocity history
	GLfloat xhistScr[NHIST]; // x screen position history 
	GLfloat yhistScr[NHIST]; // y screen position history
	GLfloat xvel;
	GLfloat yvel;

	//GLfloat xScr;  //we will access the underlying Circle's position properties for this information.
	//GLfloat yScr;
	GLfloat x0;  //this is the defined origin for any rotations or gain changes of the cursor
	GLfloat y0;

	GLfloat xgain; 
	GLfloat ygain;

	//GLint draw; // controls whether cursor will be drawn or not -- we will set the underlying Circle properties
	GLfloat rotMat[4]; //rotation matrix
	//GLfloat color[3];  
	Circle* circ;  //this is the underlying Circle for the cursor

	GLfloat hitMargin;

	GLint clamp; // controls whether clamp is on
	GLfloat thetaClamp; // clamp angle

public:
	HandCursor(Circle* c);
	~HandCursor(){};

	void Draw();
	void SetColor(GLfloat clr[]);

	GLfloat GetX();  //returns the hand-cursor position, potentially distorted
	GLfloat GetY();  //returns the hand-cursor position, potentially distorted
	GLfloat GetTrueX();  //returns the actual hand position, non-distorted
	GLfloat GetTrueY();  //returns the actual hand position, non-distorted
	GLfloat GetXVel();   //returns the actual hand velocity, non-distorted
	GLfloat GetYVel();   //returns the actual hand velocity, non-distorted
	GLfloat GetVel();    //returns the actual hand velocity, non-distorted
	GLfloat GetHitMargin();  //returns the multiplication factor for the target radius used to determine a Hit

	void UpdatePos(GLfloat x, GLfloat y);  //input is the actual hand position (non-distorted); saves both the true hand position and updates the potentially distorted cursor position. also recalculates the instantaneous hand velocity 

	void SetHitMargin(GLfloat m = 1.0f);  //set the multiplication factor for the target radius used to determine a Hit (default = 1.0f)
	void SetRotation(GLfloat theta); // set rotation angle in degrees CW
	void SetOrigin(GLfloat x, GLfloat y); // set origin around which rotation occurs
	GLfloat Distance(GLfloat x, GLfloat y); // get distance from the potentially distorted cursor to an (x,y) position
	GLfloat Distance(Circle* c);            // get distance from the potentially distorted cursor to a circle object
	GLfloat Distance(Object2D* obj);		// get distance from the potentially distorted cursor to an Object2D

	void SetGain(GLfloat xg, GLfloat yg);  // set gain
	
	void SetClamp(GLfloat th);
	void SetClampC(Circle* c1, Circle* c2); // sets a clamp through the center of two points
	void ClampOff(); //turn off the cursor clamp
	void Null();  //reset set the rotation matrix such that no rotation is present
	void On();   //turn the cursor on (e.g., allow it to draw)
	void Off();  //turn the cursor off (e.g., prevent it from being drawn)

	bool HitTarget(Circle* targ);  //compute if the potentially distorted cursor hit a circle object
	bool HitTarget(GLfloat x, GLfloat y, GLfloat rad);  //compute if the potentially distorted cursor is within rad distance of a given (x,y) position

	GLint drawState();
};

#endif