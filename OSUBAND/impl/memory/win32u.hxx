#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace memory {

    class c_win32u_exports {
    public:
        bool load( const wchar_t* path = L"C:\\Windows\\System32\\win32u.dll" ) {
            m_image.clear( );
            m_export_rva.clear( );

            const auto handle = CreateFileW( path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr );
            if ( handle == INVALID_HANDLE_VALUE )
                return false;

            const auto size = GetFileSize( handle, nullptr );
            if ( size == INVALID_FILE_SIZE || size < sizeof( IMAGE_DOS_HEADER ) ) {
                CloseHandle( handle );
                return false;
            }

            m_image.resize( size );
            DWORD read = 0;
            const auto ok = ReadFile( handle, m_image.data( ), size, &read, nullptr ) && read == size;
            CloseHandle( handle );
            if ( !ok )
                return false;

            return parse_exports( );
        }

        bool bind( ) {
            m_send_input = nullptr;
            m_get_cursor_pos = nullptr;
            m_set_cursor_pos = nullptr;

            const auto* live = GetModuleHandleW( L"win32u.dll" );
            if ( !live || m_export_rva.empty( ) )
                return false;

            const auto base = reinterpret_cast<uintptr_t>( live );

            if ( const auto rva = export_rva( "NtUserSendInput" ) )
                m_send_input = reinterpret_cast<send_input_fn>( base + rva );

            if ( const auto rva = export_rva( "NtUserGetCursorPos" ) )
                m_get_cursor_pos = reinterpret_cast<get_cursor_pos_fn>( base + rva );

            if ( const auto rva = export_rva( "NtUserSetCursorPos" ) )
                m_set_cursor_pos = reinterpret_cast<set_cursor_pos_fn>( base + rva );

            return m_send_input != nullptr;
        }

        [[nodiscard]] bool ready( ) const { return m_send_input != nullptr; }

        using send_input_fn = UINT( NTAPI* )( UINT, LPINPUT, int );
        using get_cursor_pos_fn = BOOL( NTAPI* )( LPPOINT );
        using set_cursor_pos_fn = BOOL( NTAPI* )( int, int );

        send_input_fn send_input( ) const { return m_send_input; }
        get_cursor_pos_fn get_cursor_pos( ) const { return m_get_cursor_pos; }
        set_cursor_pos_fn set_cursor_pos( ) const { return m_set_cursor_pos; }

    private:
        std::vector<uint8_t> m_image;
        std::unordered_map<std::string, uint32_t> m_export_rva;

        const uint8_t* rva_to_ptr( uint32_t rva, const IMAGE_NT_HEADERS64* nt ) const {
            const auto* section = IMAGE_FIRST_SECTION( nt );
            for ( WORD i = 0; i < nt->FileHeader.NumberOfSections; ++i, ++section ) {
                if ( rva >= section->VirtualAddress &&
                     rva < section->VirtualAddress + section->Misc.VirtualSize ) {
                    return m_image.data( ) + section->PointerToRawData + ( rva - section->VirtualAddress );
                }
            }
            return nullptr;
        }

        [[nodiscard]] uint32_t export_rva( const char* name ) const {
            const auto it = m_export_rva.find( name );
            return it != m_export_rva.end( ) ? it->second : 0;
        }

        bool parse_exports( ) {
            const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>( m_image.data( ) );
            if ( dos->e_magic != IMAGE_DOS_SIGNATURE )
                return false;

            const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>( m_image.data( ) + dos->e_lfanew );
            if ( nt->Signature != IMAGE_NT_SIGNATURE )
                return false;

            const auto& dir = nt->OptionalHeader.DataDirectory[ IMAGE_DIRECTORY_ENTRY_EXPORT ];
            if ( !dir.VirtualAddress || !dir.Size )
                return false;

            const auto* exports = reinterpret_cast<const IMAGE_EXPORT_DIRECTORY*>( rva_to_ptr( dir.VirtualAddress, nt ) );
            if ( !exports )
                return false;

            const auto* names = reinterpret_cast<const uint32_t*>( rva_to_ptr( exports->AddressOfNames, nt ) );
            const auto* ordinals = reinterpret_cast<const uint16_t*>( rva_to_ptr( exports->AddressOfNameOrdinals, nt ) );
            const auto* functions = reinterpret_cast<const uint32_t*>( rva_to_ptr( exports->AddressOfFunctions, nt ) );
            if ( !names || !ordinals || !functions )
                return false;

            for ( DWORD i = 0; i < exports->NumberOfNames; ++i ) {
                const auto* export_name = reinterpret_cast<const char*>( rva_to_ptr( names[ i ], nt ) );
                if ( !export_name )
                    continue;

                const auto ordinal = ordinals[ i ];
                const auto func_rva = functions[ ordinal ];
                if ( !func_rva )
                    continue;

                m_export_rva[ export_name ] = func_rva;
            }

            return !m_export_rva.empty( );
        }

        send_input_fn m_send_input = nullptr;
        get_cursor_pos_fn m_get_cursor_pos = nullptr;
        set_cursor_pos_fn m_set_cursor_pos = nullptr;
    };

}