#include "ProcessInfo.h"

using namespace std;

static auto LOGGER = Logger::getInstance();

const WCHAR* ThreadStateValueNames[] = {
  L"Initialized",
  L"Ready",
  L"Running",
  L"Standby",
  L"Terminated",
  L"Waiting",
  L"Transition",
  L"DeferredReady"
};

const WCHAR* ThreadWaitReasonValueNames[] = {
	L"Executive",
	L"FreePage",
	L"PageIn",
	L"PoolAllocation",
	L"DelayExecution",
	L"Suspended",
	L"UserRequest",
	L"WrExecutive ",
	L"WrFreePage",
	L"WrPageIn",
	L"WrPoolAllocation",
	L"WrDelayExecution",
	L"WrSuspended",
	L"WrUserRequest",
	L"WrEventPair",
	L"WrQueue",
	L"WrLpcReceive",
	L"WrLpcReply",
	L"WrVirtualMemory",
	L"WrPageOut",
	L"WrRendezvous",
	L"WrKeyedEvent",
	L"WrTerminated",
	L"WrProcessInSwap",
	L"WrCpuRateControl",
	L"WrCalloutStack",
	L"WrKernel",
	L"WrResource",
	L"WrPushLock",
	L"WrMutex",
	L"WrQuantumEnd",
	L"WrDispatchInt",
	L"WrPreempted",
	L"WrYieldExecution",
	L"WrFastMutex",
	L"WrGuardedMutex",
	L"WrRundown",
	L"MaximumWaitReason"
};

wstringstream wss_;

template<typename T>
wstring vectorToWstring(const vector<T>& v) {
	wstring separator = L"";
	wss_ << L"[";
	for (auto it = v.begin(); it < v.end(); ++it) {
		wss_ << separator << *it;
		separator = L",";
	}
	wss_ << L"]";
	wstring wstr = wss_.str();
	wss_.str(L"");
	wss_.clear();
	return wstr;
}

void print_bitmap(ULONG_PTR mask){
	for (int i = sizeof(mask) * 8 - 1; i >= 0; --i) {
		printf("%d", (int)(mask >> i) & 1);
	}
}

std::wstring GetLastErrorAsString() {
	DWORD errorMessageID = ::GetLastError();
	if (errorMessageID == 0) {
		return std::wstring(); //No error message has been recorded
	}

	LPWSTR messageBuffer = nullptr;

	size_t size = FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL, errorMessageID, MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US), (LPWSTR)&messageBuffer, 0, NULL);

	std::wstring message(messageBuffer, size - 2);

	while (message.back() == L'\n' || message.back() == L'\n') {
		message.pop_back();
	}

	LocalFree(messageBuffer);

	return message;
}

LONGLONG fileTimeToLongLong(const FILETIME& fileTime) {
	uint64_t uTime;
	memcpy(&uTime, &fileTime, sizeof(uTime));
	return uTime;
}

wstring systemTimeToWstring(const SYSTEMTIME& time) {
	wss_ << setfill(L'0') << setw(4) << time.wYear << L'-' << setw(2) << time.wMonth << L'-' << setw(2) << time.wDay
		<< L' '
		<< setw(2) << time.wHour << L':' << setw(2) << time.wMinute << L':' << setw(2) << time.wSecond << L'.' << setw(3) << time.wMilliseconds;
	wstring wstr = wss_.str();
	wss_.str(L"");
	wss_.clear();
	return wstr;
}

wstring ThreadStateToWstring(ULONG thread_state) {
	if (thread_state < 8)
		return ThreadStateValueNames[thread_state];
	else
		return to_wstring(thread_state);
}

wstring ThreadWaitReasonToWstring(ULONG thread_wait_reason) {
	if (thread_wait_reason < 38)
		return ThreadWaitReasonValueNames[thread_wait_reason];
	else
		return to_wstring(thread_wait_reason);
}

