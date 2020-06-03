#include "productionthread.h"

int productionThread()
{
    while(1)
    {
        //sleep 5s
        std::this_thread::sleep_for(chrono::seconds(5));   
    }
    
}