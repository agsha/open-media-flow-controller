#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <pthread.h>
#include "nkn_dashboard.h"

#define HOT_COUNT 20
#define DOMAIN_NAME_LEN 30
#define URI_NAME_LEN 50

#define DEFAULT_ANALYZER_UNIX_PATH "/vtmp/nknlogd-socks/uds-analyzer-server"

char pathname[] = "/var/nkn/dashboard/";


typedef struct dom_data{
	int32_t hotness;
	int32_t len;
	char dname[DOMAIN_NAME_LEN];
}domain_data;

typedef struct uri_data{
        int32_t hotness;
	int32_t len;
        char uname[URI_NAME_LEN];
}uri_data;

typedef struct nm_data{
	int32_t hotness;
	int32_t len;
        char nsname[DOMAIN_NAME_LEN];
}ns_data;

domain_data d_data_min[HOT_COUNT];
int d_data_mcount;
//domain_data d_data_hr[HOT_COUNT];
//int d_data_hcount;
//domain_data d_data_dy[HOT_COUNT];
//int d_data_dcount;
uri_data u_data_min[HOT_COUNT];
int u_data_mcount;

ns_data n_data_min[HOT_COUNT];
int n_data_mcount;


int fd;
//char response[25*512];


int make_connection();
int make_connection()
{
	int ret;
	int n = 0, len = 0;
	struct sockaddr_un sun;
	fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (fd < 0){
		printf("Error: Unable to open socket \n");
		return -1;
	}
	memset(&sun, 0, sizeof(struct sockaddr_un));

	sun.sun_family = AF_UNIX;
	n = sprintf(sun.sun_path, DEFAULT_ANALYZER_UNIX_PATH);
	len = n + sizeof(sun.sun_family);

	ret = connect(fd, (struct sockaddr *) &sun, len);
	if (ret < 0) {
		printf("Error: connect() failed \n");
		return -2;
	}
	return 0;
}


char* do_get_data(int type, int *data_len, int *hdr_len);
char* do_get_data(int type, int *data_len, int *hdr_len)
{
	const char *request = "GET /analyze?t=%d&n=10 HTTP/1.0\r\n\r\n";
	char *response = NULL;
	char tbuff[256];
	//int len, rlen;
	int len;
	len = snprintf(tbuff, 256, request, type);
	len = send(fd, tbuff, len, 0);
	*data_len = recv(fd, tbuff, 256, MSG_PEEK);
	if (*data_len == 0)
                return NULL;

	// process header.
	char *p = strstr(tbuff, "Content-Length");
	sscanf(p, "Content-Length: %d\r\n\r\n", data_len);
	p = strstr(tbuff, "\r\n\r\n");	
	p += 4; // for EOH
	//int hdr_len  = (p - tbuff);
	*hdr_len  = (p - tbuff);
	response = (char *)calloc(1, *hdr_len + *data_len + 1);
		
	recv(fd, response, (*hdr_len + *data_len), 0);
	response[*hdr_len + *data_len + 1] = 0;
	//printf("%s", response);
	//free(response);
	//return (hdr_len + rlen);
	return response;

}


