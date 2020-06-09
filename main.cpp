/*
#author: matt ji
#date: 2020-4-22
#purpose: 将梯控数据发送到云端，接收云端控梯指令
#增加监控功能
#2020-6-1: 软件架构优化, 将文件归类
  
*/
//#include "mqtt.h"
#include "misc.h"
#include "stdio.h"
#include <unistd.h>
#include <thread>
#include "thread.h"

//using namespace std;

int main(void)
{
    /* Init the signals to catch chld/quit/etc */
    init_signals();
    log(6, "tk-helper version V4.0 2020-5-25-17:27\n");
    printf("tk-helper version V4.0 2020-5-25-17:27\n");

    //2. 建立与云端通信的线程
    //thread (cloudThread).detach();

    //OTA 线程
    //thread (otaThread).detach();
    //监控模块
    //thread (monitorThread).detach();
    //产线设置线程
    //thread (productionThread).detach();
    thread (sqlThread).detach();

//
    while(1){
        //sleep 5s
        std::this_thread::sleep_for(chrono::seconds(5));   
    }
    return 0;
}
