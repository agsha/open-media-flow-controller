/*
 * CMMNodeStatus.cc - Manage CMM node (IP:Port) status information
 */
#include <stdint.h>
#include <string.h>
#include "cmm_misc.h"
#include "CMMNodeStatus.h"

////////////////////////////////////////////////////////////////////////////////
// Classs CURLReqCompletionData
////////////////////////////////////////////////////////////////////////////////

CURLReqCompletionData::CURLReqCompletionData()
{
    memset(&_curlmsg, 0, sizeof(_curlmsg));
    memset(&_curlreq_stats, 0, sizeof(_curlreq_stats));
}

CURLReqCompletionData::~CURLReqCompletionData()
{
}

void
CURLReqCompletionData::Set(CompletionType type, const CURLMsg* cmsg, 
			   const CURLRequestStats* creq_stats,
			   const NvsdMgmtRespStatus* mgmt_st)
{
    _completion_type = type;
    _curlmsg = *cmsg;
    _curlreq_stats = *creq_stats;
    _mgmt_st = *mgmt_st;
}

CompletionType CURLReqCompletionData::GetType() const
{
    return _completion_type;
}

const CURLMsg* const CURLReqCompletionData::GetMsg() const
{
    return &_curlmsg;
}

const CURLRequestStats* const CURLReqCompletionData::GetStats() const
{
    return &_curlreq_stats;
}

const NvsdMgmtRespStatus* const CURLReqCompletionData::GetMgmtSt() const
{
    return &_mgmt_st;
}

////////////////////////////////////////////////////////////////////////////////
// Class CMMNodeStatus
////////////////////////////////////////////////////////////////////////////////

CMMNodeStatus::CMMNodeStatus(const std::string& host_ip_port)
{
    _ip_port = host_ip_port;
    _request_pending = false;
    memset(&_update_time, 0, sizeof(_update_time));
}

CMMNodeStatus::~CMMNodeStatus()
{
}

void
CMMNodeStatus::Update(CompletionType type, const CURLMsg* cmsg, 
		      const CURLRequestStats* creq_stats, 
		      const NvsdMgmtRespStatus* mgmt_st,
		      const struct timespec* update_time)
{
    _update_time = *update_time;
    _curlreqcomp_data.Set(type, cmsg, creq_stats, mgmt_st);
}

bool 
CMMNodeStatus::RequestPending() const
{
    return _request_pending;
}

bool
CMMNodeStatus::ValidState(const struct timespec* now, 
			  long update_interval_msecs) const
{
    struct timespec ts = _update_time;

    timespec_add_msecs(&ts, update_interval_msecs);
    if (timespec_cmp((const struct timespec*)&ts, now) >= 0) {
    	return true;
    } else {
    	return false;
    }
}

void 
CMMNodeStatus::SetRequestPending(const std::string& node_handle)
{
    _request_pending = true;
    _pendingSMNodeHandle = node_handle;
}

bool
CMMNodeStatus::ClearRequestPending(std::string& node_handle)
{
    if (node_handle == _pendingSMNodeHandle) {
    	_request_pending = false;
	_pendingSMNodeHandle.clear();
	return true;
    } else {
	return false;
    }
}

const CURLReqCompletionData*  const 
CMMNodeStatus::GetCURLCompData() const
{
    return &_curlreqcomp_data;
}

bool
CMMNodeStatus::isStale(const struct timespec* now, 
		int64_t inactivity_interval_msecs) const
{
    int64_t msecs;

    msecs = timespec_diff_msecs(&_update_time, now);
    if (msecs < inactivity_interval_msecs) {
    	return false; // Not stale
    }

    if (!_request_pending && !_waiter_queue.size()) {
    	return true; // Stale
    } else {
    	return false; // Not stale
    }
}

const std::string* const CMMNodeStatus::GetHostPort() const
{
    return &_ip_port;
}

int
CMMNodeStatus::WaiterQueuePush(const std::string& node_handle)
{
    _waiter_queue.push_back(node_handle);
    return 0;
}

int
CMMNodeStatus::WaiterQueuePop(std::string& node_handle)
{
    if (_waiter_queue.size()) {
	node_handle = _waiter_queue.front();
	_waiter_queue.pop_front();
    	return 0;
    } else {
    	return 1;
    }
}

////////////////////////////////////////////////////////////////////////////////
// Class IntReqCompleteData
////////////////////////////////////////////////////////////////////////////////
IntReqCompleteData::IntReqCompleteData(const std::string& node_handle,
				       const CURLReqCompletionData *crcd)
{
    _node_handle = node_handle;
    _crcd = *crcd;
}

