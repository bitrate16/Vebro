// header.h : include file for standard system include files,
// or project specific include files
//

#pragma once

#include "targetver.h"

#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers

#include <SDKDDKVer.h>
#include <windows.h>
#include <shellapi.h>
#include <WinUser.h>
#include <Shobjidl.h>
#include <tchar.h>
#include <cmath>
#include <ctime>
#include <cstdio>
#include <string>
#include <iomanip>
#include <streambuf>
#include <io.h>
#include <fcntl.h>
#include <chrono>
#include <thread>
#include <vector>
#include <stdlib.h>
#include <malloc.h>
#include <memory.h>
#include <tchar.h>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <filesystem>
#include <sstream>
#include <istream>
#include <fstream>
#include <iostream>
#include <string>
#include <codecvt>
#include <functional>

#include <GL/glew.h>
#include <GL/gl.h>
#include <GL/glu.h>
#include <GLFW/glfw3.h>

#include "lodepng.h"

#define JSON_DIAGNOSTICS 1
#define JSON_TRY_USER if(true)
#define JSON_CATCH_USER(exception) if(false)
#define JSON_THROW_USER(exception)                           \
    {std::clog << "Error in " << __FILE__ << ":" << __LINE__ \
               << " (function " << __FUNCTION__ << ") - "    \
               << (exception).what() << std::endl;           \
     std::abort();}
#include "json.hpp"
