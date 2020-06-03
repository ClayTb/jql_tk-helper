#include <mosquitto.h>
#include <jsoncpp/json/json.h>

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