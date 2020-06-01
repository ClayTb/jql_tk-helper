#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/sendfile.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <sstream>
#include <iomanip>      // std::put_time
#include <time.h>  //localtime
#include <map>
#include <chrono>
#include <thread>


#include<sys/types.h>
#include<sys/stat.h>
#include<dirent.h>   // /usr/include/dirent.h
#include<string>
#include<iostream>
#include <vector>
#include <string.h>

using namespace std;


#include <cstdio>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <array>

#define UPDATEPATH "/home/tikong/update"
#define PROPATH  "/home/tikong/production"

#include "misc.h"
/*
std::string exec(const char* cmd) {
    std::array<char, 128> buffer;
    std::string result;
    std::shared_ptr<FILE> pipe(popen(cmd, "r"), pclose);
    if (!pipe) throw std::runtime_error("popen() failed!");
    while (!feof(pipe.get())) {
        if (fgets(buffer.data(), 128, pipe.get()) != nullptr)
            result += buffer.data();
    }
    return result;
}
*/



//int getAbsoluteFiles(string directory, vector<string>& filesAbsolutePath); //参数1[in]要变量的目录  参数2[out]存储文件名

/*
自动更新流程
1. 首先固件里要有一个update文件夹，里面是空的
2. 程序启动，每隔3秒扫描这个文件夹
5. 如果有新的文件进来，把名字记下来，ps之后如果发现有这个程序在运行，就mv这个文件到production文件夹覆盖掉原来程序
4. 然后kill 掉这个程序
6. startup脚本会再次启动这个程序，log里记录的版本号会上报到云端
7. update这个文件夹应该是保持为空的
如果是从云端发送过来的，就穿插一个到ftp下载程序到update文件夹，可能要先下载到/tmp文件夹
*/
//目前只考虑同一时间只能升级一个

int updateFile(string path, string name)
{
    //1. mv path to /home/tikong/production/
    string cmd = "mv ";
    cmd = cmd + path + " " + PROPATH;
    cout << cmd << endl;
    string result = exec(cmd.c_str());
    std::cout << result;
    //log(4, result.c_str());
    //2. pgrep fileName  直接得到PID
    cmd = "pgrep -x " + name;
    cout << cmd << endl;
    result = exec(cmd.c_str());
    std::cout << result;
    //3. kill -9 PID，之后startup会再重新启动这个程序
    // 一般情况下，PID应该只有一个
    cmd = "kill -9 " + result;
    result = exec(cmd.c_str());
    std::cout << result;
    //4. startup会检测到程序消失了，会再启动
    return 0;
}

//这里的while循环貌似可以处理很多文件
int loadFile(string directory) 
{
    DIR* dir = opendir(directory.c_str()); //打开目录   DIR-->类似目录句柄的东西 
    if ( dir == NULL )
    {
        cout<<directory<<" is not a directory or not exist!"<<endl;
        return -1;
    }

    struct dirent* d_ent = NULL;       //dirent-->会存储文件的各种属性
    char fullpath[128] = {0};
    char dot[3] = ".";                //linux每个下面都有一个 .  和 ..  要把这两个都去掉
    char dotdot[6] = "..";
    int err = 1;
    struct stat file_stat;
    time_t   mtime_now, mtime_pre;

    std::map <string, string> updateTime;

    while ( (d_ent = readdir(dir)) != NULL )    //一行一行的读目录下的东西,这个东西的属性放到dirent的变量中
    {
        if ( (strcmp(d_ent->d_name, dot) != 0)
              && (strcmp(d_ent->d_name, dotdot) != 0) )   //忽略 . 和 ..
        {

            string fileName = d_ent->d_name;
            string absolutePath = directory + string("/") + string(d_ent->d_name);  //构建绝对路径
            if( directory[directory.length()-1] == '/')  //如果传入的目录最后是/--> 例如a/b/  那么后面直接链接文件名
            {
                absolutePath = directory + string(d_ent->d_name); // /a/b/1.txt
            }
            //cout << absolutePath << "===" << fileName<< endl;   
            log(4, fileName.c_str());
            updateFile(absolutePath, fileName);                    
        }


    }

    closedir(dir);
    return 0;
}



int otaThread(void)
{
    struct stat file_stat;
    int err;
    time_t	 mtime_now, mtime_pre;
//这里可以把所有的梯控所有的程序的pid都记录以下
    //string result = exec("pgrep cti-elevator");
    //std::cout << result;
    //这里使用文件修改时间参数
    while(1)
    {
        err = stat(UPDATEPATH, &file_stat);
        if(err != 0)
        {
            perror("stat");
            return 1;
        }
        mtime_now = file_stat.st_mtime;
        if(mtime_now != mtime_pre)
        {
            //这里会打印三次，第一次时程序刚运行，初始化时间
            //第二次是scp或者ftp得到新的文件
            //第三次是从update文件夹到production文件夹
            printf("file changed, reload\n");
            log(4, "file update");
            mtime_pre = mtime_now;
            //如果update文件夹里有文件，就开始执行升级程序
            //这里是否需要sleep一点时间，让scp或者下载/拷贝完成之后再处理
            loadFile(UPDATEPATH);

        }
        std::this_thread::sleep_for(std::chrono::seconds(3)); 
    }
		return 0;
}