ProcessesInfo::ProcessesInfo() {
	GetNumaInfo();
	p_set_process_affinity_ = (pNtSetInformationProcess)GetProcAddress(GetModuleHandle(L"ntdll"), "NtSetInformationProcess");
}

ProcessesInfo& ProcessesInfo::AddFilter(wstring process_name) {
	process_filter_.insert(process_name);
	return *this;
}

void ProcessesInfo::Read() {
	ULONG buflen = 0;
	std::vector<BYTE> buffer;

	t_NtQuerySystemInformation f_NtQuerySystemInformation = (t_NtQuerySystemInformation)GetProcAddress(GetModuleHandleA("NtDll.dll"), "NtQuerySystemInformation");
	if (!f_NtQuerySystemInformation) {
		fprintf(stderr, "Error %d while retrieving adress of NtQuerySystemInformation.", GetLastError());
		return;
	}

	NTSTATUS lResult = f_NtQuerySystemInformation(SYSTEMPROCESSINFORMATION, NULL, buflen, &buflen);
	if (lResult == STATUS_INFO_LENGTH_MISMATCH)	{
		buffer.resize(buflen);
	}
	else {
		fprintf(stderr, "Error %d calling NtQuerySystemInformation.\n", GetLastError());
		return;
	}

	if (f_NtQuerySystemInformation(SYSTEMPROCESSINFORMATION, &buffer[0], buflen, &buflen)) {
		fprintf(stderr, "Error %d calling NtQuerySystemInformation.\n", GetLastError());
		return;
	}

	processes_short_.clear();
	processes_.clear();

	unsigned int i = 0;
	SYSTEM_PROCESS_INFORMATION* info;
	do {
		info = (SYSTEM_PROCESS_INFORMATION*)&buffer[i];
		std::wstring image_name = (info->ImageName.Buffer ? info->ImageName.Buffer : L"unknow");
		if (info->ProcessId == 0) image_name = L"System Idle Process";

		if (process_filter_.size() == 0 || process_filter_.find(image_name) != process_filter_.end()) {
			processes_short_.push_back({ info->ProcessId, fileTimeToLongLong(info->UserTime) });
			
			auto it_process = processes_.insert(pair<ULONG, ProcessInfo>(
				info->ProcessId,
				{
					info->ProcessId,
					move(image_name),
					info->ThreadCount,
					info->HandleCount,
					info->CreateTime,
					info->UserTime,
					info->KernelTime,
					{},
					GetProcessNumaGroup(info->ProcessId)
				}
			));

			for (unsigned int j = 0; j < info->ThreadCount; j++) {
				it_process.first->second.threads_.push_back({
					info->ThreadInfos[j].Client_Id.UniqueThread,
					info->ThreadInfos[j].ThreadState,
					info->ThreadInfos[j].ThreadWaitReason,
					{}
					});
				HANDLE thread_handle = OpenThread(THREAD_ALL_ACCESS, FALSE, info->ThreadInfos[j].Client_Id.UniqueThread);
				if (NULL != thread_handle) {
					GROUP_AFFINITY thread_group_affinity;
					GetThreadGroupAffinity(thread_handle, &(it_process.first->second.threads_.back().group_affinity_));
					CloseHandle(thread_handle);
				}
			}
		}
		i += info->NextOffset;
	} while (info->NextOffset != 0);
}

const GROUP_AFFINITY* CalculateNumaWeight(const vector<ThreadInfo>& threads, const vector<GROUP_AFFINITY>& numa_group) {
	vector<UINT32> aggregator(numa_group.size(), 0);
	for (auto it = threads.begin(); it < threads.end(); ++it) {
		++aggregator[it->group_affinity_.Group];
	}
	UINT32 count_thread = 0;
	int64_t index = -1;
	for (size_t i = 0; i < aggregator.size(); ++i) {
		if (count_thread > aggregator[i]) {
			index = i;
			count_thread = aggregator[i];
		}
	}
	
	if (index != -1) {
		return &numa_group[index];
	}
	else {
		return &numa_group[0];
	}
}

