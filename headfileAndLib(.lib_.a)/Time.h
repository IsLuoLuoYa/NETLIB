#pragma once
#include <string>
#include <chrono>

// 该函数返回一个年月日时分秒的字符串，用于日志文件生成时的文件名拼接
std::string YearMonthDayHourMinuteSecondStr();
class CTimer
{
private:
    std::chrono::time_point<std::chrono::high_resolution_clock>     MdBegin;

public:
    CTimer(){   update();   }
    void update();                              // 更新为当前的时间点
    double getElapsedSecond();                  // 获取当前秒
    double getElapsedTimeInMilliSec();          // 获取毫秒
    long long getElapsedTimeInMicroSec();       // 获取微妙
};
