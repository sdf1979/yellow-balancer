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

void ProcessesInfo::Init(int cpu_analysis_period, int switching_frequency, int maximum_cpu_value, int delta_cpu_values) {
	cpu_analysis_period_ = cpu_analysis_period;
	ring_buffer_size_ = cpu_analysis_period / switching_frequency;
	maximum_cpu_value_ = maximum_cpu_value;
	delta_cpu_values_ = delta_cpu_values;
	GetNumaInfo();
	InitPerfMonitor(cpu_analysis_period);
	InitNtSetInformationProcess();
	InitNtQuerySystemInformation();
}

void ProcessesInfo::InitPerfMonitor(int cpu_analysis_period) {
	perf_monitor_.SetCollectionPeriod(cpu_analysis_period);
	wstring computer_name = L"\\\\";
	computer_name.resize(MAX_COMPUTERNAME_LENGTH + 3, L'\0');
	DWORD sz_computer_name = static_cast<DWORD>(computer_name.size()) - 2;
	GetComputerNameW(&computer_name[2], &sz_computer_name);
	computer_name.resize(sz_computer_name + 2);

	perf_monitor_.AddCounter(wstring(computer_name).append(L"\\Processor Information(_Total)\\% Processor Time"));
	for (auto it = numa_groups_.begin(); it != numa_groups_.end(); ++it) {
		perf_monitor_.AddCounter(wstring(computer_name).append(L"\\Processor Information(").append(to_wstring(it->NodeNumber)).append(L",_Total)\\% Processor Time"));
	}
	perf_monitor_.StartCollecting();
}

void ProcessesInfo::InitNtSetInformationProcess() {
	auto handle = GetModuleHandle(L"ntdll");
	if (!handle) {
		LOGGER->Print(L"GetModuleHandle(\"ntdll\") - not found!", Logger::Type::Error);
		return;
	}
	void* p_void = GetProcAddress(handle, "NtSetInformationProcess");
	if (!p_void) {
		LOGGER->Print(L"GetProcAddress(GetModuleHandle(\"ntdll\"), \"NtSetInformationProcess\") - not found!", Logger::Type::Error);
		return;		
	}
	NtSetInformationProcess = (pNtSetInformationProcess)p_void;
}

void ProcessesInfo::InitNtQuerySystemInformation() {
	auto handle = GetModuleHandle(L"ntdll");
	if (!handle) {
		LOGGER->Print(L"GetModuleHandle(\"ntdll\") - not found!", Logger::Type::Error);
		return;
	}
	void* p_void = GetProcAddress(handle, "NtQuerySystemInformation");
	if (!p_void) {
		LOGGER->Print(L"GetProcAddress(GetModuleHandle(\"ntdll\"), \"NtQuerySystemInformation\") - not found!", Logger::Type::Error);
		return;
	}
	NtQuerySystemInformation = (pNtQuerySystemInformation)p_void;
}

ProcessesInfo::~ProcessesInfo() {
	LOGGER->Print(L"~ProcessesInfo", Logger::Type::Trace);
}

ProcessesInfo& ProcessesInfo::AddFilter(wstring process_name) {
	process_filter_.insert(process_name);
	return *this;
}

