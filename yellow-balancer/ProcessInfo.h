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
#include "perf_monitor.h"
#include "ring_buffer.h"

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

typedef NTSTATUS(WINAPI* pNtQuerySystemInformation)(int, PVOID, ULONG, PULONG);

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
	FILETIME create_time_;
	FILETIME cur_user_time_;
	RingBuffer<LONGLONG> user_time_;
	std::vector<ThreadInfo> threads_;
};

struct ProcessInfoShort {
	ULONG pid_;
	std::wstring name_;
	FILETIME create_time_;
	FILETIME user_time_;
	FILETIME kernel_time_;
	std::vector<ThreadInfo> threads_;
};

class ProcessesInfo {
public:
	~ProcessesInfo();
	ProcessesInfo& AddFilter(std::wstring process_name);
	void Init(int cpu_analysis_period, int switching_frequency, int maximum_cpu_value, int delta_cpu_values);
	void Read();
	void SetAffinity();
	void SetTest() { test = true; }
private:
	std::unordered_set<std::wstring> process_filter_;
	std::vector<ProcessInfoShort> processes_short_;
	std::unordered_map<ULONG, ProcessInfo> processes_;
	std::vector<NUMA_NODE_RELATIONSHIP> numa_nodes_;
	PerfMonitor perf_monitor_;
	int cpu_analysis_period_;
	int ring_buffer_size_;
	int maximum_cpu_value_;
	int delta_cpu_values_;
	std::vector<BYTE> buffer_active_processes;

	void InitPerfMonitor(int cpu_analysis_period);
	void InitNtSetInformationProcess();
	void InitNtQuerySystemInformation();
	void GetNumaInfo();
	std::vector<USHORT> GetProcessNumaGroup(ULONG id_process);
	std::pair<DWORD_PTR, DWORD_PTR> GetProcAffinityMask(ULONG id_process);
	pNtSetInformationProcess NtSetInformationProcess;
	pNtQuerySystemInformation NtQuerySystemInformation;
	std::unordered_map<ULONG, ProcessInfoShort> ActiveProcesses();
	void DeleteOldProcess(std::unordered_map<ULONG, ProcessInfo>& lhs, const std::unordered_map<ULONG, ProcessInfoShort>& rhs);
	void AddProcess(std::unordered_map<ULONG, ProcessInfo>& lhs, std::unordered_map<ULONG, ProcessInfoShort>& rhs);
	bool IsNeedToSetAffinity(const std::vector<double>& values);
	bool test = false;
};
