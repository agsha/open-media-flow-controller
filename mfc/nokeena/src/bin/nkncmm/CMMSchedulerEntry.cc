/*
 * CMMSchedulerEntry.cc - CMM scheduler entry 
 */
#include "CMMSchedulerEntry.h"
#include "cmm_misc.h"

CMMSchedulerEntry::CMMSchedulerEntry(std::string& node_handle, long incarnation,
		      	  	     struct timespec *schedule_at) :
				     _schedule_at(*schedule_at),
				     _node_handle(node_handle), 
				     _incarnation(incarnation)

{
}

CMMSchedulerEntry::~CMMSchedulerEntry()
{
}

bool CMMSchedulerEntry::operator<(const CMMSchedulerEntry &a) const
{
    int rv = timespec_cmp(&_schedule_at, &a._schedule_at);
    return (rv < 0) ? true : false;
}

bool CMMSchedulerEntry::operator>(const CMMSchedulerEntry &a) const
{
    int rv = timespec_cmp(&_schedule_at, &a._schedule_at);
    return (rv > 0) ? true : false;
}
	
/*
 * End of CMMSchedulerEntry.cc
 */

