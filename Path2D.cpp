#include <cmath>
#include <math.h>
#include <limits>
#include <sstream>
#include <iostream>
#include <fstream>
#include <istream>

#include "config.h"
#include "Path2D.h"




/*
	This object handles path drawing!
*/



/*
GL_LINE_LOOP� Use this primitive to close a line strip. OpenGL renders this primitive 
              like a GL_LINE_STRIP with the addition of a closing line segment between 
			  the final and first vertices.

GL_LINE_STRIP -- draw a line strep from a set of vertices, but leave the strip open
			   (does not connect the last vertex to the first vertex).
*/



void Path2D::SetNVerts(GLint sides)
{
	nVerts = sides;
	//path must have at least 1 segment!
	if (nVerts < 1)
		nVerts = 1;
	if (nVerts > 8)
		nVerts = 8;
}

void Path2D::SetPathVerts(GLfloat Verts[][6])
{
	for (int i = 0; i<nVerts; i++)
	{
		for (int j = 0; j < 6; j++)
		{
			Vertices[i][j] = Verts[i][j];
		}
	}

}

void Path2D::SetOneVert(GLint i, GLint j, GLfloat vert)
{
	Vertices[i][j] = vert;
}

void Path2D::SetPathColor(GLfloat clr[])
{
	color[0] = clr[0];
	color[1] = clr[1];
	color[2] = clr[2];
}

void Path2D::SetPathWidth(GLfloat width)
{

	PathWidth = width;  //path width, in meters

}



GLfloat Path2D::GetPathVert(GLint i, GLint j)
{
	return(Vertices[i][j]);
}

GLint Path2D::GetPathNVerts()
{
	return(nVerts);
}

Path2D Path2D::LoadPathFromFile(char* filePath)
{
	Path2D path;
	
	int nVerts = 0;
	GLfloat tmpclr[3];
	GLfloat tmpverts[8][6];
	GLfloat tmpwidth;
	char tmpline[80] = ""; 

	std::ifstream pathfile(filePath);

	if (!pathfile)
	{
		std::cerr << "Cannot open input file." << std::endl;
		path.nVerts = -1;
		return(path);
	}
	else
		std::cerr << "Opened Path File " << filePath << std::endl;


	pathfile.getline(tmpline,sizeof(tmpline),'\n');
	if (!pathfile.eof())
	{
		sscanf(tmpline,"%f %f %f",&tmpclr[0],&tmpclr[1],&tmpclr[2]);
		path.SetPathColor(tmpclr);
		std::cerr << "    Color: " << tmpclr[0] << " " << tmpclr[1] << " " << tmpclr[2] << std::endl;
	}
	else
		return(path);

	pathfile.getline(tmpline,sizeof(tmpline),'\n');
	if (!pathfile.eof())
	{
		sscanf(tmpline,"%f",&tmpwidth);
		float lineWidth[2];
		glGetFloatv(GL_LINE_WIDTH_RANGE, lineWidth);
		if (tmpwidth/PHYSICAL_RATIO > lineWidth[1])
			tmpwidth = lineWidth[1]*PHYSICAL_RATIO;
		path.SetPathWidth(tmpwidth);
		std::cerr << "    PathWidth: " << tmpwidth << std::endl;
	}
	else
		path.SetPathWidth(1.0f);


	pathfile.getline(tmpline,sizeof(tmpline),'\n');
	while(!pathfile.eof() && nVerts<10)
	{
			sscanf(tmpline, "%f %f %f %f %f %f",&tmpverts[nVerts][0],&tmpverts[nVerts][1],&tmpverts[nVerts][2],&tmpverts[nVerts][3],&tmpverts[nVerts][4],&tmpverts[nVerts][5]);
			nVerts++;
			pathfile.getline(tmpline,sizeof(tmpline),'\n');
	}
	path.SetNVerts(nVerts);
	path.SetPathVerts(tmpverts);

	std::cerr << "    N Verts: " << path.GetPathNVerts() << std::endl;

	return(path);

}


