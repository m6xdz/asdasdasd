#pragma once

#include <Windows.h>
#include <cstdint>

namespace relax {

    class c_nt_input {
    public:
#ifdef _WIN32
        c_nt_input( ) {
            LoadLibraryW( L"user32.dll" );
            HMODULE mod = LoadLibraryW( L"win32u.dll" );
            if ( mod ) {
                m_fn = reinterpret_cast<void( WINAPI* )( KEYBDINPUT*, int )>(
                    GetProcAddress( mod, "NtUserInjectKeyboardInput" ) );
            }
        }

        bool available( ) const { return m_fn != nullptr; }

        bool press( WORD vk, DWORD timestamp_ms = 0 ) {
            if ( !m_fn || !vk ) return false;
            KEYBDINPUT ki = {};
            ki.wVk = vk;
            ki.dwFlags = 0;
            ki.time = timestamp_ms ? timestamp_ms : GetTickCount( );
            m_fn( &ki, 1 );
            return true;
        }

        bool release( WORD vk, DWORD timestamp_ms = 0 ) {
            if ( !m_fn || !vk ) return false;
            KEYBDINPUT ki = {};
            ki.wVk = vk;
            ki.dwFlags = KEYEVENTF_KEYUP;
            ki.time = timestamp_ms ? timestamp_ms : GetTickCount( );
            m_fn( &ki, 1 );
            return true;
        }

    private:
        void( WINAPI* m_fn )( KEYBDINPUT*, int ) = nullptr;
#else
        c_nt_input() = default;
        bool available() const { return false; }
        bool press(WORD, DWORD = 0) { return false; }
        bool release(WORD, DWORD = 0) { return false; }
#endif
    };
}
