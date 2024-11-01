﻿/**
 * This file is part of Special K.
 *
 * Special K is free software : you can redistribute it
 * and/or modify it under the terms of the GNU General Public License
 * as published by The Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * Special K is distributed in the hope that it will be useful,
 *
 * But WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Special K.
 *
 *   If not, see <http://www.gnu.org/licenses/>.
 *
**/

#include <SpecialK/stdafx.h>

#ifdef  __SK_SUBSYSTEM__
#undef  __SK_SUBSYSTEM__
#endif
#define __SK_SUBSYSTEM__ L" DInput 7 "

#define _L2(w)  L ## w
#define  _L(w) _L2(w)

extern bool nav_usable;


using finish_pfn = void (WINAPI *)(void);

#define SK_DI7_READ(type)  SK_DI7_Backend->markRead   (type);
#define SK_DI7_WRITE(type) SK_DI7_Backend->markWrite  (type);
#define SK_DI7_VIEW(type)  SK_DI7_Backend->markViewed (type);
#define SK_DI7_HIDE(type)  SK_DI7_Backend->markHidden (type);


#define DINPUT7_CALL(_Ret, _Call) {                                     \
  dll_log->LogEx (true, L"[   Input  ]  Calling original function: ");  \
  (_Ret) = (_Call);                                                     \
  _com_error err ((_Ret));                                              \
  if ((_Ret) != S_OK)                                                   \
    dll_log->LogEx (false, L"(ret=0x%04x - %s)\n", err.WCode (),        \
                                                   err.ErrorMessage ());\
  else                                                                  \
    dll_log->LogEx (false, L"(ret=S_OK)\n");                            \
}

///////////////////////////////////////////////////////////////////////////////
//
// DirectInput 7
//
///////////////////////////////////////////////////////////////////////////////
DirectInputCreateEx_pfn
         DirectInputCreateEx_Import                       = nullptr;
DirectInputCreateA_pfn
         DirectInputCreateA_Import                        = nullptr;
DirectInputCreateW_pfn
         DirectInputCreateW_Import                        = nullptr;

IDirectInput7W_CreateDevice_pfn
        IDirectInput7W_CreateDevice_Original              = nullptr;

IDirectInputDevice7W_GetDeviceState_pfn
        IDirectInputDevice7W_GetDeviceState_Original      = nullptr;

IDirectInputDevice7W_SetCooperativeLevel_pfn
        IDirectInputDevice7W_SetCooperativeLevel_Original = nullptr;

IDirectInput7A_CreateDevice_pfn
        IDirectInput7A_CreateDevice_Original              = nullptr;

IDirectInputDevice7A_GetDeviceState_pfn
        IDirectInputDevice7A_GetDeviceState_Original      = nullptr;

IDirectInputDevice7A_SetCooperativeLevel_pfn
        IDirectInputDevice7A_SetCooperativeLevel_Original = nullptr;

HRESULT
WINAPI
IDirectInput7A_CreateDevice_Detour ( IDirectInput7A        *This,
                                     REFGUID                rguid,
                                     LPDIRECTINPUTDEVICE7A *lplpDirectInputDevice,
                                     LPUNKNOWN              pUnkOuter );

HRESULT
WINAPI
IDirectInput7W_CreateDevice_Detour ( IDirectInput7W        *This,
                                     REFGUID                rguid,
                                     LPDIRECTINPUTDEVICE7W *lplpDirectInputDevice,
                                     LPUNKNOWN              pUnkOuter );

volatile LONG __di7_ready = FALSE;

void
WINAPI
WaitForInit_DI7 (void)
{
  while (! ReadAcquire (&__di7_ready))
    MsgWaitForMultipleObjectsEx (0, nullptr, 2UL, QS_ALLINPUT, MWMO_INPUTAVAILABLE);
}


void
WINAPI
SK_BootDI7 (void);

__declspec (noinline)
HRESULT
WINAPI
DirectInputCreateEx ( HINSTANCE hinst,
                      DWORD     dwVersion,
                      REFIID    riidltf,
                      LPVOID   *ppvOut,
                      LPUNKNOWN punkOuter )
{
           SK_BootDI7 ();
      WaitForInit_DI7 ();
  

  dll_log->Log ( L"[ DInput 7 ] [!] %s (%08" _L(PRIxPTR) L"h, %lu, {...}, ppvOut="
                                      L"%08" _L(PRIxPTR) L"h, %08" _L(PRIxPTR) L"h) - "
                 L"%s",
                   L"DirectInputCreate",
                     (uintptr_t)hinst,             dwVersion, /*,*/
                     (uintptr_t)ppvOut, (uintptr_t)punkOuter,
                       SK_SummarizeCaller ().c_str () );

  HRESULT hr = E_NOINTERFACE;

  if (DirectInputCreateEx_Import != nullptr)
  {
    if (riidltf == IID_IDirectInput7A)
    {
      if ( SUCCEEDED (
             (hr = DirectInputCreateEx_Import ( hinst, dwVersion, riidltf,
                                                ppvOut, punkOuter )
             )
           ) && ppvOut != nullptr
         )
      {
        if (! IDirectInput7A_CreateDevice_Original)
        {
          void** vftable = *(void***)*ppvOut;

          SK_CreateFuncHook (       L"IDirectInput7A::CreateDevice",
                                     vftable [3],
                                     IDirectInput7A_CreateDevice_Detour,
            static_cast_p2p <void> (&IDirectInput7A_CreateDevice_Original) );

          SK_EnableHook (vftable [3]);
        }
      }
    }

    else if (riidltf == IID_IDirectInput7W)
    {
      if ( SUCCEEDED (
             (hr = DirectInputCreateEx_Import ( hinst, dwVersion, riidltf,
                                                ppvOut, punkOuter )
             )
           ) && ppvOut != nullptr
         )
      {
        if (! IDirectInput7W_CreateDevice_Original)
        {
          void** vftable = *(void***)*ppvOut;

          SK_CreateFuncHook (       L"IDirectInput7W::CreateDevice",
                                     vftable [3],
                                     IDirectInput7W_CreateDevice_Detour,
            static_cast_p2p <void> (&IDirectInput7W_CreateDevice_Original) );

          SK_EnableHook (vftable [3]);
        }
      }
    }
  }

  return hr;
}

