/* BOURDONNEMENT - includes systeme (Win32 ou couche macOS/Linux), bibliotheques, PI et TICK (repris de VELOCITY)
   Morceau de main.c : inclus dans l'ordre, ne se compile pas seul (voir main.c). */

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define _CRT_SECURE_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <winhttp.h>
#include <shellapi.h>
#include <mmsystem.h>
#include <GL/gl.h>
typedef int sock_len_t;
#else
#include "../plateforme/mac/mac_compat.h"
#endif
#include <ctype.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#pragma comment(lib, "opengl32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "advapi32.lib")
#endif

#define PI   3.14159265f
#define TICK (1.0f / 120.0f)
