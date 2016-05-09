#ifndef TIMER_H
#define TIMER_H
#pragma once
#include "SDL.h"
#include "SDL_mixer.h"
#include "Sound.h"

class Timer
{
private:
	Uint32 startTime;
	Uint32 alarmTime;
	Sound* alarmSound;
	bool alarmSounded;
	bool alarmOn;
	bool stopped;

public:
	Timer(Uint32 alarmtime, Sound* alarm);
	Timer(void);
	~Timer(void);

	void Reset();
	Uint32 Elapsed();
	void SetAlarmTime(Uint32 alarmT);
	Uint32 Remaining();
	void Stop();

	void CheckAlarm();
};

#endif