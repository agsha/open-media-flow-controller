#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <pthread.h>

#define DEFAULT_ANALYZER_UNIX_PATH "/vtmp/nknlogd-socks/uds-analyzer-server"

#define BUFF_SIZE 	50*512
#define MAX_DATA	50


//display format
#define DISP_STDOUT 	0
#define DISP_HTML_OUT	1

//parser type
#define DOMAIN		1
#define URI	 	2
#define FILE_SIZE  	3
#define RESP_CODE	4
#define ALL		5

const char * data_cmd[] = {"", "domain", "uri", "filesize", "respcode", "all"};

#define RGIF		8
#define FGIF		8
const char * rcode_image[] = {	"/images/green_v.gif",
				"/images/cyan_v.gif",
				"/images/purple_v.gif",
				"/images/brown_v.gif",
				"/images/yellow_v.gif",
				"/images/blue_v.gif",
				"/images/orange_v.gif",
				"/images/magenda_v.gif"};



const char * fsize_image[] = {	"/images/yellow_v.gif", "/images/blue_v.gif", 
				"/images/orange_v.gif", "/images/magenda_v.gif",
				"/images/green_v.gif", "/images/cyan_v.gif", 
				"/images/purple_v.gif", "/images/brown_v.gif"};
		 


//domain data structure
typedef struct dm_data_t{
	char name[64];
	unsigned int hotness;
	float bw; //bandwidth
}dm_data_t;
dm_data_t dm_rows[MAX_DATA];

//uri and filesize and respcode data structure {name, value} pair
typedef struct data_t{
	char name[512];
	unsigned int hotness;
}data_t;

data_t d_rows[MAX_DATA];
int data_count; //actual hot data received

int thour;	 // User request time_interval in hours
int tmin;	 // user request time_interval in mins 
int depth;	 // User request for #of  hot records

char domain[512]; //User input  domain name to get uri data
char uri[512]; 	  // User input URI name to get complete record info

int fd;
int display_type; // DISP_STDOUT or DISP_HTML_OUT
int data_type;   // DOMAIN, URI, FILE_SIZE, RESP_CODE
int auto_expand; // 0, default or 1 if turned on

///////////////////////////////////////////////////////////////////////////////////////////
//Data Get/parse/manipulation functions
int mk_connection(void);
int get_expanded_uri_info(int hour, int min, int recs, int display, char * req);
int create_request(int hour, int min, int recs, int display, char * req);
int do_get_data(char *query);
int parse_data(char* data, int data_len,  int TYPE);

unsigned int find_max(unsigned int *data, int data_points);
unsigned int scale_val(unsigned int val, double scale, int buffer);
void usage (void);
//Data display function
int show_data(void);
int display_data_stdout(void);
int display_header(int type);
int display_footer(void);
int display_rows(int type);

int display_html_file_size(void);
int display_html_resp_code(void);

////////////////////////////////////////////////////////////////////////////////////////////

unsigned int scale_val(unsigned int val, double scale, int buffer)
{
	unsigned int new_val;

	new_val = (val/scale) + buffer;
	return new_val;
}

unsigned int find_max(unsigned int *data, int data_points)
{
	unsigned int max_val;
	int i; 

	if (data_points == 0)
		return 0;
	
	max_val = data[0];
	for(i = 1; i < data_points; i++){
		if(max_val < data[i])
			max_val = data[i];
	}
	return max_val;
}