unordered_map<ULONG, ProcessInfoShort> ProcessesInfo::ActiveProcesses() {
	ULONG buflen = 0;
	std::vector<BYTE> buffer;

	NTSTATUS lResult = NtQuerySystemInformation(SYSTEMPROCESSINFORMATION, NULL, buflen, &buflen);
	if (lResult == STATUS_INFO_LENGTH_MISMATCH) {
		buffer.resize(buflen);
	}
	else {
		LOGGER->Print(GetLastErrorAsString(), Logger::Type::Error);
		return {};
	}

	if (NtQuerySystemInformation(SYSTEMPROCESSINFORMATION, &buffer[0], buflen, &buflen)) {
		LOGGER->Print(GetLastErrorAsString(), Logger::Type::Error);
		return {};
	}

	unordered_map<ULONG, ProcessInfoShort> res;
	unsigned int i = 0;
	SYSTEM_PROCESS_INFORMATION* info = nullptr;
	do {
		info = (SYSTEM_PROCESS_INFORMATION*)&buffer[i];
		std::wstring image_name = (info->ImageName.Buffer ? info->ImageName.Buffer : L"unknow");
		if (info->ProcessId == 0) image_name = L"System Idle Process";
		if (process_filter_.size() == 0 || process_filter_.find(image_name) != process_filter_.end()) {
			auto it_process = res.insert(pair<ULONG, ProcessInfoShort>(
				info->ProcessId,
				{
					info->ProcessId,
					move(image_name),
					info->CreateTime,
					info->UserTime,
					info->KernelTime,
					{}
				}
			));

			if (LOGGER->LogType() == Logger::Type::Trace) {
				auto process_numa_group = GetProcessNumaGroup(info->ProcessId);
				wstring msg = L"";
				msg
					.append(it_process.first->second.name_)
					.append(L" pid ").append(to_wstring(it_process.first->second.pid_))
					.append(L" groups ").append(vectorToWstring(process_numa_group));
				LOGGER->Print(msg, Logger::Type::Trace);
			}
			/*auto it_process = processes_.insert(pair<ULONG, ProcessInfo>(
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
			));*/

			for (unsigned int j = 0; j < info->ThreadCount; j++) {
				it_process.first->second.threads_.push_back({
					info->ThreadInfos[j].Client_Id.UniqueThread,
					info->ThreadInfos[j].ThreadState,
					info->ThreadInfos[j].ThreadWaitReason,
					{}
					});
				HANDLE thread_handle = OpenThread(THREAD_ALL_ACCESS, FALSE, info->ThreadInfos[j].Client_Id.UniqueThread);
				if (NULL != thread_handle) {
					GetThreadGroupAffinity(thread_handle, &(it_process.first->second.threads_.back().group_affinity_));
					CloseHandle(thread_handle);
					if (LOGGER->LogType() == Logger::Type::Trace) {
						wstring msg = L"";
						msg
							.append(L"tid ").append(to_wstring(it_process.first->second.threads_.back().thread_id_))
							.append(L" group ").append(to_wstring(it_process.first->second.threads_.back().group_affinity_.Group))
							.append(L" mask ").append(to_wstring(it_process.first->second.threads_.back().group_affinity_.Mask));
						LOGGER->Print(msg, Logger::Type::Trace);
					}
				}
			}
		}
		i += info->NextOffset;
	} while (info->NextOffset != 0);
	return res;
}

void ProcessesInfo::DeleteOldProcess(unordered_map<ULONG, ProcessInfo>& lhs, const unordered_map<ULONG, ProcessInfoShort>& rhs) {
	vector<ULONG> key_delete;
	for (auto it = lhs.begin(); it != lhs.end(); ++it) {
		if (rhs.find(it->first) == rhs.end()) key_delete.push_back(it->first);
	}
	for (auto it = key_delete.begin(); it != key_delete.end(); ++it) {
		lhs.erase(*it);
	}
}

void ProcessesInfo::AddProcess(std::unordered_map<ULONG, ProcessInfo>& lhs, std::unordered_map<ULONG, ProcessInfoShort>& rhs) {
	for (auto it_rhs = rhs.begin(); it_rhs != rhs.end(); ++it_rhs) {
		auto it_lhs = lhs.find(it_rhs->first);
		if (it_lhs != lhs.end()) {
			it_lhs->second.user_time_.Add(fileTimeToLongLong(it_rhs->second.user_time_) - fileTimeToLongLong(it_lhs->second.cur_user_time_));
			it_lhs->second.cur_user_time_ = it_rhs->second.user_time_;
			it_lhs->second.threads_ = move(it_rhs->second.threads_);
			if (LOGGER->LogType() == Logger::Type::Trace && it_lhs->second.user_time_.Size() == it_lhs->second.user_time_.Capacity()) {
				wstring msg = L"AVG USER_TIME=";
				msg.append(to_wstring(it_lhs->second.user_time_.Avg()))
					.append(L" for process ").append(it_lhs->second.name_)
					.append(L" with pid ").append(to_wstring(it_lhs->second.pid_));
				LOGGER->Print(msg, Logger::Type::Trace);
			}
		}
		else {
			lhs.insert(pair<ULONG, ProcessInfo>(
				it_rhs->first,
				{
					it_rhs->second.pid_,
					move(it_rhs->second.name_),
					it_rhs->second.create_time_,
					it_rhs->second.user_time_,
					RingBuffer<LONGLONG>(ring_buffer_size_),
					move(it_rhs->second.threads_)
				}
			));
		}
	}
}

void ProcessesInfo::Read() {
	unordered_map<ULONG, ProcessInfoShort> active_processes = ActiveProcesses();
	DeleteOldProcess(processes_, active_processes);
	AddProcess(processes_, active_processes);
}

