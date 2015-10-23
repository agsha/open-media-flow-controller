/*
 * Filename :   nkn_geodbd.c
 * Date:        04 July 2011
 * Author:      Muthukumar
 *
 * (C) Copyright 2011, Juniper Networks, Inc.
 * All rights reserved.
 *
 */

#undef __FD_SETSIZE
#define __FD_SETSIZE  65536

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <pthread.h>
#include <dirent.h>
#include <ctype.h>
#include <stdint.h>
#include <sys/time.h>
#include <netdb.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <pthread.h>
#include <stdint.h>
#include <zlib.h>

#include <sys/resource.h>
#include <ares.h>

#include "nkn_defs.h"
#include "nkn_debug.h"
#include "nkn_mgmt_agent.h"
#include "nkn_cfg_params.h"
#include "nkn_log_strings.h"
#include "nkn_trace.h"
#include "pe_geodb.h"

#include "GeoIP.h"
#include "GeoIPCity.h"

GEOIP_API void _GeoIP_setup_dbfilename(void);

NKNCNT_DEF(geodb_lookup_called, AO_t, "", "total geodb task")
NKNCNT_DEF(geodb_lookup_completed, AO_t, "", "total geodb task")
NKNCNT_DEF(geodb_lookup_no_info, AO_t, "", "total geodb tasks failed")
NKNCNT_DEF(geodb_no_database, AO_t, "", "total geodb tasks failed")
NKNCNT_DEF(geodb_install_cnt, AO_t, "", "total geodb database install")
NKNCNT_DEF(geodb_install_completed_cnt, AO_t, "", "total geodb install completed")
NKNCNT_DEF(geodb_install_failed_cnt, AO_t, "", "total geodb install failed")

pthread_mutex_t geodb_mutex = PTHREAD_MUTEX_INITIALIZER;
extern int send_msg(geo_ip_t*);
static GeoIP * gi;
static GeoIP * gi_isp;
volatile int channel_ready = 0;

/*Scheduler input function*/
int geodb_lookup(geo_ip_req_t *ip_data, geo_ip_t *geo_data)
{
	int ret = -1;
	int i;
	GeoIPRecord    *gir = NULL;

	pthread_mutex_lock(&geodb_mutex);
	AO_fetch_and_add1(&glob_geodb_lookup_called);
	geo_data->magic = GEODB_MAGIC;
	geo_data->ip = ip_data->ip;
	geo_data->pe_task_id = ip_data->pe_task_id;

	if (channel_ready) {
		/* Call MaxMind API to get Geo data from the database
		 */
		gir = GeoIP_record_by_ipnum(gi, ip_data->ip);
		if (NULL == gir) {
			geo_data->ginf.status_code = 404;
			AO_fetch_and_add1(&glob_geodb_lookup_no_info);
		}
		else {
			geo_data->ginf.status_code = 200;
			AO_fetch_and_add1(&glob_geodb_lookup_completed);
			//printf("%s: %s, %s, %s, %s, %f, %f\n", GeoIPDBDescription[i], gir->country_code, _mk_NA(gir->region),
			//       _mk_NA(gir->city), _mk_NA(gir->postal_code), gir->latitude, gir->longitude);
	                //_say_range_by_ip(gi, ipnum);
			if (gir->continent_code) strncpy( geo_data->ginf.continent_code, gir->continent_code, 4);
			if (gir->country_code) strncpy( geo_data->ginf.country_code, gir->country_code, 4);
			//strncpy( geo_data->state_code, 4); 
			if (gir->country_name) strncpy( geo_data->ginf.country, gir->country_name, 64);
			if (gir->region) strncpy( geo_data->ginf.state, GeoIP_region_name_by_code(gir->country_code, gir->region), 64);
			if (gir->city) strncpy( geo_data->ginf.city, gir->city, 64);
			if (gir->postal_code) strncpy( geo_data->ginf.postal_code, gir->postal_code, 16);
			geo_data->ginf.latitude = gir->latitude;
			geo_data->ginf.longitude = gir->longitude;
			if (gi_isp) {
				/* Call MaxMind API to get ISP data from the database
				 */
				char *isp;

				isp = GeoIP_name_by_ipnum(gi_isp, ip_data->ip);
				if (isp) strncpy( geo_data->ginf.isp, isp, 64);
			}
			/* Call MaxMind API to release memory for the record
			 */
			GeoIPRecord_delete(gir);
			ret = 0;
		}
	}
	else {
		AO_fetch_and_add1(&glob_geodb_no_database);
		geo_data->ginf.status_code = 500;
	}
	pthread_mutex_unlock(&geodb_mutex);
	return ret;
}


