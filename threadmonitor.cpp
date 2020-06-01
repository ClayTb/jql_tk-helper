#include "timer.h"
#include <jsoncpp/json/json.h>
#include "workerthread.h"
#include "misc.h"
#include <iostream>
//#include "mqtt.h"
#include "misc.h"

//{"ID":"", "floorNum":"", "state":"", "door":"", "floorNum_r" : ""}
//{"ID":"", "hostname":"", "msg":"", "timestamp":""}
//string state, state_last, door, door_last;

#include "mqtt.h"

//#include <boost/timer/timer.hpp>


//err这个topic的定义：{"ID":hostname, "timestamp":"", "error":"楼层突变"}
 //后期把heartbeat和error合成一个包，服务器端去区分
std::map<string, string> topic = {
    { "state", "/cti/ele/state" },
    { "heartbeat", "/cti/ele/hb" },
    { "err", "/cti/ele/error1" }, //这里是为了不收到之前已经部署的监控程序发过来的消息
};


/**公共函数开始**/
void mosq_log_callback(struct mosquitto *mosq, void *userdata, int level, const char *str)
{
    /* Pring all log messages regardless of level. */
  
  switch(level){
    //case MOSQ_LOG_DEBUG:
    //case MOSQ_LOG_INFO:
    //case MOSQ_LOG_NOTICE:
    case MOSQ_LOG_WARNING:
    case MOSQ_LOG_ERR: {
      log(4, "%i:%s\n", level, str);
    }
  }  
}

//订阅成功后的callback
void local_subscribe_callback(struct mosquitto *mosq, void *userdata, int mid, int qos_count, const int *granted_qos)
{
    int i;

    printf("local Subscribed (mid: %d): %d", mid, granted_qos[0]);
    log(6, "local Subscribed (mid: %d): %d", mid, granted_qos[0]);
    for(i=1; i<qos_count; i++){
        printf(", %d", granted_qos[i]);
        log(6, ", %d", granted_qos[i]);
    }
    printf("\n");
}
int mqtt_send(struct mosquitto *mosq, string topic, const char *msg){
  return mosquitto_publish(mosq, NULL, topic.c_str(), strlen(msg), msg, 0, 0);
}
/****公共函数结束***/



/***云端mqtt部分结束**/
struct mosquitto *mosq_c = NULL;
int connected_c = 0;


//这里自动重连间隔时间时多少
void cloud_connect_callback(struct mosquitto *mosq, void *userdata, int result)
{
    if(!result){
        //已经连接上的全局变量
        connected_c = 1;
        printf("cloud connect success\n");
        log(6, "cloud connect success");
    }else{
        fprintf(stderr, "cloud Connect failed\n");
        log(4, "cloud connect failed");
        connected_c = 0;
    }
}


int monitorEleState()
{
    cout<<"监控本地数据线程\n";
    log(6,"监控本地数据线程");
    std::string data, data_last;
    int ret = -1;

    while(1) 
    {                       
        if(!monitor_state_q.queue_.empty())
        {
            //不管云端有没有离线，都要把数据pop出来
            data = monitor_state_q.pop();
            parseEle(data);      
        }
        else
        {
            std::this_thread::sleep_for(chrono::seconds(1)); 
            //sleep(1);
        }            

    }
    return 0;
}
//定时发送心跳包以及错误包给cloud
int eleAliCloudThread()
{
    cout<<"send cloud data thread\n";
    log(6,"send cloud data thread");
    std::string data, data_last;
    int ret = -1;
    ele.insert(std::make_pair("state", make_pair("unknown", ts)));
    ele.insert(std::make_pair("door", make_pair("unknown", td)));
    hostname = exec("hostname");
    //去除最后的换行 pop_back()
    hostname.pop_back();
    std::size_t fnd;
    while(1) 
    {                       
        if(!ele_monitor_q.queue_.empty())
        {
            //不管云端有没有离线，都要把数据pop出来
            data = ele_monitor_q.pop();
            if(connected_c == 1)
            {
                fnd = data.find("heartbeat"); 
                if (fnd!=std::string::npos)
                {
                    ret = mqtt_send(mosq_c, topic.find("heartbeat")->second, data.c_str());
                    if(ret != 0)
                    {
                        //4g路由器半夜会重启，会导致这里发送出错
                        //但是一旦连上之后，mqtt会自动再去连接，这里发送就不会有问题了
                        log(4, "mqtt_send error=%i\n", ret);
                        connected_c = 0;
                    }
                }
                else
                {         
                    ret = mqtt_send(mosq_c, topic.find("err")->second,data.c_str());
                    if(ret != 0)
                    {
                        log(4, "mqtt_send error=%i\n", ret);
                        connected_c = 0;
                    }   
                }       
            }      
        }
        else
        {
            std::this_thread::sleep_for(chrono::seconds(1)); 
            //sleep(1);
        }            

    }
    return 1;
}


int eleMonitorThread(void)
{
    //2. 先建立与ali私有监控云端线程，因为要初始化ele map数组
    thread (eleAliCloudThread).detach();
    //初始化云端mqtt，需要把错误状态publish到云端
    mqtt_setup_alicloud();
    //初始化本地mqtt，订阅楼层信息
    //无须再订阅本地消息，另外一个线程已经订阅了并把消息压入队列了
    //mqtt_setup_local();
    //建立本地解析监控电梯数据的线程
    thread (monitorEleState).detach();


    //3. 不断检测是不是有错误要上报
    while(1)
    {
        //sleep xs
        std::this_thread::sleep_for(chrono::seconds(CHECKTIME)); 
        checkEle();     
    }
    
    return 0;
    
}