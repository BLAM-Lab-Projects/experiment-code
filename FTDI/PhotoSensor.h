#ifndef PHOTOSENSOR_H
#define PHOTOSENSOR_H
#pragma once

#include "ftd2xx.h"

// Handles mouse actions
class PhotoSensor
{
public:
	PhotoSensor() { }
	~PhotoSensor() { }
	//initialize sensor
	static int InitSensor(int i, FT_HANDLE *ftHandle, int devMode = 1, UCHAR mask = 0x00);
	//get and process data from sensor
	static int GetSensorData(FT_HANDLE ftHandle);
	static int GetSensorBitBang(FT_HANDLE ftHandle,int bit = -1);
	static int SetSensorBitBang(FT_HANDLE ftHandle, UCHAR mask, int bit, int value);  //set lines on the ftdi
	//shutdown sensor
	static bool CloseSensor(FT_HANDLE ftHandle, int devMode = 1);

};

#endif
