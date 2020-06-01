/*
#author: matt ji
#date: 2020-4-22
#purpose: 将梯控数据发送到云端，接收云端控梯指令
#增加监控功能
  
*/
#include "mqtt.h"
#include "misc.h"
#include "stdio.h"
#include <unistd.h>
#include <thread>
#include "workerthread.h"
#include "ota.h"

//using namespace std;

int main(void)
{
    /* Init the signals to catch chld/quit/etc */
    init_signals();
    log(6, "tk-helper version V4.0 2020-5-25-17:27\n");
    printf("tk-helper version V4.0 2020-5-25-17:27\n");

//初始化和梯控云端的通信
    if(cloudSetup() != 0)
    {
        log(3,"云端初始化失败");
        return 1;
    }


    //2. 建立处理云端数据线程cmd
    thread (cloudThread).detach();
    //3. 消耗处理state消息，发给云端
    thread (localStateThread).detach();
    //处理消耗cmd rsp消息，发给云端
    thread (localRspThread).detach();
    //检测config有没有被改动的线程
    thread (readConfigThread).detach();
    return 0;



    

    //初始化云端mqtt，需要把状态以及回复publish到云端，并订阅cmd
    mqtt_setup_cloud();
    //初始化本地mqtt，订阅楼层信息以及梯控的回复转发到云端
    //另外把消息压入队列来监控电梯
    mqtt_setup_local();
    //OTA 线程
    thread (otaThread).detach();
    //监控模块
    thread (eleMonitorThread).detach();


    while(1){
        //sleep 5s
        std::this_thread::sleep_for(chrono::seconds(5));   
    }
    return 0;
}
