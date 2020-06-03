#ifndef _FUNC_h
#define _FUNC_h
#include <string>
using namespace std;

extern int REGISTERED;


bool parseCloud(string data);
void autoOpen(string state);
void registerFloor(string floor);
int readConfigThread(void);


#endif
