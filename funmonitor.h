#ifndef _FUNM_H
#define _FUNM_H

#include <string>
using namespace std;
int parseEle(string pkt);
void checkEle();

//上下行2分钟就认为超时在维护
#define STATETIMEOUT 2*60
//开门超过2分钟认为有问题
#define DOORTIMEOUT 2*60
#define CHECKTIME 3

extern string hostname;
extern Queue<string> ele_monitor_q;


#endif