const NUMA_NODE_RELATIONSHIP* CalculateNumaWeight(const vector<ThreadInfo>& threads, const vector<NUMA_NODE_RELATIONSHIP>& numa_group) {
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

bool ProcessesInfo::IsNeedToSetAffinity(const vector<double>& values) {
	double min = values[1];
	double max = values[1];
	for (size_t i = 2; i < values.size(); ++i) {
		if (values[i] < min) min = values[i];
		if (values[i] > max) max = values[i];
	}
	return max > maximum_cpu_value_ && max - min > delta_cpu_values_;
}

void ProcessesInfo::SetAffinity() {
	if (!NtSetInformationProcess) return;
	
	vector<double> avg_values = perf_monitor_.GetAvgValues();
	if (!IsNeedToSetAffinity(avg_values)) return;
	
	vector<unordered_map<ULONG, ProcessInfo>::iterator> processes_affinity;
	for (auto it = processes_.begin(); it != processes_.end(); ++it) {
		if (it->second.user_time_.Size() == it->second.user_time_.Capacity()) processes_affinity.push_back(it);
	}
	
	sort(processes_affinity.begin(), processes_affinity.end(),
		[](unordered_map<ULONG, ProcessInfo>::iterator lhs, unordered_map<ULONG, ProcessInfo>::iterator rhs)->bool {
			return lhs->second.user_time_.Avg() > rhs->second.user_time_.Avg();
		}
	);
		
	int index_numa_group = 0;
	const NUMA_NODE_RELATIONSHIP* cur_numa = nullptr;
	for (auto it = processes_affinity.begin(); it < processes_affinity.end(); ++it) {
			
		ProcessInfo& process = (*it)->second;
		auto process_numa_groups = GetProcessNumaGroup(process.pid_);
			
		if (!cur_numa) {
			if (process_numa_groups.size() == 1) {
				cur_numa = &numa_groups_[process_numa_groups[index_numa_group]];
			}
			else if (process_numa_groups.size() > 1) {
				cur_numa = CalculateNumaWeight(process.threads_, numa_groups_);
			}
			else {
				LOGGER->Print(L"Error get numa groups for process", Logger::Type::Error);
				break;
			}
		}

		if (process_numa_groups.size() > 1 || process_numa_groups[0] != cur_numa->GroupMask.Group) {
			if (SetProcessAffinity(NtSetInformationProcess, process.pid_, &cur_numa->GroupMask)) {
				LOGGER->Print(
					wstring(process.name_).
					append(L";pid=").append(to_wstring(process.pid_)).
					append(L";numa=").append(vectorToWstring(process_numa_groups)).
					append(L";new numa=[").append(to_wstring(cur_numa->GroupMask.Group)).append(L"]"),
					true
				);
			}
			else {
				LOGGER->Print(L"Error set process affinity!", Logger::Type::Error);
			}
		}

		for (auto it_thread = process.threads_.begin(); it_thread < process.threads_.end(); ++it_thread) {
			if (it_thread->group_affinity_.Group != cur_numa->GroupMask.Group) {
				if (SetThreadAffinity(it_thread->thread_id_, &cur_numa->GroupMask)) {
					LOGGER->Print(
						wstring(process.name_).
						append(L";pid=").append(to_wstring(process.pid_)).
						append(L";tid=").append(to_wstring(it_thread->thread_id_)).
						append(L";numa=[").append(to_wstring(it_thread->group_affinity_.Group)).append(L"]").
						append(L";new numa=[").append(to_wstring(cur_numa->GroupMask.Group)).append(L"]"),
						true
					);
				}
				else {
					LOGGER->Print(L"Error set thread affinity!", Logger::Type::Error);
				}
			}
		}			

		++index_numa_group;
		if (index_numa_group >= numa_groups_.size()) index_numa_group = 0;
		cur_numa = &numa_groups_[index_numa_group];
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
			numa_groups_.push_back(p->NumaNode);
			if (LOGGER->LogType() == Logger::Type::Trace) {
				wstring msg = L"";
				msg
					.append(L"NodeNumber=").append(to_wstring(p->NumaNode.NodeNumber))
					.append(L";GroupMask.Group=").append(to_wstring(p->NumaNode.GroupMask.Group))
					.append(L";GroupMask.Mask=").append(to_wstring(p->NumaNode.GroupMask.Mask));
				LOGGER->Print(msg, Logger::Type::Trace);
			}
		}
	}
}

vector<USHORT> ProcessesInfo::GetProcessNumaGroup(ULONG id_process) {
	HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, id_process);
	if (hProcess != NULL) {
		std::vector<USHORT> GroupArray(128);
		USHORT GroupCount = static_cast<USHORT>(GroupArray.size());
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