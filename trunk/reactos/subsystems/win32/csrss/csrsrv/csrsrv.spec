@ stdcall CsrAddStaticServerThread(ptr ptr long)
;@ stdcall CsrCallServerFromServer(ptr ptr)
;@ stdcall CsrConnectToUser()
;@ stdcall CsrCreateProcess(ptr ptr ptr ptr long ptr)
;@ stdcall CsrCreateRemoteThread(ptr ptr)
@ stdcall CsrCreateThread(ptr ptr ptr)
;@ stdcall CsrCreateWait(ptr ptr ptr ptr ptr ptr)
;@ stdcall CsrDebugProcess(ptr)
;@ stdcall CsrDebugProcessStop(ptr)
;@ stdcall CsrDereferenceProcess(ptr)
;@ stdcall CsrDereferenceThread(ptr)
;@ stdcall CsrDereferenceWait(ptr)
;@ stdcall CsrDestroyProcess(ptr long)
;@ stdcall CsrDestroyThread(ptr)
@ stdcall CsrEnumProcesses(ptr ptr) ; Temporary hack
;@ stdcall CsrExecServerThread(ptr long)
@ stdcall CsrGetProcessLuid(ptr ptr)
@ stdcall CsrImpersonateClient(ptr)
@ stdcall CsrLockProcessByClientId(ptr ptr)
;@ stdcall CsrLockThreadByClientId(ptr ptr)
;@ stdcall CsrMoveSatisfiedWait(ptr ptr)
;@ stdcall CsrNotifyWait(ptr long ptr ptr)
;@ stdcall CsrPopulateDosDevices()
;@ stdcall CsrQueryApiPort()
;@ stdcall CsrReferenceThread(ptr)
@ stdcall CsrRevertToSelf()
@ stdcall CsrServerInitialization(long ptr)
;@ stdcall CsrSetBackgroundPriority(ptr)
;@ stdcall CsrSetCallingSpooler(long)
;@ stdcall CsrSetForegroundPriority(ptr)
;@ stdcall CsrShutdownProcesses(ptr long)
;@ stdcall CsrUnhandledExceptionFilter(ptr)
@ stdcall CsrUnlockProcess(ptr)
;@ stdcall CsrUnlockThread(ptr)
;@ stdcall CsrValidateMessageBuffer(ptr ptr long long)
;@ stdcall CsrValidateMessageString(ptr ptr)