void Path2D::Draw(GLfloat pathx, GLfloat pathy)
{
	
	int nsegments = 100;

	// Draw the path
	glColor3f(color[0],color[1],color[2]);
	//float lineWidth[2];
	//glGetFloatv(GL_LINE_WIDTH_RANGE, lineWidth);
	//glLineWidth(std::min(PathWidth/PHYSICAL_RATIO,lineWidth[1]));
	glLineWidth(PathWidth/PHYSICAL_RATIO);

	//glBegin(GL_LINE_LOOP);
	glBegin(GL_LINE_STRIP);
	

	for (int i = 0; i<nVerts; i++)
	{
		if(Vertices[i][5] <= 0.1)
		{
			//straight line segment
			if (i == 0)
			{
				//first line segment, straight line
				glVertex3f(Vertices[i][0]+pathx,Vertices[i][1]+pathy, 0.0f);
				glVertex3f(Vertices[i][2]+pathx,Vertices[i][3]+pathy, 0.0f);
			}
			else
			{
				//connecting line segment, straight line (assume there is a repeated vertex!)
				glVertex3f(Vertices[i][2]+pathx,Vertices[i][3]+pathy, 0.0f);				
			}
		} //end if(flag = 0), i.e., draw a straight line
		else
		{
			//semicircular arc curved line segment.  assume the user provided the proper parameters in the input file
			//algorithm taken from http://www.allegro.cc/forums/thread/594175/715617#target

			float theta = Vertices[i][4]/float(nsegments);  //this is a small number so we shouldn't run into undefined/infinity problems with tan
			float tangential_factor = tanf(theta);
			float radial_factor = 1 - cosf(theta);

			float x = Vertices[i][0]+pathx + Vertices[i][2]*cosf(Vertices[i][3]);
			float y = Vertices[i][1]+pathy + Vertices[i][2]*sinf(Vertices[i][3]);

			for (int j = 0; j < nsegments + 1; j++)
			{
				glVertex3f(x,y,0.0f);

				//tx and ty are tangent vectors, perpendicular to the radius (so swap x and -y)
				float tx = -(y-(Vertices[i][1]+pathy));
				float ty = x - (Vertices[i][0]+pathx);
				x += tx*tangential_factor;
				y += ty*tangential_factor;

				float rx = (Vertices[i][0]+pathx) - x;
				float ry = (Vertices[i][1]+pathy) - y;

				x += rx*radial_factor;
				y += ry*radial_factor;
			}


		} //end else (flag = 1, i.e., draw a curved arc)
		
	}//end for(nVerts)
	glEnd();

	//reset defaults after the draw
	glColor3f(1.0f,1.0f,1.0f);
	glLineWidth(1.0f);
	
}


bool Path2D::OnPath(Object2D* cursor,GLfloat pathx, GLfloat pathy)
{
	return OnPath(cursor->GetX(), cursor->GetY(), pathx, pathy);
}

bool Path2D::OnPath(HandCursor* cursor,GLfloat pathx, GLfloat pathy)
{
	return OnPath(cursor->GetX(), cursor->GetY(), pathx, pathy);
}

