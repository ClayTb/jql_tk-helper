using namespace std;
#define STOPPED true
#define STARTED false
class Timer
{
    public:
        Timer();
        bool getStatus(); //返回计时器状态，停止 正在运行 
        void start();
        void stop();
        long getStartedTime();
    private:
    	bool status = STOPPED;
    	time_t start_time = 0;
};

Timer::Timer()
{
}

bool Timer::getStatus()
{
	return status;
}

void Timer::start()
{
	if(status == STOPPED)
	{
		start_time = time(NULL);
		status = STARTED;
		return;
	}
	else if(status == STARTED)
	{
		return;
	}
}

void Timer::stop()
{
	if(status == STARTED)
	{
		start_time = 0;
		status = STOPPED;
		return;
	}
	else if(status = STOPPED)
	{
		return;
	}
}

long Timer::getStartedTime()
{
	long t = time(NULL) - start_time;
	return t;
}