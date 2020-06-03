
#include "threadmonitor.h"
#include <iostream>
//#include "mqtt.h"
#include "misc.h"
#include "funmonitor.h"

//{"ID":"", "floorNum":"", "state":"", "door":"", "floorNum_r" : ""}
//{"ID":"", "hostname":"", "msg":"", "timestamp":""}
//string state, state_last, door, door_last;

#include "mqttmonitor.h"

//#include <boost/timer/timer.hpp>


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


int monitorThread(void)
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