bool Path2D::OnPath(float xcurs, float ycurs, GLfloat pathx, GLfloat pathy)
{
	//determine if the cursor is on or off the path.

	//two cases: if on a straight line, or on the semicircular-arc curve.
	bool onpath = false;
	float x1,x2,x3,x4,x5;
	//float epsilon = 0.001f;
	float epsilon = PathWidth/2;


	//iterate through each segment of the path
	for (int i = 0; i < nVerts; i++)
	{
		if(Vertices[i][5] <= 0.1)
		{
			//straight line segment
			
			if (fabsf(Vertices[i][2]-Vertices[i][0]) < epsilon)
			{
				//check for line verticality; if so, the check is easy: is the x coordinate the same and does the y coordinate fall between the two vertices
				onpath = onpath || ( (fabsf((Vertices[i][2]+pathx) - xcurs) < epsilon) && (((Vertices[i][3]+pathy)+epsilon/2 > ycurs && (Vertices[i][1]+pathy)-epsilon/2 < ycurs) || ((Vertices[i][1]+pathy)+epsilon/2 > ycurs && (Vertices[i][3]+pathy)-epsilon/2 < ycurs)) );
			}
			else
			{
				//not a vertical line, so see if the cursor is on the line: point falls on the line's equation, and between the two x values of the vertices
				x1 = ((Vertices[i][3]+pathy)-(Vertices[i][1]+pathy))/((Vertices[i][2]+pathx)-(Vertices[i][0]+pathx));
				x2 = (Vertices[i][1]+pathy) - x1 * (Vertices[i][0]+pathx);
				onpath = onpath || ( (fabsf(ycurs - (x1*xcurs+x2)) < epsilon) && (((Vertices[i][2]+pathx)+epsilon/2 > xcurs && (Vertices[i][0]+pathx)-epsilon/2 < xcurs) || ((Vertices[i][0]+pathx)+epsilon/2 > xcurs && (Vertices[i][2]+pathx)-epsilon/2 < xcurs)) );
			}

		}
		else
		{
			//curved line segment: point is (radius) away from the arc center, and at an angle between start_angle and (start_angle+arc_length)
			x1 = sqrtf( powf(xcurs - Vertices[i][0]+pathx,2.0f) + powf(ycurs - Vertices[i][1]+pathy,2.0f) );  //distance from the cursor to center of the arc
			x2 = atan2f(ycurs - Vertices[i][1]+pathy,xcurs - Vertices[i][0]+pathx);	//angle of the cursor from the center of the arc
			x2 = (x2 <= 0 ? x2 + 2*PI : x2 );  //set the angle to be between 0 and 2*pi
			x3 = (Vertices[i][3] <= 0 ? Vertices[i][3] + 2*PI : Vertices[i][3]);  //set the start angle between 0 and 2*pi
			x4 = (Vertices[i][3]+Vertices[i][4] <= 0 ? Vertices[i][3]+Vertices[i][4] + 2*PI : Vertices[i][3]+Vertices[i][4]);  //set the end angle between 0 and 2*pi if negative
			x4 = (x4 >= 2*PI ? x4 - 2*PI : x4);  //set the end angle between 0 and 2*pi if greater than 2*pi
			x5 = fabs(atanf(epsilon/Vertices[i][2]));  //angle "slop", or angular equivalent of a segment of length equal to the path width at the circle radius

			onpath = onpath || ( (fabsf(x1 - Vertices[i][2]) < epsilon) && ( (x2 > x3-x5 && x2 < x4+x5) || (x2 < x3+x5 && x2 > x4-x5) ) );
		}

	}
	
	return(onpath);


}