__declspec (noinline)
HRESULT
WINAPI
DirectInputCreateA ( HINSTANCE       hinst,
                     DWORD           dwVersion,
                     LPDIRECTINPUTA* lplpDirectInput,
                     LPUNKNOWN       punkOuter )
{
           SK_BootDI7 ();
      WaitForInit_DI7 ();

  dll_log->Log ( L"[ DInput 7 ] [!] %s (%08" _L(PRIxPTR) L"h, %lu, {...}, "
                      L"lplpDirectInput=%08" _L(PRIxPTR) L"h, %08" _L(PRIxPTR)
                 L"h) - %s",
                   L"DirectInputCreateA",
                     (uintptr_t)hinst,                      dwVersion, /*,*/
                     (uintptr_t)lplpDirectInput, (uintptr_t)punkOuter,
                       SK_SummarizeCaller ().c_str () );

  HRESULT hr = E_NOINTERFACE;

  if (DirectInputCreateA_Import != nullptr)
  {
    if ( SUCCEEDED (
           (hr = DirectInputCreateA_Import ( hinst,           dwVersion,
                                             lplpDirectInput, punkOuter )
           )
         ) && lplpDirectInput != nullptr
       )
    {
      if (! IDirectInput7A_CreateDevice_Original)
      {
        void** vftable = *(void***)*lplpDirectInput;

        SK_CreateFuncHook (       L"IDirectInput7A::CreateDevice",
                                   vftable [3],
                                   IDirectInput7A_CreateDevice_Detour,
          static_cast_p2p <void> (&IDirectInput7A_CreateDevice_Original) );

        SK_EnableHook (vftable [3]);
      }
    }
  }

  return hr;
}

__declspec (noinline)
HRESULT
WINAPI
DirectInputCreateW ( HINSTANCE       hinst,
                     DWORD           dwVersion,
                     LPDIRECTINPUTW* lplpDirectInput,
                     LPUNKNOWN       punkOuter )
{
           SK_BootDI7 ();
      WaitForInit_DI7 ();

  dll_log->Log ( L"[ DInput 7 ] [!] %s (%08" _L(PRIxPTR) L"h, %lu, {...}, "
                      L"lplpDirectInput=%08" _L(PRIxPTR) L"h, %08" _L(PRIxPTR)
                 L"h) - %s",
                   L"DirectInputCreateW",
                     (uintptr_t)hinst,                      dwVersion, /*,*/
                     (uintptr_t)lplpDirectInput, (uintptr_t)punkOuter,
                       SK_SummarizeCaller ().c_str () );

  HRESULT hr = E_NOINTERFACE;

  if (DirectInputCreateW_Import != nullptr)
  {
    if ( SUCCEEDED (
           (hr = DirectInputCreateW_Import ( hinst,           dwVersion,
                                             lplpDirectInput, punkOuter )
           )
         ) && lplpDirectInput != nullptr
       )
    {
      if (! IDirectInput7W_CreateDevice_Original)
      {
        void** vftable = *(void***)*lplpDirectInput;

        SK_CreateFuncHook (       L"IDirectInput7W::CreateDevice",
                                   vftable [3],
                                   IDirectInput7W_CreateDevice_Detour,
          static_cast_p2p <void> (&IDirectInput7W_CreateDevice_Original) );

        SK_EnableHook (vftable [3]);
      }
    }
  }

  return hr;
}