bool SetThreadAffinity(DWORD tid, const GROUP_AFFINITY* group_affinity) {
	HANDLE thread_handle = OpenThread(THREAD_ALL_ACCESS, FALSE, tid);
	if (NULL != thread_handle) {
		GROUP_AFFINITY thread_group_affinity;
		SetThreadGroupAffinity(thread_handle, group_affinity, NULL);
		CloseHandle(thread_handle);
		return true;
	}
	return false;
}

bool SetProcessAffinity(pNtSetInformationProcess p_set_process_affinity, DWORD pid, const GROUP_AFFINITY* group_affinity) {
	HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
	if (hProcess != NULL) {
		NTSTATUS status = p_set_process_affinity(hProcess, (PROCESS_INFORMATION_CLASS)0x15, (void*)group_affinity, sizeof(GROUP_AFFINITY));
		if (!status) {
			LOGGER->Print(wstring(L"Error set process affinity: ").append(to_wstring(status)), Logger::Type::Error);
			CloseHandle(hProcess);
			return false;
		}
		CloseHandle(hProcess);
		return true;
	}
	else {
		LOGGER->Print(wstring(L"Error get  hadle process: ").append(GetLastErrorAsString()), Logger::Type::Error);
		return false;
	}
}

void ProcessesInfo::SetAffinity() {
	sort(processes_short_.begin(), processes_short_.end(),
		[](const ProcessInfoShort& lhs, const ProcessInfoShort& rhs)->bool {
			return lhs.user_time_ > rhs.user_time_;
		}
	);

	const GROUP_AFFINITY* cur_numa = nullptr;
	const GROUP_AFFINITY* end_numa = &numa_groups_[0] + numa_groups_.size();
	for (auto it = processes_short_.begin(); it < processes_short_.end(); ++it) {
		auto it_process = processes_.find(it->pid_);
		if (it_process != processes_.end()) {
			
			ProcessInfo& process = it_process->second;
			
			if (!cur_numa) {
				if (process.numa_groups_.size() == 1) {
					cur_numa = &numa_groups_[process.numa_groups_[0]];
				}
				else if (process.numa_groups_.size() > 1) {
					cur_numa = CalculateNumaWeight(process.threads_, numa_groups_);
				}
				else {
					LOGGER->Print(L"Error get numa groups for process", Logger::Type::Error);
				}
			}

			if (process.numa_groups_.size() > 1 || process.numa_groups_[0] != cur_numa->Group) {
				if (SetProcessAffinity(p_set_process_affinity_, process.pid_, cur_numa)) {
					LOGGER->Print(
						wstring(process.name_).
						append(L";pid=").append(to_wstring(process.pid_)).
						append(L";numa=").append(vectorToWstring(process.numa_groups_)).
						append(L";new numa=[").append(to_wstring(cur_numa->Group)).append(L"]")
					);
				}
			}
			else {
				LOGGER->Print(
					wstring(process.name_).
					append(L";pid=").append(to_wstring(process.pid_)).
					append(L";numa=").append(vectorToWstring(process.numa_groups_))
				);
			}

			bool is_not_new_numa = true;
			for (auto it_thread = process.threads_.begin(); it_thread < process.threads_.end(); ++it_thread) {
				if (it_thread->group_affinity_.Group != cur_numa->Group) {
					is_not_new_numa = false;
					if (SetThreadAffinity(it_thread->thread_id_, cur_numa)) {
						LOGGER->Print(
							wstring(process.name_).
							append(L";pid=").append(to_wstring(process.pid_)).
							append(L";tid=").append(to_wstring(it_thread->thread_id_)).
							append(L";numa=[").append(to_wstring(it_thread->group_affinity_.Group)).append(L"]").
							append(L";new numa=[").append(to_wstring(cur_numa->Group)).append(L"]")
						);
					}
					else {
						LOGGER->Print(
							wstring(process.name_).
							append(L";pid=").append(to_wstring(process.pid_)).
							append(L";tid=").append(to_wstring(it_thread->thread_id_)).
							append(L";numa=[").append(to_wstring(it_thread->group_affinity_.Group)).append(L"]").
							append(L";new numa=[").append(to_wstring(cur_numa->Group)).append(L"]"),
							Logger::Type::Error
						);
					}
				}
			}
			if (is_not_new_numa) {
				LOGGER->Print(
					wstring(process.name_).
					append(L";pid=").append(to_wstring(process.pid_)).
					append(L";no threads for new numa")
				);
			}			

			++cur_numa;
			if (cur_numa >= end_numa) {
				cur_numa = &numa_groups_[0];
			}
		}
	}
}

