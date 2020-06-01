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

//和云端通信
//mqtt也需要和本地通信，发送复位到1楼消息
void mqtt_setup_alicloud()
{
    cout<<"set up cloud mqtt\n";
    log(6,"set up cloud mqtt");
    //云端IP
    //char *host = "120.78.152.73";
    int port = 1883;
    int keepalive = 60;
    bool clean_session = true;
  
    mosquitto_lib_init();
    mosq_c = mosquitto_new(NULL, clean_session, NULL);
    if(!mosq_c){
        fprintf(stderr, "Error: Out of memory.\n");
        exit(1);
    }
  
    mosquitto_log_callback_set(mosq_c, mosq_log_callback);
    mosquitto_connect_callback_set(mosq_c, cloud_connect_callback);
    //mosquitto_subscribe_callback_set(mosq_c, my_subscribe_callback);
    //mosquitto_message_callback_set(mosq_c, cloudEleThread);
//只需要建立连接
    if(mosquitto_connect(mosq_c, remote_host.c_str(), port, keepalive)){
        fprintf(stderr, "Unable to connect cloud.\n");
        log(4, "Unable to connect cloud");
        //这里出现一个问题，就是4g路由器还没有完全能联网，导致这里整个程序就退出了
        //exit(1);
        //
    }
    mosquitto_reconnect_delay_set(mosq_c, 5, 30, true);
    int loop = mosquitto_loop_start(mosq_c);
    if(loop != MOSQ_ERR_SUCCESS){
        fprintf(stderr, "Unable to start loop: %i\n", loop);
        log(3, "Unable to start loop: %i", loop);
        exit(1);
    }
    
}

//{"ID":"", "hostname":"", "msg":"", "timestamp":""}
//2020-3-27 这里检测状态和电梯门状态，目的是为了检测出是否在维修
void checkEle()
{
    Json::Value rsp;
    Json::StreamWriterBuilder builder;
    builder["indentation"] = ""; // If you want whitespace-less output
    rsp["hostname"] = hostname;
    rsp["timestamp"] = getTimeStamp();
    rsp["ID"] = ID;
    time_t t = ele["door"].second.getStartedTime();


    if(ele["state"].second.getStartedTime() > STATETIMEOUT)
    {
        //2020-4-3：这里只根据一直上行或下行来判断是否在维护
        //这里一直报警没有问题
        //只有在up down的时候才会计时
        //if(ele["state"].first.compare("stop") != 0)
        {
            rsp["msg"] = "电梯状态超时，可能在维护";
            printf("电梯状态超时，可能在维护\n");
            log(3,"电梯状态超时，可能在维护");
            //发送成功给云端
            string data = Json::writeString(builder, rsp)+'\n';
            ele_monitor_q.push(data);
            //停止计时
            if(ele["state"].second.getStatus() == STARTED)
            {
                ele["state"].second.stop();
            }
        }
    }
    if(  t  > DOORTIMEOUT)
    {
        //这里没有问题，因为在报一次错误之后，会重新计时
        //只有开门的时候才开始计时，所以这里不需要判断是不是开门
        //if(ele["door"].first.compare("opened") == 0)
        {
            rsp["msg"] = "电梯门开门状态超时";
            log(3,"电梯开门状态超时");
            //发送成功给云端
            string data = Json::writeString(builder, rsp)+'\n';
            ele_monitor_q.push(data);
            //停止计时
            if(ele["door"].second.getStatus() == STARTED)
            {
                ele["door"].second.stop();
            }
        }
    }

    rsp["msg"] = "heartbeat";
    //无论上面有没有错误信息要发送，都要发送一个heartbeat
    string data = Json::writeString(builder, rsp)+'\n';
    ele_monitor_q.push(data);

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