void
WINAPI
SK_BootDI7 (void)
{
  static volatile LONG hooked = FALSE;

  if (! InterlockedCompareExchange (&hooked, TRUE, FALSE))
  {
    if (DirectInputCreateEx_Import == nullptr)
    {
      HMODULE hBackend =
        //(SK_GetDLLRole () & DLL_ROLE::DInput7) ? backend_dll :
                                        SK_Modules->LoadLibraryLL (L"dinput.dll");

      dll_log->Log (L"[ DInput 7 ] Importing DirectInputCreate....");
      dll_log->Log (L"[ DInput 7 ] ===============================");

      if (! _wcsicmp (SK_GetModuleName (SK_GetDLL ()).c_str (), L"dinput.dll"))
      {
        if ( MH_OK ==
                SK_CreateDLLHook2 (      L"dinput.dll",
                                          "DirectInputCreateEx",
                                           DirectInputCreateEx,
                  static_cast_p2p <void> (&DirectInputCreateEx_Import) )
           )
        {
          SK_CreateDLLHook2 (      L"dinput.dll",
                                    "DirectInputCreateA",
                                     DirectInputCreateA,
            static_cast_p2p <void> (&DirectInputCreateA_Import) );

          SK_CreateDLLHook2 (      L"dinput.dll",
                                    "DirectInputCreateW",
                                     DirectInputCreateW,
            static_cast_p2p <void> (&DirectInputCreateW_Import) );

          dll_log->Log (L"[ DInput 7 ]   DirectInputCreateEx: %08" _L(PRIxPTR)
                                  L"h  { Hooked }",
            (uintptr_t)(DirectInputCreateEx_Import) );
          dll_log->Log (L"[ DInput 7 ]   DirectInputCreateA:  %08" _L(PRIxPTR)
                                  L"h  { Hooked }",
            (uintptr_t)(DirectInputCreateA_Import) );
          dll_log->Log (L"[ DInput 7 ]   DirectInputCreateW:  %08" _L(PRIxPTR)
                                  L"h  { Hooked }",
            (uintptr_t)(DirectInputCreateW_Import) );
        }
      }

      else if (hBackend != nullptr)
      {
        if ( MH_OK ==
               SK_CreateDLLHook2 ( L"dinput.dll",
                                    "DirectInputCreateEx",
                                     DirectInputCreateEx,
             static_cast_p2p <void>(&DirectInputCreateEx_Import) )
           )
        {
          SK_CreateDLLHook2 (      L"dinput.dll",
                                    "DirectInputCreateA",
                                     DirectInputCreateA,
            static_cast_p2p <void> (&DirectInputCreateA_Import) );

          SK_CreateDLLHook2 (      L"dinput.dll",
                                    "DirectInputCreateW",
                                     DirectInputCreateW,
            static_cast_p2p <void> (&DirectInputCreateW_Import) );

          dll_log->Log (L"[ DInput 7 ]   DirectInputCreateEx: %08" _L(PRIxPTR)
                                  L"h  { Hooked }",
            (uintptr_t)(DirectInputCreateEx_Import) );
          dll_log->Log (L"[ DInput 7 ]   DirectInputCreateA:  %08" _L(PRIxPTR)
                                  L"h  { Hooked }",
            (uintptr_t)(DirectInputCreateA_Import) );
          dll_log->Log (L"[ DInput 7 ]   DirectInputCreateW:  %08" _L(PRIxPTR)
                                  L"h  { Hooked }",
            (uintptr_t)(DirectInputCreateW_Import) );
        }
      }
    }


    //
    // This whole thing is as smart as a sack of wet mice in DirectInput mode...
    //   let's get to the real work and start booting graphics APIs!
    //
    static bool gl   = false, vulkan = false, d3d9  = false, d3d11 = false, d3d12 = false,
                dxgi = false, d3d8   = false, ddraw = false, glide = false;

    SK_TestRenderImports (
      SK_GetModuleHandle (nullptr),
        &gl, &vulkan,
          &d3d9, &dxgi, &d3d11, &d3d12,
            &d3d8, &ddraw, &glide
    );


    // Load user-defined DLLs (Plug-In)
    SK_RunLHIfBitness (64, SK_LoadEarlyImports64 (), SK_LoadEarlyImports32 ());


//#define SPAWN_THREAD
#ifdef SPAWN_THREAD
CreateThread (nullptr, 0x00, [](LPVOID/*user*/) -> DWORD
{
UNREFERENCED_PARAMETER (user);
#endif

    // OpenGL
    //
    if (gl || (SK_IsModuleLoaded (L"OpenGL32.dll") && !SK_IsModuleLoaded (L"EOSOVH-Win64-Shipping.dll")))
      SK_BootOpenGL ();


    // Vulkan
    //
    //if (vulkan && GetModuleHandle (L"Vulkan-1.dll"))
    //  SK_BootVulkan ();


    // D3D9
    //
    if (d3d9 || SK_GetModuleHandle (L"d3d9.dll"))
      SK_BootD3D9 ();


    // D3D11
    //
    if (d3d11 || SK_GetModuleHandle (L"d3d11.dll"))
      SK_BootDXGI ();


    // D3D12
    //
    if (d3d12 || SK_GetModuleHandle (L"d3d12.dll"))
      SK_BootDXGI ();

    // Alternate form (or D3D10, but we don't care about that right now)
    else if (dxgi || SK_GetModuleHandle (L"dxgi.dll"))
      SK_BootDXGI ();


    // Load user-defined DLLs (Plug-In)
    SK_RunLHIfBitness (64, SK_LoadPlugIns64 (), SK_LoadPlugIns32 ());

#ifdef SPAWN_THREAD
  SK_CloseHandle (GetCurrentThread ());

  return 0;
}, nullptr, 0x00, nullptr);
#endif

    InterlockedIncrement (&hooked);
    InterlockedExchange  (&__di7_ready, TRUE);
  }

  while (ReadAcquire (&hooked) < 2)
    ;
}

DEFINE_GUID(CLSID_DirectInput,        0x25E609E0,0xB259,0x11CF,0xBF,0xC7,\
                                            0x44,0x45,0x53,0x54,0x00,0x00);
DEFINE_GUID(CLSID_DirectInputDevice,  0x25E609E1,0xB259,0x11CF,0xBF,0xC7,\
                                            0x44,0x45,0x53,0x54,0x00,0x00);

HRESULT
WINAPI
CoCreateInstance_DI7 (
  _In_  LPUNKNOWN pUnkOuter,
  _In_  DWORD     dwClsContext,
  _In_  REFIID    riid,
  _Out_ LPVOID   *ppv,
  _In_  LPVOID    pCallerAddr )
{
           SK_BootDI7 ();
      WaitForInit_DI7 ();

  dll_log->Log ( L"[ DInput 7 ] [!] %s (%08" _L(PRIxPTR) L"h, %lu, {...}, "
                               L"ppvOut=%08" _L(PRIxPTR) L"h, %08" _L(PRIxPTR) L"h) - "
                 L"%s",
                   L"DirectInputCreate <CoCreateInstance> ",
                     0, 0x700,
                     (uintptr_t)ppv, (uintptr_t)pUnkOuter,
                       SK_SummarizeCaller (pCallerAddr).c_str () );

  HRESULT hr =
    E_NOINTERFACE;

  if (riid == IID_IDirectInput7A)
  {
    if ( SUCCEEDED (
           (hr = CoCreateInstance_Original ( CLSID_DirectInput, pUnkOuter,
                                               dwClsContext, riid, ppv )
           )
         ) && config.input.gamepad.hook_dinput7 && ppv != nullptr
       )
    {
      if (! IDirectInput7A_CreateDevice_Original)
      {
        void** vftable = *(void***)*ppv;

        SK_CreateFuncHook (       L"IDirectInput7A::CreateDevice",
                                   vftable [3],
                                   IDirectInput7A_CreateDevice_Detour,
          static_cast_p2p <void> (&IDirectInput7A_CreateDevice_Original) );

        SK_EnableHook (vftable [3]);
      }
    }
  }

  else if (riid == IID_IDirectInput7W)
  {
    if ( SUCCEEDED (
           (hr = CoCreateInstance_Original ( CLSID_DirectInput, pUnkOuter,
                                               dwClsContext, riid, ppv )
           )
         ) && config.input.gamepad.hook_dinput7 && ppv != nullptr
       )
    {
      if (! IDirectInput7W_CreateDevice_Original)
      {
        void** vftable = *(void***)*ppv;

        SK_CreateFuncHook (       L"IDirectInput7W::CreateDevice",
                                   vftable [3],
                                   IDirectInput7W_CreateDevice_Detour,
          static_cast_p2p <void> (&IDirectInput7W_CreateDevice_Original) );

        SK_EnableHook (vftable [3]);
      }
    }
  }

  return hr;
}

