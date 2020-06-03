#include "mqtt.h"
#include "misc.h"
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
void my_subscribe_callback(struct mosquitto *mosq, void *userdata, int mid, int qos_count, const int *granted_qos)
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