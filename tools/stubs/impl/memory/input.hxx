#pragma once
#include <Windows.h>
namespace input { inline UINT send_inputs(UINT count, LPINPUT inputs, int size) { return SendInput(count, inputs, size); } }