HRESULT
STDAPICALLTYPE
CoCreateInstanceEx_DI7 (
  _In_    REFCLSID     rclsid,
  _In_    IUnknown     *pUnkOuter,
  _In_    DWORD        dwClsCtx,
  _In_    COSERVERINFO *pServerInfo,
  _In_    DWORD        dwCount,
  _Inout_ MULTI_QI     *pResults,
  _In_    LPVOID        pCallerAddr )
{
           SK_BootDI7 ();
      WaitForInit_DI7 ();

  dll_log->Log ( L"[ DInput 7 ] [!] %s (%08" _L(PRIxPTR) L"h, %lu, {...}, "
                               L"ppvOut=%08" _L(PRIxPTR) L"h) - "
                 L"%s",
                   L"DirectInputCreate <CoCreateInstanceEx> ",
                     0, 0x700,
                     (uintptr_t)pResults->pItf,
                       SK_SummarizeCaller (pCallerAddr).c_str () );

  HRESULT hr =
    E_NOINTERFACE;

  if (rclsid == CLSID_DirectInput)
  {
    if ( SUCCEEDED (
           (hr = CoCreateInstanceEx_Original ( CLSID_DirectInput, pUnkOuter,
                                                 dwClsCtx, pServerInfo,
                                                   dwCount, pResults )
           )
         ) && config.input.gamepad.hook_dinput7 && pResults != nullptr
       )
    {
      if (SUCCEEDED (pResults->hr) && pResults->pItf != nullptr)
      {
        if (*pResults->pIID == IID_IDirectInput7A)
        {
          if (! IDirectInput7A_CreateDevice_Original)
          {
            void** vftable = *(void***)*&pResults->pItf;

            SK_CreateFuncHook (       L"IDirectInput7A::CreateDevice",
                                       vftable [3],
                                       IDirectInput7A_CreateDevice_Detour,
              static_cast_p2p <void> (&IDirectInput7A_CreateDevice_Original) );

            SK_EnableHook (vftable [3]);
          }
        }

        else if (*pResults->pIID == IID_IDirectInput7W)
        {
          if (! IDirectInput7W_CreateDevice_Original)
          {
            void** vftable = *(void***)*&pResults->pItf;

            SK_CreateFuncHook (       L"IDirectInput7W::CreateDevice",
                                       vftable [3],
                                       IDirectInput7W_CreateDevice_Detour,
              static_cast_p2p <void> (&IDirectInput7W_CreateDevice_Original) );

            SK_EnableHook (vftable [3]);
          }
        }
      }
    }
  }

  return hr;
}

unsigned int
__stdcall
SK_HookDI7 (LPVOID user)
{
  UNREFERENCED_PARAMETER (user);

  SK_AutoCOMInit auto_com;
  {
    SK_BootDI7 ();

    if (! (SK_GetDLLRole () & DLL_ROLE::DXGI))
      SK::DXGI::StartBudgetThread_NoAdapter ();
  }

  return 0;
}

void
WINAPI
di7_init_callback (finish_pfn finish)
{
  if (! SK_IsHostAppSKIM ())
  {
             SK_HookDI7 (nullptr);

        WaitForInit_DI7 ();
  }

  finish ();
}


bool
SK::DI7::Startup (void)
{
  return SK_StartupCore (L"dinput", di7_init_callback);
}

bool
SK::DI7::Shutdown (void)
{
  return SK_ShutdownCore (L"dinput");
}


SK_LazyGlobal <SK_DI7_Keyboard> _dik7;
SK_LazyGlobal <SK_DI7_Mouse>    _dim7;


__declspec (noinline)
SK_DI7_Keyboard*
WINAPI
SK_Input_GetDI7Keyboard (void)
{
  return _dik7.getPtr ();
}

__declspec (noinline)
SK_DI7_Mouse*
WINAPI
SK_Input_GetDI7Mouse (void)
{
  return _dim7.getPtr ();
}

__declspec (noinline)
bool
WINAPI
SK_Input_DI7Mouse_Acquire (SK_DI7_Mouse* pMouse)
{
  if (pMouse == nullptr && _dim7->pDev != nullptr)
    pMouse = _dim7.getPtr ();

  if (pMouse != nullptr)
  {
    if (IDirectInputDevice7W_SetCooperativeLevel_Original)
    {
      IDirectInputDevice7W_SetCooperativeLevel_Original (
        pMouse->pDev,
          game_window.hWnd,
            pMouse->coop_level
      );
    }

    else
    {
      IDirectInputDevice7A_SetCooperativeLevel_Original (
        (IDirectInputDevice7A *)pMouse->pDev,
          game_window.hWnd,
            pMouse->coop_level
      );
    }

    return true;
  }

  return false;
}

