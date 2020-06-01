Date: 2020-5-15:  
Author: matt ji  
Purpose: 对接平台云的同时，也对接梯控云 
		改名为tk-helper,除了梯控本体的程序之外所有的功能：梯控云通信 监控 产线设置 都集成起来   
+++++++++++++++++++++++++++   
###协议  
这里模拟云端发送呼梯：
mosquitto_pub -t cmd/IotApp/de:5b:dc:93:ec:79/ping/plain/5ea2b243b3098dd6a9d34d5e  -m '{"ID":"8888", "requestID":"6666", "cmd":"call", "floorNum_r":"5"}'  
开门
mosquitto_pub -t cmd/IotApp/de:5b:dc:93:ec:79/ping/plain/5ea2b243b3098dd6a9d34d5e  -m '{"ID":"8888", "requestID":"6666", "cmd":"open", "duration":"15"}'  

1. 本地增加自动开门10秒
	时间可以配置，做到config.ini里
	AUTO_OPEN:10

2. 增加读取config接口，读取AUTO_OPEN，
    并且config文件有变化，重新读取   

2020-5-22:  
1. 增加OTA功能  

2020-5-26：  
将eleMonitor和tk-helper合并  

2020-5-29:  
软件结构重新布局  