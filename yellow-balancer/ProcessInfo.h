#pragma once

#define _WIN32_WINNT 0x0601

#include <iostream>
#include <windows.h>
#include <string>
#include <unordered_set>
#include <vector>
#include <unordered_map>
#include <processtopologyapi.h>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include "Logger.h"

typedef LONG KPRIORITY;

struct CLIENT_ID {
	DWORD UniqueProcess; // Process ID
#ifdef _WIN64
	ULONG pad1;
#endif
	DWORD UniqueThread;  // Thread ID
#ifdef _WIN64
	ULONG pad2;
#endif
};

typedef struct {
	FILETIME ProcessorTime;
	FILETIME UserTime;
	FILETIME CreateTime;
	ULONG WaitTime;
#ifdef _WIN64
	ULONG pad1;
#endif
	PVOID StartAddress;
	CLIENT_ID Client_Id;
	KPRIORITY CurrentPriority;
	KPRIORITY BasePriority;
	ULONG ContextSwitchesPerSec;
	ULONG ThreadState;
	ULONG ThreadWaitReason;
	ULONG pad2;
} SYSTEM_THREAD_INFORMATION;

typedef struct {
	USHORT Length;
	USHORT MaximumLength;
	PWSTR  Buffer;
} UNICODE_STRING;

typedef struct {
	ULONG_PTR PeakVirtualSize;
	ULONG_PTR VirtualSize;
	ULONG PageFaultCount;
#ifdef _WIN64
	ULONG pad1;
#endif
	ULONG_PTR PeakWorkingSetSize;
	ULONG_PTR WorkingSetSize;
	ULONG_PTR QuotaPeakPagedPoolUsage;
	ULONG_PTR QuotaPagedPoolUsage;
	ULONG_PTR QuotaPeakNonPagedPoolUsage;
	ULONG_PTR QuotaNonPagedPoolUsage;
	ULONG_PTR PagefileUsage;
	ULONG_PTR PeakPagefileUsage;
} VM_COUNTERS;

typedef struct {
	ULONG NextOffset;
	ULONG ThreadCount;
	LARGE_INTEGER WorkingSetPrivateSize;
	ULONG HardFaultCount;
	ULONG NumberOfThreadsHighWatermark;
	ULONGLONG CycleTime;
	FILETIME CreateTime;
	FILETIME UserTime;
	FILETIME KernelTime;
	UNICODE_STRING ImageName;
	KPRIORITY BasePriority;
#ifdef _WIN64
	ULONG pad1;
#endif
	ULONG ProcessId;
#ifdef _WIN64
	ULONG pad2;
#endif
	ULONG InheritedFromProcessId;
#ifdef _WIN64
	ULONG pad3;
#endif
	ULONG HandleCount;
	ULONG SessionId;
	ULONG_PTR UniqueProcessKey; // always NULL, use SystemExtendedProcessInformation (57) to get value
	VM_COUNTERS VirtualMemoryCounters;
	ULONG_PTR PrivatePageCount;
	IO_COUNTERS IoCounters;
	SYSTEM_THREAD_INFORMATION ThreadInfos[1];
} SYSTEM_PROCESS_INFORMATION;

#define SYSTEMPROCESSINFORMATION 5
#define STATUS_INFO_LENGTH_MISMATCH ((NTSTATUS) 0xC0000004)

typedef NTSTATUS(WINAPI* t_NtQuerySystemInformation)(int, PVOID, ULONG, PULONG);

typedef NTSTATUS(NTAPI* pNtSetInformationProcess)(
	HANDLE ProcessHandle,
	PROCESS_INFORMATION_CLASS ProcessInformationClass,
	PVOID ProcessInformation,
	ULONG ProcessInformationLength
	);

typedef enum {
	ThreadStateInitialized,
	ThreadStateReady,
	ThreadStateRunning,
	ThreadStateStandby,
	ThreadStateTerminated,
	ThreadStateWaiting,
	ThreadStateTransition,
	ThreadStateDeferredReady
} THREAD_STATE;

typedef enum {
	ThreadWaitReasonExecutive,
	ThreadWaitReasonFreePage,
	ThreadWaitReasonPageIn,
	ThreadWaitReasonPoolAllocation,
	ThreadWaitReasonDelayExecution,
	ThreadWaitReasonSuspended,
	ThreadWaitReasonUserRequest,
	ThreadWaitReasonWrExecutive,
	ThreadWaitReasonWrFreePage,
	ThreadWaitReasonWrPageIn,
	ThreadWaitReasonWrPoolAllocation,
	ThreadWaitReasonWrDelayExecution,
	ThreadWaitReasonWrSuspended,
	ThreadWaitReasonWrUserRequest,
	ThreadWaitReasonWrEventPair,
	ThreadWaitReasonWrQueue,
	ThreadWaitReasonWrLpcReceive,
	ThreadWaitReasonWrLpcReply,
	ThreadWaitReasonWrVirtualMemory,
	ThreadWaitReasonWrPageOut,
	ThreadWaitReasonWrRendezvous,
	ThreadWaitReasonWrKeyedEvent,
	ThreadWaitReasonWrTerminated,
	ThreadWaitReasonWrProcessInSwap,
	ThreadWaitReasonWrCpuRateControl,
	ThreadWaitReasonWrCalloutStack,
	ThreadWaitReasonWrKernel,
	ThreadWaitReasonWrResource,
	ThreadWaitReasonWrPushLock,
	ThreadWaitReasonWrMutex,
	ThreadWaitReasonWrQuantumEnd,
	ThreadWaitReasonWrDispatchInt,
	ThreadWaitReasonWrPreempted,
	ThreadWaitReasonWrYieldExecution,
	ThreadWaitReasonWrFastMutex,
	ThreadWaitReasonWrGuardedMutex,
	ThreadWaitReasonWrRundown,
	ThreadWaitReasonMaximumWaitReason
} THREAD_WAIT_REASON;

struct ThreadInfo {
	DWORD thread_id_;
	ULONG thread_state_;
	ULONG thread_wait_reason_;
	GROUP_AFFINITY group_affinity_;
};

struct ProcessInfo {
	ULONG pid_;
	std::wstring name_;
	ULONG number_threads_;
	ULONG handle_count_;
	FILETIME create_time_;
	FILETIME user_time_;
	FILETIME kernel_time_;
	std::vector<ThreadInfo> threads_;
	std::vector<WORD> numa_groups_;
};

struct ProcessInfoShort {
	ULONG pid_;
	LONGLONG user_time_;
};

class ProcessesInfo {
public:
	ProcessesInfo();
	ProcessesInfo& AddFilter(std::wstring process_name);
	void Read();
	void SetAffinity();
	void Print();
private:
	std::unordered_set<std::wstring> process_filter_;
	std::vector<ProcessInfoShort> processes_short_;
	std::unordered_map<ULONG, ProcessInfo> processes_;
	std::vector<GROUP_AFFINITY> numa_groups_;

	void GetNumaInfo();
	std::vector<USHORT> GetProcessNumaGroup(ULONG id_process);
	pNtSetInformationProcess p_set_process_affinity_;
};