__declspec (noinline)
bool
WINAPI
SK_Input_DI7Mouse_Release (SK_DI7_Mouse* pMouse)
{
  if (pMouse == nullptr && _dim7->pDev != nullptr)
    pMouse = _dim7.getPtr ();

  if (pMouse != nullptr)
  {
    if (IDirectInputDevice7W_SetCooperativeLevel_Original)
    {
      IDirectInputDevice7W_SetCooperativeLevel_Original (
        pMouse->pDev,
          game_window.hWnd,
            (pMouse->coop_level & (~DISCL_EXCLUSIVE)) | DISCL_NONEXCLUSIVE
      );
    }

    else
    {
      IDirectInputDevice7A_SetCooperativeLevel_Original (
        (IDirectInputDevice7A *)pMouse->pDev,
          game_window.hWnd,
            (pMouse->coop_level & (~DISCL_EXCLUSIVE)) | DISCL_NONEXCLUSIVE
      );
    }

    return true;
  }

  return false;
}


HRESULT
WINAPI
IDirectInputDevice7_GetDeviceState_Detour ( LPDIRECTINPUTDEVICE7       This,
                                            DWORD                      cbData,
                                            LPVOID                     lpvData )
{
  SK_LOG_FIRST_CALL

  SK_LOG4 ( ( L" DirectInput 7 - GetDeviceState: cbData = %lu",
                cbData ),
              __SK_SUBSYSTEM__ );

  HRESULT hr = S_OK;

  if (lpvData != nullptr)
  {
    if (cbData == sizeof DIJOYSTATE2)
    {
      SK_DI7_READ (sk_input_dev_type::Gamepad)
      static DIJOYSTATE2 last_state;

      auto* out =
        static_cast <DIJOYSTATE2 *> (lpvData);

      ///////////////////////SK_DI8_TranslateToXInput (reinterpret_cast <DIJOYSTATE *> (out));

      bool disabled_to_game =
        ( config.input.gamepad.disabled_to_game == SK_InputEnablement::Disabled ||
          SK_ImGui_WantGamepadCapture ()           );

      if (disabled_to_game)
      {
        memcpy (out, &last_state, cbData);

        out->rgdwPOV [0] = std::numeric_limits <DWORD>::max ();
        out->rgdwPOV [1] = std::numeric_limits <DWORD>::max ();
        out->rgdwPOV [2] = std::numeric_limits <DWORD>::max ();
        out->rgdwPOV [3] = std::numeric_limits <DWORD>::max ();
      } else
        memcpy (&last_state, out, cbData);
    }

    else if (cbData == sizeof DIJOYSTATE)
    {
      SK_DI7_READ (sk_input_dev_type::Gamepad)

      //dll_log.Log (L"Joy");

      static DIJOYSTATE last_state;

      auto* out =
        static_cast <DIJOYSTATE *> (lpvData);

      ///////////////////////SK_DI8_TranslateToXInput (out);

      bool disabled_to_game =
        ( config.input.gamepad.disabled_to_game ||
          SK_ImGui_WantGamepadCapture ()           );

      if (disabled_to_game)
      {
        memcpy (out, &last_state, cbData);

        out->rgdwPOV [0] = std::numeric_limits <DWORD>::max ();
        out->rgdwPOV [1] = std::numeric_limits <DWORD>::max ();
        out->rgdwPOV [2] = std::numeric_limits <DWORD>::max ();
        out->rgdwPOV [3] = std::numeric_limits <DWORD>::max ();
      }
      else
        memcpy (&last_state, out, cbData);

#if 0
      XINPUT_STATE xis;
      SK_XInput_PollController (0, &xis);

      out->rgbButtons [ 9] = (( xis.Gamepad.wButtons & XINPUT_GAMEPAD_START          ) != 0x0 ? 0xFF : 0x00);
      out->rgbButtons [ 8] = (( xis.Gamepad.wButtons & XINPUT_GAMEPAD_BACK           ) != 0x0 ? 0xFF : 0x00);
      out->rgbButtons [10] = (( xis.Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_THUMB     ) != 0x0 ? 0xFF : 0x00);
      out->rgbButtons [11] = (( xis.Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_THUMB    ) != 0x0 ? 0xFF : 0x00);
      out->rgbButtons [ 6] = (( xis.Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_TRIGGER   ) != 0x0 ? 0xFF : 0x00);
      out->rgbButtons [ 7] = (( xis.Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_TRIGGER  ) != 0x0 ? 0xFF : 0x00);
      out->rgbButtons [ 4] = (( xis.Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER  ) != 0x0 ? 0xFF : 0x00);
      out->rgbButtons [ 5] = (( xis.Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER ) != 0x0 ? 0xFF : 0x00);
      out->rgbButtons [ 1] = (( xis.Gamepad.wButtons & XINPUT_GAMEPAD_A              ) != 0x0 ? 0xFF : 0x00);
      out->rgbButtons [ 2] = (( xis.Gamepad.wButtons & XINPUT_GAMEPAD_B              ) != 0x0 ? 0xFF : 0x00);
      out->rgbButtons [ 0] = (( xis.Gamepad.wButtons & XINPUT_GAMEPAD_X              ) != 0x0 ? 0xFF : 0x00);
      out->rgbButtons [ 3] = (( xis.Gamepad.wButtons & XINPUT_GAMEPAD_Y              ) != 0x0 ? 0xFF : 0x00);

      out->rgdwPOV [0] += (( xis.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_UP           ) != 0x0 ?      0 : 0);
      out->rgdwPOV [0] += (( xis.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT        ) != 0x0 ?  90000 : 0);
      out->rgdwPOV [0] += (( xis.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_DOWN         ) != 0x0 ? 180000 : 0);
      out->rgdwPOV [0] += (( xis.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_LEFT         ) != 0x0 ? 270000 : 0);

      if (out->rgdwPOV [0] == 0)
        out->rgdwPOV [0] = -1;
#endif
    }

    else if (This == _dik7->pDev || cbData == 256)
    {
      SK_DI7_READ (sk_input_dev_type::Keyboard)

      if (SK_ImGui_WantKeyboardCapture ())
      {
        memset (lpvData, 0, cbData);

        SK_DI7_HIDE (sk_input_dev_type::Keyboard)
      }

      else
        SK_DI7_VIEW (sk_input_dev_type::Keyboard)
    }

    else if ( cbData == sizeof (DIMOUSESTATE2) ||
              cbData == sizeof (DIMOUSESTATE)  )
    {
      SK_DI7_READ (sk_input_dev_type::Mouse)

      if (SK_ImGui_WantMouseCapture ())
      {
        SK_DI7_HIDE (sk_input_dev_type::Mouse)

        switch (cbData)
        {
          case sizeof (DIMOUSESTATE2):
            static_cast <DIMOUSESTATE2 *> (lpvData)->lX = 0;
            static_cast <DIMOUSESTATE2 *> (lpvData)->lY = 0;
            static_cast <DIMOUSESTATE2 *> (lpvData)->lZ = 0;
            memset (static_cast <DIMOUSESTATE2 *> (lpvData)->rgbButtons, 0, 8);
            break;

          case sizeof (DIMOUSESTATE):
            static_cast <DIMOUSESTATE *> (lpvData)->lX = 0;
            static_cast <DIMOUSESTATE *> (lpvData)->lY = 0;
            static_cast <DIMOUSESTATE *> (lpvData)->lZ = 0;
            memset (static_cast <DIMOUSESTATE *> (lpvData)->rgbButtons, 0, 4);
            break;
        }
      }

      else
      {
        SK_DI7_VIEW (sk_input_dev_type::Mouse)
      }
    }
  }

  return hr;
}

