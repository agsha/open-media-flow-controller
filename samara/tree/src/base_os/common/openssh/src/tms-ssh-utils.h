/*
 *
 * (C) Copyright 2013-2015 Juniper Networks, Inc. All Rights reserved.
 * (C) Copyright 2002-2012 Tall Maple Systems, Inc.
 * All rights reserved.
 * The contents of this file are subject to the terms of the MIT License.
 * You may not use this file except in compliance with the License.
 * See the file License.txt at the top of this source tree.
 * You can obtain a copy of the license at https://opensource.org/licenses/MIT
 *
 */

#ifndef __TMS_SSH_UTILS_H_
#define __TMS_SSH_UTILS_H_

#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif

/**
 * Operations that one might perform on a file.
 */
typedef enum {
    tfot_none = 0,
    tfot_read = 1 << 0,
    tfot_write = 1 << 1,
    tfot_all = tfot_read | tfot_write,
} tms_file_oper_type;

/* Bit field of tms_file_oper_type ORed together */
typedef unsigned int tms_file_oper_type_bf;

#if 0
static const lc_enum_string_map tms_file_oper_type_map[] = {
    { tfot_none, "none" },
    { tfot_read, "read" },
    { tfot_write, "write" },
    { tfot_all, "read and write" },
    { 0, NULL },
};
#endif

/*
 * Accepts a path, which may be either absolute, or relative to the
 * cwd.  This may resolve to a file, or to a directory.  Determines
 * whether we want to allow the specified operation(s) to this file;
 * if this is a directory, to the directory itself or to any file
 * within the directory.
 *
 * The 'oper' parameter may contain a single operation, or it may 
 * contain multiple, ORed together.  If it contains multiple, the
 * file must be approved for ALL of the specified operations for it
 * to be considered OK.
 *
 * Returns 1 if the path is OK for the operation(s); 0 if not.
 *
 * NOTE: if the symlink at FILE_RESTRICTED_CMDS_LICENSE_PATH points to
 * "1" (which we take to mean that a valid RESTRICTED_CMDS license is
 * installed), then we will always OK every operation, regardless of
 * what rules are set up.  We check this link at most once per second.
 */
int tms_path_check_ok(const char *path, tms_file_oper_type_bf opers);


#ifdef __cplusplus
}
#endif

#endif /* __TMS_SSH_UTILS_H_ */
