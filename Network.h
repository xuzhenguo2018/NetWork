#ifndef __NETWORK_H__
#define __NETWORK_H__

#include <string>
#include <map>
#include <list>
using namespace std;

#pragma warning(disable : 4251)

#ifdef NETWORK_EXPORTS
#define NETWORK_API __declspec(dllexport)
#else
#define NETWORK_API __declspec(dllimport)
#endif

#endif