#ifndef _CTHREAD_H
#define _CTHREAD_H
#include <stdio.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string>
#include <iostream>
#include <pthread.h>  //pthread_create/join/pthread_exit 
#include <chrono>
#include <thread>
using namespace std;
int localStateThread();
int localRspThread();
int cloudThread();
//和梯控云端通信
int cloudSetup();

#endif
