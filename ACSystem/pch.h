// 入门提示: 
//   1. 使用解决方案资源管理器窗口添加/管理文件
//   2. 使用团队资源管理器窗口连接到源代码管理
//   3. 使用输出窗口查看生成输出和其他消息
//   4. 使用错误列表窗口查看错误
//   5. 转到“项目”>“添加新项”以创建新的代码文件，或转到“项目”>“添加现有项”以将现有代码文件添加到项目
//   6. 将来，若要再次打开此项目，请转到“文件”>“打开”>“项目”并选择 .sln 文件

#ifndef PCH_H
#define PCH_H

#include <cstdio>
#include <Windows.h>
#include <regex>
#include <http_client.h>
#include <filestream.h>
#include <http_listener.h>              // HTTP server
#include <json.h>                       // JSON library
#include <uri.h>                        // URI library
#include <ws_client.h>                  // WebSocket client
#include <containerstream.h>            // Async streams backed by STL containers
#include <interopstream.h>              // Bridges for integrating Async streams with STL and WinRT streams
#include <rawptrstream.h>               // Async streams backed by raw pointer to memory
#include <producerconsumerstream.h>     // Async streams for producer consumer scenarios

#include <ctime>
#include <cmath>

#include "../DBMS/dbms.h"
#include "../Logger//log.h"
#include "../Communicator/com.h"
#include "../Usr/usr.h"

#endif //PCH_H
