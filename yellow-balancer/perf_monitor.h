#pragma once

#include <Pdh.h>
#include <PdhMsg.h>
#include <string>
#include <vector>
#include <thread>
#include <optional>
#include <mutex>
#include <numeric>
#include "Logger.h"
#include "ring_buffer.h"

#pragma comment(lib,"pdh.lib")

class PerfMonitor;
void StartCollectingThread(PerfMonitor* perf_monitor);

class PerfMonitor{
public:
	void SetCollectionPeriod(int collection_period);
	int CollectionPeriod() { return collection_period_; }
	void AddCounter(const std::wstring& full_name);
	void StartCollecting();
	void StopCollecting();
	std::vector<double> GetAvgValues();
	~PerfMonitor();
private:
	friend void StartCollectingThread(PerfMonitor* perf_monitor);
	PDH_HQUERY pdh_query_ = NULL;
	int collection_period_ = 60;
	std::vector<PDH_HCOUNTER*> counters_;
	std::vector<std::wstring> counters_name_;
	std::vector<RingBuffer<double>> counters_values_;
	std::thread collector_thread_;
	bool is_collect_ = false;
	std::thread::id collector_thread_id_;
	void Collect();
	std::mutex access_counters_;
};