/* Init function to initialize the geo ip library */
int geodb_open(uint32_t init_flags)
{
	char dir_path[] = "/nkn/maxmind/db/";
	
	/*
	 * Call MaxMind API to Setup directory and database filenames
	 */
	GeoIP_setup_custom_directory(dir_path);
	_GeoIP_setup_dbfilename();

	/* Try to open IP City edition database
	 */
	gi = GeoIP_open(GeoIPDBFileName[GEOIP_CITY_EDITION_REV1], GEOIP_MEMORY_CACHE);
	if (gi == NULL) {
		DBG_LOG(SEVERE, MOD_GEODB,"Geo IP DB init error");
		
		/* Try to open IP Country edition database
		 */
		gi = GeoIP_open(GeoIPDBFileName[GEOIP_COUNTRY_EDITION], GEOIP_MEMORY_CACHE);
		if (gi == NULL) {
			DBG_LOG(SEVERE, MOD_GEODB,"Geo IP DB init error");
			return -1;
		}
	}
	/* Try to open IP ISP database if available
	 */
	gi_isp = GeoIP_open(GeoIPDBFileName[GEOIP_ISP_EDITION], GEOIP_MEMORY_CACHE);
	if (gi_isp == NULL) {
		DBG_LOG(WARNING, MOD_GEODB, "Geo IP ISP DB not available");
	}
	channel_ready = 1;
	return 0;
}

int geodb_close(void){
	if (gi) {
		GeoIP_delete(gi);
	}
	if (gi_isp) {
		GeoIP_delete(gi_isp);
	}
	channel_ready = 0;
	return 0;
}

int geodb_version(char *ver, int length) {
	char *db_info;
	
	if (gi) {
		db_info = GeoIP_database_info(gi);
	}
	strncpy(ver, db_info, length);
	return 0;
}

int geodb_edition(void) {
        int dbtype = -1;

	if (gi) {
	    dbtype = GeoIP_database_edition(gi);
	}
	
	return dbtype;
}

int geodb_info( const char *name, char *ret_output, int length);

int geodb_info( const char *name, char *ret_output, int length)
{
	if (gi && gi_isp) {
		snprintf(ret_output, length, "GeoIP Database version:\r\n%s\r\n%s\r\nAPI Version: %s\r\n",
			GeoIP_database_info(gi),
			GeoIP_database_info(gi_isp),
			GeoIP_lib_version());
	}
	else if (gi) {
		snprintf(ret_output, length, "GeoIP Database version:%s\r\nAPI Version %s\r\n",
			GeoIP_database_info(gi),
			GeoIP_lib_version());
	}
	else {
		snprintf(ret_output, length, "No database installed\r\nAPI Version %s\r\n",
			GeoIP_lib_version());
	}
	
	return 0;
}

#define BLOCK_SIZE 1024
int geodb_install( const char *file_path_gz, char *ret_output);

/* This function uses code from the MaxMind sample code to verify
 * if the database is a vaild MaxMind database.
 */

