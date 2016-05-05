#ifndef REGION2D_H
#define REGION2D_H
#pragma once

#include "SDL.h"
#include "SDL_opengl.h"
#include "Object2D.h"
#include "HandCursor.h"

// Stores an Region as a series of vertices, and enables rendering capabilties
class Region2D
{
private:
	GLint nSides;
	GLfloat Vertices[4][2];  //vertices in meters from the origin (bottom left corner of the screen)
	GLfloat color[3];  //rgb array, with range 0-1
public:

	~Region2D() { }
	
	// Gets a requsted polygon vertex
	GLfloat GetPolyVert(GLint i, GLint j);
	
	// Gets number of sides of the polygon
	GLint GetPolySides();
	
	//set the number of sides in the polygon
	void SetNSides(GLint sides);
	
	// Sets one polygon vertex of the object (in meters)
	void SetOneVert(GLint i, GLint j, GLfloat vert);
	
	// Sets the polygon vertices of the object (in meters)
	void SetPolyVerts(GLfloat Verts[][2]);
	
	// Sets the polygon color
	void SetPolyColor(GLfloat clr[]);
	
	//load polygon from file
	static Region2D LoadPolyFromFile(char* filePath);
	
	// Draw the object
	void Draw(GLfloat centerx, GLfloat centery);
	
	// Determines if the cursor is located within the region
	bool InRegion(Object2D* cursor, GLfloat centerx, GLfloat centery);
	bool InRegion(HandCursor* cursor, GLfloat centerx, GLfloat centery);
	bool InRegion(GLfloat xcurs, GLfloat ycurs, GLfloat centerx, GLfloat centery);
};

#endif
