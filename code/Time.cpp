#include <time.h>
#include "Time.h"
#include "win_linux_HEAD.h"

std::string YearMonthDayHourMinuteSecondStr()
{
    const time_t t = time(NULL);
    struct tm Time;
#ifndef WIN32
    localtime_r(&t, &Time);
#else
    localtime_s(&Time, &t);
#endif
    char Temp[128];
    snprintf(Temp, 128, "%d-%d-%d_%d-%d-%d", Time.tm_year + 1900, Time.tm_mon + 1, Time.tm_mday, Time.tm_hour, Time.tm_min, Time.tm_sec);
    return Temp;
}

void CTimer::update()
{
    MdBegin = std::chrono::high_resolution_clock::now();
}

double CTimer::getElapsedSecond()
{
    return  (double)getElapsedTimeInMicroSec() * 0.000001;
}

double CTimer::getElapsedTimeInMilliSec()
{
    return (double)this->getElapsedTimeInMicroSec() * 0.001;
}

long long CTimer::getElapsedTimeInMicroSec()
{
    return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - MdBegin).count();
}