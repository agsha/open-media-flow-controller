//
// (C) Copyright 2014, Juniper Networks, Inc.
// All rights reserved.
//

//
// Utils.h
//
#define PRT(_fmt, ...) printmsg(__FILE__, __LINE__, _fmt, ##__VA_ARGS__)

int printmsg(const char *file, int line, const char *fmt, ...);
int MMapFile(const char *file, int *fd, char **addr, size_t *size);
char *UnescapeStr(const char *p, int len, int query_str, char *outbuf,
                  int outbufsz, int *outbytesused /* Not including NULL */ );
int validFQDNPort(const char *name, int name_len);

//
// End of Utils.h
//
