#pragma once

#ifdef _WIN64
#define WIN32_LEAN_AND_MEAN
#pragma comment(lib, "ws2_32.lib")
#include <Windows.h>
#include <direct.h>
#include <Winsock2.h>
#endif

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#pragma comment(lib, "ws2_32.lib")
#include <Windows.h>
#include <direct.h>
#include <Winsock2.h>
#include <io.h>
#endif

#ifndef WIN32
#ifndef _WIN64
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>
#include <linux/if.h>
#include <netinet/in.h>
#include <arpa/inet.h> 
#include <sys/ioctl.h>
#include <dirent.h>
#include <sys/signal.h>
#define SOCKET int
#define INVALID_SOCKET  (SOCKET)(~0)
#define SOCKET_ERROR            (-1)
#endif
#endif
