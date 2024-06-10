﻿// header.h: 标准系统包含文件的包含文件，
// 或特定于项目的包含文件
//

#pragma once

#include "targetver.h"
// #define WIN32_LEAN_AND_MEAN             // 从 Windows 头文件中排除极少使用的内容
// Windows 头文件
#include <afxwin.h>         // MFC 核心组件和标准组件
#include <afxdlgs.h>
#include <afxcmn.h>

// C 运行时头文件
#include <stdlib.h>
#include <malloc.h>
#include <memory.h>
#include <tchar.h>

#include <dbghelp.h>
#pragma comment(lib, "dbghelp.lib")

#include <map>
#include <tuple>
#include <string>
#include <vector>
#include <iostream>
#include <sstream>

#include "CommonUtils.h"
#include "..\ArkDriver\typedef.h"
#include "..\DuiLib\UIlib.h"


using namespace DuiLib;


#include "binary2strings/binary2strings.hpp"