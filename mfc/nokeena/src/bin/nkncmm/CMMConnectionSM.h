/*
 * CMMConnectionSM.h - CMM Connection State Machine 
 */
#ifndef CMMCONNECTIONSM_H
#define CMMCONNECTIONSM_H

#include <atomic_ops.h>
#include "curl/curl.h"
#include "nkn_cmm_request.h"
#include <string>
#include <queue>
#include <ext/hash_map>

#include "CMMSchedulerEntry.h"
#include "CMMSharedMemoryMgr.h"

using namespace __gnu_cxx;

namespace __gnu_cxx
{
    template<> struct hash<std::string >
    {
	size_t operator()(const std::string& x) const
	{
	    return hash<const char* >()(x.c_str());
	}
    };
}

class CMMConnectionSM;
typedef hash_map<std::string, CMMConnectionSM*> CMMConnectionSMHash_t;

// greater() => CMMSchedQ_t.top() smallest element (ascending order)
typedef std::priority_queue<CMMSchedulerEntry, vector<CMMSchedulerEntry>, 
		       	    std::greater<CMMSchedulerEntry> > CMMSchedQ_t;

class NvsdMgmtRespStatus {
    public:
    	NvsdMgmtRespStatus();
    	~NvsdMgmtRespStatus();

    	bool _valid_response_body;
	bool _processed;
    	bool _nvsd_online;
    	long _load_metric;
    	long _version;
};

class NvsdMgmtResponseBody {
    public:
    	NvsdMgmtResponseBody();
	~NvsdMgmtResponseBody();
	void Reset();

	// Returns: 0 => Success
	int AddData(const char* data, size_t bytes);

	// Returns: True => Valid body
	bool GetBodyData(bool& nvsd_online, long& load_metric, long& version);
	bool GetBodyData(NvsdMgmtRespStatus& st);

	void SetBodyData(const NvsdMgmtRespStatus& st);

    private:

	enum {
	    MAX_BODY_SIZE=128
	} Limits;

	int ParseData();
        
	char _response_body[MAX_BODY_SIZE+1];
	int _response_body_bytes;
	NvsdMgmtRespStatus _rs;
};

struct CURLRequestStats
{
    double namelookup_time;
    double connect_time;
    double pre_transfer_time;
    double start_transfer_time;
    double total_time;
    double redirect_time;
};

typedef enum {
    CB_INVALID=0,
    CB_MULTI_SCHED_FAILED,
    CB_REQ_COMPLETE,
    CB_INTERNAL_REQ_COMPLETE
} CompletionType;

class CMMConnectionSM {
    public:
	typedef enum {
	    SM_INVALID=0,
	    SM_INIT,
	    SM_ONLINE,
	    SM_OFFLINE
	} SMState;

    	CMMConnectionSM(CURLM* mch, const cmm_node_config_t* config,
			const char* ns_name, int ns_name_strlen);
    	~CMMConnectionSM();

	// Returns: 0 => Success
	int ProcessResponseBody(const char* buf, size_t bytes);

	void SetMgmtResp(const NvsdMgmtRespStatus& st);

	// Returns: 0 => Success
	int CompletionHandler(CompletionType type, const CURLMsg* msg=0);

	// Returns: 0 => Success
	static int CMMConnectionSMinit();

	// Returns: 0 => Success
	static int AddCMMConnectionSM(CURLM* mch, 
				      const cmm_node_config_t* config,
				      const char* ns_name, int ns_name_strlen);
	// Returns: 0 => Success
	static int CancelCMMConnectionSM(const char* node_handle);

	// Returns: 0 => Success
	static int CancelCMMConnectionSMptrLen(const char* node_handle,
					       int node_handle_strlen);

	static CMMConnectionSM* NodeHandleToSM(const std::string& node_handle,
				   CMMConnectionSMHash_t::iterator* itr = 0);
	static CMMConnectionSM* NodeHandleToSM(const char* node_handle,
				   CMMConnectionSMHash_t::iterator* itr = 0);