int parse_data(int type, char* data, int data_len, int data_frequency);
int parse_data(int type, char* data, int data_len, int data_frequency)
{
	int ret; 
	int count = 0;
	int n;
	int hotness;
	char *ptr;
	char temp[256];

	
	ptr = data;

	switch(type){
	case 1: //namespace data
		while(1){//for each line)
			ret = sscanf(ptr, "%255[^,]%n", temp, &n);
			if (ret == 1){
				//copy namespace name 
				//should have updata_table(type, frequency, count,)
				strncpy(n_data_min[count].nsname, temp, n);
				n_data_min[count].nsname[n] = 0;
				n_data_min[count].len = n;
				//printf("%s\n", temp); //namespace name
			}
			else break;
			ptr += n;
			if (*ptr != ',')//our delimeter
				break; 
			++ptr;	
			ret = sscanf(ptr, "%256[^,]%n", temp, &n);//this is domain name, just skip this
			if (ret != 1) break; //some thing wrong
			ptr += n; 
			if (*ptr != ',') break; 
			++ptr;
			ret = sscanf(ptr, "%256[^,]%n", temp, &n);//this is uri, just skip this
			if (ret != 1) break; //some thing wrong
			ptr += n;
			if (*ptr != ',') break;
			++ptr;
			ret = sscanf(ptr, "%256[^,]%n", temp, &n);
			sscanf(temp, "%d", &hotness);
			if (ret == 1){
				//copy into hotness table
				sscanf(temp, "%d", &hotness);
				n_data_min[count].hotness = hotness;
				count ++; //we have both namespace name and hotness
				//printf("%d\n", hotness);
			}
			ptr += n;
			if((*ptr =='\r') ||(*ptr =='\n')){
				//another line;
				break;
			}
			char* p = strstr(ptr, "\n");
			if (p != 0){
				ptr = p;
				ptr ++;
			}
			else break;
			
			if(ptr >= (data + data_len))
				break;
		}
		n_data_mcount = count;	
	break; 
	case 2: //domain data
		while(1){//for each line)
			ret = sscanf(ptr, "%255[^,]%n", temp, &n);//namespace data
			if(ret != 1) break; //some thing wrong
			ptr += n;
			if (*ptr != ',') break; //should get a , 
			++ptr; //skip ,

			ret = sscanf(ptr, "%255[^,]%n", temp, &n);//domain name
			if(ret != 1) break; //some thing wrong
			//save domain name
			strncpy(d_data_min[count].dname, temp, n);
			d_data_min[count].dname[n] = 0;
			d_data_min[count].len = n;
					
			ptr += n;
			if (*ptr != ',') break; //should get ,
			++ptr; //skip ,
		
			ret = sscanf(ptr, "%255[^,]%n", temp, &n); //uri, skip
			if(ret != 1) break; //some thing wrong
			ptr += n;
			if (*ptr != ',') break; //should get ,
			++ptr; //skip ,

			ret = sscanf(ptr, "%256[^,]%n", temp, &n); //hotness
			if(ret != 1) break;
			sscanf(temp, "%d", &hotness);
			d_data_min[count].hotness = hotness;
			count++; // we have both domain and hotness count
			ptr += n;
			if((*ptr =='\r') ||(*ptr =='\n')){
				 //another line;
				break;
			}
			char* p = strstr(ptr, "\n");
			if (p != 0){
				ptr = p;
				ptr ++; //skip new line 
			}
			else break;

			if(ptr >= (data + data_len)) break;				
		}
		d_data_mcount = count;	
	break;
	case 3: //uri info
		while(1){//for each line)
			if (count >= HOT_COUNT) break; //too many data
			ret = sscanf(ptr, "%255[^,]%n", temp, &n);//namespace data, skip
			if(ret != 1) break; //some thing wrong
			ptr += n;
			if (*ptr != ',') break; //should get a ,
			++ptr; //skip ,
			
			ret = sscanf(ptr, "%255[^,]%n", temp, &n);//domain name, skip
			if(ret != 1) break; //some thing wrong
			ptr += n;
			if (*ptr != ',') break; //should get a ,
			++ptr; //skip ,
	
			ret = sscanf(ptr, "%255[^,]%n", temp, &n); //uri
			if(ret != 1) break; //some thing wrong
			strncpy(u_data_min[count].uname, temp, n);
			u_data_min[count].uname[n] = 0;
			u_data_min[count].len = n;
			ptr += n;
			if (*ptr != ',') break; //should get ,
			++ptr; //skip ,

			ret = sscanf(ptr, "%256[^,]%n", temp, &n); //hotness
			if(ret != 1) break;
			sscanf(temp, "%d", &hotness);
			u_data_min[count].hotness = hotness;
			count++; // we have both uri and hot count
			ptr += n;
			if((*ptr =='\r') ||(*ptr =='\n')){
				 //another line;
				break;
			}
			char* p = strstr(ptr, "\n");
			if (p != 0){
				ptr = p;
				ptr ++; //skip new line
			}
			else break;
			
			if(ptr >= (data + data_len)) break;	
		}
		u_data_mcount = count;

	break;
	default:
	ret = -1;
	break;

	}
	return ret;
}
void generate_domain_info(void);
void generate_domain_info(void)
{
	char filename[] = "domain.min.html";
        FILE *fp;
        int i;
        int scale;
        char full_fname[256];
        const char * list_even = "list_even";
        const char * list_odd = "list_odd";
        const char * listing;
        const char * title = "Domain Hotness Analyzer";
        const char * fmt = "<html><head><meta http-equiv=\"refresh\" content=\"10\">\n"
                "<link title=\"default\" rel=\"stylesheet\" type=\"text/css\" "
                "href=\"/tms-default.css\"></head><body>%s";

        const char * fmt1 = "<h2><center><u>%s</u></center></h2>\n";
        const char * fmt2= "<table width=\"100%\" border=\"0\" cellspacing=\"0\" cellpadding=\"3\">%s"
                "<tr id=\"analyzer_info_ptitle1\">\n"
                "<td width=\"20%\"><b><u># of requests</u></b></td>\n"
                "<td width=\"20%\"><b><u> --  </u></b></td>\n"
                "<td width=\"60%\"><b><u>Domain</u></b></td></tr>\n";
        const char * fmt3 = "<tr id=\"%s\">\n<td align=\"right\">"
			"<img src=\"/images/0066CC.gif\" width=\"%d\" height=\"12\" /></td>\n"
                        "<td>%ld</td><td><A href=\"/admin/dashboard_load.pl?file=%s&type=html&seed=5\">%s</A></td></tr>";
        const char *fmt_end = "</table>\n</body>\n</html>%s";

	char uri_fname[256];
	
        strcpy(full_fname, pathname);
        strcat(full_fname, filename);
        fp = fopen(full_fname, "w");
        if(!fp){
                printf("unable to open file :%s\n", full_fname);
                return;
        }
	// the information bar should not be more than 100 px, scale it up
        if (d_data_mcount == 0) scale = 1;
        else{
                scale = (d_data_min[0].hotness / 100)+1;
                if (scale == 0) scale = 1;
        }
	fprintf(fp, fmt, "\n ");
        fprintf(fp, fmt1, title);
        fprintf(fp, fmt2, "\n");
        for (i = 0; i < d_data_mcount; i++){
                if((i % 2) == 0) listing = list_even;
                else listing = list_odd;
		//create appropriate uri-filename"
		sprintf(uri_fname, "uri.min.%d", i+1);
                fprintf(fp, fmt3, listing, d_data_min[i].hotness/scale, d_data_min[i].hotness, uri_fname, d_data_min[i].dname);
        }
        fprintf(fp, fmt_end, "\n");
        fclose(fp);

return;
}