int display_html_file_size(void)
{
	const char * ffmt =  "<html><head>\n<meta http-equiv=\"Content-Type\" "
			"content=\"text/html; charset=UTF-8\"> \n<title>Filesize table</title>\n"
			"<link title=\"default\" rel=\"stylesheet\" type=\"text/css\" "
            "href=\"/tms-default.css\">\n<script type=\"text/javascript\" "
			"src=\"/tms-utils.js\"></script> "
			"\n<script type=\"text/javascript\" src=\"/dashboard.js\"></script>\n"
			"</head>\n<body>%s";
	const char * ffmt1 = "<h2><center><i><u>%s %d %s</u></i></center></h2>\n"
                "<center>%s %d %s <b>%u</b></center>\n";
	const char * ffmt2 = "<table class=\"fsize_table\" align=\"center\">\n <tr>%s";
	const char * ffmt3 = "<td>%d<br /><img src=\"%s\" width=\"18\" height=\"%u\" /></td>\n";
	const char* ffmt4 = "</tr>\n<tr class=\"label\">\n";
	const char* ffmt5 = "<td>%s</td>\n";
	const char* ffmt6 = "</tr>\n</table>\n</body>\n</html>%s";
	
	unsigned int max;
	unsigned int total;
	unsigned int data[MAX_DATA];
	//unsigned int scale_height = 150;
	double scale;
	int i;

	total = 0;
	for (i = 0; i < data_count; i++){
		data[i] = d_rows[i].hotness;
		total += d_rows[i].hotness;
	}
	max = find_max(data, data_count);
	if (max > 0)
		scale = (max /130.0); //only 170 pixels
	else scale = 1.0;
	
	fprintf(stdout, ffmt, "\n");
	if (thour == 1) {
		fprintf(stdout, ffmt1, "File size distribution last ", thour, "hour",
			"Total transaction last ", thour, "hour : ", total);
	}
	else if (thour > 1){
		fprintf(stdout, ffmt1, "File size distribution last ", thour, "hours",
			 "Total transaction last ", thour, "hours : ", total);
	}
	else {
		fprintf(stdout, ffmt1, "File size distribution last ", tmin, "minutes",
			 "Total transaction last ", tmin, "mins : ", total);
	}
	fprintf(stdout, ffmt2, "\n");
	for (i = 0; i < data_count; i++){
 		fprintf(stdout, ffmt3, d_rows[i].hotness, fsize_image[i%FGIF], 
				scale_val(d_rows[i].hotness, scale, 2));
	}
	fprintf(stdout, ffmt4, "\n");
	for (i = 0; i < data_count; i++){
                fprintf(stdout, ffmt5, d_rows[i].name);
        }
	fprintf(stdout, ffmt6, "\n");
	return 0;
}

int display_html_resp_code(void)
{
	//const char * fmt_title = "Http Response Code last %d minutes";
	//const char * comment1 = "Total transaction last %d min: %d";
	const char * rfmt =  "<html><head>\n<meta http-equiv=\"Content-Type\" "
			"content=\"text/html; charset=UTF-8\"> "
			"\n<title>Respcode table</title>\n<link title=\"default\" "
			"rel=\"stylesheet\" type=\"text/css\" "
			"href=\"/tms-default.css\">\n<script type=\"text/javascript\" "
			"src=\"/tms-utils.js\"></script> "
			"\n<script type=\"text/javascript\" src=\"/dashboard.js\"></script>\n"
			"</head>\n<body>%s";
	const char * rfmt1 = "<h2><center><i><u>%s %d %s</u></i></center></h2>\n"
                "<center>%s %d %s <b>%u</b></center>\n";
	const char * rfmt2 = "<table class=\"rcode_table\" align=\"center\">\n <tr>%s";	
	const char * rfmt3 = "<td>%d<br /><img src=\"%s\" width=\"47\" height=\"%u\" /></td>\n";
	const char* rfmt4 = "</tr>\n<tr class=\"label\">\n";
	const char* rfmt5 = "<td>%s</td>\n";
	const char* rfmt6 = "</tr>\n</table>\n</body>\n</html>%s";
	//const char* listing = "list_small";
	
	unsigned int max;
	unsigned int total;
	unsigned int data[MAX_DATA]; 
	//unsigned int scale_height = 150;
	double scale;
	int i;

	total = 0;
	for (i = 0; i < data_count; i++){
		data[i] = d_rows[i].hotness;
		total += d_rows[i].hotness;
	}
	max = find_max(data, data_count);
	if(max > 0)
		scale = (max /130.0); //only 170 pixels
	else scale = 1.0;

	fprintf(stdout, rfmt, "\n");
 
	if(thour == 1){
		fprintf(stdout, rfmt1, "Http response code last ", thour, "hours",
			"Total transaction last ", thour, "hour: ", total);
	}
	else if(thour > 1){
		 fprintf(stdout, rfmt1, "Http response code last ", thour, "hours",
			"Total transaction last ", thour, "hours: ", total);
	}
	else{
		fprintf(stdout, rfmt1, "Http response code last ", tmin, "minutes", 
			"Total transaction last ", tmin, "mins: ", total);
	}
	fprintf(stdout, rfmt2, "\n");
	for (i = 0; i < data_count; i++){
		fprintf(stdout, rfmt3, d_rows[i].hotness, rcode_image[i%RGIF], 
				scale_val(d_rows[i].hotness, scale, 2));
	}
	fprintf(stdout, rfmt4, "\n");
	for (i = 0; i < data_count; i++){
		fprintf(stdout, rfmt5, d_rows[i].name);
	}
	fprintf(stdout, rfmt6, "\n");

	return 0;
}

