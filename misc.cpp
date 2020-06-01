#include <stdlib.h> //getenv malloc free
#include <stdio.h> //fwrite
#include <time.h>
// for getpid
#include <unistd.h>
// for va_start etc functions
#include <stdarg.h>
#include <string.h>

#include <time.h> //// for strftime
#include <sys/time.h>

#include <sys/stat.h> //mkdir
#include <sys/types.h> //mkdir
#include <fcntl.h> //O_RDONLY
#include <errno.h> //errno and EEXIST
#include <string>
#include <syslog.h>

#include "misc.h"
#include <chrono>
#include <sstream>
#include <fstream>
#include <sys/types.h>
#include <sys/wait.h>

using namespace std;


#include <bits/stdc++.h> 
#include <stdio.h>      /* printf, NULL */
#include <stdlib.h>     /* srand, rand */
#include <time.h>       /* time */
const int MAX_SIZE = 36;
char letters[MAX_SIZE] = {'a','b','c','d','e','f','g','h','i','j','k','l','m','n','o','p','q',\
  'r','s','t','u','v','w','x','y','z','0','1','2','3','4','5','6','7','8','9'};
  
string randomstring(int n)
{
  string ran = "";
  for (int i=0;i<n;i++) 
  {
    ran=ran + letters[rand() % MAX_SIZE];
  }
  //cout << ran << endl;
  return ran;
}

//检测mac地址有几个：冒号
int macOK(string a)
{
   int count = 0;
   const char *p=a.c_str();

   for(int i = 0; *p != '\0'; p++) {
      if(*p == ':')
         count++;
   }
   //cout<<"Frequency is "<<count << endl;
   return count;
}
string getMac()
{
    string cmd = "cat /sys/class/net/eth0/address";
    string buf = exec(cmd.c_str());
    buf.pop_back();
    cout << buf << endl;
    return buf;
}
  
void log(int level, const char *format...)
{  
    va_list vlist; 
    va_start(vlist, format);  
    //这里记录到syslog里的tikong.log，并且会发送到云端的rsyslog
/*  只用3级 3 4 6
Severity的定义如下：
       Numerical         Severity
        Code
         0       Emergency: system is unusable 
         1       Alert: action must be taken immediately
         2       Critical: critical conditions
         3       Error: error conditions  用
         4       Warning: warning conditions 用
         5       Notice: normal but significant condition
         6       Informational: informational messages 用
         7       Debug: debug-level messages 
*/
    //直接使用level写入
    vsyslog(level, format, vlist);

    va_end(vlist);  
}


string getTimeStamp()
{
  auto timeNow = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now().time_since_epoch());
  char bufTime[16]{ 0 };
  sprintf(bufTime, "%lld", timeNow.count());
  return bufTime;
}

#include <stdio.h>  //popen fgets FILE
std::string exec(const char* cmd) {
    char buffer[128];
    std::string result = "";
    FILE* pipe = popen(cmd, "r");
    if (!pipe) throw std::runtime_error("popen() failed!");
    try {
        while (fgets(buffer, sizeof buffer, pipe) != NULL) {
            result += buffer;
        }
    } catch (...) {
        pclose(pipe);
        throw;
    }
    pclose(pipe);
    return result;
}

#include <signal.h>
/**@internal
 * @brief Handles SIGCHLD signals to avoid zombie processes
 *
 * When a child process exits, it causes a SIGCHLD to be sent to the
 * process. This handler catches it and reaps the child process so it
 * can exit. Otherwise we'd get zombie processes.
 */
 //20160918: i think there should nerver reach here, because we didn't fork process.
void
sigchl_handler(int s)
{
    int status;
    pid_t rc;

    log(4, "WARNING: Handler for SIGCHLD called. Trying to reap a child");
    printf("WARNING: Handler for SIGCHLD called. Trying to reap a child\n");
    rc = waitpid(-1, &status, WNOHANG);

    log(4, "WARNING: Handler for SIGCHLD reaped child PID %d", rc);
    printf("WARNING: Handler for SIGCHLD reaped child PID %d\n", rc);
}

/** Exits cleanly after cleaning up the firewall.  
 *  Use this function anytime you need to exit after firewall initialization.
 *  @param s Integer that is really a boolean, true means voluntary exit, 0 means error.
 */
void
termination_handler(int s)
{
    log(4, "WARNING: Handler for termination caught signal %d", s);
    printf("WARNING: Handler for termination caught signal %d\n", s);
    log(4, "WARNING: Exiting...\n\n");
    printf("WARNING: Exiting...\n");
    exit(s == 0 ? 1 : 0);
}


/** @internal 
 * Registers all the signal handlers
 */
void
init_signals(void)
{
    struct sigaction sa;

    log(6, "INFO: Initializing signal handlers");

    sa.sa_handler = sigchl_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        log(3, "ERROR: sigaction(): %s", strerror(errno));
        exit(1);
    }

    /* Trap SIGPIPE */
    /* This is done so that when libhttpd does a socket operation on
     * a disconnected socket (i.e.: Broken Pipes) we catch the signal
     * and do nothing. The alternative is to exit. SIGPIPE are harmless
     * if not desirable.
     */
    sa.sa_handler = SIG_IGN;
    if (sigaction(SIGPIPE, &sa, NULL) == -1) {
        log(3, "ERROR: sigaction(): %s", strerror(errno));
        exit(1);
    }
    if (sigaction(SIGTTIN, &sa, NULL) == -1) {
        log(3, "ERROR: sigaction(): %s", strerror(errno));
        exit(1);
    }

    sa.sa_handler = termination_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;

    /* Trap SIGTERM */
    if (sigaction(SIGTERM, &sa, NULL) == -1) {
        log(3, "ERROR: sigaction(): %s", strerror(errno));
        exit(1);
    }

    /* Trap SIGQUIT */
    if (sigaction(SIGQUIT, &sa, NULL) == -1) {
        log(3, "ERROR: sigaction(): %s", strerror(errno));
        exit(1);
    }

    /* Trap SIGINT */
    if (sigaction(SIGINT, &sa, NULL) == -1) {
        log(3, "ERROR: sigaction(): %s", strerror(errno));
        exit(1);
    }
    
}

#include <iostream>
#include <sstream>  

bool isNum(string str)  
{  
    stringstream sin(str);  
    double d;  
    char c;  
    if(!(sin >> d))  
    {
        return false;
    }
    if (sin >> c) 
    {
        return false;
    }  
    return true;  
}

