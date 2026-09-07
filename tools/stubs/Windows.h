#pragma once
#include <cstdint>
#include <vector>
using WORD=uint16_t;using DWORD=uint32_t;using UINT=unsigned;
constexpr unsigned INPUT_KEYBOARD=1,KEYEVENTF_KEYUP=2;
struct KEYBDINPUT{WORD wVk=0;DWORD dwFlags=0;};struct INPUT{unsigned type=0;KEYBDINPUT ki;}; using LPINPUT=INPUT*;
namespace fake_input { inline std::vector<INPUT> events;inline bool fail=false; }
inline UINT SendInput(UINT n,INPUT* inputs,int){if(fake_input::fail)return 0;for(unsigned i=0;i<n;++i)fake_input::events.push_back(inputs[i]);return n;}