int display_header(int type)
{
	int ret = 0; 
	const char * title1 = "domain table";
	const char * title2 = "URI table";
	
	const char * list_even = "list_even";
	const char * list_odd = "list_odd";
	const char * fmt = "<html><head>\n<meta http-equiv=\"Content-Type\" "
		"content=\"text/html; charset=UTF-8\"> "
		"\n<title>%s</title>\n<link title=\"default\" rel=\"stylesheet\" type=\"text/css\" "
        "href=\"/tms-default.css\">\n<script type=\"text/javascript\" "
		"src=\"/tms-utils.js\"></script> "
		"\n<script type=\"text/javascript\" src=\"/dashboard.js\"></script>"
		"</head>\n<body>\n ";
	const char * hdr1 = "<h2><center><i><u>Top %d domain: last %d minutes</u></i>"
			"</center></h2>\n";
	const char * hdr1_1 = "<h2><center><i><u>Top %d domain: last %d hour</u></i>"
			"</center></h2>\n";
	const char * hdr1_2 = "<h2><center><i><u>Top %d domain: last %d hours</u></i>"
			"</center></h2>\n";
	const char * hdr2 = "<h2><center><i><u>Top %d URI for domain %s: last %d minutes</u></i>"
			"</center></h2>\n";
 	const char * hdr2_1 = "<h2><center><i><u>Top %d URI for domain %s: last %d hour</u>"
			"</i></center></h2>\n";
	const char * hdr2_2 = "<h2><center><i><u>Top %d URI for domain %s: last %d hours</u>"
			"</i></center></h2>\n";

	const char * fmt2 =  "<table width=\"100%\" border=\"0\" cellspacing=\"0\" "
		"cellpadding=\"3\">%s"
		"<tr id=\"analyzer_info_ptitle1\">\n" 
		"<td width=\"20%\"><b><u># of requests</u></b></td>\n"
		"<td><b><u> --  </u></b></td>\n";
	const char * fmt3 = "<td><b><u>Domain</u></b></td><td><b><u>BW in MBytes</u></b></td></tr>\n%s";
	const char * fmt4 = "<td><b><u>URI</u></b></td></tr>\n%s";
	const char * fmt5 = "<td><b><u>Name</u></b></td></tr>\n%s";
	switch (type){
	case DOMAIN:
		fprintf(stdout, fmt,  title1);
		if(thour == 1)
			fprintf(stdout, hdr1_1, data_count, thour);
		else if (thour > 1)
			fprintf(stdout, hdr1_2, data_count, thour);
		else
			fprintf(stdout, hdr1, data_count, tmin);
		fprintf(stdout, fmt2, " ");
		fprintf(stdout, fmt3, " ");
		break;
	case URI:
		fprintf(stdout, fmt,  title2);
		if (thour == 1)
			fprintf(stdout, hdr2_1, data_count, domain, thour);
		else if (thour > 1)
			fprintf(stdout, hdr2_2, data_count, domain, thour);
		else
			fprintf(stdout, hdr2, data_count, domain, tmin);
		fprintf(stdout, fmt2, " ");
		fprintf(stdout, fmt4, " ");
		break;
	default:
		fprintf(stdout, fmt, " ");
		fprintf(stdout, fmt2, " ");
		fprintf(stdout, fmt5, " ");
		break;
	}
	return ret;
}

int display_footer(void)
{
	const char *fmt_end = "</table>\n</body>\n</html>%s";
 	fprintf(stdout, fmt_end, " \n");
	fflush(stdout);
	return 0;
}

