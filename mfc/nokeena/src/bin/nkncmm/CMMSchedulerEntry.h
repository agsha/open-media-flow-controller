/*
 * CMMSchedulerEntry.h - CMM scheduler entry 
 */
#ifndef _CMMSCHEDULERENTRY_H
#define _CMMSCHEDULERENTRY_H
#include <string>

class CMMSchedulerEntry
{
    public:
    	friend class CMMConnectionSM;

    	CMMSchedulerEntry(std::string& node_handle, long incarnation,
		      	  struct timespec* schedule_at);
    	~CMMSchedulerEntry();
	bool operator<(const CMMSchedulerEntry& a) const;
	bool operator>(const CMMSchedulerEntry& a) const;
	
    private:
    	struct timespec _schedule_at;
	std::string _node_handle;
	long _incarnation;
};
#endif /* _CMMSCHEDULERENTRY_H */

/*
 * End of CMMSchedulerEntry.h
 */