bool
SK_DInput7_HasKeyboard (void)
{
  return (_dik7->pDev && ( IDirectInputDevice7A_SetCooperativeLevel_Original ||
                           IDirectInputDevice7W_SetCooperativeLevel_Original ) );
}

bool
SK_DInput7_BlockWindowsKey (bool block)
{
  DWORD dwFlags =
    block ? DISCL_NOWINKEY : 0x0;

  dwFlags &= ~DISCL_EXCLUSIVE;
  dwFlags &= ~DISCL_BACKGROUND;

  dwFlags |= DISCL_NONEXCLUSIVE;
  dwFlags |= DISCL_FOREGROUND;

  if (SK_DInput7_HasKeyboard ())
  {
    if (IDirectInputDevice7W_SetCooperativeLevel_Original)
      IDirectInputDevice7W_SetCooperativeLevel_Original (                        _dik7->pDev, game_window.hWnd, dwFlags);
    else
      IDirectInputDevice7A_SetCooperativeLevel_Original ((IDirectInputDevice7A *)_dik7->pDev, game_window.hWnd, dwFlags);
  }
  else
    return false;

  return block;
}

bool
SK_DInput7_HasMouse (void)
{
  return (_dim7->pDev && ( IDirectInputDevice7A_SetCooperativeLevel_Original ||
                           IDirectInputDevice7W_SetCooperativeLevel_Original ) );
}

HRESULT
WINAPI
IDirectInputDevice7A_GetDeviceState_Detour ( LPDIRECTINPUTDEVICE7A      This,
                                             DWORD                      cbData,
                                             LPVOID                     lpvData )
{
  HRESULT hr =
    IDirectInputDevice7A_GetDeviceState_Original ( This, cbData, lpvData );

  if (SUCCEEDED (hr))
    IDirectInputDevice7_GetDeviceState_Detour ( (LPDIRECTINPUTDEVICE7)This, cbData, lpvData );

  return hr;
}

HRESULT
WINAPI
IDirectInputDevice7W_GetDeviceState_Detour ( LPDIRECTINPUTDEVICE7W      This,
                                             DWORD                      cbData,
                                             LPVOID                     lpvData )
{
  HRESULT hr =
    IDirectInputDevice7W_GetDeviceState_Original ( This, cbData, lpvData );

  if (SUCCEEDED (hr))
    IDirectInputDevice7_GetDeviceState_Detour ( This, cbData, lpvData );

  return hr;
}



HRESULT
WINAPI
IDirectInputDevice7W_SetCooperativeLevel_Detour ( LPDIRECTINPUTDEVICE7W This,
                                                  HWND                  hwnd,
                                                  DWORD                 dwFlags )
{
  if (config.input.keyboard.block_windows_key)
    dwFlags |= DISCL_NOWINKEY;

  HRESULT hr =
    IDirectInputDevice7W_SetCooperativeLevel_Original (This, hwnd, dwFlags);

  if (SUCCEEDED (hr))
  {
    // Mouse
    if (This == _dim7->pDev)
      _dim7->coop_level = dwFlags;

    // Keyboard   (why do people use DirectInput for keyboards? :-\)
    else if (This == _dik7->pDev)
      _dik7->coop_level = dwFlags;

    // Anything else is probably not important
  }

  if (SK_ImGui_WantMouseCapture ())
  {
    dwFlags &= ~DISCL_EXCLUSIVE;

    IDirectInputDevice7W_SetCooperativeLevel_Original (This, hwnd, dwFlags);
  }

  return hr;
}

HRESULT
WINAPI
IDirectInputDevice7A_SetCooperativeLevel_Detour ( LPDIRECTINPUTDEVICE7A This,
                                                  HWND                  hwnd,
                                                  DWORD                 dwFlags )
{
  if (config.input.keyboard.block_windows_key)
    dwFlags |= DISCL_NOWINKEY;

  HRESULT hr =
    IDirectInputDevice7A_SetCooperativeLevel_Original (This, hwnd, dwFlags);

  if (SUCCEEDED (hr))
  {
    // Mouse
    if (This == (IDirectInputDevice7A *)_dim7->pDev)
      _dim7->coop_level = dwFlags;

    // Keyboard   (why do people use DirectInput for keyboards? :-\)
    else if (This == (IDirectInputDevice7A *)_dik7->pDev)
      _dik7->coop_level = dwFlags;

    // Anything else is probably not important
  }

  if (SK_ImGui_WantMouseCapture ())
  {
    dwFlags &= ~DISCL_EXCLUSIVE;

    IDirectInputDevice7A_SetCooperativeLevel_Original (This, hwnd, dwFlags);
  }

  return hr;
}


