/*
 *
 * (C) Copyright 2013-2015 Juniper Networks, Inc. All Rights reserved.
 * (C) Copyright 2002-2006 Tall Maple Systems, Inc.
 * All rights reserved.
 * The contents of this file are subject to the terms of the MIT License.
 * You may not use this file except in compliance with the License.
 * See the file License.txt at the top of this source tree.
 * You can obtain a copy of the license at https://opensource.org/licenses/MIT
 *
 */

#ifndef __ZONECHANGE_GPL_H_
#define __ZONECHANGE_GPL_H_

#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif

/*
 * Call this if you think the timezone may have changed.  This is required
 * because of a deficiency in many libc's that if /etc/localtime is changed,
 * running processes do not use the new timezone in evaluating localtime().
 */
int gpl_lt_timezone_handle_changed(void);

#ifdef __cplusplus
}
#endif

#endif /* __ZONECHANGE_GPL_H_ */
