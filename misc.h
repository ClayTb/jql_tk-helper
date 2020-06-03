#ifndef _LOG_H
#define _LOG_H
#include <iostream>
#include <string>
#include <string.h>  //strlen
#include <jsoncpp/json/json.h>
#include <chrono>


using namespace std;  
void log(int level, const char *format...);
std::string getMac();
std::string randomstring(int n);
int macOK(std::string a);
std::string exec(const char* cmd);
void init_signals(void);
bool isNum(string str);  
string getTimeStamp();
extern string MAC;
extern string hostname;
extern string remote_host;


#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
//重新定义Queue这个类，使之线程安全
template <typename T>
class Queue
{
 public:

  T pop() 
  {
    std::unique_lock<std::mutex> mlock(mutex_);
    while (queue_.empty())
    {
      cond_.wait(mlock);
    }
    auto val = queue_.front();
    queue_.pop();
    return val;
  }

  void pop(T& item)
  {
    std::unique_lock<std::mutex> mlock(mutex_);
    while (queue_.empty())
    {
      cond_.wait(mlock);
    }
    item = queue_.front();
    queue_.pop();
  }

  void push(const T& item)
  {
    std::unique_lock<std::mutex> mlock(mutex_);
    queue_.push(item);
    mlock.unlock();
    cond_.notify_one();
  }
  Queue()=default;
  Queue(const Queue&) = delete;            // disable copying
  Queue& operator=(const Queue&) = delete; // disable assignment
  std::queue<T> queue_;

  
 private:
  //std::queue<T> queue_;
  std::mutex mutex_;
  std::condition_variable cond_;
};
extern Queue<string> monitor_state_q;

extern string autoTime, ID;

#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/sendfile.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
int readAuto();


#endif