#include <cmath>
#include "Object2D.h"

Object2D::Object2D(Image* i)
{
	image = i;
	angle = 0.0f;
	drawOn = 0;
}

GLfloat Object2D::GetWidth() const
{
	return image->GetWidth();
}

GLfloat Object2D::GetHeight() const
{
	return image->GetHeight();
}

GLfloat Object2D::GetX() const
{
	return xPos;
}

GLfloat Object2D::GetY() const
{
	return yPos;
}

void Object2D::SetPos(GLfloat x, GLfloat y)
{
	xPos = x;
	yPos = y;
}

void Object2D::SetAngle(GLfloat theta)
{
	angle = theta;
}

void Object2D::Draw()
{
	if(drawOn)
		image->Draw(xPos, yPos, angle);
}

void Object2D::Draw(GLfloat width, GLfloat height)
{
	if(drawOn)
		image->Draw(xPos, yPos, width, height, angle);
}

float Object2D::Distance(Object2D* ob1, Object2D* ob2)
{
	return Distance(ob1, ob2->GetX(), ob2->GetY());
}

/*
float Object2D::Distance(Object2D* ob1, HandCursor *cursor);
{
	return Distance(ob1, cursor->GetX(), cursor->GetY());
}

float Object2D::Distance(Object2D* ob1, Circle *circ);
{
	return Distance(ob1, circ->GetX(), circ->GetY());
}
*/

float Object2D::Distance(Object2D* ob1, GLfloat x, GLfloat y)
{
	return sqrtf(powf(x - ob1->xPos, 2.0f) + powf(y - ob1->yPos, 2.0f));
}

void Object2D::On()
{
	drawOn = 1;
}

void Object2D::Off()
{
	drawOn = 0;
}