int geodb_install( const char *file_path_gz, char *str) {
	int offset = 0, err;
	char file_path_test[1024];
	char db_file_path[1024];
	gzFile gz_fh;
	FILE *comp_fh, *gi_fh;
	char block[BLOCK_SIZE];
	int block_size = BLOCK_SIZE;
	GeoIP * gip;
	char * db_info;
	int dbtype;
	int lookupresult = 0;
	char dir_path[] = "/nkn/maxmind/db/";
	int len;
	int known_type = 0;

	AO_fetch_and_add1(&glob_geodb_install_cnt);
	/*
	 * Setup directory and database filenames
	 */
	GeoIP_setup_custom_directory(dir_path);
	_GeoIP_setup_dbfilename();

	/* Use GeoIP as the name
	 */
	strcpy(file_path_test,GeoIPDBFileName[GEOIP_COUNTRY_EDITION]);
	strcat(file_path_test,".new");

	/* Open the file for writing ungziped data
	 */
	gi_fh = fopen(file_path_test, "wb");
	if (gi_fh == NULL) {
		sprintf(str, "Error: Unable to open file for writing %s\n", file_path_test);
		goto error_return;
	}
	
	len = strlen(file_path_gz);
	if ((strcmp(file_path_gz + len - 4, ".dat") == 0 ) ||
		(strcmp(file_path_gz + len - 7, ".dat.gz") == 0)) {
		/* uncompress gzip file */
		offset = 0;
		gz_fh = gzopen(file_path_gz, "rb");
		if (gz_fh == NULL) {
			sprintf(str, "Error: Unable to open file for reading %s\n", file_path_gz);
			fclose(gi_fh);
			goto error_return;
		}
		for (;;) {
			int amt;
			amt = gzread(gz_fh, block, block_size);
			if (amt == -1) {
				gzclose(gz_fh);
				fclose(gi_fh);
				sprintf(str, "Error: Read error %s\n", file_path_gz);
				goto error_return;
			}
			if (amt == 0) {
				break;
			}
			if (amt != (int)fwrite(block,1,amt,gi_fh) ){
				sprintf(str, "Error: Write error %s\n", file_path_test);
				gzclose(gz_fh);
				fclose(gi_fh);
				goto error_return;
			}
		}
		gzclose(gz_fh);
	}
	else {
		sprintf(str, "Error: Only .dat or .dat.gz files supported %s\n", file_path_gz);
		fclose(gi_fh);
		goto error_return;
	}
	unlink(file_path_gz);
	fclose(gi_fh);

	/* 
	 * Do Sanity check 
	 */
	gip = GeoIP_open(file_path_test, GEOIP_STANDARD);

	if (gip == NULL) {
		sprintf(str, "Error: Error opening database for sanity check\n");
		goto error_return;
	}


	/* get the database type */
	dbtype = GeoIP_database_edition(gip);

	/* this checks to make sure the files is complete, since info is at the end
		 dependent on future databases having MaxMind in info (ISP and Organization databases currently don't have info string */

	if ((dbtype != GEOIP_ISP_EDITION)&&
			(dbtype != GEOIP_ORG_EDITION)) {
		db_info = GeoIP_database_info(gip);
		if (db_info == NULL) {
			GeoIP_delete(gip);
			sprintf(str, "Error: Install FAIL\n");
			goto error_return;
		}
		if (strstr(db_info, "MaxMind") == NULL) {
			free(db_info);
			GeoIP_delete(gip);
			sprintf(str, "Error: Install FAIL, MaxMind signature not found\n");
			goto error_return;
		}
		free(db_info);
	}
#if 0
	/* this performs an IP lookup test of a US IP address */
	if (dbtype == GEOIP_NETSPEED_EDITION) {
		int netspeed = GeoIP_id_by_name(gip,"24.24.24.24");
		lookupresult = 0;
		if (netspeed == GEOIP_CABLEDSL_SPEED){
			lookupresult = 1;
		}
	}
#endif
	if (dbtype == GEOIP_COUNTRY_EDITION) {
		/* if data base type is country then call the function
		 * named GeoIP_country_code_by_addr */
		lookupresult = 1;
		known_type = 1;
		if (strcmp(GeoIP_country_code_by_addr(gip,"24.24.24.24"), "US") != 0) {
			lookupresult = 0;
		}
	}
#if 0
	if (dbtype == GEOIP_REGION_EDITION_REV1) {
		/* if data base type is region then call the function
		 * named GeoIP_region_by_addr */
		GeoIPRegion *r = GeoIP_region_by_addr(gip,"24.24.24.24");
		lookupresult = 0;
		if (r != NULL) {
			lookupresult = 1;
			free(r);
		}
	}
#endif
	if (dbtype == GEOIP_CITY_EDITION_REV1) {
		/* if data base type is city then call the function
		 * named GeoIP_record_by_addr */
		GeoIPRecord *r = GeoIP_record_by_addr(gip,"24.24.24.24");
		lookupresult = 0;
		known_type = 1;
		if (r != NULL) {
			lookupresult = 1;
			free(r);
		}
	}
	if (dbtype == GEOIP_ISP_EDITION) {
		/* if data base type is isp or org then call the function
		 * named GeoIP_org_by_addr */
		GeoIPRecord *r = (GeoIPRecord*)GeoIP_org_by_addr(gip,"24.24.24.24");
		lookupresult = 0;
		known_type = 1;
		if (r != NULL) {
			lookupresult = 1;
			free(r);
		}
	}
	if (lookupresult == 0) {
		GeoIP_delete(gip);
		if (known_type) {
			sprintf(str, "Error: Install FAIL. GeoIP lookup failed\n");
		}
		else {
			sprintf(str, "Error: Install FAIL. Unsupported database\n");
		}
		goto error_return;
	}
	GeoIP_delete(gip);
	AO_fetch_and_add1(&glob_geodb_install_completed_cnt);
	sprintf(str, "Installation successful\n");

#if 0
	/* install GeoIP.dat.test -> GeoIP.dat */
	err = rename(file_path_test, geoipfilename);
	if (err != 0) {
		//GeoIP_printf(f,"GeoIP Install error while renaming file\n");
		return 8; //GEOIP_RENAME_ERR;
	}

	free(geoipfilename);
#endif
	pthread_mutex_lock(&geodb_mutex);
	geodb_close();

	err = unlink(GeoIPDBFileName[dbtype]);
	err = rename(file_path_test, GeoIPDBFileName[dbtype]);

	geodb_open(0);
	pthread_mutex_unlock(&geodb_mutex);

	return 0;

error_return:
	AO_fetch_and_add1(&glob_geodb_install_failed_cnt);
	return 1;
}