static SK_LazyGlobal <concurrency::concurrent_unordered_map <uint32_t, LPDIRECTINPUTDEVICE7W>> devices7_w;
static SK_LazyGlobal <concurrency::concurrent_unordered_map <uint32_t, LPDIRECTINPUTDEVICE7A>> devices7_a;

HRESULT
WINAPI
IDirectInput7W_CreateDevice_Detour ( IDirectInput7W        *This,
                                     REFGUID                rguid,
                                     LPDIRECTINPUTDEVICE7W *lplpDirectInputDevice,
                                     LPUNKNOWN              pUnkOuter )
{
  uint32_t guid_crc32c = crc32c (0, &rguid, sizeof (REFGUID));

  const wchar_t* wszDevice = (rguid == GUID_SysKeyboard)   ? L"Default System Keyboard" :
                                (rguid == GUID_SysMouse)   ? L"Default System Mouse"    :
                                  (rguid == GUID_Joystick) ? L"Gamepad / Joystick"      :
                                                             L"Other Device";

  if (devices7_w.get ().count (guid_crc32c))
  {
    *lplpDirectInputDevice = devices7_w [guid_crc32c];
                             devices7_w [guid_crc32c]->AddRef ();
    return S_OK;
  }

  if (config.system.log_level > 1)
  {
    dll_log->Log ( L"[   Input  ] [!] IDirectInput7W::CreateDevice (%08" _L(PRIxPTR) L"h, %s, "
                                                                  L"%08" _L(PRIxPTR) L"h, "
                                                                  L"%08" _L(PRIxPTR) L"h)",
                      (uintptr_t)This,
                                   wszDevice,
                          (uintptr_t)lplpDirectInputDevice,
                            (uintptr_t)pUnkOuter );
  }
  else
  {
    dll_log->Log ( L"[   Input  ] [!] IDirectInput7W::CreateDevice         [ %24s ]",
                        wszDevice );
  }

  HRESULT hr;
  DINPUT7_CALL ( hr,
                  IDirectInput7W_CreateDevice_Original ( This,
                                                          rguid,
                                                           lplpDirectInputDevice,
                                                            pUnkOuter ) );

  if (SUCCEEDED (hr) && (! IDirectInputDevice7A_GetDeviceState_Original))
  {
    void** vftable =
      *reinterpret_cast <void ***> (*lplpDirectInputDevice);

    if (IDirectInputDevice7W_GetDeviceState_Original == nullptr)
    {
      SK_CreateFuncHook (      L"IDirectInputDevice7W::GetDeviceState",
                                 vftable [9],
                                 IDirectInputDevice7W_GetDeviceState_Detour,
        static_cast_p2p <void> (&IDirectInputDevice7W_GetDeviceState_Original) );
      MH_EnableHook (vftable [9]);
    }

    if (! IDirectInputDevice7W_SetCooperativeLevel_Original)
    {
      SK_CreateFuncHook (      L"IDirectInputDevice7W::SetCooperativeLevel",
                                 vftable [13],
                                 IDirectInputDevice7W_SetCooperativeLevel_Detour,
        static_cast_p2p <void> (&IDirectInputDevice7W_SetCooperativeLevel_Original) );
      MH_EnableHook (vftable [13]);
    }

    if (rguid == GUID_SysMouse)
    {
      _dim7->pDev = *lplpDirectInputDevice;
    }
    else if (rguid == GUID_SysKeyboard)
      _dik7->pDev = *lplpDirectInputDevice;

    devices7_w [guid_crc32c] = *lplpDirectInputDevice;
    devices7_w [guid_crc32c]->AddRef ();

    //SK_ApplyQueuedHooks ();
  }

#if 0
  if (SUCCEEDED (hr) && lplpDirectInputDevice != nullptr)
  {
    DWORD dwFlag = DISCL_FOREGROUND | DISCL_NONEXCLUSIVE;

    if (config.input.block_windows)
      dwFlag |= DISCL_NOWINKEY;

    (*lplpDirectInputDevice)->SetCooperativeLevel (SK_GetGameWindow (), dwFlag);
  }
#endif

  return hr;
}

