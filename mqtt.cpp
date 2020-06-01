#include "mqtt.h"
#include "misc.h"
#include <mosquitto.h>
#include <jsoncpp/json/json.h>
#include <string.h>  //strlen
//#include <boost/timer/timer.hpp>
Queue<string> cloud_state_q, cloud_rsp_q;
Queue<string> local_q;

/**公共函数开始**/
void mosq_log_callback(struct mosquitto *mosq, void *userdata, int level, const char *str)
{
    /* Pring all log messages regardless of level. */
  
  switch(level){
    //待查 TODO
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
void my_subscribe_callback(struct mosquitto *mosq, void *userdata, int mid, int qos_count, const int *granted_qos)
{
    //Subscribed (mid: 1): 0 1
//Subscribed (mid: 2): 0 1
//Subscribed (mid: 1): 128 1
    int i;
    printf("Subscribed (mid: %d): %d %d", mid, granted_qos[0], qos_count);
    log(6, "Subscribed (mid: %d): %d %d", mid, granted_qos[0], qos_count);
    for(i=1; i<qos_count; i++){
        printf(", %d", granted_qos[i]);
        log(6, ", %d", granted_qos[i]);
    }
    printf("\n");
}
//TOTO: 这里qos都是0
int mqtt_send(struct mosquitto *mosq, string topic, const char *msg){
  return mosquitto_publish(mosq, NULL, topic.c_str(), strlen(msg), msg, 0, 0);
}
/****公共函数结束***/



/***本地mqtt部分***/
struct mosquitto *mosq_l = NULL;
int connected_l = 0;
const char* LCMD = "/cti/ele/cmd";
const char* LRSP = "/cti/ele/cmd-rsp";
const char* LSTATE = "/cti/ele/state";
string cloud_state;

void local_callback(struct mosquitto *mosq, void *userdata, const struct mosquitto_message *message)
{
	if(message->payloadlen){
        //printf("topic: %s, payload: %s", message->topic, (char *)message->payload);
        //放入队列   
        //log(6,"recv %s: ", (char *)message->payload);
        std::string data = (char*)(message->payload);
        //std::string data = static_cast<char*>(message->payload);
        //放入云端队列，由云端线程处理 
        if(strcmp(message->topic, LSTATE) == 0)
        {
            //压入队列，监控梯控
            monitor_state_q.push(data);
            //给云端永远发送最新的状态
            cloud_state = data;
        }
        else if(strcmp(message->topic, LRSP) == 0)
        {
            cloud_rsp_q.push(data);
        }

	}else{
		log(4, "message %s (null)\n", message->topic);
	}
	//fflush(stdout);
}

//这里去订阅楼层信息
/*std::map<string, string> topic = {
    { "state", "/cti/ele/state" },
    { "response", "/cti/ele/cmd-rsp" },
    { "cmd", "/cti/ele/cmd" }, 
};*/

void local_connect_callback(struct mosquitto *mosq, void *userdata, int result)
{
	int i;
	if(!result){
        printf("local connected success\n");
        log(4, "local connected success");
        connected_l = 1;
		//mosquitto_subscribe(mosq, NULL, "$SYS/#", 2);
        //这里订阅本地的cmd命令，2的意思Qos，0 表示This is the fastest method and requires only 1 message. 
        //It is also the most unreliable transfer mode.
        //订阅状态以及命令的回复
		mosquitto_subscribe(mosq, NULL, LSTATE, 0);
        mosquitto_subscribe(mosq, NULL, LRSP, 0);

	}else{
		fprintf(stderr, "local Connect failed\n");
	}
}

void mqtt_setup_local()
{
    cout<<"set up local mqtt\n";
    log(6,"set up local mqtt");
    string host = "localhost";
	int port = 1883;
	int keepalive = 60;
	bool clean_session = true;
  
    mosquitto_lib_init();
    mosq_l = mosquitto_new(NULL, clean_session, NULL);
    if(!mosq_l){
		fprintf(stderr, "Error: Out of memory in local.\n");
		exit(1);
	}
    mosquitto_log_callback_set(mosq_l, mosq_log_callback);
    //连接之后去订阅
    mosquitto_connect_callback_set(mosq_l, local_connect_callback);
    //订阅之后处理函数
	mosquitto_subscribe_callback_set(mosq_l, my_subscribe_callback);
    //消息处理函数
    mosquitto_message_callback_set(mosq_l, local_callback);

    //keepalive 是client 定期发消息给 server
    if(mosquitto_connect(mosq_l, host.c_str(), port, keepalive)){
		fprintf(stderr, "Unable to connect local.\n");
        log(3,"Unable to connect local.");
		//exit(2);
	}
    mosquitto_reconnect_delay_set(mosq_l, 5, 30, true);

    //已经连接上的全局变量
    //connected_l = 1;
    int loop = mosquitto_loop_start(mosq_l);
    if(loop != MOSQ_ERR_SUCCESS){
        fprintf(stderr, "Unable to start loop in local: %i\n", loop);
        log(3, "Unable to start loop in local: %i\n", loop);
        exit(3);
    }
}
/***本地mqtt部分结束**/


/***云端mqtt部分开始**/
struct mosquitto *mosq_c = NULL;
int connected_c = 0;
void cloud_message_callback(struct mosquitto *mosq, void *userdata, const struct mosquitto_message *message)
{
    if(message->payloadlen){
        printf("cloud topic: %s, payload: %s\n", message->topic, (char *)message->payload);
        //放入队列   
        log(6,"recv %s: ", (char *)message->payload);
        std::string data = static_cast<char*>(message->payload);
        //放入本地处理队列，由本地线程处理 
  //收到就存起来，在另外的地方解析，不影响这里的回调效率
        local_q.push(data);            

    }else{
        log(4, "message %s (null)\n", message->topic);
    }
}

//这里去订阅楼层信息
/*
std::map<string, string> cloud_topic = {
    //{ "state", "upload_data/IotApp/fa:04:39:46:16:2b/sample/5e9d86a893c2a0bf5069ffe888" },
    { "state", "upload_data/IotApp/" },
    //{ "cmd", "cmd/IotApp/fa:04:39:46:16:2b/+/+/+/#"}, 
    { "cmd", "cmd/IotApp/"}, 
};*/
string CSTATE = "upload_data/IotApp/";
string CCMD = "cmd/IotApp/";
//cmd_resp/:ProductName/:DeviceName/:CommandName/:RequestID/:MessageID
string CRSP = "cmd_resp/IotApp/";

void cloud_connect_callback(struct mosquitto *mosq, void *userdata, int result)
{
    if(!result){
        printf("cloud connected success\n");
        log(4, "cloud connected success");
        //mosquitto_subscribe(mosq, NULL, "$SYS/#", 2);
        //这里订阅本地的cmd命令，2的意思Qos，0 表示This is the fastest method and requires only 1 message. 
        //It is also the most unreliable transfer mode.
        //订阅云端命令
		connected_c = 1;
        mosquitto_subscribe(mosq, NULL, CCMD.c_str(), 0);
    }else{
        fprintf(stderr, "cloud Connect failed\n");
		connected_c = 0;
       
//MOSQ_ERR_SUCCESS    on success.
//MOSQ_ERR_INVAL  if the input parameters were invalid.
//MOSQ_ERR_ERRNO  if a system call returned an error.  The variable errno contains the error code, even on Windows.  Use strerror_r() where available or FormatMessage() on Windows.

    }
}
//和云端通信
//mqtt也需要和本地通信，发送复位到1楼消息
void mqtt_setup_cloud()
{
    cout<<"set up cloud mqtt\n";
    log(6,"set up cloud mqtt");
    //云端IP
	string host = "49.233.183.83";
	//string host = "localhost";
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
    mosquitto_subscribe_callback_set(mosq_c, my_subscribe_callback);
  	mosquitto_message_callback_set(mosq_c, cloud_message_callback);
    string username = "IotApp/"+MAC;
    int ret = mosquitto_username_pw_set(mosq_c, username.c_str(),"TKtXLPUuGkfaRHA");
    log(6, "set username: %s pw: %s", username.c_str(), "TKtXLPUuGkfaRHA");
    cout << "set username: "<< username << ", pw: "<< "TKtXLPUuGkfaRHA" << endl;
    if(ret != MOSQ_ERR_SUCCESS)
    {
        fprintf(stderr, "Unable to set pw: %i\n", ret);
        log(3, "Unable to set pw: %i", ret);
        exit(1);
    }
//只需要建立连接
    if(mosquitto_connect(mosq_c, host.c_str(), port, keepalive)){
		fprintf(stderr, "Unable to connect cloud.\n");
        log(4, "Unable to connect cloud");
        //这里出现一个问题，就是4g路由器还没有完全能联网，导致这里整个程序就退出了
		//exit(1);
        //
	}
    //connected_c = 1;
    mosquitto_reconnect_delay_set(mosq_c, 5, 30, true);
    int loop = mosquitto_loop_start(mosq_c);
    if(loop != MOSQ_ERR_SUCCESS){
        fprintf(stderr, "Unable to start loop: %i\n", loop);
        log(3, "Unable to start loop: %i", loop);
        exit(1);
    }
    
}
/****云端mqtt结束***/