bool Path2D::PathCollision(Object2D* cursor,GLfloat pathx, GLfloat pathy,Object2D* LastCursorPos)
{
	//calculate the intersection of 2 lines (the line connecting cursor and LastCursorPos, and any segment of the path)
	//  note, this is currently NOT written to support the semicircular curved path segments!
	bool onpath = false;
	float p[2],q[2],r[2],s[2],e[2];
	float t,u,v;
	bool tflag,vflag;
	float epsilon = PathWidth/100;
	float x3,x4;

	//iterate through each segment of the path
	for (int i = 0; i < nVerts; i++)
	{
		if(Vertices[i][5] <= 0.1)
		{
			//straight line segment
			
			//method taken from http://stackoverflow.com/questions/563198/how-do-you-detect-where-two-line-segments-intersect
			// this method uses vector cross products to determine if the two segments intersect. it should be robust to detect
			// both intersections and also points where the cursor is on the path.
			
			//define seg1 as p to p+r, and seg2 as q to q+s.
			p[0] = LastCursorPos->GetX();
			p[1] = LastCursorPos->GetY();
			r[0] = cursor->GetX()-p[0];
			r[1] = cursor->GetY()-p[1];

			q[0] = (Vertices[i][0]+pathx);
			q[1] = (Vertices[i][1]+pathy);
			s[0] = (Vertices[i][2]+pathx)-q[0];
			s[1] = (Vertices[i][3]+pathy)-q[1];

			if ( fabsf(r[0]*s[1] - r[1]*s[0]) < epsilon )
			{
				//the lines are parallel
				//onpath = false;  //we don't need to set this again!
			}
			else
			{
				//the lines are not parallel. test if they intersect.

				t = ( (q[0]-p[0])*s[1] - (q[1]-p[1])*s[0] ) / (r[0]*s[1] - r[1]*s[0]);
				u = ( (q[0]-p[0])*r[1] - (q[1]-p[1])*r[0] ) / (r[0]*s[1] - r[1]*s[0]);

				if ( (t >= -epsilon && t <= 1+epsilon) && (u >= -epsilon && u <= 1+epsilon) )
					onpath = true;
				else
				{
					//onpath = false; //we don't need to set this again!
				}
			}

		}  //end if straight line segment
		else //else, curved path segment
		{
			//calculate if the line will intersect the circle at all
			p[0] = LastCursorPos->GetX();
			p[1] = LastCursorPos->GetY();
			r[0] = cursor->GetX()-p[0];
			r[1] = cursor->GetY()-p[1];
			
			q[0] = p[0] - (Vertices[i][0]+pathx);
			q[1] = p[1] - (Vertices[i][1]+pathy);

			u = pow(r[0]*q[0]+r[1]*q[1],2.0f) - pow(r[0]*r[0] + r[1]*r[1],2.0f) * ( pow(q[0]*q[0] + q[1]*q[1],2.0f) - Vertices[i][2]*Vertices[i][2] );

			//calculate the endpoints of the arc
			x3 = Vertices[i][3];  //the start angle
			x4 = Vertices[i][3]+Vertices[i][4];  //the end angle
			//if the arc length is negative, reverse these
			if (Vertices[i][4] <= 0)
			{
				x3 = x4;
				x4 = Vertices[i][3];
			}
			//calculate the start and end points of the arc, assuming it runs ccw from S to E
			s[0] = (Vertices[i][0]+pathx) + Vertices[i][2]*cos(x3);
			s[1] = (Vertices[i][1]+pathy) + Vertices[i][2]*sin(x3);
			e[0] = (Vertices[i][0]+pathx) + Vertices[i][2]*cos(x4);
			e[1] = (Vertices[i][1]+pathy) + Vertices[i][2]*sin(x4);

			if (u < 0)
			{
				//onpath = false;  //no intersection of line and circle  //we don't need to set this again!
			}
			else if (fabs(u) < epsilon)
			{
				//one intersection point; must test where the tangent point lies
				t = (-pow(r[0]*q[0]+r[1]*q[1],2.0f) + sqrt(fabs(u)) ) / pow(r[0]*r[0] + r[1]*r[1],2.0f);
				
				//test if the intersection point is off the line segment
				if (t < 0 || t > 1)
				{
					//onpath = false;  //we don't need to set this again!
				}

				//test if the intersection point is on the arc
				if ( (p[0]+t*r[0] - s[0])*(-(e[1]-s[1])) + (p[1]+t*r[1] - s[1])*(e[0]-s[0]) >= -epsilon)
					onpath = true;
				else
				{
					//onpath = false; //we don't need to set this again!
				}


			} //end one intersection point exists
			else
			{
				//two intersection points; must test where these points lie
				//one intersection point; must test where the tangent point lies
				t = (-pow(r[0]*q[0]+r[1]*q[1],2.0f) + sqrt(fabs(u)) ) / pow(r[0]*r[0] + r[1]*r[1],2.0f);
				v = (-pow(r[0]*q[0]+r[1]*q[1],2.0f) - sqrt(fabs(u)) ) / pow(r[0]*r[0] + r[1]*r[1],2.0f);

				//test if any intersection point is on the line segment; if not, then return false.
				if (t < 0 || t > 1)
					tflag = false;
				else
					tflag = true;

				if (v < 0 || v > 1)
					vflag = false;
				else
					vflag = true;

				if (!tflag && !vflag)
				{
					//onpath = false;  //we don't need to set this again!
				}
				
				//test if the intersection points lie on the same side as the arc
				if (tflag)
				{
					//if t is on the line segment, see if it is on the arc; if so, set the value to be true.
					if ( (p[0]+t*r[0] - s[0])*(-(e[1]-s[1])) + (p[1]+t*r[1] - s[1])*(e[0]-s[0]) >= -epsilon)
						onpath = true;
				}

				if (vflag)
				{
					//if v is on the line segment, see if it is on the arc; if so, set the value to be true.
					if ( (p[0]+v*r[0] - s[0])*(-(e[1]-s[1])) + (p[1]+v*r[1] - s[1])*(e[0]-s[0]) >= -epsilon)
						onpath = true;
				}

			} //end else 2 intersection points exist


		} //end else evaluate intersection for curved path segment

	} // end for loop

	return(onpath);

}




