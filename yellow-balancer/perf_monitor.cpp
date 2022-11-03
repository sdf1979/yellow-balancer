#include "perf_monitor.h"

using namespace std;

static auto LOGGER = Logger::getInstance();

void StartCollectingThread(PerfMonitor* perf_monitor) {
	perf_monitor->is_collect_ = true;
	while (perf_monitor->is_collect_) {
		auto start = chrono::high_resolution_clock::now();
		perf_monitor->Collect();
		this_thread::sleep_for(chrono::microseconds(1000000) - chrono::duration_cast<chrono::microseconds>(chrono::high_resolution_clock::now() - start));
	}
	perf_monitor->collector_thread_id_ = std::thread::id();
}

PerfMonitor::~PerfMonitor(){
	is_collect_ = false;
	while (collector_thread_id_ != std::thread::id()) {
		this_thread::sleep_for(std::chrono::milliseconds(100));
	}
	for (auto it = counters_.begin(); it < counters_.end(); ++it) {
		GlobalFree(*it);
		PdhRemoveCounter(*it);
	}
	if (pdh_query_) {
		PdhCloseQuery(pdh_query_);
	}
	LOGGER->Print(L"~PerfMonitor", Logger::Type::Trace);
}

void PerfMonitor::SetCollectionPeriod(int collection_period) {
	collection_period_ = collection_period;
	for (auto it = counters_values_.begin(); it < counters_values_.end(); ++it) {
		*it = RingBuffer<double>(collection_period_);
	}
}

void PerfMonitor::AddCounter(const wstring& full_name) {
	if (!pdh_query_) {
		if (PdhOpenQueryW(NULL, NULL, &pdh_query_) != ERROR_SUCCESS) {
			pdh_query_ = NULL;
			LOGGER->Print(L"Error open query", Logger::Type::Error);
		}
	}
	counters_.push_back((HCOUNTER*)GlobalAlloc(GPTR, sizeof(HCOUNTER)));
	PdhAddEnglishCounterW(pdh_query_, &full_name[0], 0, counters_.back());
	counters_name_.push_back(full_name);
	counters_values_.push_back(RingBuffer<double>(collection_period_));
}

void PerfMonitor::StartCollecting() {
	collector_thread_ = thread(StartCollectingThread, this);
	collector_thread_id_ = collector_thread_.get_id();
	collector_thread_.detach();
}

void PerfMonitor::StopCollecting() {
	is_collect_ = false;
	std::thread::id empty;
	while (collector_thread_id_ != empty) {
		this_thread::sleep_for(std::chrono::milliseconds(100));
	}
}

void PerfMonitor::Collect() {
	if (pdh_query_) {
		PDH_STATUS pdhStatus;
		pdhStatus = PdhCollectQueryData(pdh_query_);
		if (pdhStatus == ERROR_SUCCESS) {
			PDH_FMT_COUNTERVALUE pdhValue;
			DWORD dwType;
			lock_guard<mutex> guard(access_counters_);
			for (int i =0; i < counters_.size(); ++i) {
				pdhStatus = PdhGetFormattedCounterValue(*counters_[i], PDH_FMT_DOUBLE, &dwType, &pdhValue);
				if (pdhStatus == ERROR_SUCCESS) {
					counters_values_[i].Add(pdhValue.doubleValue);
				}
				if (LOGGER->LogType() == Logger::Type::Trace) {
					if (pdhStatus == PDH_INVALID_ARGUMENT) {
						LOGGER->Print(L"PDH_INVALID_ARGUMENT", Logger::Type::Trace);
					}
					else if (pdhStatus == PDH_INVALID_DATA) {
						LOGGER->Print(L"PDH_INVALID_DATA", Logger::Type::Trace);
					}
					else if (pdhStatus == PDH_INVALID_HANDLE) {
						LOGGER->Print(L"PDH_INVALID_HANDLE", Logger::Type::Trace);
					}
					else {
						LOGGER->Print(L"PDH_UNKNOW_HANDLE", Logger::Type::Trace);
					}
				}
			}
		}
		else if(pdhStatus == PDH_INVALID_HANDLE){
			LOGGER->Print(L"PDH_INVALID_HANDLE", Logger::Type::Trace);
		}
		else if (pdhStatus == PDH_NO_DATA) {
			LOGGER->Print(L"PDH_NO_DATA", Logger::Type::Trace);
		}
		else {
			LOGGER->Print(L"PDH_UNKNOW_HANDLE", Logger::Type::Trace);
		}
	}
}

vector<double> PerfMonitor::GetAvgValues() {
	vector<double> res(counters_.size(), 0);
	{
		lock_guard<mutex> guard(access_counters_);
		auto it_res = res.begin();
		for (auto it = counters_values_.begin(); it < counters_values_.end(); ++it) {
			if (it->Size() == it->Capacity()) {
				*it_res = it->Avg();
				++it_res;
			}
		}
	}
	if (LOGGER->LogType() == Logger::Type::Trace) {
		for (size_t i = 0; i < counters_name_.size(); ++i) {
			wstring msg = L"AVG for ";
			msg.append(counters_name_[i]);
			msg.append(L"=").append(to_wstring(res[i]));
			LOGGER->Print(msg, Logger::Type::Trace);
		}
	}
	return res;
}