	// Returns: 0 => Success
	static int AddToScheduler(CMMConnectionSM* pConnSM);

	// Returns: Entries Scheduled
	static int ScheduleCurlRequests(CURLM* mch, int& pending_request,
				    struct timespec& pending_request_deadline);

	// Returns: Completions processed
	static int HandleCurlCompletions(CURLM* mch);

	static void HandleInternalCompletions();

	// CURL callback functions
	static size_t CURLWriteData(void* buffer, size_t size, size_t nmemb, 
				    void* userp);
	static int CURLDebugInfo(CURL* ech, curl_infotype type, char* buf, 
			       	 size_t bufsize, void* handle);

	static struct timespec NULLtimespec;
	static long _connSMIncarnation;


    private:
    	CMMConnectionSM();
    	CMMConnectionSM(CMMConnectionSM& a);
	CMMConnectionSM& operator=(const CMMConnectionSM a);

	inline void BoundValue(int& value, const int min, const int max) {
	    if (value < min) {
    		value = min;
	    }
	    if (value > max) {
    		value = max;
	    }
	}
	int SetConfig(const cmm_node_config_t* config, 
		      bool allocShmIfNone=false);
	int CURLinit();
	int CURLfree();
	int CURLreset();
	CURLMcode InitiateRequest(void* mch, const struct timespec* now);
	int UpdateNodeStatus(CompletionType type, const CURLMsg* msg, 
			     const NvsdMgmtRespStatus &st);
	int PerformActions(const CURLMsg* msg, bool realCURLcompletion);
	int SendNodeStatus(int online);
	int DumpSMdata();

    private:
    	static CMMConnectionSMHash_t _nodeHandleHash;
	static CMMSchedQ_t _schedulerPQ;
	static struct timespec _NULLtimespec;

	static const int _min_heartBeatInterval;
	static const int _max_heartBeatInterval;

	static const int _min_connectTimeout;
	static const int _max_connectTimeout;

	static const int _min_allowedConnectFailures;
	static const int _max_allowedConnectFailures;

	static const int _min_readTimeout;
	static const int _max_readTimeout;

	// Configuration data
    	CURLM* _mch;
	CURL* _ech;
	std::string _nsName;
	std::string _nodeHandle;
	std::string _nodeHostPort;
	std::string _heartBeatURL;
	int _heartBeatInterval; // msecs
	int _connectTimeout; // msecs
	int _allowedConnectFailures;
	int _readTimeout; // msecs

        // State machine data
	long _incarnation;
	struct timespec _schedule_at;
	struct timespec _completion;
	struct timespec _last_update;
	bool _cancel;
	bool _running; // CURL is executing request
	SMState _state;
	int _failedRequests;
	struct timespec _node_online_time;

	long _op_status_success; // Request status success count
	long _op_status_timeout; // Request status timeout count
	long _op_status_other; // Request status other count
	long _op_status_unexpected; // Request status unexpected count
	long _op_status_online; // Request status nvsd online
	long _op_status_offline; // Request status nvsd offline

	NvsdMgmtResponseBody _response_body;

	// CURL error returns denoting timeout as noted by _op_status_timeout
	long _curl_op_no_connect;
	long _curl_op_timeout;
	long _curl_op_snd_err;
	long _curl_op_recv_err;

	// Shared memory data
	char* _shm_data;
	int _shm_datasize;
	shm_token_t _shm_token;
	int _shm_data_offset; // Free data start offset

	cmm_loadmetric_entry_t* _shm_loadmetric_data;
	int _shm_loadmetric_datasize;
	shm_token_t _shm_loadmetric_token;

	// CURL request stats
	CURLRequestStats _crs;
};
#endif /* CMMCONNECTIONSM_H */

/*
 * End of CMMConnectionSM.h
 */