int display_rows(int type)
{
	const char * fmt = "<tr id=\"%s\">\n<td align=\"right\">"
                        "<img src=\"/images/green_h.gif\" width=\"%d\" height=\"12\" /></td>\n"
                        "<td>%ld</td> ";
	const char * fmt2 = "<td><A href=\"/admin/accesslog.pl?seed=1&h=%d&m=%d&n=%d&D=%s"
						"&type=uri\">%s</A></td><td>%.3f</td></tr>";
	const char * fmt3 = "<td>%s</td></tr>";

	const char * list_even = "list_even";
        const char * list_odd = "list_odd";
	const char * listing;
	int bar_width; 
	double scale;
	int i; 

	// the information bar should not be more than 100 px, scale it up
	if (data_count == 0) scale = 1;
	else if (type == DOMAIN){
		scale = (dm_rows[0].hotness / 100.0);
	}
	else {
		//scale = (d_rows[0].hotness / 100) + 1;
		//if (scale == 0 ) scale = 1;
		scale = (d_rows[0].hotness / 100.0);
	}
	
	for ( i = 0; i < data_count; i++ ){
		if((i % 2) == 0) listing = list_even;
		else listing = list_odd;

		//fprintf(stdout, fmt, listing,(int)((double)d_rows[i].hotness/scale), d_rows[i].hotness);
		if(type == DOMAIN){
			fprintf(stdout, fmt, listing,(int)((double)dm_rows[i].hotness/scale), dm_rows[i].hotness);
			fprintf(stdout, fmt2, thour, tmin, depth, dm_rows[i].name, dm_rows[i].name, dm_rows[i].bw); 	
		}
		else{ 	
			fprintf(stdout, fmt, listing,(int)((double)d_rows[i].hotness/scale), d_rows[i].hotness);
			fprintf(stdout, fmt3, d_rows[i].name);
		}
	}//end for
	
	return 0;
}

int display_data_stdout(void)
{
	int i; 
	switch(data_type){
	case DOMAIN:
	for ( i = 0; i < data_count; i++){
		fprintf(stdout,  "%30s %10d %15.2f\n",  dm_rows[i].name,  dm_rows[i].hotness, dm_rows[i].bw);
	}
	break;
	case URI:
	for ( i = 0; i < data_count; i++){
		fprintf(stdout,  "%30s %10d\n",  d_rows[i].name,  d_rows[i].hotness);
	}
	break;
	case FILE_SIZE:
		fprintf(stdout, "=====File size=====\n");
		for (i = 0; i < data_count; i++){
			fprintf(stdout, "%30s %10d\n", d_rows[i].name,  d_rows[i].hotness);
		}
		fprintf(stdout, "\n");
	break;
	case RESP_CODE:
		fprintf(stdout, "=====Resp Code=====\n");
		for (i = 0; i < data_count; i++){
			fprintf(stdout, "%30s %10d\n", d_rows[i].name,  d_rows[i].hotness);
		}
		fprintf(stdout, "\n");
	break;
	default:
	break;
	}
	return 0;
}

int parse_data(char* data, int data_len, int type)
{
	char *ptr, *p;
	int i, j, dlen; 
	char *token, *subtoken, *str1, *str2;
	char  *saveptr1, *saveptr2;	
	unsigned int value; 

	ptr = data;

	//This is really simple, skip header, we have label and hotness followed by new line
	//process header
	p = strstr(ptr, "Content-Length");
	sscanf(p, "Content-Length: %d\r\n\r\n", &dlen);
	if(dlen == 0) //we have no data for this period
		return 0;
	p = strstr(ptr, "\r\n\r\n");
	p += 4; //for EOH
	
	//Parse data line by line
	switch (type){
	case DOMAIN:
		i = 0;
		for (i = 0, str1 = p; ; i++, str1 = NULL){
			token = strtok_r(str1, "\n", &saveptr1);
			if(token == NULL)
				break;
			
			for(j = 0, str2 = token ; ; j++, str2 = NULL){
				subtoken = strtok_r(str2, " ", &saveptr2);
				if(subtoken == NULL)
					break;
				if(j == 0){ // this is the domain name
					strncpy(dm_rows[i].name, subtoken, 512);
					dm_rows[i].name[strlen(dm_rows[i].name)]= 0;
				}
				else if (j == 1){ // This is the domain count
					sscanf(subtoken, "%u", &value);
					dm_rows[i].hotness = value;
				}
				else {
					sscanf(subtoken, "%f", &dm_rows[i].bw);
				} 
			}//end for j 
		} //end for i

	break;
	default:
		i = 0;
		for (i = 0, str1 = p; ; i++, str1 = NULL){
			token = strtok_r(str1, "\n", &saveptr1);
			if(token == NULL)
				break;
			//printf("%d: %s\n", i, token);
	
			for(j = 0, str2 = token ; ; j++, str2 = NULL){
				subtoken = strtok_r(str2, " ", &saveptr2);
				if(subtoken == NULL)
					break;
				if(j%2 == 0){ // this is the label
					strncpy(d_rows[i].name, subtoken, 512);
					d_rows[i].name[strlen(d_rows[i].name)]= 0;
				}
				else{ //this is the value data
					sscanf(subtoken, "%u", &value);
					d_rows[i].hotness = value;
				}
			}

		}
	break;
	}
	//printf("Total Data = %d\n", i);
	//for (j = 0; j < i; j++){
	//printf("%30s  %5d\n", d_rows[j].name, d_rows[j].hotness);
	//}
	
	data_count = i;
	return 0;
}

