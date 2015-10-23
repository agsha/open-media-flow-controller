/*
 * CMMNodeStatus.h - Manage CMM node (IP:Port) status information
 *
 *	Remove redundant node status requests genereted by different
 *	namespaces by maintaining a node status cache which is indexed by
 *	host:port.
 *	Namespace's identify nodes via NodeHandle(s) (token:instance:host_port)
 *	which contain the host:port.
 *
 *	Prior to initiating a status request to a node, the CMMNodeStatusCache
 *	object is consulted to (1) Wait on a pending request to the node;
 *	(2) Use the cached status if valid; (3) Initiate a status request to the
 *	node.
 */
#ifndef CMMNODESTATUS_H
#define CMMNODESTATUS_H

#include <time.h>
#include "curl/curl.h"

#include <string>
#include <list>
#include <ext/hash_map>

#include "CMMConnectionSM.h"

class CURLReqCompletionData
{
    public:
    	CURLReqCompletionData();
    	~CURLReqCompletionData();
    	void Set(CompletionType type, const CURLMsg* cmsg, 
		 const CURLRequestStats* curlreqstats,
		 const NvsdMgmtRespStatus* mgmt_st);
	CompletionType GetType() const;
	const CURLMsg* const GetMsg() const;
	const CURLRequestStats* const GetStats() const;
	const NvsdMgmtRespStatus* const GetMgmtSt() const;

    private:
    	CompletionType		_completion_type;
	CURLMsg			_curlmsg;
	CURLRequestStats	_curlreq_stats;
	NvsdMgmtRespStatus	_mgmt_st;
};

class CMMNodeStatus {
    public:
    	CMMNodeStatus(const std::string& host_ip_port);
    	~CMMNodeStatus();

    	void Update(CompletionType type, const CURLMsg* cmsg,
		    const CURLRequestStats* cmsg_stats,
		    const NvsdMgmtRespStatus* mgmt_st,
		    const struct timespec* update_time);
	bool RequestPending() const;
	bool ValidState(const struct timespec* now, 
			long update_interval_msecs) const;
	void SetRequestPending(const std::string& node_handle);

	// Returns: True, if node_handle is the pending request
	bool ClearRequestPending(std::string& node_handle);

	const CURLReqCompletionData* const GetCURLCompData() const;

	bool isStale(const struct timespec* now, 
		     int64_t inactivity_interval_msecs) const;
	const std::string* const GetHostPort() const;

	// Returns: 0 => Success
	int WaiterQueuePush(const std::string& node_handle);

	// Returns: 0 => Success
	int WaiterQueuePop(std::string& node_handle);

    private:
   	std::string 	_ip_port; // Associated host IP:Port
	bool 		_request_pending; // Request pending for node status
	std::string	_pendingSMNodeHandle; // Instance with pending request
	struct timespec _update_time; // Time of last update

	// Node status information associated with last request
	CURLReqCompletionData	_curlreqcomp_data;

	// Waiters on pending request
	std::list<std::string> _waiter_queue;
};

typedef hash_map<std::string, CMMNodeStatus*> CMMNodeStatusHash_t;
typedef hash_map<std::string, CMMNodeStatus*>::iterator CMMNodeStatusHashIter_t;

class IntReqCompleteData
{
    public:
    	IntReqCompleteData(const std::string& node_handle,
			   const CURLReqCompletionData* crcd);
	~IntReqCompleteData();

	std::string _node_handle;
	CURLReqCompletionData _crcd;
};

class CMMNodeStatusCache {
    public:
	CMMNodeStatusCache();
	~CMMNodeStatusCache();

	// Cache operations

	// Returns: 0 => Success
	int Add(const std::string& node_ip_port, 
		CMMNodeStatus* node_status_info);

	// Returns: 0 => Success
	int Delete(const std::string& node_ip_port);

	// Returns: !=0 => Success
	CMMNodeStatus* Lookup(const std::string& node_ip_port,
			      CMMNodeStatusHashIter_t* piter = 0);
	// Returns: Number flushed entries
	int Flush(const struct timespec* now); //Remove stale entries

	// SM completion handling

	// Returns: 0 => Success
	int CompletionQueuePush(const std::string& node_handle, 
				CMMNodeStatus* node_status);
	// Returns: 0 => Success
	int CompletionQueuePop(std::string& node_handle, CompletionType& type,
			       CURLMsg& cmsg, CURLRequestStats& creq_stats,
			       NvsdMgmtRespStatus& mgmt_st);

    private:
    	// Node IP:Port => CMMNodeStatus hash map
    	CMMNodeStatusHash_t _ns_hash_map;

	// CMMConnectionSM internal request completion queue
	std::list<IntReqCompleteData*> _completion_q;

	struct timespec _last_flush_time;
};

#endif /* CMMNODESTATUS_H */

/*
 * End of CMMNodeStatus.h
 */
