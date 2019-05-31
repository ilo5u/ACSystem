// stdafx.h: 标准系统包含文件的包含文件，
// 或是经常使用但不常更改的
// 项目特定的包含文件
//

#pragma once

#include "targetver.h"

#define WIN32_LEAN_AND_MEAN             // 从 Windows 头文件中排除极少使用的内容

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

#include "../Communicator/com.h"