HRESULT
WINAPI
IDirectInput7A_CreateDevice_Detour ( IDirectInput7A        *This,
                                     REFGUID                rguid,
                                     LPDIRECTINPUTDEVICE7A *lplpDirectInputDevice,
                                     LPUNKNOWN              pUnkOuter )
{
  uint32_t guid_crc32c = crc32c (0, &rguid, sizeof (REFGUID));

  const wchar_t* wszDevice = (rguid == GUID_SysKeyboard)   ? L"Default System Keyboard" :
                                (rguid == GUID_SysMouse)   ? L"Default System Mouse"    :
                                  (rguid == GUID_Joystick) ? L"Gamepad / Joystick"      :
                                                             L"Other Device";

  if (devices7_a.get ().count (guid_crc32c))
  {
    *lplpDirectInputDevice = devices7_a [guid_crc32c];
                             devices7_a [guid_crc32c]->AddRef ();
    return S_OK;
  }


  if (config.system.log_level > 1)
  {
    dll_log->Log ( L"[   Input  ] [!] IDirectInput7A::CreateDevice (%08" _L(PRIxPTR) L"h, %s, "
                                                                  L"%08" _L(PRIxPTR) L"h, "
                                                                  L"%08" _L(PRIxPTR) L"h)",
                      (uintptr_t)This,
                                   wszDevice,
                          (uintptr_t)lplpDirectInputDevice,
                            (uintptr_t)pUnkOuter );
  }
  else
  {
    dll_log->Log ( L"[   Input  ] [!] IDirectInput7A::CreateDevice         [ %24s ]",
                        wszDevice );
  }


  HRESULT hr;
  DINPUT7_CALL ( hr,
                  IDirectInput7A_CreateDevice_Original ( This,
                                                          rguid,
                                                           lplpDirectInputDevice,
                                                            pUnkOuter ) );

  if (SUCCEEDED (hr) && (! IDirectInputDevice7W_GetDeviceState_Original))
  {
    void** vftable =
      *reinterpret_cast <void ***> (*lplpDirectInputDevice);

    if (IDirectInputDevice7A_GetDeviceState_Original == nullptr)
    {
      SK_CreateFuncHook (      L"IDirectInputDevice7A::GetDeviceState",
                                 vftable [9],
                                 IDirectInputDevice7A_GetDeviceState_Detour,
        static_cast_p2p <void> (&IDirectInputDevice7A_GetDeviceState_Original) );
      MH_EnableHook (vftable [9]);
    }

    if (! IDirectInputDevice7A_SetCooperativeLevel_Original)
    {
      SK_CreateFuncHook (      L"IDirectInputDevice7A::SetCooperativeLevel",
                                 vftable [13],
                                 IDirectInputDevice7A_SetCooperativeLevel_Detour,
        static_cast_p2p <void> (&IDirectInputDevice7A_SetCooperativeLevel_Original) );
      MH_EnableHook (vftable [13]);
    }

    if (rguid == GUID_SysMouse)
    {
      _dim7->pDev = *(LPDIRECTINPUTDEVICE7W *)lplpDirectInputDevice;
    }
    else if (rguid == GUID_SysKeyboard)
      _dik7->pDev = *(LPDIRECTINPUTDEVICE7W *)lplpDirectInputDevice;

    devices7_a [guid_crc32c] = *lplpDirectInputDevice;
    devices7_a [guid_crc32c]->AddRef ();

    ////SK_ApplyQueuedHooks ();
  }

#if 0
  if (SUCCEEDED (hr) && lplpDirectInputDevice != nullptr)
  {
    DWORD dwFlag = DISCL_FOREGROUND | DISCL_NONEXCLUSIVE;

    if (config.input.block_windows)
      dwFlag |= DISCL_NOWINKEY;

    (*lplpDirectInputDevice)->SetCooperativeLevel (SK_GetGameWindow (), dwFlag);
  }
#endif

  return hr;
}

void
SK_Input_HookDI7 (void)
{
  if (! config.input.gamepad.hook_dinput7)
    return;

  static volatile LONG hooked = FALSE;

  if (! SK_GetModuleHandle (L"dinput.dll"))
           SK_LoadLibraryW (L"dinput.dll");

  if (SK_GetModuleHandle (L"dinput.dll"))
  {
    if (! InterlockedCompareExchange (&hooked, TRUE, FALSE))
    {
      ///if (SK_GetDLLRole () & DLL_ROLE::DInput7)
      ///  return;
      /// 

      // DirectInput is layered on top of HID, so we should start
      //   by hooking HID.
      SK_Input_HookHID ();

      SK_LOG0 ( ( L"Game uses DirectInput 7, installing input hooks..." ),
                    L"  Input   " );

      //HMODULE hBackend =
      //  (SK_GetDLLRole () & DLL_ROLE::DInput8) ? backend_dll :
      //                                  SK_GetModuleHandle (L"dinput8.dll");

      if (SK_GetProcAddress (SK_GetModuleHandle (L"dinput.dll"), "DirectInputCreateEx"))
      {
        SK_CreateDLLHook2 (      L"dinput.dll",
                                  "DirectInputCreateEx",
                                   DirectInputCreateEx,
          static_cast_p2p <void> (&DirectInputCreateEx_Import) );

        SK_CreateDLLHook2 (      L"dinput.dll",
                                  "DirectInputCreateA",
                                   DirectInputCreateA,
          static_cast_p2p <void> (&DirectInputCreateA_Import) );

        SK_CreateDLLHook2 (      L"dinput.dll",
                                  "DirectInputCreateW",
                                   DirectInputCreateW,
          static_cast_p2p <void> (&DirectInputCreateW_Import) );
      }

      if (SK_GetModuleHandle (L"dinput8.dll"))
      {
        SK_Input_HookDI8 ();
      }

      InterlockedIncrementRelease (&hooked);
    }

    else
      SK_Thread_SpinUntilAtomicMin (&hooked, 2);
  }
}

void
SK_Input_PreHookDI7 (void)
{
  if (! config.input.gamepad.hook_dinput7)
    return;

  if (! InterlockedCompareExchangePointer ((void **)&DirectInputCreateEx_Import, (PVOID)1, nullptr))
  {
    static sk_import_test_s tests [] = { { "dinput.dll",  false },
                                         { "dinput8.dll", false } };

    SK_TestImports (SK_GetModuleHandle (nullptr), tests, 2);

    if (tests [1].used || SK_GetModuleHandle (L"dinput8.dll"))
    {              SK_Modules->LoadLibraryLL (L"dinput8.dll");

      if (SK_GetDLLRole () != DLL_ROLE::DInput8)
        SK_Input_PreHookDI8 ();
    }

    if (tests [0].used || SK_GetModuleHandle (L"dinput.dll"))
   {               SK_Modules->LoadLibraryLL (L"dinput.dll");

      //if (SK_GetDLLRole () != DLL_ROLE::DInput7)
        SK_Input_HookDI7 ();
    }

    else
    {
      InterlockedExchangePointer ((void **)&DirectInputCreateEx_Import, nullptr);
    }
  }
}

SK_LazyGlobal <sk_input_api_context_s> SK_DI7_Backend;