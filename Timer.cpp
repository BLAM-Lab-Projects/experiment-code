#include "Timer.h"
#include "SDL.h"
#include "SDL_mixer.h"
#include "Sound.h"

Timer::Timer(void)
{
	startTime = SDL_GetTicks();
	alarmTime = 0;
	alarmSound = NULL;
	alarmSounded = 0;
	alarmOn = 0;
	stopped = 1;
}

Timer::Timer(Uint32 alarmtime, Sound* alarm)
{
	alarmSound = alarm;
	alarmTime = alarmtime;
	alarmSounded = 1;
	alarmOn = 1;
	startTime = SDL_GetTicks();
	stopped = 1;
}

Timer::~Timer(void)
{
}

void Timer::Reset(void)
{
	startTime = SDL_GetTicks();
	stopped = 0;
}

Uint32 Timer::Elapsed(void)
{
	if(stopped)
	{
		return 0;
	}
	else
	{
		return SDL_GetTicks()-startTime;
	}
}


void Timer::SetAlarmTime(Uint32 alarmT)
{
	alarmTime = alarmT;
	alarmSounded = 0;
	Reset();
	stopped = 0;
}

Uint32 Timer::Remaining()
{
	return startTime+alarmTime - SDL_GetTicks();
}

void Timer::CheckAlarm()
{
	if(SDL_GetTicks()-startTime > alarmTime & alarmOn & !alarmSounded)
	{
		alarmSound->Play();
		alarmSounded = 1;
	}
}

void Timer::Stop()
{
	stopped = 1;
}