IntReqCompleteData::~IntReqCompleteData()
{
}

////////////////////////////////////////////////////////////////////////////////
// Class CMMNodeStatusCache
////////////////////////////////////////////////////////////////////////////////

CMMNodeStatusCache::CMMNodeStatusCache()
{
    clock_gettime(CLOCK_MONOTONIC, &_last_flush_time);
}

CMMNodeStatusCache::~CMMNodeStatusCache()
{
    std::string node_handle;
    CompletionType type;
    CURLMsg cmsg; 
    CURLRequestStats creq_stats;
    NvsdMgmtRespStatus mgmt_st;

    if (!_ns_hash_map.empty()) {
    	CMMNodeStatusHashIter_t iter;

    	for (iter = _ns_hash_map.begin(); 
	     iter != _ns_hash_map.end(); iter++) {
	    delete iter->second;
    	}
    }

    while (!CompletionQueuePop(node_handle, type, cmsg, creq_stats, mgmt_st)) {
    	;
    }
}

int
CMMNodeStatusCache::Add(const std::string& node_ip_port, 
			CMMNodeStatus* node_status_info)
{
    if (!Lookup(node_ip_port)) {
    	_ns_hash_map[node_ip_port] = node_status_info;
	return 0;
    }
    return 1; // Not added
}

int
CMMNodeStatusCache::Delete(const std::string& node_ip_port)
{
    CMMNodeStatus* ns;
    CMMNodeStatusHashIter_t iter;

    ns = Lookup(node_ip_port, &iter);
    if (ns) {
    	delete ns;
	_ns_hash_map.erase(iter);
	return 0;
    }
    return 1; // Not deleted
}

CMMNodeStatus*
CMMNodeStatusCache::Lookup(const std::string& node_ip_port, 
			   CMMNodeStatusHashIter_t* piter)
{
    CMMNodeStatusHashIter_t iter;

    if (!piter) {
    	piter = &iter;
    }

    *piter = _ns_hash_map.find(node_ip_port);
    if (*piter != _ns_hash_map.end()) {
    	return (*piter)->second;
    } else {
    	return NULL;
    }
}

int
CMMNodeStatusCache::Flush(const struct timespec* now)
{
    int64_t msecs;
    CMMNodeStatusHashIter_t iter;
    int flushed_entries = 0;
    std::list<std::string> hash_delete_list;

    const int64_t SCAN_INTERVAL_MSECS =  300 * MILLI_SECS_PER_SEC;
    const int64_t STALE_INTERVAL_MSECS = 120 * MILLI_SECS_PER_SEC;

    msecs = timespec_diff_msecs(&_last_flush_time, now);
    if (msecs < SCAN_INTERVAL_MSECS) {
        return flushed_entries;
    }
    _last_flush_time = *now;

    // Delete stale entries
    for (iter = _ns_hash_map.begin(); iter != _ns_hash_map.end(); iter++) {
	if (iter->second->isStale(now, STALE_INTERVAL_MSECS)) {
	    hash_delete_list.push_back(*iter->second->GetHostPort());
	    delete iter->second;
	    flushed_entries++;
	}
    }

    // Delete associated hash table entries
    while (hash_delete_list.size()) {
    	_ns_hash_map.erase(hash_delete_list.front());
	hash_delete_list.pop_front();
    }
    return flushed_entries;
}

int CMMNodeStatusCache::CompletionQueuePush(const std::string& node_handle, 
					    CMMNodeStatus* node_status)
{
    IntReqCompleteData *ircd;

    ircd = new IntReqCompleteData(node_handle, node_status->GetCURLCompData());
    _completion_q.push_back(ircd);

    return 0;

}

int CMMNodeStatusCache::CompletionQueuePop(std::string& node_handle, 
					   CompletionType& type,
					   CURLMsg& cmsg, 
					   CURLRequestStats& creq_stats,
					   NvsdMgmtRespStatus& mgmt_st)
{
    IntReqCompleteData *ircd;

    if (_completion_q.size()) {
	ircd = _completion_q.front();
	node_handle = ircd->_node_handle;
	type = ircd->_crcd.GetType();
	cmsg = *(ircd->_crcd.GetMsg());
	creq_stats = *(ircd->_crcd.GetStats());
	mgmt_st = *(ircd->_crcd.GetMgmtSt());

	_completion_q.pop_front();
	delete ircd;
    	return 0;
    } else {
    	return 1;
    }
}

/*
 * End of CMMNodeStatus.cc
 */