int do_get_data(char *tbuff)
{
	int len, rlen;
	int data_len;

	len = strlen(tbuff);
	tbuff[len]=0;
	len++;
	send(fd, tbuff, len, 0);

	switch (data_type){
	case DOMAIN:
		while(1){
			rlen = recv(fd, tbuff, BUFF_SIZE, 0);
			//printf("DEBUG %s\n", tbuff);
			if (rlen <= 0)
				return 1;
			
			tbuff[rlen]=0;
			parse_data(tbuff, rlen, DOMAIN);
			//show_data_row();
		}
	break;
	case URI:
		while(1){
			rlen = recv(fd, tbuff, BUFF_SIZE, 0);
			//printf("DEBUG %s\n", tbuff);
			if (rlen <= 0)
				return 1;
			tbuff[rlen]=0;
			parse_data(tbuff, rlen, URI);
			//show_data_row();
		}
	break;
	case FILE_SIZE:
		rlen = recv(fd, tbuff, BUFF_SIZE, 0);
		//printf("DEBUG %s\n", tbuff);
		if (rlen <= 0)
			return 1;
		tbuff[rlen]=0;
		parse_data(tbuff, rlen, FILE_SIZE);
	break;
	case RESP_CODE:
		rlen = recv(fd, tbuff, BUFF_SIZE, 0);
		//printf("DEBUG %s\n", tbuff);
		if (rlen <= 0)
			return 1;
		tbuff[rlen]=0;
		parse_data(tbuff, rlen, RESP_CODE);
	break;
	case ALL:
	default:
	break;
	}

	return 0;
}

int show_data(void)
{
	if( display_type == DISP_HTML_OUT){
		switch (data_type){
		case DOMAIN:
			display_header(DOMAIN);
			display_rows(DOMAIN);
			display_footer();
		break;
		case URI:
			display_header(URI);
			display_rows(URI);
			display_footer();
		break;
		case FILE_SIZE:
			display_html_file_size();
		break;
		case RESP_CODE:
			display_html_resp_code();
		break;
		default:
		break;
		}
	}
	else{
		display_data_stdout();
	}
	return 0;
}

/*
 * Creates the connection
 * Sets proper fd
 * returns 0 if success or -ve value
 */
int mk_connection()
{
	int ret; 
	int n = 0, len = 0;
	struct sockaddr_un sun;


	fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if ( fd < 0 )
		return -1;
	
	memset(&sun, 0, sizeof(struct sockaddr_un));
	
	sun.sun_family = AF_UNIX;
	n = sprintf(sun.sun_path, DEFAULT_ANALYZER_UNIX_PATH);
	len = n + sizeof(sun.sun_family);

	ret = connect(fd, (struct sockaddr *) &sun, len);
	if (ret < 0) {
		perror("connect() failed");
		return -2;
	}

	return ret;
}

int create_request(int hr, int min, int recs, int display, char * req)
{
	int ret = 0; 
	char user_cmd [1024];
	char cmd[] = "GET /%s?%s HTTP/1.0\r\n\r\n";
		

	user_cmd[0] = 0;
	sprintf(user_cmd, "thour=%d&tmin=%d&depth=%d", thour, tmin, depth);
	user_cmd[strlen(user_cmd)] = 0;
	switch (data_type){
	case DOMAIN:
		sprintf(req, cmd, data_cmd[DOMAIN], user_cmd);
	break;
	case URI:
		strcat(user_cmd, "&domain=");
		strcat(user_cmd, domain);
		user_cmd[strlen(user_cmd)] = 0;
		sprintf(req, cmd, data_cmd[URI], user_cmd);
	break;
	case RESP_CODE:
		sprintf(req, cmd, data_cmd[RESP_CODE], user_cmd);
	break;
	case FILE_SIZE:
		sprintf(req, cmd, data_cmd[FILE_SIZE], user_cmd);
	break;
	default:
	break;
	}

	req[strlen(req)] = 0;
	return ret;
}


