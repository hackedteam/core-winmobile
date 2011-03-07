#pragma once

#include <vector>
using namespace std;

#include <Windows.h>
#include "TAPI.h"

#ifndef STRINGUTILS
namespace std {
	typedef std::basic_string<WCHAR> Wstring;
}
#endif

typedef vector<HLINE> LineVector;
typedef vector<DWORD> DwordVector;
typedef vector<BYTE>  ByteVector;
typedef vector<LPCTSTR> VectorStringList;

#define vectorptr(v) ((v).empty()?NULL:&(v)[0])
