/**
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

#include <SpecialK/framerate.h>


#define ALLOW_UNDOCUMENTED


using NTSTATUS = _Return_type_success_(return >= 0) LONG;


HMODULE                    NtDll                  = nullptr;
NtQueryTimerResolution_pfn NtQueryTimerResolution = nullptr;
NtSetTimerResolution_pfn   NtSetTimerResolution   = nullptr;

NtDelayExecution_pfn       NtDelayExecution       = nullptr;


LPVOID pfnQueryPerformanceCounter = nullptr;
LPVOID pfnSleep                   = nullptr;

Sleep_pfn                   Sleep_Original                     = nullptr;
SleepEx_pfn                 SleepEx_Original                   = nullptr;
QueryPerformanceCounter_pfn QueryPerformanceCounter_Original   = nullptr;
QueryPerformanceCounter_pfn ZwQueryPerformanceCounter          = nullptr;
QueryPerformanceCounter_pfn RtlQueryPerformanceCounter         = nullptr;
QueryPerformanceCounter_pfn QueryPerformanceFrequency_Original = nullptr;
QueryPerformanceCounter_pfn RtlQueryPerformanceFrequency       = nullptr;


extern HWND WINAPI SK_GetActiveWindow (SK_TLS *pTLS);

void
SK_Thread_WaitWhilePumpingMessages (DWORD dwMilliseconds, SK_TLS *pTLS)
{
  ////if (! SK_Win32_IsGUIThread ())
  ////{
  ////  while (GetQueueStatus (QS_ALLEVENTS) == 0)
  ////  {
  ////    SK_Sleep (dwMilliseconds /= 2);
  ////
  ////    if (dwMilliseconds == 1)
  ////    {
  ////      SK_SleepEx (dwMilliseconds, TRUE);
  ////      return;
  ////    }
  ////  }
  ////}

  HWND hWndThis = SK_GetActiveWindow (pTLS);
  bool bUnicode =
    IsWindowUnicode (hWndThis);

  auto PeekAndDispatch =
  [&]
  {
    MSG msg     = {      };
    msg.message = WM_NULL ;

    // Avoid having Windows marshal Unicode messages like a dumb ass
    if (bUnicode)
    {
      if ( PeekMessageW ( &msg, 0, 0, 0,
                                            PM_REMOVE | QS_ALLINPUT)
               &&          msg.message != WM_NULL
         )
      {
        SK_LOG0 ( ( L"Dispatched Message: %x to Unicode HWND: %x while "
                    L"framerate limiting!", msg.message, msg.hwnd ),
                    L"Win32-Pump" );

        DispatchMessageW (&msg);
      }
    }

    else
    {
      if ( PeekMessageA ( &msg, 0, 0, 0,
                                            PM_REMOVE | QS_ALLINPUT)
               &&          msg.message != WM_NULL
         )
      {
        SK_LOG0 ( ( L"Dispatched Message: %x to ANSI HWND: %x while "
                    L"framerate limiting!", msg.message, msg.hwnd ),
                    L"Win32-Pump" );
        DispatchMessageA (&msg);
      }
    }
  };


  if (dwMilliseconds == 0)
  {
    return;
  }


  LARGE_INTEGER liStart      = SK_CurrentPerf ();
  long long     liTicksPerMS = SK_GetPerfFreq ().QuadPart / 1000LL;
  long long     liEnd        = liStart.QuadPart +
                             ( liTicksPerMS     * dwMilliseconds );

  LARGE_INTEGER liNow = liStart;

  while ((liNow = SK_CurrentPerf ()).QuadPart < liEnd)
  {
    DWORD dwMaxWait =
      narrow_cast <DWORD> (std::max (0LL, (liEnd - liNow.QuadPart) /
                                                   liTicksPerMS  ) );

    if (dwMaxWait < INT_MAX && dwMaxWait > 1)
    {
      dwMaxWait = std::min (150UL, dwMaxWait);

      DWORD dwWait =
        MsgWaitForMultipleObjectsEx (
          1, &__SK_DLL_TeardownEvent, dwMaxWait,
            QS_ALLINPUT, MWMO_ALERTABLE    |
                         MWMO_INPUTAVAILABLE
                                    );

      if ( dwWait == WAIT_OBJECT_0     ||
           dwWait == WAIT_TIMEOUT      ||
           dwWait == WAIT_IO_COMPLETION )
      {
        break;
      }

      // (dwWait == WAIT_OBJECT_0 + 1) { Message Waiting }

      else
      {
        PeekAndDispatch ();
      }
    }

    else
      break;
  }
}


bool fix_sleep_0 = false;


float
SK_Sched_ThreadContext::most_recent_wait_s::getRate (void)
{
  if (sequence > 0)
  {
    double ms =
      SK_DeltaPerfMS (
        SK_CurrentPerf ().QuadPart - (last_wait.QuadPart - start.QuadPart), 1
      );

    return
      static_cast <float> ( ms / long_double_cast (sequence) );
  }

  // Sequence just started
  return -1.0f;
}


NtWaitForSingleObject_pfn
NtWaitForSingleObject_Original    = nullptr;

NtWaitForMultipleObjects_pfn
NtWaitForMultipleObjects_Original = nullptr;


#define STATUS_SUCCESS                   ((NTSTATUS)0x00000000L) // ntsubauth
#define STATUS_ALERTED                   ((NTSTATUS)0x00000101L)

DWORD
WINAPI
SK_WaitForSingleObject_Micro ( _In_  HANDLE          hHandle,
                               _In_ PLARGE_INTEGER pliMicroseconds )
{
#ifdef ALLOW_UNDOCUMENTED
  if (NtWaitForSingleObject_Original != nullptr)
  {
    NTSTATUS NtStatus =
      NtWaitForSingleObject_Original (
        hHandle,
          FALSE,
            pliMicroseconds
      );

    switch (NtStatus)
    {
      case STATUS_SUCCESS:
        return WAIT_OBJECT_0;
      case STATUS_TIMEOUT:
        return WAIT_TIMEOUT;
      case STATUS_ALERTED:
        return WAIT_IO_COMPLETION;
      case STATUS_USER_APC:
        return WAIT_IO_COMPLETION;
    }

    // ???
    return
      WAIT_FAILED;
  }
#endif

  DWORD dwMilliseconds =
    ( pliMicroseconds == nullptr ?
              INFINITE : static_cast <DWORD> (
                           -pliMicroseconds->QuadPart / 10000LL
                         )
    );

  return
    WaitForSingleObject ( hHandle,
                            dwMilliseconds );
}

DWORD
WINAPI
SK_WaitForSingleObject (_In_ HANDLE hHandle,
                        _In_ DWORD  dwMilliseconds )
{
  if (dwMilliseconds == INFINITE)
  {
    return
      SK_WaitForSingleObject_Micro ( hHandle, nullptr );
  }

  LARGE_INTEGER usecs;
                usecs.QuadPart =
                  -static_cast <LONGLONG> (
                    dwMilliseconds
                  ) * 10000LL;

  return
    SK_WaitForSingleObject_Micro ( hHandle, &usecs );
}

NTSTATUS
NTAPI
NtWaitForMultipleObjects_Detour (
  IN ULONG                ObjectCount,
  IN PHANDLE              ObjectsArray,
  IN OBJECT_WAIT_TYPE     WaitType,
  IN BOOLEAN              Alertable,
  IN PLARGE_INTEGER       TimeOut OPTIONAL )
{
  // Unity spins a loop to signal its job semaphore
  if (TimeOut != nullptr && TimeOut->QuadPart == 0)
  {
    return
      NtWaitForMultipleObjects_Original (
           ObjectCount, ObjectsArray,
             WaitType, Alertable,
               TimeOut                  );
  }

  if (! ( SK_GetFramesDrawn () && SK_MMCS_GetTaskCount () > 1 ) )
  {
    return
      NtWaitForMultipleObjects_Original (
           ObjectCount, ObjectsArray,
             WaitType, Alertable,
               TimeOut                  );
  }

  SK_TLS *pTLS =
    SK_TLS_Bottom ();

  if ( pTLS->scheduler->mmcs_task > (SK_MMCS_TaskEntry *)1 )
  {
    //if (pTLS->scheduler->mmcs_task->hTask > 0)
    //    pTLS->scheduler->mmcs_task->disassociateWithTask ();
  }

  DWORD dwRet =
    NtWaitForMultipleObjects_Original (
         ObjectCount, ObjectsArray,
           WaitType, Alertable,
             TimeOut                  );

  if ( pTLS->scheduler->mmcs_task > (SK_MMCS_TaskEntry *)1 )
  {
    auto task =
      pTLS->scheduler->mmcs_task;

    //if (pTLS->scheduler->mmcs_task->hTask > 0)
    //  task->reassociateWithTask ();

    if (InterlockedCompareExchange (&task->change.pending, 0, 1))
    {
      task->setPriority            ( task->change.priority );

      ///if (_stricmp (task->change.task0, task->task0) ||
      ///    _stricmp (task->change.task1, task->task1))
      ///{
      ///  task->disassociateWithTask  ();
      ///  task->setMaxCharacteristics (task->change.task0, task->change.task1);
      ///}
    }
  }

  return dwRet;
}

// -------------------
// This code is largely obsolete, but will rate-limit
//   Unity's Asynchronous Procedure Calls if they are
//     ever shown to devestate performance like they
//       were in PoE2.
// -------------------
//

extern volatile LONG SK_POE2_Horses_Held;
extern volatile LONG SK_POE2_SMT_Assists;
extern volatile LONG SK_POE2_ThreadBoostsKilled;
extern          bool SK_POE2_FixUnityEmployment;
extern          bool SK_POE2_Stage2UnityFix;
extern          bool SK_POE2_Stage3UnityFix;

NTSTATUS
WINAPI
NtWaitForSingleObject_Detour (
  IN HANDLE         Handle,
  IN BOOLEAN        Alertable,
  IN PLARGE_INTEGER Timeout  )
{
  if (Timeout != nullptr && Timeout->QuadPart == 0)
  {
    return
      NtWaitForSingleObject_Original (
        Handle, Alertable, Timeout
      );
  }

#ifndef _UNITY_HACK
  if (! ( SK_GetFramesDrawn () && SK_MMCS_GetTaskCount () > 1 ) )
#else
  if (  ! SK_GetFramesDrawn () )
#endif
  {
    return
      NtWaitForSingleObject_Original (
        Handle, Alertable, Timeout
      );
  }

#ifdef _UNITY_HACK
  if (bAlertable)
    InterlockedIncrement (&pTLS->scheduler->alert_waits);

  // Consider double-buffering this since the information
  //   is used almost exclusively by OHTER threads, and
  //     we have to do a synchronous copy the get at this
  //       thing without thread A murdering thread B.
  SK_Sched_ThreadContext::wait_record_s& scheduled_wait =
    (*pTLS->scheduler->objects_waited) [hHandle];

  scheduled_wait.calls++;

  if (dwMilliseconds == INFINITE)
    scheduled_wait.time = 0;//+= dwMilliseconds;

  LARGE_INTEGER liStart =
    SK_QueryPerf ();

  bool same_as_last_time =
    ( pTLS->scheduler->mru_wait.handle == hHandle );

      pTLS->scheduler->mru_wait.handle = hHandle;

  auto ret =
    NtWaitForSingleObject_Original (
      Handle, Alertable, Timeout
    );

  InterlockedAdd ( &scheduled_wait.time_blocked,
                       static_cast <uint64_t> (
                         SK_DeltaPerfMS (liStart.QuadPart, 1)
                       )
                   );

  // We're waiting on the same event as last time on this thread
  if ( same_as_last_time )
  {
    pTLS->scheduler->mru_wait.last_wait = liStart;
    pTLS->scheduler->mru_wait.sequence++;
  }

  // This thread found actual work and has stopped abusing the kernel
  //   waiting on the same always-signaled event; it can have its
  //     normal preemption behavior back  (briefly anyway).
  else
  {
    pTLS->scheduler->mru_wait.start     = liStart;
    pTLS->scheduler->mru_wait.last_wait = liStart;
    pTLS->scheduler->mru_wait.sequence  = 0;
  }

  if ( ret            == WAIT_OBJECT_0 &&   SK_POE2_FixUnityEmployment &&
       dwMilliseconds == INFINITE      && ( bAlertable != FALSE ) )
  {
    // Not to be confused with the other thing
    bool hardly_working =
      (! StrCmpW (pTLS->debug.name, L"Worker Thread"));

    if ( SK_POE2_Stage3UnityFix || hardly_working )
    {
      if (pTLS->scheduler->mru_wait.getRate () >= 0.00666f)
      {
        // This turns preemption of threads in the same priority level off.
        //
        //    * Yes, TRUE means OFF.  Use this wrong and you will hurt
        //                              performance; just sayin'
        //
        if (pTLS->scheduler->mru_wait.preemptive == -1)
        {
          GetThreadPriorityBoost ( GetCurrentThread (),
                                   &pTLS->scheduler->mru_wait.preemptive );
        }

        if (pTLS->scheduler->mru_wait.preemptive == FALSE)
        {
          SetThreadPriorityBoost ( GetCurrentThread (), TRUE );
          InterlockedIncrement   (&SK_POE2_ThreadBoostsKilled);
        }

        //
        // (Everything below applies to the Unity unemployment office only)
        //
        if (hardly_working)
        {
          // Unity Worker Threads have special additional considerations to
          //   make them less of a pain in the ass for the kernel.
          //
          LARGE_INTEGER core_sleep_begin =
            SK_QueryPerf ();

          if (SK_DeltaPerfMS (liStart.QuadPart, 1) < 0.05)
          {
            InterlockedIncrement (&SK_POE2_Horses_Held);
            SwitchToThread       ();

            if (SK_POE2_Stage2UnityFix)
            {
              // Micro-sleep the core this thread is running on to try
              //   and salvage its logical (HyperThreaded) partner's
              //     ability to do work.
              //
              while (SK_DeltaPerfMS (core_sleep_begin.QuadPart, 1) < 0.0000005)
              {
                InterlockedIncrement (&SK_POE2_SMT_Assists);

                // Very brief pause that is good for next to nothing
                //   aside from voluntarily giving up execution resources
                //     on this core's superscalar pipe and hoping the
                //       related Logical Processor can work more
                //         productively if we get out of the way.
                //
                YieldProcessor       (                    );
                //
                // ^^^ Literally does nothing, but an even less useful
                //       nothing if the processor does not support SMT.
                //
              }
            }
          };
        }
      }

      else
      {
        if (pTLS->scheduler->mru_wait.preemptive == -1)
        {
          GetThreadPriorityBoost ( GetCurrentThread (),
                                  &pTLS->scheduler->mru_wait.preemptive );
        }

        if (pTLS->scheduler->mru_wait.preemptive != FALSE)
        {
          SetThreadPriorityBoost (GetCurrentThread (), FALSE);
          InterlockedIncrement   (&SK_POE2_ThreadBoostsKilled);
        }
      }
    }
  }

  // They took our jobs!
  else if (pTLS->scheduler->mru_wait.preemptive != -1)
  {
    SetThreadPriorityBoost (
      GetCurrentThread (),
        pTLS->scheduler->mru_wait.preemptive );

    // Status Quo restored: Jobs nobody wants are back and have
    //   zero future relevance and should be ignored if possible.
    pTLS->scheduler->mru_wait.preemptive = -1;
  }
#endif

  SK_TLS *pTLS =
    SK_TLS_Bottom ();

  if (! pTLS)
  {
    return
      NtWaitForSingleObject_Original (
        Handle, Alertable, Timeout
      );
  }

  //if (config.system.log_level > 0)
  //{
  //  dll_log.Log ( L"tid=%lu (\"%s\") WaitForSingleObject [Alertable: %lu] Timeout: %lli",
  //                  pTLS->debug.tid,
  //                    pTLS->debug.mapped ? pTLS->debug.name : L"Unnamed",
  //                      Alertable,
  //                        Timeout != nullptr ? Timeout->QuadPart : -1
  //              );
  //}
  //
  //if (Timeout != nullptr)
  //  Timeout = nullptr;

  if (pTLS->scheduler->mmcs_task > (SK_MMCS_TaskEntry *) 1)
  {
    //if (pTLS->scheduler->mmcs_task->hTask > 0)
    //    pTLS->scheduler->mmcs_task->disassociateWithTask ();
  }

  auto ret =
    Handle == nullptr ? STATUS_SUCCESS :
    NtWaitForSingleObject_Original (
      Handle, Alertable, Timeout
    );

  if (pTLS->scheduler->mmcs_task > (SK_MMCS_TaskEntry *) 1)
  {
    auto task =
      pTLS->scheduler->mmcs_task;

    //if (pTLS->scheduler->mmcs_task->hTask > 0)
    //                         task->reassociateWithTask ();

    if (InterlockedCompareExchange (&task->change.pending, 0, 1))
    {
      task->setPriority             ( task->change.priority );

      ///if (_stricmp (task->change.task0, task->task0) ||
      ///    _stricmp (task->change.task1, task->task1))
      ///{
      ///  task->disassociateWithTask  ();
      ///  task->setMaxCharacteristics (task->change.task0, task->change.task1);
      ///}
    }
  }

  return ret;
}



typedef BOOL (WINAPI *SwitchToThread_pfn)(void);
                      SwitchToThread_pfn
                      SwitchToThread_Original = nullptr;


BOOL
WINAPI
SwitchToThread_Detour (void)
{
#ifdef _M_AMD64
  static bool is_mhw =
    ( SK_GetCurrentGameID () == SK_GAME_ID::MonsterHunterWorld     );
  static bool is_aco =
    ( SK_GetCurrentGameID () == SK_GAME_ID::AssassinsCreed_Odyssey );

  if (! (is_mhw || is_aco))
  {
#endif
    return
      SwitchToThread_Original ();
#ifdef _M_AMD64
  }


  static volatile DWORD dwAntiDebugTid = 0;

  extern bool   __SK_MHW_KillAntiDebug;
  if (is_mhw && __SK_MHW_KillAntiDebug)
  {
    DWORD dwTid =
      ReadULongAcquire (&dwAntiDebugTid);

    if ( dwTid == 0 ||
         dwTid == SK_Thread_GetCurrentId () )
    {
      SK_TLS *pTLS =
        SK_TLS_Bottom ();

      if ( pTLS->win32->getThreadPriority () !=
             THREAD_PRIORITY_HIGHEST )
      {
        return
          SwitchToThread_Original ();
      }

      else if (dwTid == 0)
      {
        dwTid =
          SK_Thread_GetCurrentId ();

        InterlockedExchange (&dwAntiDebugTid, dwTid);
      }

      if (pTLS->debug.tid == dwTid)
      {
        ULONG ulFrames =
          SK_GetFramesDrawn ();

        if (pTLS->scheduler->last_frame   < (ulFrames - 3) ||
            pTLS->scheduler->switch_count > 3)
        {
          pTLS->scheduler->switch_count = 0;
        }

        if ( WAIT_TIMEOUT !=
               MsgWaitForMultipleObjectsEx (
                 0, nullptr,
                    pTLS->scheduler->switch_count++,
                      QS_ALLEVENTS & ~QS_INPUT,
                        0x0                )
           )
        {
          pTLS->scheduler->last_frame =
            ulFrames;

          return FALSE;
        }

      //SK_Sleep (pTLS->scheduler->switch_count++);

        pTLS->scheduler->last_frame =
          ulFrames;

        return
          TRUE;
      }
    }

    return
      SwitchToThread_Original ();
  }


  //SK_LOG0 ( ( L"Thread: %x (%s) is SwitchingThreads",
  //            SK_GetCurrentThreadId (), SK_Thread_GetName (
  //              GetCurrentThread ()).c_str ()
  //            ),
  //         L"AntiTamper");


  BOOL bRet = FALSE;

  if (__SK_MHW_KillAntiDebug && is_aco)
  {
    config.render.framerate.enable_mmcss = true;

    SK_TLS *pTLS =
      SK_TLS_Bottom ();

    if (pTLS->scheduler->last_frame != SK_GetFramesDrawn ())
        pTLS->scheduler->switch_count = 0;

    bRet = TRUE;

    if (pTLS->scheduler->mmcs_task > (SK_MMCS_TaskEntry *) 1)
    {
      //if (pTLS->scheduler->mmcs_task->hTask > 0)
      //    pTLS->scheduler->mmcs_task->disassociateWithTask ();
    }

    if (pTLS->scheduler->switch_count++ < 20)
    {
      if (pTLS->scheduler->switch_count < 15)
        SK_Sleep (0);
      else
        bRet = SwitchToThread_Original ();
    }
    else
      SK_Sleep (1);

    if (pTLS->scheduler->mmcs_task > (SK_MMCS_TaskEntry *)1)
    {
      auto task =
        pTLS->scheduler->mmcs_task;

      //if (pTLS->scheduler->mmcs_task->hTask > 0)
      //                         task->reassociateWithTask ();

      if (InterlockedCompareExchange (&task->change.pending, 0, 1))
      {
        task->setPriority            ( task->change.priority );

        ///if (_stricmp (task->change.task0, task->task0) ||
        ///    _stricmp (task->change.task1, task->task1))
        ///{
        ///  task->disassociateWithTask  ();
        ///  task->setMaxCharacteristics (task->change.task0, task->change.task1);
        ///}
      }
    }

    pTLS->scheduler->last_frame =
      SK_GetFramesDrawn ();
  }

  else
  {
    bRet =
      SwitchToThread_Original ();
  }

  return
    bRet;
#endif
}


volatile LONG __sleep_init = 0;

DWORD
WINAPI
SK_SleepEx (DWORD dwMilliseconds, BOOL bAlertable)
{
  if (! ReadAcquire (&__sleep_init))
    return SleepEx (dwMilliseconds, bAlertable);

  return
    SleepEx_Original != nullptr       ?
      SleepEx_Original (dwMilliseconds, bAlertable) :
      SleepEx          (dwMilliseconds, bAlertable);
}

void
WINAPI
SK_Sleep (DWORD dwMilliseconds)
{
  SK_SleepEx (dwMilliseconds, FALSE);
}


DWORD
WINAPI
SleepEx_Detour (DWORD dwMilliseconds, BOOL bAlertable)
{
  if (   ReadAcquire (&__SK_DLL_Ending  ) ||
      (! ReadAcquire (&__SK_DLL_Attached) ) )
  {
    return 0;
  }

  static auto& rb =
    SK_GetCurrentRenderBackend ();

  static auto game_id =
    SK_GetCurrentGameID ();

  if ( game_id        == SK_GAME_ID::FinalFantasyXV &&
       dwMilliseconds == 0                          && fix_sleep_0 )
  {
    SwitchToThread ();
    return 0;
  }


  const bool sleepless_render = config.render.framerate.sleepless_render;
  const bool sleepless_window = config.render.framerate.sleepless_window;

  bool bWantThreadClassification =
    ( sleepless_render ||
      sleepless_window );

  bWantThreadClassification |=
    ( game_id == SK_GAME_ID::Tales_of_Vesperia &&
            1 == dwMilliseconds );

  DWORD dwTid =
    bWantThreadClassification   ?
      SK_Thread_GetCurrentId () : 0;

  SK_TLS *pTLS =
    nullptr;

  BOOL bGUIThread    = sleepless_window ?     SK_Win32_IsGUIThread (dwTid, &pTLS)         :
                                                                                   false;
  BOOL bRenderThread = sleepless_render ? ((DWORD)ReadULongAcquire (&rb.thread) == dwTid) :
                                                                                   false;

  if (bRenderThread)
  {
#ifdef _M_AMD64
    if (game_id == SK_GAME_ID::Tales_of_Vesperia)
    {
      extern bool SK_TVFix_NoRenderSleep (void);

      if (SK_TVFix_NoRenderSleep ())
      {
        YieldProcessor ( );
        return 0;
      }
    }
#endif

    if (sleepless_render && dwMilliseconds != INFINITE)
    {
      static bool reported = false;
            if (! reported)
            {
              dll_log->Log ( L"[FrameLimit] Sleep called from render "
                             L"thread: %lu ms!", dwMilliseconds );
              reported = true;
            }

      SK::Framerate::events->getRenderThreadStats ().wake (dwMilliseconds);

      //if (bGUIThread)
      //  SK::Framerate::events->getMessagePumpStats ().wake (dwMilliseconds);

      if (dwMilliseconds <= 1)
      {
        return
          SleepEx_Original (0, bAlertable);
      }

      return 0;
    }

    SK::Framerate::events->getRenderThreadStats ().sleep  (dwMilliseconds);
  }

  if (bGUIThread)
  {
    if (sleepless_window && dwMilliseconds != INFINITE)
    {
      static bool reported = false;
            if (! reported)
            {
              dll_log->Log ( L"[FrameLimit] Sleep called from GUI thread: "
                             L"%lu ms!", dwMilliseconds );
              reported = true;
            }

      SK::Framerate::events->getMessagePumpStats ().wake   (dwMilliseconds);

      if (bRenderThread)
        SK::Framerate::events->getMessagePumpStats ().wake (dwMilliseconds);

      SK_Thread_WaitWhilePumpingMessages (dwMilliseconds, pTLS);

      return 0;
    }

    SK::Framerate::events->getMessagePumpStats ().sleep (dwMilliseconds);
  }

  DWORD max_delta_time =
    narrow_cast <DWORD> (config.render.framerate.max_delta_time);


#ifdef _M_AMD64
  extern bool SK_TVFix_ActiveAntiStutter (void);

  if ( game_id == SK_GAME_ID::Tales_of_Vesperia &&
                   SK_TVFix_ActiveAntiStutter () )
  {
    if (dwTid == 0)
        dwTid = SK_Thread_GetCurrentId ();

    static DWORD   dwTidBusy = 0;
    static DWORD   dwTidWork = 0;
    static SK_TLS *pTLSBusy  = nullptr;
    static SK_TLS *pTLSWork  = nullptr;

    if (dwTid != 0 && ( dwTidBusy == 0 || dwTidWork == 0 ))
    {
      if (pTLS == nullptr)
          pTLS = SK_TLS_Bottom ();

           if (! _wcsicmp (pTLS->debug.name, L"BusyThread")) { dwTidBusy = dwTid; pTLSBusy = pTLS; }
      else if (! _wcsicmp (pTLS->debug.name, L"WorkThread")) { dwTidWork = dwTid; pTLSWork = pTLS; }
    }

    if ( dwTidBusy == dwTid || dwTidWork == dwTid )
    {
      pTLS = ( dwTidBusy == dwTid ? pTLSBusy :
                                    pTLSWork );

      ULONG ulFrames =
        SK_GetFramesDrawn ();

      if (pTLS->scheduler->last_frame < ulFrames)
      {
        pTLS->scheduler->switch_count = 0;
      }

      pTLS->scheduler->last_frame =
        ulFrames;

      YieldProcessor ();

      return 0;
    }

    else if ( dwMilliseconds == 0 ||
              dwMilliseconds == 1 )
    {
      max_delta_time = std::max (2UL, max_delta_time);
      //dll_log.Log ( L"Sleep %lu - %s - %x", dwMilliseconds,
      //               thread_name.c_str (), SK_Thread_GetCurrentId () );
    }
  }
#endif

  // TODO: Stop this nonsense and make an actual parameter for this...
  //         (min sleep?)
  if ( max_delta_time <= dwMilliseconds )
  {
    //dll_log.Log (L"SleepEx (%lu, %s) -- %s", dwMilliseconds, bAlertable ?
    //             L"Alertable" : L"Non-Alertable",
    //             SK_SummarizeCaller ().c_str ());
    return
      SK_SleepEx (dwMilliseconds, bAlertable);
  }

  else
  {
    static volatile LONG __init = 0;

    long double dMinRes = 0.498800;

    if (! InterlockedCompareExchange (&__init, 1, 0))
    {
      NtDll =
        LoadLibrary (L"ntdll.dll");

      NtQueryTimerResolution =
        reinterpret_cast <NtQueryTimerResolution_pfn> (
          GetProcAddress (NtDll, "NtQueryTimerResolution")
        );

      NtSetTimerResolution =
        reinterpret_cast <NtSetTimerResolution_pfn> (
          GetProcAddress (NtDll, "NtSetTimerResolution")
        );

      if (NtQueryTimerResolution != nullptr &&
          NtSetTimerResolution   != nullptr)
      {
        ULONG min, max, cur;
        NtQueryTimerResolution (&min, &max, &cur);
        dll_log->Log ( L"[  Timing  ] Kernel resolution.: %f ms",
                            long_double_cast (cur * 100)/1000000.0L );
        NtSetTimerResolution   (max, TRUE,  &cur);
        dll_log->Log ( L"[  Timing  ] New resolution....: %f ms",
                            long_double_cast (cur * 100)/1000000.0L );

        dMinRes =
                            long_double_cast (cur * 100)/1000000.0L;
      }
    }
  }

  return 0;
}

void
WINAPI
Sleep_Detour (DWORD dwMilliseconds)
{
  SleepEx_Detour (dwMilliseconds, FALSE);
}

#ifdef _M_AMD64
float __SK_SHENMUE_ClockFuzz = 20.0f;
extern volatile LONG SK_BypassResult;
#endif

BOOL
WINAPI
QueryPerformanceFrequency_Detour (_Out_ LARGE_INTEGER *lpPerfFreq)
{
  if (lpPerfFreq)
  {
    *lpPerfFreq =
      SK_GetPerfFreq ();

    return TRUE;
  }

  return
    FALSE;
}

BOOL
WINAPI
SK_QueryPerformanceCounter (_Out_ LARGE_INTEGER *lpPerformanceCount)
{
  if (RtlQueryPerformanceCounter != nullptr)
    return RtlQueryPerformanceCounter (lpPerformanceCount);

  else if (QueryPerformanceCounter_Original != nullptr)
    return QueryPerformanceCounter_Original (lpPerformanceCount);

  else
    return QueryPerformanceCounter (lpPerformanceCount);
}

#ifdef _M_AMD64
extern bool SK_Shenmue_IsLimiterBypassed   (void              );
extern bool SK_Shenmue_InitLimiterOverride (LPVOID pQPCRetAddr);
extern bool SK_Shenmue_UseNtDllQPC;
#endif

BOOL
WINAPI
QueryPerformanceCounter_Detour (_Out_ LARGE_INTEGER *lpPerformanceCount)
{
#ifdef _M_AMD64
  struct SK_ShenmueLimitBreaker
  {
    bool detected = false;
    bool pacified = false;
  } static
      shenmue_clock {
        SK_GetCurrentGameID () == SK_GAME_ID::Shenmue,
      //SK_GetCurrentGameID () == SK_GAME_ID::DragonQuestXI,
        false
      };


  if ( lpPerformanceCount != nullptr   &&
       shenmue_clock.detected          &&
       shenmue_clock.pacified == false &&
    (! SK_Shenmue_IsLimiterBypassed () ) )
  {
    extern volatile LONG
      __SK_SHENMUE_FinishedButNotPresented;

    if (ReadAcquire (&__SK_SHENMUE_FinishedButNotPresented))
    {
      if ( SK_Thread_GetCurrentId () == SK_GetRenderThreadID () )
      {
        if ( SK_GetCallingDLL () == SK_Modules->HostApp () )
        {
          static std::unordered_set <LPCVOID> ret_addrs;

          if (ret_addrs.emplace (_ReturnAddress ()).second)
          {
            SK_LOG1 ( ( L"New QueryPerformanceCounter Return Addr: %ph -- %s",
                        _ReturnAddress (), SK_SummarizeCaller ().c_str () ),
                        L"ShenmueDbg" );

            // The instructions we're looking for are jl ...
            //
            //   Look-ahead up to about 12-bytes and if not found,
            //     this isn't the primary limiter code.
            //
            if ( reinterpret_cast <uintptr_t> (
                   SK_ScanAlignedEx (
                     "\x7C\xEE",          2,
                     nullptr,     (void *)_ReturnAddress ()
                   )
                 ) < (uintptr_t)_ReturnAddress () + 12 )
            {
              shenmue_clock.pacified =
                SK_Shenmue_InitLimiterOverride (_ReturnAddress ());

              SK_LOG0 ( (L"Shenmue Framerate Limiter Located and Pacified"),
                         L"ShenmueDbg" );
            }
          }

          InterlockedExchange (&__SK_SHENMUE_FinishedButNotPresented, 0);

          BOOL bRet =
            RtlQueryPerformanceCounter ?
            RtlQueryPerformanceCounter          (lpPerformanceCount) :
               QueryPerformanceCounter_Original ?
               QueryPerformanceCounter_Original (lpPerformanceCount) :
               QueryPerformanceCounter          (lpPerformanceCount);

          static LARGE_INTEGER last_poll {
            lpPerformanceCount->u.LowPart,
            lpPerformanceCount->u.HighPart
          };

          LARGE_INTEGER pre_fuzz {
            lpPerformanceCount->u.LowPart,
            lpPerformanceCount->u.HighPart
          };

          lpPerformanceCount->QuadPart +=
            static_cast <LONGLONG> (
                                     ( long_double_cast (lpPerformanceCount->QuadPart) -
                                       long_double_cast (last_poll.QuadPart)           *
                                       long_double_cast (__SK_SHENMUE_ClockFuzz)       )
                                   );

          last_poll.QuadPart =
           pre_fuzz.QuadPart;

          return bRet;
        }
      }
    }
  }
#endif

  return
    RtlQueryPerformanceCounter          ?
    RtlQueryPerformanceCounter          (lpPerformanceCount) :
       QueryPerformanceCounter_Original ?
       QueryPerformanceCounter_Original (lpPerformanceCount) :
       QueryPerformanceCounter          (lpPerformanceCount);
}


LARGE_INTEGER
SK_GetPerfFreq (void)
{
  static LARGE_INTEGER freq = { 0UL };
  static volatile LONG init = FALSE;

  if (ReadAcquire (&init) < 2)
  {
      RtlQueryPerformanceFrequency =
        (QueryPerformanceCounter_pfn)
    SK_GetProcAddress ( L"NtDll",
                         "RtlQueryPerformanceFrequency" );

    if (! InterlockedCompareExchange (&init, 1, 0))
    {
      if (RtlQueryPerformanceFrequency != nullptr)
          RtlQueryPerformanceFrequency            (&freq);
      else if (QueryPerformanceFrequency_Original != nullptr)
               QueryPerformanceFrequency_Original (&freq);
      else
        QueryPerformanceFrequency                 (&freq);

      InterlockedIncrement (&init);

      return freq;
    }

    if (ReadAcquire (&init) < 2)
    {
      LARGE_INTEGER freq2 = { };

      if (RtlQueryPerformanceFrequency != nullptr)
          RtlQueryPerformanceFrequency            (&freq2);
      else if (QueryPerformanceFrequency_Original != nullptr)
               QueryPerformanceFrequency_Original (&freq2);
      else
        QueryPerformanceFrequency                 (&freq2);

      return
        freq2;
    }
  }

  return freq;
}







void SK_Scheduler_Init (void)
{
  RtlQueryPerformanceFrequency =
    (QueryPerformanceCounter_pfn)
    SK_GetProcAddress ( L"NtDll",
                         "RtlQueryPerformanceFrequency" );

  RtlQueryPerformanceCounter =
  (QueryPerformanceCounter_pfn)
    SK_GetProcAddress ( L"NtDll",
                         "RtlQueryPerformanceCounter" );

  ZwQueryPerformanceCounter =
    (QueryPerformanceCounter_pfn)
    SK_GetProcAddress ( L"NtDll",
                         "ZwQueryPerformanceCounter" );

  NtDelayExecution =
    (NtDelayExecution_pfn)
    SK_GetProcAddress ( L"NtDll",
                         "NtDelayExecution" );

//#define NO_HOOK_QPC
#ifndef NO_HOOK_QPC
  SK_GetPerfFreq ();
  SK_CreateDLLHook2 (      L"kernel32",
                            "QueryPerformanceFrequency",
                             QueryPerformanceFrequency_Detour,
    static_cast_p2p <void> (&QueryPerformanceFrequency_Original) );

  SK_CreateDLLHook2 (      L"kernel32",
                            "QueryPerformanceCounter",
                             QueryPerformanceCounter_Detour,
    static_cast_p2p <void> (&QueryPerformanceCounter_Original),
    static_cast_p2p <void> (&pfnQueryPerformanceCounter) );
#endif

  SK_CreateDLLHook2 (      L"kernel32",
                            "Sleep",
                             Sleep_Detour,
    static_cast_p2p <void> (&Sleep_Original),
    static_cast_p2p <void> (&pfnSleep) );

  SK_CreateDLLHook2 (      L"KernelBase.dll",
                            "SleepEx",
                             SleepEx_Detour,
    static_cast_p2p <void> (&SleepEx_Original) );

  SK_CreateDLLHook2 (      L"KernelBase.dll",
                            "SwitchToThread",
                             SwitchToThread_Detour,
    static_cast_p2p <void> (&SwitchToThread_Original) );

  SK_CreateDLLHook2 (      L"NtDll",
                            "NtWaitForSingleObject",
                             NtWaitForSingleObject_Detour,
    static_cast_p2p <void> (&NtWaitForSingleObject_Original) );

  SK_CreateDLLHook2 (      L"NtDll",
                            "NtWaitForMultipleObjects",
                             NtWaitForMultipleObjects_Detour,
    static_cast_p2p <void> (&NtWaitForMultipleObjects_Original) );

  //SK_ApplyQueuedHooks ();
  InterlockedExchange (&__sleep_init, 1);

#ifdef NO_HOOK_QPC
  QueryPerformanceCounter_Original =
    reinterpret_cast <QueryPerformanceCounter_pfn> (
      GetProcAddress ( SK_GetModuleHandle (L"kernel32"),
                         "QueryPerformanceCounter" )
    );
#endif

  if (! config.render.framerate.enable_mmcss)
  {
    if (NtDll == nullptr)
    {
      NtDll =
        LoadLibrary (L"ntdll.dll");

      NtQueryTimerResolution =
        reinterpret_cast <NtQueryTimerResolution_pfn> (
          GetProcAddress (NtDll, "NtQueryTimerResolution")
        );

      NtSetTimerResolution =
        reinterpret_cast <NtSetTimerResolution_pfn> (
          GetProcAddress (NtDll, "NtSetTimerResolution")
        );

      if (NtQueryTimerResolution != nullptr &&
          NtSetTimerResolution   != nullptr)
      {
        ULONG min, max, cur;
        NtQueryTimerResolution (&min, &max, &cur);
        dll_log->Log ( L"[  Timing  ] Kernel resolution.: %f ms",
                         static_cast <float> (cur * 100)/1000000.0f );
        NtSetTimerResolution   (max, TRUE,  &cur);
        dll_log->Log ( L"[  Timing  ] New resolution....: %f ms",
                         static_cast <float> (cur * 100)/1000000.0f );
      }
    }
  }
}

void
SK_Scheduler_Shutdown (void)
{
  if (NtDll != nullptr)
    FreeLibrary (NtDll);

  SK_DisableHook (pfnSleep);
  //SK_DisableHook (pfnQueryPerformanceCounter);
}