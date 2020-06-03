#ifndef _MQTT_H
#define _MQTT_H
#include <mosquitto.h>
#include <string>
using namespace std;
void mosq_log_callback(struct mosquitto *mosq, void *userdata, int level, const char *str);
void my_subscribe_callback(struct mosquitto *mosq, void *userdata, int mid, int qos_count, const int *granted_qos);
int mqtt_send(struct mosquitto *mosq, string topic, const char *msg);
#endif