void generate_uri_info(void);
void generate_uri_info(void)
{	
	char filename[] = "uri.min.1.html";//we should create a filename based on the domain
	FILE *fp;
	int i;
	int scale; 
	char full_fname[256];
	const char * list_even = "list_even";
	const char * list_odd = "list_odd";
	const char * listing;
	const char * title = "Uri Hotness Analyzer";
	const char * fmt = "<html><head><meta http-equiv=\"refresh\" content=\"10\">\n"
		"<link title=\"default\" rel=\"stylesheet\" type=\"text/css\" "
		"href=\"/tms-default.css\"></head><body>%s";

	const char * fmt1 = "<h2><center><u>%s</u></center></h2>\n";
	const char * fmt2= "<table width=\"100%\" border=\"0\" cellspacing=\"0\" cellpadding=\"3\">%s"
		"<tr id=\"analyzer_info_ptitle1\">\n"
		"<td width=\"20%\"><b><u># of requests</u></b></td>\n"
		"<td width=\"20%\"><b><u> --  </u></b></td>\n"
		"<td width=\"60%\"><b><u>Uri</u></b></td></tr>\n";
	const char * fmt3 = "<tr id=\"%s\">\n"
			"<td align=\"right\"><img src=\"/images/0066CC.gif\" width=\"%d\" height=\"8\" /></td>\n" 
			"<td>%ld</td><td>%s</td></tr>";
	const char *fmt_end = "</table>\n</body>\n</html>%s";

	strcpy(full_fname, pathname);
	strcat(full_fname, filename);
	fp = fopen(full_fname, "w");
	if(!fp){
		printf("unable to open file :%s\n", full_fname);
		return;
	}
	//the information bar should not be more than 100 px
	if (u_data_mcount == 0) scale = 1;
	else{
		scale = (u_data_min[0].hotness / 100) + 1;
		if (scale == 0) scale = 1;
	}
	fprintf(fp, fmt, "\n ");
	fprintf(fp, fmt1, title);
	fprintf(fp, fmt2, "\n");
	for (i = 0; i < u_data_mcount; i++){
		if((i % 2) == 0) listing = list_even;
		else listing = list_odd;
		fprintf(fp, fmt3, listing, u_data_min[i].hotness/scale, u_data_min[i].hotness, u_data_min[i].uname);
	}
	fprintf(fp, fmt_end, "\n");
	fclose(fp);


return;
}

