#pragma once
#include <Windows.h>
#include <shlobj.h>
// Keep the last unhandled fault code/address for support. No memory dump or token.
namespace crash_report {
inline wchar_t path[MAX_PATH]{};
inline LONG WINAPI handler(EXCEPTION_POINTERS* info){
 HANDLE file=CreateFileW(path,GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
 if(file!=INVALID_HANDLE_VALUE){char text[256]{};const int n=wsprintfA(text,"OSU BAND 1.1.0-beta.2\r\nException: 0x%08lX\r\nAddress: %p\r\n",info->ExceptionRecord->ExceptionCode,info->ExceptionRecord->ExceptionAddress);DWORD done=0;WriteFile(file,text,n,&done,nullptr);CloseHandle(file);}
 return EXCEPTION_CONTINUE_SEARCH;
}
inline void install(){
 wchar_t root[MAX_PATH]{};if(FAILED(SHGetFolderPathW(nullptr,CSIDL_LOCAL_APPDATA,nullptr,0,root)))return;
 wsprintfW(path,L"%s\\OSUBAND",root);CreateDirectoryW(path,nullptr);
 wsprintfW(path,L"%s\\OSUBAND\\Beta",root);CreateDirectoryW(path,nullptr);
 wsprintfW(path,L"%s\\OSUBAND\\Beta\\last-crash.txt",root);SetUnhandledExceptionFilter(handler);
}
}