int Path2D::HitViaPts(Object2D* cursor,GLfloat pathx, GLfloat pathy, GLfloat dist)
{
	return HitViaPts(cursor->GetX(), cursor->GetY(), pathx,  pathy, dist);
}

int Path2D::HitViaPts(HandCursor* cursor,GLfloat pathx, GLfloat pathy, GLfloat dist)
{
	return HitViaPts(cursor->GetX(), cursor->GetY(), pathx,  pathy, dist);
}

int Path2D::HitViaPts(float xcurs, float ycurs,GLfloat pathx, GLfloat pathy, GLfloat dist)
{
	//check if the cursor is in the vicinity of the vertices; if so, return the vertex number
	int vert = -10;
	int ppdist = 0;
	int mindist = 10000;   //minimum distance; initialize to a large number 
	float x,y;

	//we will do this the "easy" way, by calculating the distance to each vertex and keeping track of the minimum
	for (int i = 0; i < nVerts; i++)
	{
		if(Vertices[i][5] <= 0.1)
		{
			//straight line
			if (i == 0)
			{
				//check also the starting vertex
				ppdist = sqrtf(powf(Vertices[i][0]+pathx - xcurs, 2.0f) + powf(Vertices[i][1]+pathy - ycurs, 2.0f));
				if (ppdist < mindist)
				{
					vert = -1;
					mindist = ppdist;
				}
			}
			ppdist = sqrtf(powf(Vertices[i][2]+pathx - xcurs, 2.0f) + powf(Vertices[i][3]+pathy - ycurs, 2.0f));
			if (ppdist < mindist)
			{
				vert = i;
				mindist = ppdist;
			}
		}
		else
		{
			//arc; we have to calculate the two vertices.

			if (i == 0)
			{
				//check also the starting vertex
				x = Vertices[i][0]+pathx + Vertices[i][2]*cosf(Vertices[i][3]);  //center + radius*cos(start_angle)
				y = Vertices[i][1]+pathy + Vertices[i][2]*sinf(Vertices[i][3]);	 //center + radius*sin(start_angle)
				ppdist = sqrtf(powf(x - xcurs, 2.0f) + powf(y - ycurs, 2.0f));
				if (ppdist < mindist)
				{
					vert = -1;
					mindist = ppdist;
				}
			}
			x = Vertices[i][0]+pathx + Vertices[i][2]*cosf(Vertices[i][3]);	 //center + radius*cos(end_angle)
			y = Vertices[i][1]+pathy + Vertices[i][2]*sinf(Vertices[i][3]);	 //center + radius*sin(end_angle)
			ppdist = sqrtf(powf(Vertices[i][2]+pathx - xcurs, 2.0f) + powf(Vertices[i][3]+pathy - ycurs, 2.0f));
			if (ppdist < mindist)
			{
				vert = i;
				mindist = ppdist;
			}

		}  //end else


	} //end for


	//now that we have the minimum distance and vertex number, determine if it is close enough to be "on" the vertex
	if (mindist <= dist)
		return(vert+1);
	else
		return(-99);

}