//void * generate_graph(void * arg)
void * log_analyzer_info(void *arg)
{
	int ret;
	int data_len, hdr_len;
	int type; 
	char * response;
	char header[256];
	char *ns_info;
	char *p;
	int i;	

	ret = make_connection();	
	if(ret < 0)
		goto bail;
	//set clock and timer to get data every 5 min, 1 hour and last day info
	//

	while(1){
		
		//Get data every 5 min
		//Get name space info
		type = 1;
		ns_info = do_get_data(type, &data_len, &hdr_len);
		strncpy(header, ns_info, hdr_len);
		header[hdr_len] = 0;
		////printf("Header len = %d\n", hdr_len);
		////printf("Header_data: %s", header);
		p = ns_info + hdr_len;
		////printf("ns_data: %s", p);
		ret = parse_data(type, p, data_len, 1); 
		//printf("**************\n\n");
		//printf("Namespace name    Hotness\n");
		//printf("-------------------------\n");
		for (i = 0; i < n_data_mcount; i++){
			printf("%10s %10d\n", n_data_min[i].nsname, n_data_min[i].hotness);
		}
		//printf("*********************\n");
		////if hour has met, parse_hour and if 1 day over parse_day
		if (ns_info) free (ns_info);

		
		//Get domain table  t = 2
		type = 2;
		response = do_get_data(type, &data_len, &hdr_len);
		p = response + hdr_len;
		////printf("Header len = %d\n", hdr_len);
		strncpy(header, response, hdr_len);
		header[hdr_len] = 0;
		////printf("Header-->  \n %s\n", header);
		////printf("Data len = %d\n", data_len);
		////printf("%s", p);

		ret = parse_data(type, p, data_len, 1);
		//printf("**************\n\n");
		//printf("Domain name    Hotness\n");
		//printf("-------------------------\n");
		for (i = 0; i < d_data_mcount; i++){
			printf("%10s %10d\n", d_data_min[i].dname, d_data_min[i].hotness);		
		}
		//printf("**************\n\n");

		generate_domain_info();

		if (response != 0)
			free (response);	
		type = 3;
		response = do_get_data(type, &data_len, &hdr_len);
		////printf("%s", response);
		////printf("Header len = %d\n", hdr_len);
		strncpy(header, response, hdr_len);
		header[hdr_len] = 0;
		////printf("Header-->  \n%s\n", header);
		////printf("Data len = %d\n", data_len);
		p = response + hdr_len;
		////printf("%s", p);
		ret = parse_data(type, p, data_len, 1);
		//printf("************************************\n\n");
		//printf("URI name                          Hotness\n");
		//printf("----------------------------------------------\n");
		for (i = 0; i < u_data_mcount; i++){
			printf("%10s %10d\n", u_data_min[i].uname, u_data_min[i].hotness);
		}
		//printf("***********************************************************\n\n");
		generate_uri_info();
		if (response != 0)
			free (response);
		
		//Get Data every min
		sleep(1*60);
		
		
		//Get Data every hour
	
		//Get last day data
	}
	close(fd);

bail:
	printf("Error : return = %d\n", ret);
	return NULL;
}
