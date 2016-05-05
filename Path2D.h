#ifndef PATH2D_H
#define PATH2D_H
#pragma once

#include "SDL.h"
#include "SDL_opengl.h"
#include "Object2D.h"
#include "HandCursor.h"

// Stores a path as a series of vertices, and enables rendering capabilties
class Path2D
{
private:
	GLint nVerts;
	GLfloat PathWidth;		 //path width, in meters
	GLfloat Vertices[8][6];  //either 2 pairs of points and a 0 flag (straight line) OR arc center (0-1), radius (2), start angle (3), and arc length (4) with a 1 flag (circular arc - 5).  angles in radians
	GLfloat color[3];
public:
	~Path2D() { }
	
	// Gets a requsted polygon vertex
	GLfloat GetPathVert(GLint i, GLint j);
	
	// Gets number of vertices in the path
	GLint GetPathNVerts();
	
	//set the number of sides in the polygon
	void SetNVerts(GLint sides);
	
	// Set path line width
	void SetPathWidth(GLfloat width);
	
	// Sets one polygon vertex of the object (in meters)
	void SetOneVert(GLint i, GLint j, GLfloat vert);
	
	// Sets the polygon vertices of the object (in meters)
	void SetPathVerts(GLfloat Verts[][6]);
	
	// Sets the polygon color
	void SetPathColor(GLfloat clr[]);
	
	//load polygon from file
	static Path2D LoadPathFromFile(char* filePath);
	
	// Draw the object.  pathx and pathy are the (center) offset of the path
	void Draw(GLfloat pathx, GLfloat pathy);
	
	// Determines if the cursor is located on the path
	bool OnPath(Object2D* cursor, GLfloat pathx, GLfloat pathy);
	bool OnPath(HandCursor* cursor, GLfloat pathx, GLfloat pathy);
	bool OnPath(float xcurs, float ycurs, GLfloat pathx, GLfloat pathy);
	bool PathCollision(Object2D* cursor, GLfloat pathx, GLfloat pathy, Object2D* LastCursorPos);
	int HitViaPts(Object2D* cursor, GLfloat pathx, GLfloat pathy, GLfloat dist);
	int HitViaPts(HandCursor* cursor, GLfloat pathx, GLfloat pathy, GLfloat dist);
	int HitViaPts(float xcurs, float ycurs, GLfloat pathx, GLfloat pathy, GLfloat dist);
};

#endif