int get_expanded_uri_info(int hr, int min, int recs, int display, char * req)
{
	int ret;

	if (fd) close(fd);
	ret = mk_connection();
	if (ret < 0)
		return  ret;

	// All correct
	// create request string
	// get data
	data_type = URI;
	strncpy(domain, d_rows[0].name, 511);
	domain[strlen(domain)] = 0;
	ret = create_request(hr, min, recs, display, req);
	
	do_get_data(req);
	return ret;

}

void usage (void)
{
	printf("\n");
	printf("querylog utility \n");
	printf("Usage: \n");

	printf("-h  <time-interval in hour>\n");
	printf("-m  <time-interval in minute>\n");
	printf("-n  <#of records>\n");
	printf("-d  #for domain name analysis\n");
	printf("-u  #for uri analysis\n");
	printf("-D  <domain name> required for uri analysis\n");
	printf("-f  #for file size analysis\n");
	printf("-r  #for resp code analysis\n");
	printf("-S  #display format, 0 text, 1 html\n");
	printf("-H  This help\n");
	printf("-v  Version info\n");
	printf("-e  Expanded uri data, only valid for single domain entry\n");
	printf("Example: \n");
	printf("  To Run:\n");
	printf("1)    querylog -h 1 -m 5 -n 20 -d -S 0\n");
	printf("        [shows top 20 domain name for last one hour and 5 min as plain text] \n");	
	printf("2)    querylog -h 1 -m 5 -n 20 -d 172.19.172.133 -u -S 0\n");
	printf("	[show top 20 uri for domain 172.19.172.133 for last hour and 5 min as plain text]\n");
	printf("3)    querylog -h 1 -m 10 -r -S 0\n");
	printf("	[show responce code analysis for last hour and 5 min as plain text]\n");
	printf("4)    querylog -h 1 -m 5 -f -S 0\n");
	printf("	[show file size analysis for last hour amd 5 min as plain text format]\n");
	printf("5)    querylog -h 1 -m 5 -n 20 -d -e -S 0\n");
	printf("        [shows top 20 uri name for last one hour and 5 min as plain text if there is only one domain] \n");
	printf("\n");
	exit(1);

}

/*
 * command: querylog t=0 n=20 ...
 */
int main(int argc, char *argv[])
{
	int ret;
	char opt;
	char dbuf[BUFF_SIZE];
	

	//initialize data
	thour = 0;
	tmin = 5; 
	depth = 20;
	display_type = DISP_HTML_OUT;
	data_type = DOMAIN;
	auto_expand = 0;

	//Get the user options
	while ((ret = getopt(argc, argv, "h:m:n:D:dufrveHS:")) != -1) {
		switch (ret) {
		case 'h':
			thour = atoi(optarg);
			break;
		case 'm':
			tmin = atoi(optarg);
			break;
		case 'n':
			depth =  atoi(optarg);
			break;
		case 'D':
			strncpy(domain, optarg, 511);
			domain[strlen(domain)] = 0;
			data_type = URI;
			break;
		case 'd':
			data_type = DOMAIN;
			break;
		case 'u':
			data_type = URI;
			break;
		case 'r':
			data_type = RESP_CODE;
			break;
		case 'f':
			data_type = FILE_SIZE;
			break;
		case 'e':
			auto_expand = 0;		
			break;
		case 'v':
			printf("Querylog version 1.0\n");
			return 1;
		case 'H':
			usage();
			return 1;
		case 'S':
			display_type = atoi(optarg);
			if (display_type != 0) // user typed something wrong
				display_type = DISP_HTML_OUT;
			break;
		default:
			usage();
                        return 1;			
		}
	}

	ret = create_request(thour, tmin, depth, display_type, dbuf);

	ret = mk_connection();
	if (ret < 0)
		return  ret;

	do_get_data(dbuf);

	/* 
	 * If the request for domain info, if we have only one domain we can show the URI
	 * Only of the user requested for expanded version
	 */
	if((data_type == DOMAIN) && (auto_expand == 1) && (data_count == 1)){
		get_expanded_uri_info(thour, tmin, depth, display_type, dbuf);
	}
	
	show_data();
bail:
	close(fd);
	return 0;
}
