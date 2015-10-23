/*
 * CMMConnectionSM.cc - CMM Connection State Machine 
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <uuid/uuid.h>
#include <assert.h>

#include <string>
#include <ext/hash_map>
using namespace __gnu_cxx;

#include "cmm_defs.h"
#include "cmm_misc.h"
#include "cmm_output_fqueue.h"
#include "CMMConnectionSM.h"
#include "CMMNodeStatus.h"

struct CURLMsg null_CURLMsg;

extern CMMSharedMemoryMgr* CMMNodeInfo;
extern CMMSharedMemoryMgr* CMMNodeLoadMetric;

extern CMMNodeStatusCache* CMMNodeCache;

////////////////////////////////////////////////////////////////////////////////
// Public functions
////////////////////////////////////////////////////////////////////////////////

CMMConnectionSM::CMMConnectionSM(CURLM* mch, const cmm_node_config_t* config,
				 const char* ns_name, int ns_name_strlen)
{
    int rv;
    int retval = 0;
    struct timespec now;

    while (1) { // Begin while

    rv = CMMNodeInfo->CMMGetShm(_shm_data, _shm_datasize, _shm_token);
    if (rv) {
    	_shm_data = NULL;
	DBG("[%s] CMMNodeInfo CMMGetShm() failed, rv=%d this=%p, "
	    "disabling use of shared memory", config->node_handle, rv, this);
	::g_getshm_data_failed++;
    }
    _shm_data_offset = 0;

    rv = CMMNodeLoadMetric->CMMGetShm(*((char**)&_shm_loadmetric_data), 
				      _shm_loadmetric_datasize, 
				      _shm_loadmetric_token);
    if (rv) {
    	_shm_loadmetric_data = NULL;
	DBG("[%s] CMMNodeLoadMetric CMMGetShm() failed, rv=%d this=%p, "
	    "disabling use of shared memory", config->node_handle, rv, this);
	::g_getshm_lm_failed++;
    }

    _nsName = std::string(ns_name, ns_name_strlen);
    _mch = mch;
    rv = SetConfig(config);
    if (rv) {
    	DBG("[%s] SetConfig() failed, rv=%d", config->node_handle, rv);
	retval = 1;
	break;
    }

    // CURL interface initializations
    rv = CURLinit();
    if (rv) {
    	DBG("[%s] CURLinit() failed, rv=%d", config->node_handle, rv);
	retval = 2;
	break;
    }

    clock_gettime(CLOCK_MONOTONIC, &now);
    _incarnation = _connSMIncarnation++;
    _schedule_at = now;
    _completion = NULLtimespec;
    _last_update = now;
    _cancel = false;
    _running = false;
    _state = SM_INIT;
    _failedRequests = 0;
    _node_online_time = NULLtimespec;
    _op_status_success = 0;
    _op_status_timeout = 0;
    _op_status_other = 0;
    _op_status_unexpected = 0;
    _op_status_online = 0;
    _op_status_offline = 0;

    _curl_op_no_connect = 0;
    _curl_op_timeout = 0;
    _curl_op_snd_err = 0;
    _curl_op_recv_err = 0;

    // LoadMetric shared memory initialization
    if (_shm_loadmetric_data) {
    	_shm_loadmetric_data->u.loadmetric.load_metric = 0;
    	_shm_loadmetric_data->u.loadmetric.incarnation = _incarnation;
	strncpy(_shm_loadmetric_data->u.loadmetric.node_handle,
		_nodeHandle.c_str(), 
		sizeof(_shm_loadmetric_data->u.loadmetric.node_handle)-1);
    }

    ::g_active_SM++;
    return; // Success

    } // End while

    // Error exit
    if (_shm_data) {
    	rv = CMMNodeInfo->CMMFreeShm(_shm_token);
	if (rv) {
	    DBG("[%s:%s] CMMNodeInfo CMMFreeShm(token=%d) failed, "
	    	"rv=%d data=%p", 
	    	_nsName.c_str(), _nodeHandle.c_str(), _shm_token, 
		rv, _shm_data);
	}
    }

    if (_shm_loadmetric_data) {
    	rv = CMMNodeLoadMetric->CMMFreeShm(_shm_loadmetric_token);
	if (rv) {
	    DBG("[%s:%s] CMMNodeLoadMetric CMMFreeShm(token=%d) failed, "
	    	"rv=%d data=%p", 
		_nsName.c_str(), _nodeHandle.c_str(), _shm_loadmetric_token, 
		rv, _shm_loadmetric_data);
	}
    }
    // TBD: Throw exception
    return; // Error
}

CMMConnectionSM::~CMMConnectionSM()
{
    int rv;
    CMMConnectionSMHash_t::iterator itr;
    CMMConnectionSM* pConnSM = NodeHandleToSM(_nodeHandle.c_str(), &itr);

    if (pConnSM) {
	// Remove from hash table
	_nodeHandleHash.erase(itr);
    }

    CURLfree();
    if (_shm_data) {
    	rv = CMMNodeInfo->CMMFreeShm(_shm_token);
	if (rv) {
	    DBG("[%s:%s] CMMNodeInfo CMMFreeShm(token=%d) failed, "
	    	"rv=%d data=%p", 
	    	_nsName.c_str(), _nodeHandle.c_str(), _shm_token, 
		rv, _shm_data);
	}
    }

    if (_shm_loadmetric_data) {
    	_shm_loadmetric_data->u.loadmetric.load_metric = 0;
    	_shm_loadmetric_data->u.loadmetric.incarnation = 0;

    	rv = CMMNodeLoadMetric->CMMFreeShm(_shm_loadmetric_token);
	if (rv) {
	    DBG("[%s:%s] CMMNodeLoadMetric CMMFreeShm(token=%d) failed, "
	    	"rv=%d data=%p", 
	    	_nsName.c_str(), _nodeHandle.c_str(), 
		_shm_loadmetric_token, rv, _shm_loadmetric_data);
	}
    }
    ::g_active_SM--;
}

int
CMMConnectionSM::ProcessResponseBody(const char* buf, size_t bytes)
{
    return _response_body.AddData(buf, bytes);
}

void
CMMConnectionSM::SetMgmtResp(const NvsdMgmtRespStatus& st)
{
    _response_body.SetBodyData(st);
}

int
CMMConnectionSM::CompletionHandler(CompletionType type, const CURLMsg* msg)
{
    int rv;
    CURLMcode mret;
    NvsdMgmtRespStatus mgmt_st;

    _running = false;
    clock_gettime(CLOCK_MONOTONIC, &_completion);

    _response_body.GetBodyData(mgmt_st);
    rv = UpdateNodeStatus(type, msg, mgmt_st);

    if (type != CB_INTERNAL_REQ_COMPLETE) {
    	mret = curl_multi_remove_handle(_mch, _ech);
    	if (mret != CURLM_OK) {
		DBG("[%s:%s] curl_multi_remove_handle(mch=%p, ech=%p) failed, "
		    "rv=%d", _nsName.c_str(), _nodeHandle.c_str(), 
		    _mch, _ech, mret);
    	}
    }

    switch (type) {
    case CB_MULTI_SCHED_FAILED:
	rv = PerformActions(msg, false);
	if (rv) {
	    DBG("[%s:%s] PerformActions() failed, rv=%d", 
	    	_nsName.c_str(), _nodeHandle.c_str(), rv);
	}
    	break;

    case CB_REQ_COMPLETE:
    case CB_INTERNAL_REQ_COMPLETE:
    {
	rv = PerformActions(msg, (type == CB_REQ_COMPLETE ? true : false));
	if (rv) {
	    DBG("[%s:%s] PerformActions() failed, rv=%d", 
	    	_nsName.c_str(), _nodeHandle.c_str(), rv);
	}
    	break;
    }

    default:
    	// Should never happen
	DBG("[%s:%s] Invalid type=%d mch=%p ech=%p", 
	    _nsName.c_str(), _nodeHandle.c_str(), type, _mch, _ech);
	assert(!"Invalid type");
    	break;
    }
    return 0;
}

////////////////////////////////////////////////////////////////////////////////
// Private functions
////////////////////////////////////////////////////////////////////////////////

CMMConnectionSM::CMMConnectionSM() // Disallow
{
}

CMMConnectionSM::CMMConnectionSM(CMMConnectionSM& a) // Disallow
{
    (void)a;
}

CMMConnectionSM&
CMMConnectionSM::operator=(const CMMConnectionSM a) // Disallow
{
    (void)a;

    return *this;
}

int
CMMConnectionSM::SetConfig(const cmm_node_config_t* config, 
			   bool allocShmIfNone)
{
    int rv;
    uint64_t ns_token;
    int instance;
    char host_port[4096];

    if (allocShmIfNone) {
    	if (!_shm_data) {
	    rv = CMMNodeInfo->CMMGetShm(_shm_data, _shm_datasize, _shm_token);
	    if (!rv) {
	    	::g_getshm_data_retry_success++;
	    } else {
	    	::g_getshm_data_retry_failed++;
	    }
    	}
    	if (!_shm_loadmetric_data) {
	    rv = CMMNodeLoadMetric->CMMGetShm(*((char**)&_shm_loadmetric_data), 
				  _shm_loadmetric_datasize, 
				  _shm_loadmetric_token);
	    if (!rv) {
	    	::g_getshm_lm_retry_success++;
	    } else {
	    	::g_getshm_lm_retry_failed++;
	    }
    	}
    }

    _nodeHandle = config->node_handle;
    rv = cmm_node_handle_deserialize(config->node_handle, 
				     strlen(config->node_handle), &ns_token,
				     &instance, host_port);
    if (rv) {
    	return 1; // Failure
    }
    _nodeHostPort = host_port;
    _heartBeatURL = config->heartbeaturl;

    _heartBeatInterval = config->heartbeat_interval;
    BoundValue(_heartBeatInterval, 
    	       _min_heartBeatInterval, _max_heartBeatInterval);

    _connectTimeout = config->connect_timeout;
    BoundValue(_connectTimeout, _min_connectTimeout, _max_connectTimeout);

    _allowedConnectFailures = config->allowed_connect_failures;
    BoundValue(_allowedConnectFailures,
    	       _min_allowedConnectFailures, _max_allowedConnectFailures);

    _readTimeout = config->read_timeout;
    BoundValue(_readTimeout, _min_readTimeout, _max_readTimeout);

    if (_shm_data) { // Copy config to shared memory data with trailing space
        rv = snprintf(_shm_data, _shm_datasize, 
		      "%s=%s|%s=%s|%s=%s|%s=%d|%s=%d|%s=%d|%s=%d| ",
		      CMM_NM_NAMESPACE, _nsName.c_str(),
		      CMM_NM_NODE, _nodeHandle.c_str(), 
		      CMM_NM_CFG_HEARTBEAT_URL, _heartBeatURL.c_str(),
		      CMM_NM_CFG_HEARTBEAT_INTERVAL, _heartBeatInterval,
		      CMM_NM_CFG_CONNECT_TIMEOUT, _connectTimeout,
		      CMM_NM_CFG_ALLOWED_FAILS, _allowedConnectFailures,
		      CMM_NM_CFG_READ_TIMEOUT, _readTimeout);
	if (rv >= _shm_datasize) {
	    _shm_data[_shm_datasize-1] = '\0';
	    DBG("[%s:%s] Buffer size exceeded, rv=%d bufsize=%d this=%p", 
	    	_nsName.c_str(), _nodeHandle.c_str(), rv, _shm_datasize, this);
	}
	_shm_data_offset = rv;
    }
    return 0; // Success
}

int
CMMConnectionSM::CURLinit()
{
    int retval = 0;
    CURLcode eret;
    long arglval;

    while (1) { // Begin while

    _ech = curl_easy_init();
    if (!_ech) {
    	DBG("[%s:%s] curl_easy_init() failed, ech=%p", 
	    _nsName.c_str(), _nodeHandle.c_str(), _ech);
	retval = 1;
	break;
    }

    eret = curl_easy_setopt(_ech, CURLOPT_PRIVATE, this); 
    if (eret != CURLE_OK) {
    	DBG("[%s:%s] curl_easy_setopt(CURLOPT_PRIVATE) failed, rv=%d", 
	    _nsName.c_str(), _nodeHandle.c_str(), eret);
	retval = 2;
	break;
    }

    eret = curl_easy_setopt(_ech, CURLOPT_URL, _heartBeatURL.c_str());
    if (eret != CURLE_OK) {
    	DBG("[%s:%s] curl_easy_setopt(CURLOPT_URL) failed, rv=%d", 
	    _nsName.c_str(), _nodeHandle.c_str(), eret);
	retval = 3;
	break;
    }

    eret = curl_easy_setopt(_ech, CURLOPT_WRITEFUNCTION, 
    			    &CMMConnectionSM::CURLWriteData); 
    if (eret != CURLE_OK) {
    	DBG("[%s:%s] curl_easy_setopt(CURLOPT_WRITEFUNCTION) failed, rv=%d", 
	    _nsName.c_str(), _nodeHandle.c_str(), eret);
	retval = 4;
	break;
    }

    eret = curl_easy_setopt(_ech, CURLOPT_WRITEDATA, this); 
    if (eret != CURLE_OK) {
    	DBG("[%s:%s] curl_easy_setopt(CURLOPT_WRITEDATA) failed, rv=%d", 
	    _nsName.c_str(), _nodeHandle.c_str(), eret);
	retval = 5;
	break;
    }

    eret = curl_easy_setopt(_ech, CURLOPT_DEBUGFUNCTION,
    			    &CMMConnectionSM::CURLDebugInfo); 
    if (eret != CURLE_OK) {
    	DBG("[%s:%s] curl_easy_setopt(CURLOPT_DEBUGFUNCTION) failed, rv=%d", 
	    _nsName.c_str(), _nodeHandle.c_str(), eret);
	retval = 6;
	break;
    }

    eret = curl_easy_setopt(_ech, CURLOPT_DEBUGDATA, this); 
    if (eret != CURLE_OK) {
    	DBG("[%s:%s] curl_easy_setopt(CURLOPT_DEBUGDATA) failed, rv=%d", 
	    _nsName.c_str(), _nodeHandle.c_str(), eret);
	retval = 7;
	break;
    }

    arglval = 1;
    eret = curl_easy_setopt(_ech, CURLOPT_VERBOSE, arglval);
    if (eret != CURLE_OK) {
    	DBG("[%s:%s] curl_easy_setopt(CURLOPT_VERBOSE) failed, rv=%d", 
	    _nsName.c_str(), _nodeHandle.c_str(), eret);
	retval = 8;
	break;
    }

    arglval = 1;
    eret = curl_easy_setopt(_ech, CURLOPT_NOSIGNAL, arglval);
    if (eret != CURLE_OK) {
    	DBG("[%s:%s] curl_easy_setopt(CURLOPT_NOSIGNAL) failed, rv=%d", 
	    _nsName.c_str(), _nodeHandle.c_str(), eret);
	retval = 9;
	break;
    }

    arglval = _connectTimeout;
    eret = curl_easy_setopt(_ech, CURLOPT_CONNECTTIMEOUT_MS, arglval);
    if (eret != CURLE_OK) {
    	DBG("[%s:%s] curl_easy_setopt(CURLOPT_CONNECTTIMEOUT_MS) failed, "
	    "rv=%d", _nsName.c_str(), _nodeHandle.c_str(), eret);
	retval = 10;
	break;
    }

    arglval = _readTimeout;
    eret = curl_easy_setopt(_ech, CURLOPT_TIMEOUT_MS, arglval);
    if (eret != CURLE_OK) {
    	DBG("[%s:%s] curl_easy_setopt(CURLOPT_TIMEOUT_MS) failed, rv=%d", 
	    _nsName.c_str(), _nodeHandle.c_str(), eret);
	retval = 11;
	break;
    }

    if (!::enable_connectionpool) {
    	arglval = 1;
    	eret = curl_easy_setopt(_ech, CURLOPT_FRESH_CONNECT, arglval);
    	if (eret != CURLE_OK) {
	    DBG("[%s:%s] curl_easy_setopt(CURLOPT_FRESH_CONNECT) failed, "
	    	"rv=%d", _nsName.c_str(), _nodeHandle.c_str(), eret);
	    retval = 12;
	    break;
    	}

    	arglval = 1;
    	eret = curl_easy_setopt(_ech, CURLOPT_FORBID_REUSE, arglval);
    	if (eret != CURLE_OK) {
	    DBG("[%s:%s] curl_easy_setopt(CURLOPT_FORBID_REUSE) failed, rv=%d", 
	    	_nsName.c_str(), _nodeHandle.c_str(), eret);
	    retval = 13;
	    break;
    	}
    }

    arglval = CURL_IPRESOLVE_V4;
    eret = curl_easy_setopt(_ech, CURLOPT_IPRESOLVE, arglval);
    if (eret != CURLE_OK) {
       DBG("[%s:%s] curl_easy_setopt(CURLOPT_IPRESOLVE) failed, rv=%d", 
           _nsName.c_str(), _nodeHandle.c_str(), eret);
       retval = 14;
       break;
    }

    _response_body.Reset();

    return retval; // Success

    } // End of while

    return retval; // Error
}

int
CMMConnectionSM::CURLfree()
{
    if (_ech) {
    	curl_easy_cleanup(_ech);
    	_ech = NULL;
    }
    return 0;
}

int
CMMConnectionSM::CURLreset()
{
    int rv;
    int retval = 0;

    while (1) { // Begin while

    rv = CURLfree();
    if (rv) {
    	DBG("[%s:%s] CURLfree() failed, rv=%d", 
	    _nsName.c_str(), _nodeHandle.c_str(), rv);
	retval = 1;
	break;
    }

    rv = CURLinit();
    if (rv) {
    	DBG("[%s:%s] CURLinit() failed, rv=%d", 
	    _nsName.c_str(), _nodeHandle.c_str(), rv);
	retval = 2;
	break;
    }

    return retval; // Success

    } // End while

    return retval; // Error
}

CURLMcode
CMMConnectionSM::InitiateRequest(void* mch, const struct timespec* now)
{
    CMMNodeStatus* nodeStatus;
    long total_requests =  g_nsc_req_waits + g_nsc_hits + g_nsc_misses;
    total_requests = total_requests ? total_requests : 1;
    double hit_rate = 
    	((double)((g_nsc_req_waits + g_nsc_hits)) / (double)total_requests);

    DBG("NSC Stats: hit_rate=%f waits=%ld hits=%ld stale_misses=%ld misses=%ld "
    	"nsc_entries_flushed=%ld",
    	hit_rate * 100.0, g_nsc_req_waits, g_nsc_hits, g_nsc_stale_misses, 
	g_nsc_misses, g_nsc_entries_flushed);

    nodeStatus = CMMNodeCache->Lookup(_nodeHostPort);
    if (nodeStatus) {
    	if (nodeStatus->RequestPending()) {
	    DBG("NSC: Awaiting request completion, key=%s ns=%s "
	    	"my_node_handle=%s",
	    	_nodeHostPort.c_str(), _nsName.c_str(), _nodeHandle.c_str());

	    nodeStatus->WaiterQueuePush(_nodeHandle);
	    ::g_nsc_req_waits++;
	    return CURLM_OK;
	}

	if (nodeStatus->ValidState(now, _heartBeatInterval)) {
	    DBG("NSC: Using cached node status, key=%s ns=%s my_node_handle=%s",
	    	_nodeHostPort.c_str(), _nsName.c_str(), _nodeHandle.c_str());

	    CMMNodeCache->CompletionQueuePush(_nodeHandle, nodeStatus);
	    ::g_nsc_hits++;
	    return CURLM_OK;
	} else {
	    ::g_nsc_stale_misses++;
	    nodeStatus->SetRequestPending(_nodeHandle);
	}
    } else {
    	nodeStatus = new CMMNodeStatus(_nodeHostPort);
	nodeStatus->SetRequestPending(_nodeHandle);

    	CMMNodeCache->Add(_nodeHostPort, nodeStatus);
	DBG("NSC: Add CMMNodeStatus to cache, key=%s", _nodeHostPort.c_str());
    }

    DBG("NSC: Initiating request, key=%s ns=%s my_node_handle=%s", 
    	_nodeHostPort.c_str(), _nsName.c_str(), _nodeHandle.c_str());

    ::g_nsc_misses++;
    return curl_multi_add_handle(mch, _ech);
}

int 
CMMConnectionSM::UpdateNodeStatus(CompletionType type, const CURLMsg* msg,
				const NvsdMgmtRespStatus& st)
{
    CMMNodeStatus* nodeStatus;
    std::string NodeHandle;

    if (!msg) {
    	msg = &::null_CURLMsg;
    }

    nodeStatus = CMMNodeCache->Lookup(_nodeHostPort);
    if (nodeStatus) {
    	if (nodeStatus->ClearRequestPending(_nodeHandle)) {
	    nodeStatus->Update(type, msg, &_crs, &st, &_completion);

	    DBG("NSC: Pending request complete, key=%s ns=%s my_node_handle=%s "
	    	"type=%d",
	    	_nodeHostPort.c_str(), _nsName.c_str(), _nodeHandle.c_str(), 
		type);

	    while (!nodeStatus->WaiterQueuePop(NodeHandle)) {
	    	DBG("NSC: Queue resume SM, key=%s target_node_handle=%s",
		    _nodeHostPort.c_str(), NodeHandle.c_str());

	    	CMMNodeCache->CompletionQueuePush(NodeHandle, nodeStatus);
	    }
	}
    }
    return 0;
}

int
CMMConnectionSM::PerformActions(const CURLMsg* msg, bool realCURLcompletion)
{
    int rv;
    int success;
    struct timespec now;
    int64_t tdiff_msecs;
    bool nvsd_online;
    long load_metric;
    long version;

    // Delete canceled SM(s)
    if (_cancel) {
	DBG("[%s:%s] Deleting canceled SM, this=%p", _nsName.c_str(), 
	    _nodeHandle.c_str(), this);
    	delete this;
	::g_canceled_SM++;
	return 0;
    }

    // Delete stale SM(s)
    clock_gettime(CLOCK_MONOTONIC, &now);
    tdiff_msecs = timespec_diff_msecs(&_last_update, &now);
    if (tdiff_msecs > ::stale_sm_timeout_msecs) {
	DBG("[%s:%s] Deleting stale SM, this=%p", _nsName.c_str(), 
	    _nodeHandle.c_str(), this);
	delete this;
	::g_stale_SM++;
	return 0;
    }

    // Get operation status
    if (!msg) {
	DBG("[%s:%s] Non CURL completion", 
	    _nsName.c_str(), _nodeHandle.c_str());
    	success = 0;
	_op_status_other++;
	::g_op_other++;
    } else if (msg->data.result == CURLE_OK) {
    	if (_response_body.GetBodyData(nvsd_online, load_metric, version)) {
	    // Valid nkn mgmt server body
	    if (nvsd_online) {
	    	success = 1;
	    	_op_status_success++;
	    	::g_op_success++;

	    	_op_status_online++;
	    	::g_op_online++;
	    } else {
	    	success = 0;
	    	_op_status_offline++;
	    	::g_op_offline++;
	    }

	    if (_shm_loadmetric_data) {
    		_shm_loadmetric_data->u.loadmetric.load_metric = load_metric;
    		_shm_loadmetric_data->u.loadmetric.version = version;
	    }
	} else {
	    success = 1;
	    _op_status_success++;
	    ::g_op_success++;
	}
    } else if ((msg->data.result == CURLE_COULDNT_CONNECT) ||
   	       (msg->data.result == CURLE_OPERATION_TIMEOUTED) ||
   	       (msg->data.result == CURLE_SEND_ERROR) ||
   	       (msg->data.result == CURLE_RECV_ERROR)) {
    	success = 0;

	switch(msg->data.result) {
	case CURLE_COULDNT_CONNECT:
	    _curl_op_no_connect++;
	    break;
	case CURLE_OPERATION_TIMEOUTED:
	    _curl_op_timeout++;
	    break;
	case CURLE_SEND_ERROR:
	    _curl_op_snd_err++;
	    break;
	case CURLE_RECV_ERROR:
	    _curl_op_recv_err++;
	    break;
	default:
	    break;
	}
    	_op_status_timeout++;
	::g_op_timeout++;
    } else {
    	// Should never happen, retry
	DBG("[%s:%s] Unexpected CURL status=%d", 
	    _nsName.c_str(), _nodeHandle.c_str(), msg->data.result);
    	success = 0;
	_op_status_unexpected++;
	::g_op_unexpected++;
    }

    bool init_case = false;

    switch (_state) {
    case SM_INIT:
    	_state = SM_ONLINE;
	init_case = true;
	// Fall through

    case SM_ONLINE:
    	if (success) {
	    if (init_case) {
	    	rv = SendNodeStatus(1);
		if (rv) {
		    DBG("[%s:%s] SendNodeStatus() failed, rv=%d",
		    	_nsName.c_str(), _nodeHandle.c_str(), rv);
		}
	    }
	    _failedRequests = 0;
	} else {
	    if (init_case || (++_failedRequests >= _allowedConnectFailures)) {
	    	rv = SendNodeStatus(0);
		if (rv) {
		    DBG("[%s:%s] SendNodeStatus() failed, rv=%d",
		    	_nsName.c_str(), _nodeHandle.c_str(), rv);
		}
		_node_online_time = NULLtimespec;
	    	_state = SM_OFFLINE;
	    }
	}
	break;

    case SM_OFFLINE:
    	if (success) {
	    if (timespec_cmp(&_node_online_time, &NULLtimespec) != 0) {
    		tdiff_msecs = timespec_diff_msecs(&_node_online_time, &now);
		if (tdiff_msecs > ::nodeonline_interval_msecs) {
		    rv = SendNodeStatus(1);
		    if (rv) {
		    	DBG("[%s:%s] SendNodeStatus() failed, rv=%d",
			    _nsName.c_str(), _nodeHandle.c_str(), rv);
		    }
		    _state = SM_ONLINE;
		}
	    } else {
	    	_node_online_time = now;
	    }
	} else {
	    _node_online_time = NULLtimespec;
	}
    	break;

    case SM_INVALID:
    default:
	DBG("[%s:%s] Invalid SM state=%d", _nsName.c_str(), 
	    _nodeHandle.c_str(), _state);
    	assert(!"Invalid state");
    	break;
    }

    // Dump SM statistics into shared memory
    rv = DumpSMdata();
    if (rv) {
    	DBG("[%s:%s] DumpSMdata() failed, rv=%d", 
	    _nsName.c_str(), _nodeHandle.c_str(), rv);
    }

    // Schedule SM
    _schedule_at = now;
    timespec_add_msecs(&_schedule_at, _heartBeatInterval);

    if (realCURLcompletion) {
    	rv = CURLreset(); // Disable CURL connection keepalive
    	if (rv) {
	    DBG("[%s:%s] CURLreset() failed, rv=%d", 
	    	_nsName.c_str(), _nodeHandle.c_str(), rv);
    	}
    }

    rv = AddToScheduler(this);
    if (rv) {
	DBG("[%s:%s] AddToScheduler() failed, rv=%d this=%p, "
	    "cancel monitoring", 
	    _nsName.c_str(), _nodeHandle.c_str(), rv, this);
	delete this;
    }
    return 0;
}

int
CMMConnectionSM::SendNodeStatus(int online)
{
    int rv;
    int bytes_used;
    int retval = 0;
    queue_element_t* qe = NULL;

    while (1) { // Begin while

    qe = (queue_element_t*)CMM_malloc(sizeof(queue_element_t));
    if (!qe) {
    	DBG("[%s:%s] CMM_malloc(sizeof(queue_element_t)) failed, qe=%p", 
	    _nsName.c_str(), _nodeHandle.c_str(), qe);
    	retval = 1;
	break;
    }

    rv = init_queue_element(&qe->fq_element, 0, "", "http://nvsd");
    if (rv) {
    	DBG("[%s:%s] init_queue_element() failed, rv=%d", 
	    _nsName.c_str(), _nodeHandle.c_str(), rv);
	retval = 2;
	break;
    }

    if (online) {
    	rv = add_attr_queue_element_len(&qe->fq_element, REQ_NVSD_ONLINE, 
					REQ_NVSD_ONLINE_STRLEN, "1", 1);
    } else {
    	rv = add_attr_queue_element_len(&qe->fq_element, REQ_NVSD_OFFLINE,
					REQ_NVSD_OFFLINE_STRLEN, "1", 1);
    }
    if (rv) {
    	DBG("[%s:%s] add_attr_queue_element_len() failed, rv=%d", 
	    _nsName.c_str(), _nodeHandle.c_str(), rv);
	retval = 3;
	break;
    }

    const char* node_handle[MAX_CMM_DATA_ELEMENTS];

    node_handle[0] = _nodeHandle.c_str();
    int pbufsize = _nodeHandle.size() * 2;
    char* pbuf = (char*)alloca(pbufsize);

    bytes_used = nvsd_request_data_serialize(1, node_handle, pbuf, pbufsize);
    if (bytes_used <= 0) {
    	DBG("[%s:%s] nvsd_request_data_serialize() bytes_used=%d pbuf=%p "
	    "pbufsize=%d", _nsName.c_str(), _nodeHandle.c_str(), 
	    bytes_used, pbuf, pbufsize);
	retval = 4;
	break;
    }

    rv = add_attr_queue_element_len(&qe->fq_element, REQ_NVSD_DATA, 
				    REQ_NVSD_DATA_STRLEN, pbuf, bytes_used);
    if (rv) {
    	DBG("[%s:%s] add_attr_queue_element_len() failed, rv=%d", 
	    _nsName.c_str(), _nodeHandle.c_str(), rv);
	retval = 5;
	break;
    }

    rv = enqueue_output_fqueue(qe);
    if (!rv) {
    	qe = NULL;
    } else {
    	DBG("[%s:%s] enqueue_output_fqueue() failed, rv=%d qe=%p", 
	    _nsName.c_str(), _nodeHandle.c_str(), rv, qe);
	retval = 6;
	break;
    }
    break;

    } // End while

    if (qe) {
    	CMM_free(qe);
    }
    return retval;
}

int CMMConnectionSM::DumpSMdata()
{
    int rv;
    int retval = 0;
    int64_t runtime_msecs = timespec_diff_msecs(&_schedule_at, &_completion);

    if (!_shm_data) {
    	return retval;
    }
    *(_shm_data + (_shm_data_offset - 1)) = '\0'; // Prevent partial read

    rv = snprintf(_shm_data + _shm_data_offset, 
		  _shm_datasize - _shm_data_offset, 
    		  "%s=%s|%s=%ld|%s=%ld|%s=%ld|%s=%ld|%s=%ld|%s=%ld|"
		  "%s=%f|%s=%f|%s=%f|%s=%f|%s=%f|"
		  "%s=%ld|"
		  "%s=%ld|%s=%ld|%s=%ld|%s=%ld",
		  CMM_NM_STATE, ((_state == SM_ONLINE) ? 
		  	CMM_VA_STATE_ON : CMM_VA_STATE_OFF),
		  CMM_NM_OP_SUCCESS, _op_status_success,
		  CMM_NM_OP_TIMEOUT, _op_status_timeout,
		  CMM_NM_OP_OTHER, _op_status_other,
		  CMM_NM_OP_UNEXPECTED, _op_status_unexpected,
		  CMM_NM_OP_ONLINE, _op_status_online,
		  CMM_NM_OP_OFFLINE, _op_status_offline,
		  CMM_NM_LOOKUP_TIME, _crs.namelookup_time,
		  CMM_NM_CONNECT_TIME, _crs.connect_time,
		  CMM_NM_PREXFER_TIME, _crs.pre_transfer_time,
		  CMM_NM_STARTXFER_TIME, _crs.start_transfer_time,
		  CMM_NM_TOTAL_TIME, _crs.total_time, 
		  CMM_NM_RUN_TIME, runtime_msecs,
		  CMM_NM_CURL_NO_CONNECT, _curl_op_no_connect,
		  CMM_NM_CURL_TIMEOUT, _curl_op_timeout,
		  CMM_NM_CURL_SND_ERR, _curl_op_snd_err,
		  CMM_NM_CURL_RECV_ERR, _curl_op_recv_err);
    if (rv >= (_shm_datasize - _shm_data_offset)) {
        _shm_data[_shm_datasize-1] = '\0';
	DBG("[%s:%s] Buffer size exceeded, rv=%d bufsize=%d this=%p", 
	    _nsName.c_str(), _nodeHandle.c_str(), rv, _shm_datasize - 
	    _shm_data_offset, this);
	retval = 1;
    }

    *(_shm_data + (_shm_data_offset - 1)) = ' '; // Allow consistent read
    return retval;
}

////////////////////////////////////////////////////////////////////////////////
// Static functions
////////////////////////////////////////////////////////////////////////////////

// CMMConnectionSM incarnation generator
long CMMConnectionSM::_connSMIncarnation = 0;

// f(NodeHandle) => CMMConnectionSM object
CMMConnectionSMHash_t CMMConnectionSM::_nodeHandleHash(10000);

// CMM scheduler priorityQ where top()=earliest CMMConnectionSM to schedule 
CMMSchedQ_t CMMConnectionSM::_schedulerPQ;

struct timespec CMMConnectionSM::NULLtimespec;

// Configuration limits
const int CMMConnectionSM::_min_heartBeatInterval = 10;
const int CMMConnectionSM::_max_heartBeatInterval = (3600 * MILLI_SECS_PER_SEC);

const int CMMConnectionSM::_min_connectTimeout = 10;
const int CMMConnectionSM::_max_connectTimeout = (3600 * MILLI_SECS_PER_SEC);

const int CMMConnectionSM::_min_allowedConnectFailures = 0;
const int CMMConnectionSM::_max_allowedConnectFailures = INT_MAX;

const int CMMConnectionSM::_min_readTimeout = 10;
const int CMMConnectionSM::_max_readTimeout = (3600 * MILLI_SECS_PER_SEC);

int 
CMMConnectionSM::CMMConnectionSMinit()
{
    uuid_t uid;
    uint32_t val;

    uuid_generate(uid);
    val = (*(uint32_t *)&uid[0]) ^ (*(uint32_t *)&uid[4]) ^
          (*(uint32_t *)&uid[8]) ^ (*(uint32_t *)&uid[12]);
    CMMConnectionSM::_connSMIncarnation = ((long)val) << 32 + 1;

    return 0;
}

int 
CMMConnectionSM::AddCMMConnectionSM(CURLM* mch, const cmm_node_config_t* config,
				    const char* ns_name,  int ns_name_strlen)
{
    int rv;
    int retval = 0;
    std::string NodeHandle(config->node_handle);
    CMMConnectionSM* pConnSM = NodeHandleToSM(NodeHandle);

    if (!pConnSM) {
    	// Not found, create SM
	pConnSM = new CMMConnectionSM(mch, config, ns_name, ns_name_strlen);

	// Add to hash table
	_nodeHandleHash[NodeHandle] = pConnSM;

	// Add SM to scheduler
	rv = AddToScheduler(pConnSM);
	if (!rv) {
	    g_add_req_new++;
	} else {
	    DBG("[%s] AddToScheduler() failed, rv=%d", config->node_handle, rv);
	    delete pConnSM;
	    retval = 1; // Error
	    g_add_req_new_sched_fail++;
	}
    } else {

    	// Exists, update configuration
    	rv = pConnSM->SetConfig(config, true);
	if (!rv) {
	    g_add_req_updt++;
	    clock_gettime(CLOCK_MONOTONIC, &pConnSM->_last_update);

	    if ((timespec_cmp(&pConnSM->_last_update, 
			      &pConnSM->_schedule_at) < 0) &&
	        (timespec_diff_msecs(&pConnSM->_last_update, 
			     	     &pConnSM->_schedule_at) > 
			    (MILLI_SECS_PER_SEC * send_config_interval_secs))) {

	    	// Schedule immediately to apply new config
	    	pConnSM->_schedule_at = pConnSM->_last_update;
	    	rv = AddToScheduler(pConnSM);
	    	if (!rv) {
		    g_add_req_updt_sched++;
		} else {
		    DBG("[%s:%s] AddToScheduler() failed, rv=%d", 
			pConnSM->_nsName.c_str(),
		    	pConnSM->_nodeHandle.c_str(), rv);
		    g_add_req_updt_sched_fail++;
	    	}
	    }
	} else {
	    DBG("[%s] SetConfig() failed, rv=%d SM=%p", config->node_handle, 
	    	rv, pConnSM);
	    g_add_req_updt_setcfg_fail++;
	}
    }
    return retval;
}

int 
CMMConnectionSM::CancelCMMConnectionSM(const char* node_handle)
{
    int retval = 0;
    int rv;
    CMMConnectionSMHash_t::iterator itr;
    CMMConnectionSM* pConnSM = NodeHandleToSM(node_handle, &itr);

    if (pConnSM) {
    	// Mark SM as canceled
    	pConnSM->_cancel = true;

	// Schedule cancel
    	clock_gettime(CLOCK_MONOTONIC, &pConnSM->_schedule_at);
	rv = AddToScheduler(pConnSM);
	if (!rv) {
	    g_cancel_req++;
	} else {
	    DBG("[%s:%s] AddToScheduler() failed, rv=%d", 
		pConnSM->_nsName.c_str(),
	    	pConnSM->_nodeHandle.c_str(), rv);
	    g_cancel_req_sched_fail++;
	    retval = 1;
	}
    } else {
	DBG("[%s] Bad handle", node_handle);
	g_cancel_req_inv_hdl++;
	retval = 2;
    }

    return retval;
}

int 
CMMConnectionSM::CancelCMMConnectionSMptrLen(const char* node_handle,
				       	     int node_handle_strlen)
{
    const std::string nodeHandle(node_handle, node_handle_strlen);

    return CancelCMMConnectionSM(nodeHandle.c_str());
}

CMMConnectionSM*
CMMConnectionSM::NodeHandleToSM(const std::string& node_handle,
				CMMConnectionSMHash_t::iterator *itr)
{
    CMMConnectionSMHash_t::iterator* pitr;
    CMMConnectionSMHash_t::iterator litr;

    if (itr) {
    	pitr = itr;
    } else {
    	pitr = &litr;
    }

    *pitr = _nodeHandleHash.find(node_handle);
    if (*pitr != _nodeHandleHash.end()) {
    	return (*pitr)->second;
    } else {
    	return NULL;
    }
}

CMMConnectionSM*
CMMConnectionSM::NodeHandleToSM(const char* node_handle,
				CMMConnectionSMHash_t::iterator *itr)
{
    return NodeHandleToSM(std::string(node_handle), itr);
}

int
CMMConnectionSM::AddToScheduler(CMMConnectionSM* pConnSM) 
{ 
    _schedulerPQ.push(CMMSchedulerEntry(pConnSM->_nodeHandle, 
    			            	pConnSM->_incarnation, 
				    	&pConnSM->_schedule_at));
    return 0;
}

int 
CMMConnectionSM::ScheduleCurlRequests(CURLM* mch, int& pending_request,
				  struct timespec& pending_request_deadline)
{
    int entries_scheduled = 0;
    struct timespec now;
    CMMConnectionSM* pConnSM;
    CURLMcode mret;

    pending_request = 0;
    clock_gettime(CLOCK_MONOTONIC, &now);

    while (!_schedulerPQ.empty()) {
        const CMMSchedulerEntry& SchedEntry = _schedulerPQ.top();

	if (timespec_cmp(&now, &SchedEntry._schedule_at) >= 0) {
	    pConnSM = NodeHandleToSM(SchedEntry._node_handle);

	    if (pConnSM) {
	        if ((pConnSM->_incarnation == SchedEntry._incarnation) &&
		    !pConnSM->_running) {
		    if (!pConnSM->_cancel) {
		    	mret = pConnSM->InitiateRequest(mch, &now);
		    	if (mret == CURLM_OK) {
			    pConnSM->_running = true;
			    entries_scheduled++;
		    	} else {
			    g_last_initiate_fail_status = mret;
			    g_initiate_req_failed++;
			    DBG("[%s:%s] curl_multi_add_handle() failed, "
			    	"rv=%d", 
			    	pConnSM->_nsName.c_str(),
			    pConnSM->_nodeHandle.c_str(), mret);
			    pConnSM->CompletionHandler(CB_MULTI_SCHED_FAILED, 
			    			       NULL);
		    	}
		    } else {
			pConnSM->CompletionHandler(CB_INTERNAL_REQ_COMPLETE, 
						   NULL);
		    }
	    	} else {
		    g_invalid_sm_incarn++;
		    DBG("[%s:%s] running=%d this._incarnation(%ld) != "
		    	"SchedEntry._incarnation(%ld), ignored, this=%p", 
			pConnSM->_nsName.c_str(),
			pConnSM->_nodeHandle.c_str(),
			pConnSM->_running,
			pConnSM->_incarnation, 
			SchedEntry._incarnation, pConnSM);
		}
	    } else {
		DBG("[%s] NodeHandleToSM() failed, ignoring request", 
		    SchedEntry._node_handle.c_str());
	    }
	    _schedulerPQ.pop();
	} else {
	    pending_request = 1;
	    pending_request_deadline = SchedEntry._schedule_at;
	    break;
	}
    }
    return entries_scheduled;
}

int 
CMMConnectionSM::HandleCurlCompletions(CURLM* mch)
{
    int rv;
    CURLMsg* msg;
    CURLMsg curlmsg;
    CURLcode eret;
    int msgs_left;
    int processed_msgs = 0;
    CMMConnectionSM* pConnSM;

    HandleInternalCompletions();

    while ((msg = curl_multi_info_read(mch, &msgs_left)) != NULL) {
        eret = curl_easy_getinfo(msg->easy_handle, CURLINFO_PRIVATE, 
				(char*)&pConnSM); 
	if (eret != CURLE_OK) {
	    DBG("curl_easy_getinfo(CURLINFO_PRIVATE) failed, rv=%d", eret);
	    continue;
	}
        eret = curl_easy_getinfo(msg->easy_handle, CURLINFO_NAMELOOKUP_TIME,
				 &pConnSM->_crs.namelookup_time);
        eret = curl_easy_getinfo(msg->easy_handle, CURLINFO_CONNECT_TIME,
				 &pConnSM->_crs.connect_time);
        eret = curl_easy_getinfo(msg->easy_handle, CURLINFO_PRETRANSFER_TIME,
				 &pConnSM->_crs.pre_transfer_time);
        eret = curl_easy_getinfo(msg->easy_handle, CURLINFO_STARTTRANSFER_TIME,
				 &pConnSM->_crs.start_transfer_time);
        eret = curl_easy_getinfo(msg->easy_handle, CURLINFO_TOTAL_TIME,
				 &pConnSM->_crs.total_time);
        eret = curl_easy_getinfo(msg->easy_handle, CURLINFO_REDIRECT_TIME,
				 &pConnSM->_crs.redirect_time);
	curlmsg = *msg;
	rv = pConnSM->CompletionHandler(CB_REQ_COMPLETE, &curlmsg);
	if (rv) {
	    DBG("CompletionHandler(CB_REQ_COMPLETE) failed, "
	    	"rv=%d pConnSM=%p", rv, pConnSM);
	}
	processed_msgs++;

    	HandleInternalCompletions();
    }
    return processed_msgs;
}

void
CMMConnectionSM::HandleInternalCompletions()
{
    int rv;
    std::string node_handle;
    CompletionType type;
    CURLMsg cmsg;
    CURLRequestStats creq_stats;
    NvsdMgmtRespStatus mgmt_st;
    CMMConnectionSM* pConnSM;

    while (!CMMNodeCache->CompletionQueuePop(node_handle, type, cmsg, 
					     creq_stats, mgmt_st)) {
    	pConnSM = NodeHandleToSM(node_handle);
	if (pConnSM) {
	    switch(type) {
	    case CB_MULTI_SCHED_FAILED:
	    	DBG("NSC: Resume SM (CB_MULTI_SCHED_FAILED), "
		    "target_node_handle=%s", node_handle.c_str());
	    	rv = pConnSM->CompletionHandler(type, NULL);
	    	break;

	    case CB_REQ_COMPLETE:
	    	DBG("NSC: Resume SM (CB_INTERNAL_REQ_COMPLETE), "
		    "target_node_handle=%s", node_handle.c_str());
	    	pConnSM->_crs = creq_stats;
		pConnSM->SetMgmtResp(mgmt_st);
	    	rv = pConnSM->CompletionHandler(CB_INTERNAL_REQ_COMPLETE, 
						&cmsg);
	    	break;

	    default:
	    	// Should never happen
	    	DBG("NSC: Invalid completion type=%d", type);
		assert(!"NSC: Invalid type");
	    	rv = 1;
	    }

	    if (rv) {
	    	DBG("CompletionHandler(type=%d) failed, "
		    "rv=%d pConnSM=%p", type, rv, pConnSM);
	    }
	} else {
	    DBG("NSC: *** Invalid resume SM, target_node_handle=%s", 
	    	node_handle.c_str());
	}
    }
}

size_t 
CMMConnectionSM::CURLWriteData(void* buffer, size_t size, size_t nmemb, 
			       void *userp)
{
    int rv;
    CMMConnectionSM* pCMM = (CMMConnectionSM*)userp;
    CURLcode eret;
    long http_response_code = 0;

    eret = curl_easy_getinfo(pCMM->_ech, CURLINFO_RESPONSE_CODE,
			     &http_response_code);
    
    if ((eret == CURLE_OK) && (http_response_code == 200)) {
    	rv = pCMM->ProcessResponseBody((const char*)buffer, (size * nmemb));
    	if (rv) {
	    DBG("ProcessResponseBody() failed, rv=%d this=%p buf=%p size=%ld",
	    	rv, userp, buffer, (size * nmemb));
    	}
    }
    return nmemb;
}

int 
CMMConnectionSM::CURLDebugInfo(CURL* ech, curl_infotype type, char* buf, 
			       size_t bufsize, void* handle)
{
    CURLcode eret;
    CMMConnectionSM* pConnSM;
    const char* nodeHandle;

    if (!cmm_debug_output) {
    	return 0;
    }

    eret = curl_easy_getinfo(ech, CURLINFO_PRIVATE, (char*)&pConnSM); 
    if (eret == CURLE_OK) {
    	nodeHandle = pConnSM->_nodeHandle.c_str();
    } else {
    	nodeHandle = "";
    	DBG("curl_easy_getinfo() failed, rv=%d", eret);
    }

    char* info = (char*)alloca(bufsize+1);
    memcpy(info, buf, bufsize);
    info[bufsize] = '\0';

    switch (type) {
    case CURLINFO_TEXT:
    	DBG("[%s] CURLINFO_TEXT [%s] buf=%p bufsize=%ld handle=%p\n", 
	    nodeHandle, info, buf, bufsize, handle);
    	break;
    case CURLINFO_HEADER_IN:
#if 0
    	DBG("[%s] CURLINFO_HEADER_IN [%s] buf=%p bufsize=%ld handle=%p\n", 
	    nodeHandle, info, buf, bufsize, handle);
#endif
    	break;
    case CURLINFO_HEADER_OUT:
    	DBG("[%s] CURLINFO_HEADER_OUT [%s] buf=%p bufsize=%ld handle=%p\n", 
	    nodeHandle, info, buf, bufsize, handle);
    	break;
    case CURLINFO_DATA_IN:
#if 0
    	DBG("[%s] CURLINFO_DATA_IN [%s] buf=%p bufsize=%ld handle=%p\n", 
	    nodeHandle, info, buf, bufsize, handle);
#endif
    	break;
    case CURLINFO_DATA_OUT:
    	DBG("[%s] CURLINFO_DATA_OUT [%s] buf=%p bufsize=%ld handle=%p\n", 
	    nodeHandle, info, buf, bufsize, handle);
    	break;
    default:
    	DBG("[%s] invalid type, [%s] buf=%p bufsize=%ld handle=%p\n", 
	    nodeHandle, info, buf, bufsize, handle);
    	break;
    }
    return 0;
}
////////////////////////////////////////////////////////////////////////////////
// Class NvsdMgmtRespStatus
////////////////////////////////////////////////////////////////////////////////

NvsdMgmtRespStatus::NvsdMgmtRespStatus()
{
    _valid_response_body = false;
    _processed = false;
    _nvsd_online = false;
    _load_metric = 0;
    _version = 0;
}

NvsdMgmtRespStatus::~NvsdMgmtRespStatus()
{
}

////////////////////////////////////////////////////////////////////////////////
// Class NvsdMgmtResponseBody
////////////////////////////////////////////////////////////////////////////////

NvsdMgmtResponseBody::NvsdMgmtResponseBody()
{
    Reset();
}

NvsdMgmtResponseBody::~NvsdMgmtResponseBody()
{
}

void
NvsdMgmtResponseBody::Reset()
{
    _response_body[MAX_BODY_SIZE] = 0;
    _response_body_bytes = 0;
    _rs._valid_response_body = false;
    _rs._processed = false;
    _rs._nvsd_online = false;
    _rs._load_metric = 0;
    _rs._version = 0;
}

int
NvsdMgmtResponseBody::AddData(const char* data, size_t bytes)
{
    int rv;

    if (!_rs._processed) {
    	if ((_response_body_bytes + bytes) <= MAX_BODY_SIZE) {
	    memcpy(&_response_body[_response_body_bytes], data, bytes);
	    _response_body_bytes += bytes;
	    _response_body[_response_body_bytes] = 0;
	    rv = ParseData();
	} else {
	    // Body exceeds expected upper limit
	    _rs._processed = true;
	    _rs._valid_response_body = false;
	}
    }
    return 0;
}

bool
NvsdMgmtResponseBody::GetBodyData(bool& nvsd_online, long& load_metric, 
				  long& version)
{
    nvsd_online = _rs._nvsd_online;
    load_metric = _rs._load_metric;
    version = _rs._version;

    return _rs._valid_response_body;
}

bool
NvsdMgmtResponseBody::GetBodyData(NvsdMgmtRespStatus& st)
{
    st = _rs;
    return _rs._valid_response_body;
}

void
NvsdMgmtResponseBody::SetBodyData(const NvsdMgmtRespStatus& st)
{
    _rs = st;
}

int
NvsdMgmtResponseBody::ParseData()
{
// Only invoked if HTTP response code=200
#define VERS "vers="
#define VERS_STRLEN 5
#define TXBW "bw="
#define TXBW_STRLEN 3
#define NVSD_STAT "nvsd="
#define NVSD_STAT_STRLEN 5

    // Body: vers=<integer>,bw=<0-100>,nvsd=<0,1>;
    char *pstart;
    char *pvers;
    char *ptxbw;
    char *pnvsdstat;

    pstart = _response_body;
    if (!pstart || !strchr(pstart, ';')) {
    	return 0;
    }

    pvers = strcasestr(pstart, VERS);
    if (pvers) {
    	_rs._version = atol(pvers + VERS_STRLEN);
    }
    ptxbw = strcasestr(pstart, TXBW);
    if (ptxbw) {
    	pnvsdstat = strcasestr(pstart, NVSD_STAT);
	if (pnvsdstat) {
	    _rs._load_metric = atol(ptxbw + TXBW_STRLEN);
	    if (_rs._load_metric < 0) {
	    	_rs._load_metric = 0;
	    } else if (_rs._load_metric > 100) {
	    	_rs._load_metric = 100;
	    }
	    if (*(pnvsdstat + NVSD_STAT_STRLEN) == '1') { 
	    	_rs._nvsd_online = true;
	    } else {
	    	_rs._nvsd_online = false;
	    }
	    _rs._valid_response_body = true;
	    _rs._processed = true;
	}
    }
    return 0;
}

/*
 * End of CMMConnectionSM.cc
 */