void ProcessesInfo::Print() {
	wcout << L"Numa nodes info:" << endl;
	for (auto it = numa_groups_.begin(); it < numa_groups_.end(); ++it) {
		wcout << L"group=" << it->Group << L";mask=" << it->Mask << endl;
	}
	wcout << endl;

	wcout << L"Current state" << endl;
	for (auto it = processes_short_.begin(); it < processes_short_.end(); ++it) {
		const ProcessInfo& process = processes_.find(it->pid_)->second;
		SYSTEMTIME systemTime;
		FileTimeToSystemTime(&process.create_time_, &systemTime);
		wcout <<
			L"pid=" << process.pid_ <<
			L";name=" << process.name_  <<
			L";create time=" << systemTimeToWstring(systemTime)	<<
			L";user time=" << fileTimeToLongLong(process.user_time_) <<
			L";kernel time=" << fileTimeToLongLong(process.kernel_time_) <<
			L";threads=" << process.number_threads_ <<
			L";handle=" << process.handle_count_ <<
			L";numa=" << vectorToWstring(process.numa_groups_) <<
			endl;
		/*
		for (auto it_thread = process.threads_.begin(); it_thread < process.threads_.end(); ++it_thread) {
			wcout <<
				L"  tid=" << it_thread->thread_id_ <<
				L";state=" << ThreadStateToWstring(it_thread->thread_state_) <<
				L";reason=" << ThreadWaitReasonToWstring(it_thread->thread_wait_reason_) <<
				L";numa=[" << it_thread->group_affinity_.Group << L"]" <<
				L";mask=" << it_thread->group_affinity_.Mask <<
				endl;
		}
		*/
	}
}

void ProcessesInfo::GetNumaInfo() {
	numa_groups_.resize(0);
	DWORD ReturnLength = 0;
	GetLogicalProcessorInformationEx(LOGICAL_PROCESSOR_RELATIONSHIP::RelationNumaNode, NULL, &ReturnLength);
	std::vector<BYTE> buffer(ReturnLength);
	SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX* p;
	if (GetLogicalProcessorInformationEx(LOGICAL_PROCESSOR_RELATIONSHIP::RelationNumaNode, (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX)&buffer[0], &ReturnLength)) {
		BYTE* pCur = (BYTE*)&buffer[0];
		BYTE* pEnd = pCur + ReturnLength;
		for (int i = 0; pCur < pEnd; pCur += ((SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*)pCur)->Size, ++i) {
			p = (SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*)pCur;
			numa_groups_.push_back(p->NumaNode.GroupMask);
		}
	}
}

vector<USHORT> ProcessesInfo::GetProcessNumaGroup(ULONG id_process) {
	HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, id_process);
	if (hProcess != NULL) {
		std::vector<USHORT> GroupArray(128);
		USHORT GroupCount = GroupArray.size();
		if (GetProcessGroupAffinity(hProcess, &GroupCount, &GroupArray[0])) {
			CloseHandle(hProcess);
			GroupArray.resize(GroupCount);
			return GroupArray;
		}
		else {
			CloseHandle(hProcess);
			//std::wstring err_wstr = GetLastErrorAsString();
			return {};
		}
		CloseHandle(hProcess);
	}
	else {
		//std::wstring err_wstr = GetLastErrorAsString();
		return {};
	}
}