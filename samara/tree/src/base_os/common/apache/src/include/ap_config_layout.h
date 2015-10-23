/* Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/**
 * @file  ap_config_layout.h
 * @brief Apache Config Layout
 */

#ifndef AP_CONFIG_LAYOUT_H
#define AP_CONFIG_LAYOUT_H

/* Configured Apache directory layout */

#ifndef PROD_ROOT_PREFIX
#define PROD_ROOT_PREFIX ""
#endif
#define DEFAULT_PREFIX PROD_ROOT_PREFIX "/opt/tms"

#define DEFAULT_EXP_EXEC_PREFIX DEFAULT_PREFIX ""
#define DEFAULT_REL_EXEC_PREFIX ""
#define DEFAULT_EXP_BINDIR DEFAULT_PREFIX "/bin"
#define DEFAULT_REL_BINDIR "bin"
#define DEFAULT_EXP_SBINDIR DEFAULT_PREFIX "/bin"
#define DEFAULT_REL_SBINDIR "bin"
#define DEFAULT_EXP_LIBEXECDIR DEFAULT_PREFIX "/modules"
#define DEFAULT_REL_LIBEXECDIR "modules"
#define DEFAULT_EXP_MANDIR DEFAULT_PREFIX "/man"
#define DEFAULT_REL_MANDIR "man"
#define DEFAULT_EXP_SYSCONFDIR DEFAULT_PREFIX "/conf"
#define DEFAULT_REL_SYSCONFDIR "conf"
#define DEFAULT_EXP_DATADIR DEFAULT_PREFIX ""
#define DEFAULT_REL_DATADIR ""
#define DEFAULT_EXP_INSTALLBUILDDIR DEFAULT_PREFIX "/lib/httpd/build"
#define DEFAULT_REL_INSTALLBUILDDIR "lib/httpd/build"
#define DEFAULT_EXP_ERRORDIR DEFAULT_PREFIX "/error"
#define DEFAULT_REL_ERRORDIR "error"
#define DEFAULT_EXP_ICONSDIR DEFAULT_PREFIX "/icons"
#define DEFAULT_REL_ICONSDIR "icons"
#define DEFAULT_EXP_HTDOCSDIR DEFAULT_PREFIX "/htdocs"
#define DEFAULT_REL_HTDOCSDIR "htdocs"
#define DEFAULT_EXP_MANUALDIR DEFAULT_PREFIX "/manual"
#define DEFAULT_REL_MANUALDIR "manual"
#define DEFAULT_EXP_CGIDIR DEFAULT_PREFIX "/cgi-bin"
#define DEFAULT_REL_CGIDIR "cgi-bin"
#define DEFAULT_EXP_INCLUDEDIR DEFAULT_PREFIX "/include"
#define DEFAULT_REL_INCLUDEDIR "include"
#define DEFAULT_EXP_LOCALSTATEDIR DEFAULT_PREFIX ""
#define DEFAULT_REL_LOCALSTATEDIR ""
#define DEFAULT_EXP_RUNTIMEDIR DEFAULT_PREFIX "/logs"
#define DEFAULT_REL_RUNTIMEDIR "logs"
#define DEFAULT_EXP_LOGFILEDIR DEFAULT_PREFIX "/logs"
#define DEFAULT_REL_LOGFILEDIR "logs"
#define DEFAULT_EXP_PROXYCACHEDIR DEFAULT_PREFIX "/proxy"
#define DEFAULT_REL_PROXYCACHEDIR "proxy"

#endif /* AP_CONFIG_LAYOUT_H */
