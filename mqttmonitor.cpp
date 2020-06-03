
#include "misc.h"
#include <jsoncpp/json/json.h>
#include "mqtt.h"




struct mosquitto *mosq_m = NULL;
int connected_m = 0;


//这里自动重连间隔时间时多少
static void cloud_connect_callback(struct mosquitto *mosq, void *userdata, int result)
{
    if(!result){
        //已经连接上的全局变量
        connected_m = 1;
        printf("cloud connect success\n");
        log(6, "cloud connect success");
    }else{
        fprintf(stderr, "cloud Connect failed\n");
        log(4, "cloud connect failed");
        connected_m = 0;
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
    mosq_m = mosquitto_new(NULL, clean_session, NULL);
    if(!mosq_m){
        fprintf(stderr, "Error: Out of memory.\n");
        exit(1);
    }
  
    mosquitto_log_callback_set(mosq_m, mosq_log_callback);
    mosquitto_connect_callback_set(mosq_m, cloud_connect_callback);
    //mosquitto_subscribe_callback_set(mosq_m, my_subscribe_callback);
    //mosquitto_message_callback_set(mosq_m, cloudEleThread);
//只需要建立连接
    if(mosquitto_connect(mosq_m, remote_host.c_str(), port, keepalive)){
        fprintf(stderr, "Unable to connect cloud.\n");
        log(4, "Unable to connect cloud");
        //这里出现一个问题，就是4g路由器还没有完全能联网，导致这里整个程序就退出了
        //exit(1);
        //
    }
    mosquitto_reconnect_delay_set(mosq_m, 5, 30, true);
    int loop = mosquitto_loop_start(mosq_m);
    if(loop != MOSQ_ERR_SUCCESS){
        fprintf(stderr, "Unable to start loop: %i\n", loop);
        log(3, "Unable to start loop: %i", loop);
        exit(1);
    }
    
}
