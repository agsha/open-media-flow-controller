#ifndef _LF_DBG_H
#define _LF_DBG_H

#include <stdio.h>
#include <stdint.h>
#include <syslog.h>

#ifdef __cplusplus 
extern "C" {
#endif

#if 0 //keeps emacs happy
}
#endif

#define LF_LOG(dlevel, dmodule, fmt, ...)				\
    {									\
	syslog(dlevel,  "["#dmodule"."#dlevel"] %s:%d: "fmt"\n",	\
	       __FUNCTION__,__LINE__, ##__VA_ARGS__);			\
    }

#ifdef __cplusplus
}
#endif

#endif //_LF_DBG_H
