#ifndef _MCLOUD_h
#define _MCLOUD_h
#include <mosquitto.h>
#include <jsoncpp/json/json.h>
#include <string.h>  //strlen
void mqtt_setup_local();
void mqtt_setup_cloud();
int mqtt_send(struct mosquitto *mosq, string topic, const char *msg);
 
#include "misc.h"

//#include <boost/timer/timer.hpp>
extern Queue<string> cloud_state_q, cloud_rsp_q;
extern Queue<string> local_q;
extern string CRSP ;
extern string cloud_state;
extern struct mosquitto *mosq_l, *mosq_c;

extern int connected_l, connected_c;

//extern const char* LCMD;
extern const char* LRSP ;
extern const char* LSTATE ;

#endif