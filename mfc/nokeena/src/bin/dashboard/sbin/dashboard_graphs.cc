#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include "nkn_dashboard.h"
#include "dashboard_graphs.h"

/*
 * To create file 
 * 	info.html, 
 * 	average_cpu.png, 
 * 	open_connection.png
 * 	disk_thr_small.png
 * 	media_delivery_bandwidth.png 
 */
bool show_dashboard   = true;  

/*
 * To create graphs:
 * 	disk_throughput.png
 * 	disk_usage.png
 */

bool show_disk_cache  = true;
/* 
 * To create graphs:
 * 	hourly_network_load.png
 * 	daily_network_load.png
 * 	weekly_network_load.png
 */

bool show_network_data = false;
/*
 * To create graphs:
 * 	data_source.png
 * 	network_bandwidth.png
 *      memory_usage.png	
 */

bool show_other = false;

typedef struct disk_thput_gr_t{
	double raw_read_rate;
	double raw_write_rate;
	int provider_type;
	char disk_name[MAX_DISK_NAME_SIZE];
}disk_thput_gr_t;

typedef struct disk_usage_gr_t{
	double free_blocks;
	double free_resv_blocks;
	double total_blocks;
	double block_size;
	int provider_type;
	char disk_name[MAX_DISK_NAME_SIZE];
}disk_usage_gr_t;

static char imgpath[40] = "/var/nkn/dashboard/";

void display_media_delivery_bandwidth_small();
void make_graph_bandwidth_savings();
void make_graph_cache_tier_throughput();
void make_cache_hit_ratio_daygraph();
void make_namespace_counter_html();
void make_namespace_chr_png();

double get_delta_data(double this_hr_data, double last_hr_data)
{
    if (this_hr_data >= last_hr_data) {
	return this_hr_data - last_hr_data;
    }
    return this_hr_data; // restarted
}

///////////////////////////////////////////////////////////////////////
// Grnerate graphs based on the flags
//////////////////////////////////////////////////////////////////////

void * generate_graph(void * arg)
{
    while(1)
    {
	pthread_mutex_lock( &mutex1 );
	make_namespace_counter_html(); // GB Delivered, Cachehit Ratio, video delivered
	make_namespace_chr_png();
	//create all graphs
	make_graph_cpu_usage();
	make_graph_open_connection();
	make_graph_bandwidth_savings();
	display_media_delivery_bandwidth_small();
	make_graph_cache_tier_throughput();
	make_cache_hit_ratio_daygraph();
	if (show_disk_cache){
	    make_graph_disk_throughput_2();
	    make_graph_disk_usage();
	}
	if (show_network_data){//should generate graph once every hour ideally
	    make_hourly_network_graph();
	    make_daily_network_graph();
	    make_weekly_network_graph();
	}
	make_graph_info_2(); // GB Delivered, Cachehit Ratio, video delivered
	if (show_other){
	    make_graph_data_source();
	    make_graph_network_bandwidth();
	    make_graph_memory();	//make memory graph
	}
	pthread_mutex_unlock( &mutex1 );
	if( 0 == sleep_time )
	    break;
	sleep(sleep_time);
    }
    return NULL;
};

double find_max(double *data, int data_points);
double find_max(double *data, int data_points)
{
    double max_val;
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

void make_graph_bandwidth_savings()
{
    char filename[100];
    const char *imgname = "weekly_bw_savings.png";
    double origin_bw[WEEK];
    double saved_bw[WEEK];
    char *label[WEEK];	
    int i;

    for (i = 0; i < WEEK; i++){
	label[i]= (char *)malloc(10);
    }

    for(i = 0; i < WEEK; i++){
	origin_bw[i] = day_bw_data[i].origin_bw + day_bw_data[i].nfs_bw + day_bw_data[i].tunnel_bw;
	saved_bw[i] = day_bw_data[i].saved_bw;
	//convert up to 2 decimal points only
	origin_bw[i] = (double)((long)(origin_bw[i]* 100))/100;
	saved_bw[i] = (double)((long)(saved_bw[i]* 100))/100;
	strcpy(label[i], day_bw_data[i].date);
    }

    strcpy(filename, imgpath); // add path to the filename
    strcat(filename, imgname); // fully qualified file name

    double * data[]= {origin_bw, saved_bw};
    const char* legend[]= {"Origin Bandwidth", "Saved Bandwidth"};
    int data_colors[]= {0xff8080, 0x80ff80};

    make_stacked_bar_chart(filename, (char *)"Weekly Bandwidth Savings", 12, 350, 250, data, 2, (char **)legend, 7, label, (char *)"", (char *)"GBytes", data_colors);  

    //free up memory
    for (i = 0; i < WEEK; i++)
	free(label[i]);

    return;
}

/*
 * make_graph_cache_tier_throughput	
 */
void make_graph_cache_tier_throughput()
{
    int i, j; 
    int num_tier = 0;

    //check how many data tiers we have
    for(i = 0; i < MAX_CACHE_TIER; i++){
	if(ctdata[i].flag == 1)
	    num_tier++;
    }

    //Set the data for graph
    double read_data[MAX_CACHE_TIER + 1];  // Serve/Read data
    double promote_data[MAX_CACHE_TIER + 1]; // Promote/Write data
    double evict_data[MAX_CACHE_TIER + 1]; // Evict/delete data
    const char * label[MAX_CACHE_TIER + 1];

    for(i = 0; i < MAX_CACHE_TIER; i++){
	label[i] = (char *)malloc(10); // 20 bytes should be larger enough
    }
    j = 0;
    for (i = 0; i < MAX_CACHE_TIER; i++){
	if(ctdata[i].flag == 1){//copy this data for graph
	    read_data[j] = ctdata[i].read_rate[MAX_DATA_NUMBER-1];
	    promote_data[j] = ctdata[i].write_rate[MAX_DATA_NUMBER-1];
	    evict_data[j] =  ctdata[i].evict_rate[MAX_DATA_NUMBER-1];
	    switch(i){
		case 0:
		    strcpy((char *)label[j], "RAM");
		    break;
		case 1:
		    strcpy((char *)label[j], "SSD");
		    break;
		case 5:
		    strcpy((char *)label[j], "SAS");
		    break;
		case 6:
		    strcpy((char *)label[j], "SATA");
		    break;
		default:
		    strcpy((char *)label[j], "Unknown");
		    break;
	    }//end switch
	    j++;
	}//end if 
    }//end for

    const char* legend[3]= {"Serve\n(Read)", "Promote\n(Write)", "Evict\n(Delete)"};

    char filename[100];
    const char *imgname = "cache_tier_throughput.png";

    strcpy(filename, imgpath); //add the path
    strcat(filename, imgname); // fully qualified pathname

    double *data[3] = {read_data, promote_data, evict_data};

    //Draw the graph here
    int xsize = 350;        // x size for the graph
    int ysize = 250;        // y size for the graph
    int x_margin = 55;      // leave 55 pixel on the left for label
    int y_margin = 55;      // leave 40 pixel on the top for title
    int title_font = 12;    // font for the title
    const char * title = "Cache Tier Throughput";
    int num_dataset = 3;    // read_data, promote_data, evict_data
    int data_points = num_tier;
    const char* y_title = "Mbits/sec";

    int xplot_area = xsize - x_margin - 10; //leave 10 pixel for right margin
    int yplot_area = ysize - y_margin - 35; //leave 35 pixel for x-lebel


    // Create a XYChart object of size xsize x ysize pixels
    XYChart *c = new XYChart(xsize, ysize);

    // Add a title to the chart using 12 pts Times Bold Italic font
    c->addTitle(title, "timesbi.ttf", title_font);

    c->setPlotArea( x_margin,
	    y_margin,
	    xplot_area,
	    yplot_area,
	    c->linearGradientColor(0, x_margin, 0, ysize, 0xf9fcff, 0xaaccff),
	    -1,
	    Chart::Transparent, 0xffffff);

    //Add a legend to the chart
    //center the legend on the screen
    c->addLegend(x_margin+10, 16, false, "timesbi.ttf", 10)->setBackground(Chart::Transparent);

    // Add a title to the y axis
    c->yAxis()->setTitle(y_title, "arialbi.ttf", 10 );

    // Set the labels on the x axis.
    c->xAxis()->setLabels(StringArray(label, data_points));
    //Set tick offset 0.5
    c->xAxis()->setTickOffset(0.5);

    // Set the axes width to 2 pixels
    c->xAxis()->setWidth(2);
    c->yAxis()->setWidth(2);


    // Add a multi-bar layer with 2 data sets and 4 pixels 3D depth
    BarLayer *layer = c->addBarLayer(Chart::Side , 4);
    layer->setBarShape(Chart::CircleShape);
    layer->addDataSet(DoubleArray(read_data, data_points), 0x00ff00, legend[0]);
    layer->addDataSet(DoubleArray(promote_data, data_points), 0xffff00, legend[1]);
    layer->addDataSet(DoubleArray(evict_data, data_points), 0xff00000, legend[2]);

    layer->setBorderColor(Chart::Transparent, Chart::glassEffect(Chart::NormalGlare, Chart::Left));
    // Configure the bars within a group to touch each others (no gap)
    layer->setBarGap(0.2, Chart::TouchBar);


    // Output the chart
    c->makeChart(filename);

    //free up resources
    delete c;

    for(i = 0; i < MAX_CACHE_TIER; i++){
	free((void *)label[i]);
    }
    return;
}

/*
 * Make a round meter chart to display averahe cpu usage
 */
void make_graph_cpu_usage()
{
    char filename[100];
    const char * title = "CPU";
    int xsize = 200;
    int ysize = 115;
    double data_val = g_cpu.avg_cpu;
    int green_zone = 60; //Range = 0 - 60
    int yellow_zone = 80; // 60 - 80
    int red_zone = 100; // 80 - 100

    const char *imgname = "average_cpu.png";

    strcpy(filename, imgpath); // add path to the filename
    strcat(filename, imgname); // fully qualified file name

    //make_round_meter_chart(filename, title,  xsize, ysize, data_val, green_zone, yellow_zone, red_zone);
    make_semi_circle_chart(filename, (char *)title,  xsize, ysize, data_val, green_zone, yellow_zone, red_zone);
    return;
}

/*
 * Make a 3d -pie chart for Cache-memory and scratch_buffer
 */

void make_graph_memory()
{
    char filename[100];
    const char *imgname = "memory_usage.png";

    double data[MAX_MEMORY];

    data[0] = g_mem.scratch_buffer;
    data[1] = g_mem.cache_mem;

    const char *labels[] = {"Free Buffer Space", "Cached Objects"};
    // The colors to use for the sectors
    int colors[] = {0x2cb135, 0x0072cf};

    strcpy(filename, imgpath); // ad the path
    strcat(filename, imgname); // fully qualified pathname
    make_3d_pie_chart( filename, (char *)"Memory Usage", 12, 260, 200, data, 2, (char **)labels, colors, 4);

    return;
};

/*
 * Network bandwidth graph, make a symbol-line chart
 */

void make_graph_network_bandwidth()
{
    char filename[100];
    const char *imgname = "network_bandwidth.png";

    strcpy(filename, imgpath); // add the pathname
    strcat(filename, imgname); //fully qualified pathname

    double* data[]= {network_bandwidth_rate}; // only one set of data
    //data[0] = network_bandwidth_rate;
    const char* legend[1]; //only one dataset
    legend[0] = "Network";
    char** label= time_str;
    int data_color[1];
    data_color[0] = 0xff0000;
    make_symbol_line_chart( filename, (char *)"Network Bandwidth", 12, 350, 250,
	    data, 1, (char **)legend, MAX_DATA_NUMBER, label, (char *)"", (char *)"Mbits / Sec", data_color);
    return;

};


/*
 *  Active sessions/ open connection  graph
 *  draw a stacked area chart to display data
 */

void make_graph_open_connection()
{
    char filename[100];
    const char *imgname = "open_connections.png";
    const char* title = "Open Connections";
    int xsize = 350;
    int ysize = 250;

    double rtsp_data[X_POINTS_10];
    double http_data[X_POINTS_10];
    double om_data[X_POINTS_10];
    const char *label[X_POINTS_10];
    int datapoints = X_POINTS_10;

    const char* x_title = "";
    const char* y_title= "Connections";
    int area_color = 0x80ff0000;
    int i, j;

    for (i = 0; i < X_POINTS_10; i++){
	label[i] = (char *) malloc(6);
    }

    for (i = 0, j = MAX_DATA_NUMBER - X_POINTS_10; i < X_POINTS_10, j < MAX_DATA_NUMBER; i++, j++){
	http_data[i] = global_http_con_data[j] + global_http_con_data_ipv6[j];
	rtsp_data[i] = global_rtsp_con_data[j];
	om_data[i] = global_om_connection_data[j] + global_om_connection_data_ipv6[j];
	memcpy((void *)label[i], (void *)time_str[j], 5);
	((char *)label[i])[5] = 0;	
    }


    strcpy(filename, imgpath); // add full path name
    strcat(filename, imgname); // fully qualified filename

    // make a tacked area chart, add http data first, and then rtsp and then on data
    // Create a XYChart object of size xsize x ysize pixels
    XYChart *c = new XYChart(xsize, ysize);
    // Set the plotarea at (55, 40) and of size 285 x 175 pixels
    int x_margin = 55; // leave 55 pixel on the left for label
    int y_margin = 50; // leave 50 pixel on the top for title
    int xplot_area = xsize - x_margin - 10; //leave 10 pixel for right margin
    int yplot_area = ysize - y_margin - 35; //leave 35 pixel for x-lebel
    c->setPlotArea( x_margin,
	    y_margin,
	    xplot_area,
	    yplot_area,
	    c->linearGradientColor(0, x_margin, 0, ysize, 0xf9fcff, 0xaaccff),
	    -1,
	    Chart::Transparent, 0xffffff);
    //center the legend on the graph
    c->addLegend(30, 15, false, "timesbi.ttf", 10)->setBackground(Chart::Transparent);
    // Add a title box to the chart using 12 pts Times Bold Italic font.
    c->addTitle(title, "timesbi.ttf", 12);
    // Add a title to the y axis
    c->yAxis()->setTitle(y_title, "arialbi.ttf", 10 );	

    //Set the scale of y axis min = 0, max = max_val + 20%
    //find max val in the data stream
    double max_data_val_http = find_max(http_data, X_POINTS_10);
    double max_data_val_rtsp = find_max(rtsp_data, X_POINTS_10);
    double max_data_val_om = find_max(om_data, X_POINTS_10);
    double upperlimit = max_data_val_http + max_data_val_rtsp + max_data_val_om + 
	((max_data_val_http + max_data_val_rtsp + max_data_val_om)  * (0.2));
    //Put the upper limit to atleast 5, so that we don't see a decimal point on the y-axis
    if( upperlimit < 5 ) upperlimit = 5;
    c->yAxis()->setLinearScale(0, upperlimit);
    c->yAxis()->setRounding(true, true);

    c->xAxis()->setLabels(StringArray(label, X_POINTS_10));
    c->xAxis()->setWidth(2);
    c->yAxis()->setWidth(2);

    // Add an stack area layer with three data sets
    AreaLayer *layer = c->addAreaLayer(Chart::Stack);
    layer->addDataSet(DoubleArray(http_data, X_POINTS_10), 0x80dd0000, "http-connection");
    layer->addDataSet(DoubleArray(rtsp_data, X_POINTS_10), 0x8000dd00, "rtsp-connection");
    layer->addDataSet(DoubleArray(om_data, X_POINTS_10), 0x80000080, "om-connection");

    c->makeChart(filename);
    //Free up resources
    delete c;

    for(i = 0; i < X_POINTS_10; i++){
	free((void *)label[i]);
    }
    return;
};

/*
 *  Data Source graph
 *  make make_soft_multibar_chart  to display
 */

void make_graph_data_source()
{
    double data_cache[X_POINTS_10];
    double data_disk[X_POINTS_10];
    double data_origin[X_POINTS_10];
    char *time_label[X_POINTS_10];
    int i, j;
    char filename[100];
    const char *imgname="data_source.png";

    for (i = 0; i < X_POINTS_10; i++) {
	time_label[i]= (char *)malloc(6);
    }

    for ( i = MAX_DATA_NUMBER-1, j = 9; i >= MAX_DATA_NUMBER-10; i--, j--){
	data_cache[j] = cache_rate[i] + rtsp_cache_rate[i];
	data_disk[j] = disk_rate[i];
	data_origin[j] = origin_rate[i] + nfs_rate[i]+ rtsp_origin_rate[i];
	memcpy((void *)time_label[j], (void *)time_str[i],5);
	((char *)time_label[j])[5] = 0;
    }

    strcpy(filename, imgpath); //add the path
    strcat(filename, imgname); // fully qualified pathname
    double* data[]= {data_cache, data_disk, data_origin};
    const char* legend[]= {"RAM", "Disk", "Origin"};
    int colors[] = {0x2cb135, 0x0072cf, 0xdc291e};

    make_soft_multibar_chart(filename, (char *)"Data Source", 12, 350, 250, data, 3,
	    (char **)legend, X_POINTS_10, time_label, (char *)"", (char *)"MBytes / sec", colors);

    for (i = 1; i < X_POINTS_10; i++) {
	free(time_label[i]);
    }
    return;
};


/*
 *       Disk throughput, read/write graph
 */

void  make_graph_disk_throughput_2()
{
    int i = 0, j = 0;

    double graph_read_rate[MAX_DISK];
    double graph_write_rate[MAX_DISK];
    const char *label[MAX_DISK];
    char filename[100];
    const char *imgname="disk_throughput.png";

    disk_thput_gr_t disk_thput_gr[MAX_DISK];
    disk_thput_gr_t temp;
    int valid_disks = 0;
    int img_width = 540;
    int img_height = 250;

    if (total_num_cache > MAX_DISK)
	return;

    //copy the information into disk_thput_gr
    j = 0;
    for(i = 0; i < total_num_cache; i++){
	if(dm2_data[i].valid_disk){//don't copy the info for invalid disks
	    disk_thput_gr[j].raw_read_rate =  dm2_data[i].raw_read_rate[MAX_DATA_NUMBER-1];
	    disk_thput_gr[j].raw_write_rate = dm2_data[i].raw_write_rate[MAX_DATA_NUMBER-1];
	    disk_thput_gr[j].provider_type = dm2_data[i].provider_type;
	    strncpy(disk_thput_gr[j].disk_name, dm2_data[i].disk_name, MAX_DISK_NAME_SIZE);
	    j++;
	    valid_disks++;
	}
    }

    //sort the data according to type
    if (valid_disks > 1){
	for (i = 1; i < valid_disks; i++){
	    for (j = valid_disks-1; j >= i; --j){
		//compare the adjecent elements
		if(disk_thput_gr[j-1].provider_type > disk_thput_gr[j].provider_type){
		    //swap elements		
		    temp.raw_read_rate = disk_thput_gr[j-1].raw_read_rate;
		    temp.raw_write_rate = disk_thput_gr[j-1].raw_write_rate;
		    temp.provider_type = disk_thput_gr[j-1].provider_type;
		    strncpy(temp.disk_name, disk_thput_gr[j-1].disk_name, MAX_DISK_NAME_SIZE);

		    disk_thput_gr[j-1].raw_read_rate = disk_thput_gr[j].raw_read_rate;
		    disk_thput_gr[j-1].raw_write_rate = disk_thput_gr[j].raw_write_rate;
		    disk_thput_gr[j-1].provider_type = disk_thput_gr[j].provider_type;
		    strncpy(disk_thput_gr[j-1].disk_name, disk_thput_gr[j].disk_name, MAX_DISK_NAME_SIZE); 

		    disk_thput_gr[j].raw_read_rate = temp.raw_read_rate;
		    disk_thput_gr[j].raw_write_rate = temp.raw_write_rate;
		    disk_thput_gr[j].provider_type = temp.provider_type;
		    strncpy(disk_thput_gr[j].disk_name, temp.disk_name, MAX_DISK_NAME_SIZE);
		}//if	
	    }//2nd for
	}//1st for
    }//if (valid_disks > 1)

    for(i = 0; i < valid_disks; i++){
	label[i] = (char *)malloc(20); // 20 bytes should be larger enough

	if (disk_thput_gr[i].provider_type == 1) { //strcpy(label[i], "Disk\nSSD"); 
	    sprintf((char *)label[i], "%s\nSSD", disk_thput_gr[i].disk_name);}
	else if (disk_thput_gr[i].provider_type == 5) { //strcpy(label[i], "Disk\nSAS");
	    sprintf((char *)label[i], "%s\nSAS", disk_thput_gr[i].disk_name); }
	else if (disk_thput_gr[i].provider_type == 6) { //strcpy(label[i], "Disk\nSATA");
	    sprintf((char *)label[i], "%s\nSATA", disk_thput_gr[i].disk_name); }
	else { strcpy((char *)label[i], "Disk\nUnknown"); }

	graph_read_rate[i] = disk_thput_gr[i].raw_read_rate * 8; // convert to bits
	graph_write_rate[i] = disk_thput_gr[i].raw_write_rate * 8; // convert to bits
    }
    strcpy(filename, imgpath);
    strcat(filename, imgname);

    if (valid_disks > 10) {
	img_width = valid_disks * 50;
    }

    make_soft_multibar_2input_chart(filename, (char *)"Disk Throughput", 14, img_width, img_height,
	    graph_read_rate, (char *)"Read", graph_write_rate, (char *)"Write", valid_disks,
	    (char **)label, (char *)"", (char *)"Mbits / sec", 0x00ff00, 0xffff00);
    //free memory
    for (i = 0; i < valid_disks; i++) {
	free((void *)label[i]);
    }

    return;
};

/*
 *       Disk usage graph
 */

void  make_graph_disk_usage()
{
    int i = 0, j = 0;
    double fr_disk[MAX_DISK], fr_resv_disk[MAX_DISK], used_disk[MAX_DISK], total_disk[MAX_DISK];
    const char *label[MAX_DISK];
    char filename[100];
    const char *imgname = "disk_usage.png";

    disk_usage_gr_t disk_usage_gr[MAX_DISK];
    disk_usage_gr_t temp;
    int valid_disks = 0;
    int img_height = 250;;
    int img_width = 540;

    if (total_num_cache >  MAX_DISK)
	return;

    //copy the dm2 information into disk_usage_gr_t
    j = 0;
    for(i = 0; i < total_num_cache; i++){
	if(dm2_data[i].valid_disk){//don't copy the info for invalid disks
	    disk_usage_gr[j].free_blocks = dm2_data[i].free_blocks;
	    disk_usage_gr[j].free_resv_blocks = dm2_data[i].free_resv_blocks;
	    disk_usage_gr[j].total_blocks =  dm2_data[i].total_blocks;
	    disk_usage_gr[j].block_size =  dm2_data[i].block_size;
	    disk_usage_gr[j].provider_type = dm2_data[i].provider_type;
	    strncpy(disk_usage_gr[j].disk_name, dm2_data[i].disk_name, MAX_DISK_NAME_SIZE);
	    j++;
	    valid_disks++;
	}//if	
    } //end for

    //sort the data according to type
    if(valid_disks > 1){
	for (i = 1; i < valid_disks; i++){
	    for (j = valid_disks-1; j >= i; --j){
		//compare the adjecent elements
		if(disk_usage_gr[j-1].provider_type > disk_usage_gr[j].provider_type){
		    //swap elements
		    temp.free_blocks = disk_usage_gr[j-1].free_blocks;
		    temp.free_resv_blocks = disk_usage_gr[j-1].free_resv_blocks;
		    temp.total_blocks = disk_usage_gr[j-1].total_blocks;
		    temp.block_size = disk_usage_gr[j-1].block_size;
		    temp.provider_type = disk_usage_gr[j-1].provider_type;
		    strncpy(temp.disk_name,  disk_usage_gr[j-1].disk_name, MAX_DISK_NAME_SIZE);

		    disk_usage_gr[j-1].free_blocks = disk_usage_gr[j].free_blocks;
		    disk_usage_gr[j-1].free_resv_blocks = disk_usage_gr[j].free_resv_blocks;
		    disk_usage_gr[j-1].total_blocks = disk_usage_gr[j].total_blocks;
		    disk_usage_gr[j-1].block_size = disk_usage_gr[j].block_size;
		    disk_usage_gr[j-1].provider_type = disk_usage_gr[j].provider_type;
		    strncpy(disk_usage_gr[j-1].disk_name, disk_usage_gr[j].disk_name, MAX_DISK_NAME_SIZE);

		    disk_usage_gr[j].free_blocks = temp.free_blocks;
		    disk_usage_gr[j].free_resv_blocks = temp.free_resv_blocks;
		    disk_usage_gr[j].total_blocks = temp.total_blocks;
		    disk_usage_gr[j].block_size = temp.block_size;
		    disk_usage_gr[j].provider_type = temp.provider_type;
		    strncpy(disk_usage_gr[j].disk_name, temp.disk_name, MAX_DISK_NAME_SIZE);
		}//if
	    }//2nd for
	}//1st for
    }//if(valid_disks > 1)

    for(i = 0; i < valid_disks; i++){
	label[i] = (char *)malloc(20);

	if (disk_usage_gr[i].provider_type == 1) { //strcpy(label[i], "Disk\nSSD");
	    sprintf((char *)label[i], "%s\nSSD", disk_usage_gr[i].disk_name);}
	else if (disk_usage_gr[i].provider_type == 5) { //strcpy(label[i], "Disk\nSAS");
	    sprintf((char *)label[i], "%s\nSAS", disk_usage_gr[i].disk_name);}
	else if (disk_usage_gr[i].provider_type == 6) { //strcpy(label[i], "Disk\nSATA");
	    sprintf((char *)label[i], "%s\nSATA", disk_usage_gr[i].disk_name); }
	else { strcpy((char *)label[i], "Disk\nUnknown"); }

	/* convert unit to GBytes/Second */
	fr_disk[i] = disk_usage_gr[i].free_blocks * disk_usage_gr[i].block_size / 1000; //in GBytes
	fr_resv_disk[i] = disk_usage_gr[i].free_resv_blocks * disk_usage_gr[i].block_size / 1000;
	total_disk[i] = disk_usage_gr[i].total_blocks *  disk_usage_gr[i].block_size / 1000;
	used_disk[i] = total_disk[i] - fr_disk[i];

	/* make two digits after dot */
	fr_disk[i] = (double)((int)(fr_disk[i] * 100.0)/ 100.0);
	fr_resv_disk[i] = (double)((int)(fr_resv_disk[i] * 100.0)/ 100.0);
	total_disk[i] = (double)((int)(total_disk[i] * 100.0)/ 100.0);
	used_disk[i] = (double)((int)(used_disk[i] * 100.0)/ 100.0);

	if (used_disk[i] < 0.0) { used_disk[i] = 0.0; }
	if (fr_disk[i] < 0.0) { fr_disk[i] = 0.0; }
    }

    strcpy(filename, imgpath); // add pathname
    strcat(filename, imgname); // fully qualified pathname

    if (valid_disks > 10) {
	img_width = valid_disks * 50;
    }

    make_stacked_2bar_chart( filename, (char *)"Disk Usage", 14, img_width, img_height,
	    used_disk, (char *)"Used Disk", fr_disk,  (char *)"Free Disk", valid_disks,
	    (char **)label, (char *)"", (char *)"GBytes", 0x0072cf, 0x2eb135);

    for (i = 0; i < valid_disks; i++) {
	free((void *)label[i]);
    }
    return;

};

/*
 *	Make daily network load grapg
 */

void make_daily_network_graph()
{
    double data[X_POINTS_DAY];
    char* label[X_POINTS_DAY];
    int i;
    char filename[100];
    int startindex = HIST_MAX_DATA_COUNT - X_POINTS_DAY;
    const char* imgname = "daily_network_load.png";

    for(i = 0; i < X_POINTS_DAY; i++)
	label[i] = (char *)malloc(6);

    //copy the data and time string
    for (i = startindex; i < HIST_MAX_DATA_COUNT; i++){
	sprintf(label[i-startindex], "%d:00",  h_network_data[i].hr);
	data[i-startindex] = h_network_data[i].netrate;
    }

    strcpy(filename, imgpath);
    strcat(filename, imgname);

    make_3d_area_chart(filename, (char *)"Daily Network Usage", 12, 1000, 250, data,
	    X_POINTS_DAY, label, 1, (char *)"", (char *)"Mbits / Sec",  0x00ff0000);

    //free up resources
    for (i = 0; i < X_POINTS_DAY; i++){
	free ((void *)label[i]);
    }
    return;

};


/*
 * 	Make weekly network load grapg
 */

void make_weekly_network_graph()
{
    double data[HIST_MAX_DATA_COUNT];
    char* label[HIST_MAX_DATA_COUNT];
    int i;
    char filename[100];
    const char* imgname = "weekly_network_load.png";

    for(i = 0; i < HIST_MAX_DATA_COUNT; i++)
	label[i] = (char *)malloc(6);

    //copy the data and time string
    for (i = 0; i < HIST_MAX_DATA_COUNT; i++){
	strcpy(label[i], h_network_data[i].day);
	data[i] = h_network_data[i].netrate;
    }

    strcpy(filename, imgpath); // add path
    strcat(filename, imgname); // fully qualified filename

    make_3d_area_chart(filename, (char *)"Weekly Network Usage", 12, 1000, 250, data,
	    HIST_MAX_DATA_COUNT, label, X_POINTS_DAY/*set label step 24, one for each day*/,
	    (char *)"", (char *)"Mbits / Sec", 0x0000ff00);

    //free up resources
    for (i = 0; i < HIST_MAX_DATA_COUNT; i++){
	free ((void *)label[i]);
    }
    return;
};

/*
 * Make hourly network grapg
 */

void make_hourly_network_graph()
{
    double data[HOUR_NETWORK_DATA_POINT];
    char * label[HOUR_NETWORK_DATA_POINT];
    int i;
    char filename[100];
    const char* imgname = "hourly_network_load.png";


    for(i = 0; i < HOUR_NETWORK_DATA_POINT; i++)
	label[i] = (char *)malloc(6);

    //copy the data and time string
    for (i = 0; i < HOUR_NETWORK_DATA_POINT; i++){
	strcpy(label[i], net_data_hr[i].time);
	data[i] = net_data_hr[i].rval;
    }

    strcpy(filename, imgpath); // add the path name
    strcat(filename, imgname); // fully qualified file name

    make_3d_area_chart(filename, (char *)"Hourly Network Usage", 12, 1000, 250, data,
	    HOUR_NETWORK_DATA_POINT, label, 60, (char *)"", (char *)"Mbits / Sec", 0x000000ff);

    //free up memory
    for (i = 0; i < HOUR_NETWORK_DATA_POINT; i++){
	free ((void *)label[i]);
    }
    return;
};

void make_namespace_counter_html() // GB Delivered, Cachehit Ratio, video delivered
{
    char filename[100];
    char * labels[MAX_NAMESPACE];
    double sum24hrs_total[MAX_NAMESPACE];
    double sum24hrs_cache[MAX_NAMESPACE];
    double sum24hrs_noncache[MAX_NAMESPACE];
    double sum24hrs_CHR[MAX_NAMESPACE];
    double tot_bw;
    int i, j;
    const char* imgname = "ns_bandwidth.png";
    int xsize = 800;
    int ysize = 250;
    const char* title = "Namespace-level Last 24 Hours' Average Cache Hit Ratio";
    const char* y_title = "Percentage";
    time_t now;
    struct tm * t;
    int tnow, this_hr, last_hr;
    int display_total_ns_data;

    now = time(NULL);
    t = localtime(&now);
    tnow = t->tm_hour;
    /*
     * Calculate the data.
     */
    tot_bw = 0.0;

    for (j=0; j<total_ns_data; j++)
    {
	labels[j] = ns_data[j].name;
	sum24hrs_cache[j] = 0.0;
	sum24hrs_noncache[j] = 0.0;
	for (i=0; i<HOURS-1; i++) {
	    this_hr = (tnow+i)%24;
	    last_hr = (tnow+i+23)%24;

	    /* Cacheable */
	    sum24hrs_cache[j] += get_delta_data(ns_data[j].total_values.out_bytes_disk[this_hr],
		    ns_data[j].total_values.out_bytes_disk[last_hr]);
	    sum24hrs_cache[j] += get_delta_data(ns_data[j].total_values.out_bytes_ram[this_hr],
		    ns_data[j].total_values.out_bytes_ram[last_hr]);

	    /* Non-cacheable */
	    sum24hrs_noncache[j] += get_delta_data(ns_data[j].total_values.out_bytes_origin[this_hr],
		    ns_data[j].total_values.out_bytes_origin[last_hr]);
	    sum24hrs_noncache[j] += get_delta_data(ns_data[j].total_values.out_bytes_tunnel[this_hr],
		    ns_data[j].total_values.out_bytes_tunnel[last_hr]);
	}
	sum24hrs_total[j] = sum24hrs_cache[j] + sum24hrs_noncache[j];
	if (sum24hrs_total[j] == 0) {
	    sum24hrs_CHR[j] = 0.0;
	}
	else {
	    sum24hrs_CHR[j] = (int)(100.0 * sum24hrs_cache[j] / sum24hrs_total[j]);
	}
	tot_bw += sum24hrs_total[j];
    }

    for (j=0; j<total_ns_data; j++) {
	if (tot_bw) {
	    sum24hrs_cache[j] = (int) (100 * sum24hrs_cache[j] / tot_bw);
	    sum24hrs_noncache[j] = (int) (100 * sum24hrs_noncache[j] / tot_bw);
	}
    }
    /*
     * Sort by bandwidth.
     */
    for (j=0; j<total_ns_data; j++) {
	for (i=0; i<total_ns_data-1; i++) {
	    if (sum24hrs_total[i] < sum24hrs_total[i+1]) {
		// swap two slots
		char * ptmp;
		ptmp = labels[i+1];
		labels[i+1]=labels[i];
		labels[i]=ptmp;

		double tmp;
		tmp = sum24hrs_total[i+1];
		sum24hrs_total[i+1]=sum24hrs_total[i];
		sum24hrs_total[i]=tmp;

		tmp = sum24hrs_cache[i+1];
		sum24hrs_cache[i+1]=sum24hrs_cache[i];
		sum24hrs_cache[i]=tmp;

		tmp = sum24hrs_noncache[i+1];
		sum24hrs_noncache[i+1]=sum24hrs_noncache[i];
		sum24hrs_noncache[i]=tmp;

		tmp = sum24hrs_CHR[i+1];
		sum24hrs_CHR[i+1]=sum24hrs_CHR[i];
		sum24hrs_CHR[i]=tmp;
	    }
	}
    }
    display_total_ns_data = (total_ns_data>30) ? 30 : total_ns_data;


    XYChart *c = new XYChart(xsize, ysize);
    // Set the plotarea at (55, 40) and of size 350 x 250  pixels
    int x_margin = 55; // leave 55 pixel on the left for label
    int y_margin = 50; // leave 50 pixel on the top for title
    int xplot_area = xsize - x_margin - x_margin; //leave 55 pixel for right margin
    int yplot_area = ysize - y_margin - 35; //leave 35 pixel for x-lebel
    if (display_total_ns_data>10) {
	yplot_area -= 30; //more space for x-label
    }
    c->setPlotArea( x_margin,
	    y_margin,
	    xplot_area,
	    yplot_area,
	    c->linearGradientColor(0, x_margin, 0, ysize, 0xf9fcff, 0xaaccff),
	    -1,
	    Chart::Transparent, 0xffffff);

    //center the legend on the graph
    c->addLegend(200, 15, false, "timesbi.ttf", 10)->setBackground(Chart::Transparent);
    c->addTitle(title, "timesbi.ttf", 12);
    if (display_total_ns_data>10) {
	c->xAxis()->setLabelStyle("arial.ttf")->setFontAngle(45);
    }
    c->xAxis()->setWidth(2);
    c->xAxis()->setLabels(StringArray(labels, display_total_ns_data));

    c->yAxis()->setWidth(2);
    c->yAxis()->setTitle(y_title, "arialbi.ttf", 10 );
    c->yAxis()->setLinearScale(0.0, 100.0, 20, 0);
    c->yAxis()->setLabelFormat("{value}%");

    // set the axis, label and title colors for the primary y axis to green
    // (0x008000) to match the second data set
    //c->yAxis()->setColors(0x0080ff, 0x0080ff, 0x0080ff);

    // Add a stacked bar layer to the chart
    BarLayer *layer = c->addBarLayer(Chart::Stack);

    // Add the remaining data sets to the chart as another stacked bar group
    layer->addDataGroup("BW");
    layer->addDataSet(DoubleArray(sum24hrs_noncache, display_total_ns_data), 0xff8080, "Origin BW");
    layer->addDataSet(DoubleArray(sum24hrs_cache, display_total_ns_data), 0x80ff80, "Bandwidth Saving");
    layer->setAggregateLabelStyle();
    layer->setDataLabelStyle();



    //layer2->setLineWidth(2);
    layer->addDataGroup("CHR");
    layer->addDataSet(DoubleArray(sum24hrs_CHR, display_total_ns_data), 0x0080ff, "Cache Hit Ratio");

    // Set the sub-bar gap to 0, so there is no gap between stacked bars with a group
    layer->setBarGap(0.2, 0);

    // Set the aggregate label format
    layer->setAggregateLabelFormat("{value}%");

    // Set the aggregate label font to 8 point Arial Bold Italic
    layer->setAggregateLabelStyle("arialbi.ttf", 8);

    //layer->setDataLabelStyle();

    strcpy(filename, imgpath); // add path
    strcat(filename, imgname); // fully qualified filename
    c->makeChart(filename);
    //printf("Done creating Graph\n");
    //Free up resources
    delete c;

    return;
};


void make_namespace_chr_png() // GB Delivered, Cachehit Ratio, video delivered
{
    char filename[100];
    char * labels[HOURS];
    double sum24hrs_total[HOURS];
    double sum24hrs_cache[HOURS];
    double sum24hrs_noncache[HOURS];
    double sum24hrs_CHR[HOURS];
    double tot_out_bytes;
    double tot_out_bytes_ns;
    int i, j, maxj;
    const char* imgname = "ns_chr.png";
    int xsize = 800;
    int ysize = 250;
    const char* title = "Namespace (%s) Hourly Cache Hit Ratio for Last 24 Hours";
    const char* y_title = "Bandwidth (GBytes)";
    const char* y_title2 = "Cache Hit %";
    time_t now;
    struct tm * t;
    int tnow, tthis, this_hr, last_hr, c_day;

    now = time(NULL);
    t = localtime(&now);
    tnow = t->tm_hour;
    c_day = t->tm_yday;

    this_hr = t->tm_hour;
    last_hr = (t->tm_hour + 23) % 24;

    /*
     * Calculate the data.
     */
    tot_out_bytes = 0.0;
    maxj = 0;
    for (j=0; j<total_ns_data; j++)
    {
	tot_out_bytes_ns = 0.0;
	for (i=0; i<HOURS-1; i++) {
	    this_hr = (tnow+i)%24;
	    last_hr = (tnow+i+23)%24;

	    /* Cacheable */
	    tot_out_bytes_ns += get_delta_data(ns_data[j].total_values.out_bytes_disk[this_hr],
		    ns_data[j].total_values.out_bytes_disk[last_hr]);
	    tot_out_bytes_ns += get_delta_data(ns_data[j].total_values.out_bytes_ram[this_hr],
		    ns_data[j].total_values.out_bytes_ram[last_hr]);

	    /* Non-cacheable */
	    tot_out_bytes_ns += get_delta_data(ns_data[j].total_values.out_bytes_origin[this_hr],
		    ns_data[j].total_values.out_bytes_origin[last_hr]);
	    tot_out_bytes_ns += get_delta_data(ns_data[j].total_values.out_bytes_tunnel[this_hr],
		    ns_data[j].total_values.out_bytes_tunnel[last_hr]);
	}
	if (tot_out_bytes_ns > tot_out_bytes) {
	    maxj = j;
	    tot_out_bytes = tot_out_bytes_ns;
	}
    }

    for (i=0; i<HOURS; i++) {
	this_hr = (tnow+i+1)%24;
	labels[i] = (char *)malloc(3);
	sprintf(labels[i], "%02d", this_hr);
	sum24hrs_CHR[i] = 0.0;
	sum24hrs_cache[i] = 0.0;
	sum24hrs_noncache[i] = 0.0;
    }

    //We have to ignore the first hour data when dashboard starts independently from nvsd.
    //It takes the counter value that has run for multiple days and shows as current hour data
    static bool init_ns_ch_hit_flag = true;
    static bool in_first_ns_24hr_flag = true;
    static int very_first_hr_ns;
    static int ns_st_day;

    if(init_ns_ch_hit_flag){
	very_first_hr_ns = tnow;
	ns_st_day = c_day;
	init_ns_ch_hit_flag = false;
    }
    if(in_first_ns_24hr_flag){
	if(c_day != ns_st_day){
	    if(tnow == very_first_hr_ns){//next day, same hour
		in_first_ns_24hr_flag = false;
	    }
	}
    }


    j = maxj;
    for (i=1; i<HOURS; i++) {
	this_hr = (tnow+i+1)%24;
	last_hr = (tnow+i)%24;

	/* Cacheable */
	sum24hrs_cache[i] = get_delta_data(ns_data[j].total_values.out_bytes_disk[this_hr],
		ns_data[j].total_values.out_bytes_disk[last_hr]);
	sum24hrs_cache[i] += get_delta_data(ns_data[j].total_values.out_bytes_ram[this_hr],
		ns_data[j].total_values.out_bytes_ram[last_hr]);

	/* Non-cacheable */
	sum24hrs_noncache[i] = get_delta_data(ns_data[j].total_values.out_bytes_origin[this_hr],
		ns_data[j].total_values.out_bytes_origin[last_hr]);
	sum24hrs_noncache[i] += get_delta_data(ns_data[j].total_values.out_bytes_tunnel[this_hr],
		ns_data[j].total_values.out_bytes_tunnel[last_hr]);

	if(in_first_ns_24hr_flag){
	    if(this_hr == very_first_hr_ns){
		sum24hrs_cache[i] = 0.0;
		sum24hrs_noncache[i] = 0.0;
	    }
	}

	sum24hrs_total[i] = sum24hrs_cache[i] + sum24hrs_noncache[i];
	if (sum24hrs_total[i] == 0) {
	    sum24hrs_CHR[i] = 0.0;
	}
	else {
	    sum24hrs_CHR[i] = 100.0 * sum24hrs_cache[i] / sum24hrs_total[i];
	}
	sum24hrs_cache[i] /= 1000.0; // Convert to GBytes
	sum24hrs_cache[i] = ((int) (100 * sum24hrs_cache[i])) /100.0;
	sum24hrs_noncache[i] /= 1000.0; // Convert to GBytes
	sum24hrs_noncache[i] = ((int) (100 * sum24hrs_noncache[i])) /100.0;
    }

    XYChart *c = new XYChart(xsize, ysize);
    // Set the plotarea at (55, 40) and of size 350 x 250  pixels
    int x_margin = 55; // leave 55 pixel on the left for label
    int y_margin = 50; // leave 50 pixel on the top for title
    int xplot_area = xsize - x_margin - x_margin; //leave 55 pixel for right margin
    int yplot_area = ysize - y_margin - 35; //leave 35 pixel for x-lebel
    c->setPlotArea( x_margin,
	    y_margin,
	    xplot_area,
	    yplot_area,
	    c->linearGradientColor(0, x_margin, 0, ysize, 0xf9fcff, 0xaaccff),
	    -1,
	    Chart::Transparent, 0xffffff);

    //center the legend on the graph
    c->addLegend(200, 15, false, "timesbi.ttf", 10)->setBackground(Chart::Transparent);
    sprintf(filename, title, ns_data[maxj].name);
    c->addTitle(filename, "timesbi.ttf", 12);
    c->yAxis()->setTitle(y_title, "arialbi.ttf", 10 );
    //c->yAxis()->setLinearScale(0.0, 100.0, 10, 0);
    c->xAxis()->setWidth(2);
    c->yAxis()->setWidth(2);
    c->xAxis()->setLabels(StringArray(labels, HOURS));
    // Add a title to the secondary (right) y axis
    c->yAxis2()->setTitle("Cache Hit Ratio");
    c->yAxis2()->setLinearScale(0.0, 100.0, 20, 0);
    c->yAxis2()->setLabelFormat("{value}%");

    // set the axis, label and title colors for the primary y axis to green
    // (0x008000) to match the second data set
    c->yAxis2()->setColors(0x0080ff, 0x0080ff, 0x0080ff);
    LineLayer *layer2 = c->addLineLayer();
    //layer2->setLineWidth(2);
    layer2->setUseYAxis2();
    layer2->addDataSet(DoubleArray(sum24hrs_CHR, HOURS), 0x0080ff, "Cache Hit Ratio")->setDataSymbol(Chart::CircleSymbol, 9, 0x0072cf);;


    // Add a stacked bar layer and set the layer 3D depth to 8 pixels
    BarLayer *layer = c->addBarLayer(Chart::Stack, 0);
    // Add the two data sets to the bar layer
    layer->addDataSet(DoubleArray(sum24hrs_noncache, HOURS), 0xff8080, "Origin BW");
    layer->addDataSet(DoubleArray(sum24hrs_cache, HOURS), 0x80ff80, "Bandwidth Saving");
    layer->setAggregateLabelStyle();
    layer->setDataLabelStyle();

    strcpy(filename, imgpath); // add path
    strcat(filename, imgname); // fully qualified filename
    c->makeChart(filename);
    //printf("Done creating Graph\n");
    //Free up resources
    delete c;
    for (i=0; i<HOURS; i++) {
	free((void *)labels[i]);
    }

    return;
};


/*
 * Make info.html file to display:
 * 	Total GB delivered
 * 	Cachehit Ratio
 * 	Total  video delivered
 */

static double get_delta(double thishr, double lasthr)
{
    if (thishr >= lasthr) return (thishr - lasthr);
    return thishr;
}

void make_graph_info_2() // GB Delivered, Cachehit Ratio, video delivered
{
    int i, peaki;

    double total, save, var1, var2, var3, var4;	//Bytes
    double bs_rate_cumu = 0.0;	//in bytes
    double bs_rate_24hr = 0.0;	//in bytes
    char filename[100];
    double bytes_rate_24hr = 0.0;
    double bytes_rate_cumu = 0.0;
    double trans_rate_24hr = 0.0;
    double trans_rate_cumu = 0.0;
    double bs_peak_24hr = 0.0;

    time_t tv;
    struct tm * tm;
    struct tm * stime;
    char timestr[50];
    FILE * fp;
    const char * title_id = "\"dashb_info_ptitle1\"";
    const char * content_id = "\"dashb_info_pcontent1\"";
    const char * content_id2 = "\"dashb_info_pcontent2\"";

    const char * fmt = "<html><head><meta http-equiv=\"refresh\" content=\"10\">"
	"<link title=\"default\" rel=\"stylesheet\" type=\"text/css\""
	"href=\"/tms-default.css\"></head><body>"
	"<p id=%s>Cumulative since</br>%s</p>"
	"<p id=%s>GB Delivered<br />%.2f</p>"

	"<p id=%s>Bandwidth Savings<br/>"
	"<table id=%s width=100%>"
	"<tr><td>Cumulative</td><td align=right> %.0f%%</td></tr>"
	"<tr><td>Last 24 Hour</td><td align=right>%.0f%%</td></tr>"
	"<tr><td>Peak Hour (%d%s)</td><td align=right>%.0f%%</td></tr>"
	"</table></p>"

	"<p id=%s>Cache Hit Ratio<br/>"
	"<table id=%s width=100%>"
	"<tr><td>Bytes (Cumulative)</td><td align=right> %.0f%%</td></tr>"
	"<tr><td>Bytes (Last 24 Hr)</td><td align=right> %.0f%%</td></tr>"
	"<tr><td>Objects (Cumulative)</td><td align=right>%.0f%%</td></tr>"
	"<tr><td>Objects (Last 24 Hr)</td><td align=right>%.0f%%</td></tr>"
	"</table></p>"

	"<p id=%s>Objects Delivered<br />%ld</p>"
	"</body></html>";

    time_t now;
    struct tm * t;
    int this_hr, last_hr, ahr, last_ahr;

    now = time(NULL);
    t = localtime(&now);
    this_hr = t->tm_hour;
    last_hr = (t->tm_hour + 23) % 24;

    // Calculate bs_rate_cumu
    bs_rate_cumu = 0.0;
    if(hr_client_send_tot_bytes[this_hr] > 0){
	bs_rate_cumu = 100.00 * 
	    (hr_client_send_tot_bytes[this_hr] - 
	     hr_origin_recv_tot_bytes[this_hr]) /
	    hr_client_send_tot_bytes[this_hr];
	if (bs_rate_cumu > 100) bs_rate_cumu = 100.0; 	
    }
    else if(hr_client_send_tot_bytes[last_hr] > 0){
	/* chance at the time clock just changed */
	bs_rate_cumu = 100.00 *
	    (hr_client_send_tot_bytes[last_hr] -
	     hr_origin_recv_tot_bytes[last_hr]) /
	    hr_client_send_tot_bytes[last_hr];
	if (bs_rate_cumu > 100) bs_rate_cumu = 100.0;
    }

    // Calculate bs_rate_24hr
    // Calculate bs_peak_24hr
    total = save = 0.0;
    bs_peak_24hr = 0.0;
    bs_rate_24hr = 0.0;
    var4 = 0.0;
    for (i=0, peaki=0; i<HOURS-1; i++) {
	last_ahr = (this_hr + 1 + i) %24;
	ahr = (this_hr + 2 + i) % 24;
	var1 = get_delta(hr_client_send_tot_bytes[ahr],
		hr_client_send_tot_bytes[last_ahr]);
	var2 = get_delta(hr_origin_recv_tot_bytes[ahr],
		hr_origin_recv_tot_bytes[last_ahr]);
	total += var1;
	save += var1 - var2;
	if (var1>var4) {
	    var4 = var1;
	    var3 = 100 * (var1 - var2) / var1;
	    if (var3 > 100.0) var3 = 100.0;
	    bs_peak_24hr = var3;
	    peaki = i;
	}
    }
    if (total > 0) {
	bs_rate_24hr = 100.0 * save / total;
	if (bs_rate_24hr > 100) bs_rate_24hr = 100.0;
    }

    // Calculate trans_rate_cumu
    trans_rate_cumu = 0.0;
    if(tot_video_delivered > 0){
	trans_rate_cumu = 100.00 * tot_video_delivered_with_hit / tot_video_delivered;
	if (trans_rate_cumu > 100) trans_rate_cumu = 100.0; 	
    }

    // Calculate trans_rate_24hr
    total = save = 0.0;
    trans_rate_24hr = 0.0;
    for (i=0; i<HOURS-1; i++) {
	last_ahr = (this_hr + 1 + i) %24;
	ahr = (this_hr + 2 + i) % 24;
	var1 = get_delta(hr_tot_video_delivered[ahr],
		hr_tot_video_delivered[last_ahr]);
	var2 = get_delta(hr_tot_video_delivered_with_hit[ahr],
		hr_tot_video_delivered_with_hit[last_ahr]);
	total += var1;
	save += var2;
    }
    if (total > 0) {
	trans_rate_24hr = 100.0 * save / total;
	if (trans_rate_24hr > 100) trans_rate_24hr = 100.0;
    }

    // Calculate bytes_rate_cumu
    bytes_rate_cumu = 0.0;
    if (hr_tot_client_send[this_hr] > 0) {
	bytes_rate_cumu = 100.0 * 
	    hr_tot_client_send_from_bm_or_dm[this_hr] / 
	    hr_tot_client_send[this_hr];
	if (bytes_rate_cumu > 100) bytes_rate_cumu = 100.0; 	
    }
    else if (hr_tot_client_send[last_hr] > 0) {
	/* chance at the time clock just changed */
	bytes_rate_cumu = 100.0 * 
	    hr_tot_client_send_from_bm_or_dm[last_hr] / 
	    hr_tot_client_send[last_hr];
	if (bytes_rate_cumu > 100) bytes_rate_cumu = 100.0; 	
    }

    // Calculate bytes_rate_24hr
    total = save = 0.0;
    bytes_rate_24hr = 0.0;
    for (i=0; i<HOURS-1; i++) {
	last_ahr = (this_hr + 1 + i) %24;
	ahr = (this_hr + 2 + i) % 24;
	var1 = get_delta(hr_tot_client_send[ahr],
		hr_tot_client_send[last_ahr]);
	var2 = get_delta(hr_tot_client_send_from_bm_or_dm[ahr],
		hr_tot_client_send_from_bm_or_dm[last_ahr]);
	total += var1;
	save += var2;
    }
    if (total > 0) {
	bytes_rate_24hr = 100.0 * save / total;
	if (bytes_rate_24hr > 100) bytes_rate_24hr = 100.0;
    }


    sprintf(filename, "%sinfo.html", imgpath);
    fp = fopen(filename, "w");
    if(!fp){
	printf("unable to open file :%s\n", filename);
	return;
    }
    //find time in the desired format
    stime = localtime((time_t *)&nvsd_uptime_since);
    strftime(timestr, 50, "%a %b %d %Y, %H:%M:%S", stime);


    fprintf(fp, fmt, title_id, timestr,
	    content_id, tot_gb_delivered,

	    content_id, content_id2,
	    bs_rate_cumu, bs_rate_24hr,
	    peaki % 12, (peaki<12)?"AM":"PM", bs_peak_24hr,

	    content_id, content_id2,
	    bytes_rate_cumu, bytes_rate_24hr,
	    trans_rate_cumu, trans_rate_24hr,

	    content_id, tot_video_delivered);

    fclose(fp);
    return;
};



/*
 * This graph conbines Data Source and Network Bandwidth data
 * together to a single graph
 */
void display_media_delivery_bandwidth()
{
    double data_cache[X_POINTS_10]; //This graph will graph the last 10 data points
    double data_disk[X_POINTS_10];
    double data_origin[X_POINTS_10];
    double network_data[X_POINTS_10];
    char *time_label[X_POINTS_10];
    int i, j;
    char filename[100];

    for (i = 0; i < X_POINTS_10; i++)
	time_label[i]= (char *)malloc(6);

    for ( i = MAX_DATA_NUMBER-1, j = X_POINTS_10 -1; i >= MAX_DATA_NUMBER-X_POINTS_10; i--, j--){
	data_cache[j] = (cache_rate[i]+ rtsp_cache_rate[i]) * 8; // convert data from MBytes to Mbits
	data_disk[j] = disk_rate[i] * 8;
	data_origin[j] = (origin_rate[i] + nfs_rate[i] + rtsp_origin_rate[i]) * 8;
	//network_data[j] = network_bandwidth_rate[i]; // This data is already in Mbits
	network_data[j] = data_cache[j] + data_disk[j] + data_origin[j];
	memcpy((void *)time_label[j], (void *)time_str[i],5);
	time_label[j][5] = 0;
    }

    XYChart *c = new XYChart(350, 550);
    //c->addTitle("  Media Delivery Bandwidth", "timesbi.ttf", 12);
    c->addTitle("   Cache Throughput", "timesbi.ttf", 12);
    c->setPlotArea(60, 60, 280, 455,
	    c->linearGradientColor(0, 35, 0, 455, 0xf9fcff, 0xaaccff),
	    -1, 0xffffff, 0xffffff);
    c->addLegend(85, 12, false, "timesbi.ttf", 10)->setBackground(Chart::Transparent);
    c->xAxis()->setLabels(StringArray(time_label, 10));
    //c->xAxis()->setLabelStep(5);
    // Add a title to the y axis
    c->yAxis()->setTitle("Mbits / sec", "arialbi.ttf", 10 );

    c->xAxis()->setWidth(2);
    c->yAxis()->setWidth(2);
    // Add a multi-bar layer with 3 data sets
    BarLayer *layer = c->addBarLayer(Chart::Side);
    layer->addDataSet(DoubleArray(data_cache, X_POINTS_10), 0x2cb135, "RAM"); //Green
    layer->addDataSet(DoubleArray(data_disk, X_POINTS_10), 0xffff00, "Disk"); //Blue 0x0072cf, change to yellow 0xffff00
    layer->addDataSet(DoubleArray(data_origin, X_POINTS_10), 0xdc291e, "Origin");//Red


    // Configure the bars within a group to touch each others (no gap)
    layer->setBarGap(0.2, Chart::TouchBar);

    LineLayer *layer2 = c->addLineLayer();
    // Set the default line width to 2 pixels
    layer2->setLineWidth(2);
    layer2->addDataSet(DoubleArray(network_data, X_POINTS_10), 0x0072cf, "Bandwidth")->setDataSymbol(Chart::CircleSymbol, 9, 0x0072cf);

    layer2->setBorderColor(Chart::Transparent, Chart::glassEffect(Chart::NormalGlare, Chart::Left));

    strcpy(filename, imgpath);
    strcat (filename, "media_delivery_bandwidth.png");
    c->makeChart(filename);

    delete c;
    for (i = 1; i < X_POINTS_10; i++){
	free((void *)time_label[i]);
    }

    return;
};

void display_media_delivery_bandwidth_small()
{
    double data_cache[X_POINTS_10]; //This graph will graph the last 10 data points
    double data_disk[X_POINTS_10];
    double data_origin[X_POINTS_10];
    double client_delivery_data[X_POINTS_10]; //This is client delivery data
    double total_delivery_data[X_POINTS_10];
    char *time_label[X_POINTS_10];
    int i, j;
    char filename[100];
    const char* imgname = "media_delivery_bandwidth.png";

    for (i = 0; i < X_POINTS_10; i++)
	time_label[i]= (char *)malloc(6);

    for ( i = MAX_DATA_NUMBER-1, j = X_POINTS_10 -1; i >= MAX_DATA_NUMBER-X_POINTS_10; i--, j--){
	data_cache[j] = (cache_rate[i] + rtsp_cache_rate[i]) * 8; // convert data from MBytes to Mbits
	data_disk[j] = disk_rate[i] * 8;
	data_origin[j] = (origin_rate[i] + nfs_rate[i] + tunnel_rate[i] + rtsp_origin_rate[i]) * 8;
	//network_data[j] = network_bandwidth_rate[i]; // This data is already in Mbits
	client_delivery_data[j] = data_cache[j] + data_disk[j] + data_origin[j];
	total_delivery_data[j] = data_cache[j] + data_disk[j] + (data_origin[j] * 2); //Data from origin to MFC is added
	memcpy((char *)time_label[j], (char *)time_str[i],5);
	time_label[j][5] = 0;
    }


    strcpy(filename, imgpath); // add path
    strcat(filename, imgname); // fully qualified filename

    //Graph related data
    int xsize = 350; 	// x size for the graph
    int ysize = 250;	// y size for the graph
    int x_margin = 55; 	// leave 55 pixel on the left for label
    int y_margin = 55; 	// leave 40 pixel on the top for title
    int title_font = 12; 	// font for the title
    //char * title = "Media Delivery Bandwidth";
    const char * title = "Cache Throughput";
    int num_dataset = 3; 	// data_cache, data_disk, data_origin

    int xplot_area = xsize - x_margin - 10; //leave 10 pixel for right margin
    int yplot_area = ysize - y_margin - 35; //leave 35 pixel for x-lebel

    // Create a XYChart object of size 540 x 375 pixels
    XYChart *c = new XYChart(xsize, ysize);

    // Add a title to the chart using 18 pts Times Bold Italic font
    c->addTitle(title, "timesbi.ttf", title_font);

    //Set the plat area for this graph
    c->setPlotArea( x_margin,
	    y_margin,
	    xplot_area,
	    yplot_area,
	    c->linearGradientColor(0, x_margin, 0, ysize, 0xf9fcff, 0xaaccff),
	    -1,
	    Chart::Transparent, 0xffffff);
    //Add a legend to the chart
    //center the legend on the screen
    c->addLegend((xsize-x_margin)/num_dataset - 40, 12, false, "timesbi.ttf", 9)->setBackground(Chart::Transparent);

    // Set the labels on the x axis.
    c->xAxis()->setLabels(StringArray(time_label, X_POINTS_10));

    // Add a title to the y axis
    c->yAxis()->setTitle("Mbits / sec", "arialbi.ttf", 10 );

    c->xAxis()->setWidth(2);
    c->yAxis()->setWidth(2);
    // Add a multi-bar layer with 3 data sets
    BarLayer *layer = c->addBarLayer(Chart::Side);
    layer->addDataSet(DoubleArray(data_cache, X_POINTS_10), 0x2cb135, "RAM"); //Green
    layer->addDataSet(DoubleArray(data_disk, X_POINTS_10), 0xffff00, "Disk"); //Blue 0x0072cf, change to yellow 0xffff00
    layer->addDataSet(DoubleArray(data_origin, X_POINTS_10), 0xdc291e, "Origin");//Red


    // Configure the bars within a group to touch each others (no gap)
    layer->setBarGap(0.2, Chart::TouchBar);

    LineLayer *layer2 = c->addLineLayer();
    //SplineLayer *layer2 = c->addSplineLayer();

    // Set the default line width to 2 pixels
    layer2->setLineWidth(2);
    layer2->addDataSet(DoubleArray(client_delivery_data, X_POINTS_10), 0x0072cf, "Client Delivery BW")->setDataSymbol(Chart::CircleSymbol, 9, 0x0072cf);
    layer2->addDataSet(DoubleArray(total_delivery_data, X_POINTS_10), 0x2cb135, "Total Delivery BW")->setDataSymbol(Chart::CircleSymbol, 9, 0x2bc135);

    layer2->setBorderColor(Chart::Transparent, Chart::glassEffect(Chart::NormalGlare, Chart::Left));

    c->makeChart(filename);

    //Free up resources
    delete c;
    for (i = 1; i < X_POINTS_10; i++){
	free((void *)time_label[i]);
    }
    return;
};

void make_cache_hit_ratio_daygraph()
{
    int i, j;
    const char* f1 = "Hours           :";
    const char* f2 = "Cache BW(GBytes):";
    const char* f3 = "Total BW(GBytes):";
    const char* f4 = "Cache Hit Ratio :";

    //Create the graph
    double sum24hrs_CHR[HOURS]; //This graph will show the data for full day, last 24 hours
    char *labels[HOURS];
    double sum24hrs_cache[HOURS];
    double sum24hrs_noncache[HOURS];
    double sum24hrs_total[HOURS];

    const char* title = "Global Hourly Cache Hit Ratio for Last 24 Hours";
    const char* x_title = "Hours->";
    const char* y_title = "Bandwidth (GBytes)";
    const char* y_title2 = "Cache Hit %";
    int xsize = 800;
    int ysize = 250;
    struct tm * t;
    int this_hr, last_hr, tnow, c_day;
    time_t now;

    char filename[100];
    const char* imgname = "ch_ratio_bw.png";

    now = time(NULL);
    t = localtime(&now);
    tnow = t->tm_hour;
    c_day = t->tm_yday;

    for (i=0; i<HOURS; i++) {
	this_hr = (tnow+i+1)%24;
	labels[i] = (char *)malloc(3);
	sprintf(labels[i], "%02d", this_hr);
	sum24hrs_CHR[i] = 0.0;
	sum24hrs_cache[i] = 0.0;
	sum24hrs_noncache[i] = 0.0;
	sum24hrs_total[i] = 0.0;
    }

    //We have to ignore the first hour data when dashboard starts independently from nvsd.
    //It takes the counter value that has run for multiple days and shows as current hour data
    static bool init_gl_ch_hit_flag = true;
    static bool in_first_24hr_flag = true;
    static int very_first_hr;
    static int st_day;

    if(init_gl_ch_hit_flag){
	very_first_hr = tnow;
	st_day = c_day;
	init_gl_ch_hit_flag = false;
    }
    if(in_first_24hr_flag){
	if(c_day != st_day){
	    if(tnow == very_first_hr){//next day, same hour
		in_first_24hr_flag = false;
	    }
	}
    }

    for (i=1; i<HOURS; i++) {
	this_hr = (tnow+i+1)%24;
	last_hr = (tnow+i)%24;

	sum24hrs_cache[i] = 0.0;
	sum24hrs_noncache[i] = 0.0;
	for (j=0; j<total_ns_data; j++) {
	    /* Cacheable */
	    sum24hrs_cache[i] += get_delta_data(ns_data[j].total_values.out_bytes_disk[this_hr],
		    ns_data[j].total_values.out_bytes_disk[last_hr]);
	    sum24hrs_cache[i] += get_delta_data(ns_data[j].total_values.out_bytes_ram[this_hr],
		    ns_data[j].total_values.out_bytes_ram[last_hr]);

	    /* Non-cacheable */
	    sum24hrs_noncache[i] += get_delta_data(ns_data[j].total_values.out_bytes_origin[this_hr],
		    ns_data[j].total_values.out_bytes_origin[last_hr]);
	    sum24hrs_noncache[i] += get_delta_data(ns_data[j].total_values.out_bytes_tunnel[this_hr],
		    ns_data[j].total_values.out_bytes_tunnel[last_hr]);
	}
	if(in_first_24hr_flag){
	    if(this_hr == very_first_hr){
		sum24hrs_cache[i] = 0.0;
		sum24hrs_noncache[i] = 0.0;
	    }
	}
	sum24hrs_total[i] = sum24hrs_cache[i] + sum24hrs_noncache[i];
	if (sum24hrs_total[i] == 0) {
	    sum24hrs_CHR[i] = 0.0;
	}
	else {
	    sum24hrs_CHR[i] = 100.0 * sum24hrs_cache[i] / sum24hrs_total[i];
	}
	sum24hrs_cache[i] /= 1000.0; // Convert to GBytes
	sum24hrs_cache[i] = ((int) (100 * sum24hrs_cache[i])) /100.0;
	sum24hrs_noncache[i] /= 1000.0; // Convert to GBytes
	sum24hrs_noncache[i] = ((int) (100 * sum24hrs_noncache[i])) /100.0;
    }

    XYChart *c = new XYChart(xsize, ysize);
    // Set the plotarea at (55, 40) and of size 350 x 250  pixels
    int x_margin = 55; // leave 55 pixel on the left for label
    int y_margin = 50; // leave 50 pixel on the top for title
    int xplot_area = xsize - x_margin - x_margin; //leave 55 pixel for right margin
    int yplot_area = ysize - y_margin - 35; //leave 35 pixel for x-lebel
    c->setPlotArea( x_margin,
	    y_margin,
	    xplot_area,
	    yplot_area,
	    c->linearGradientColor(0, x_margin, 0, ysize, 0xf9fcff, 0xaaccff),
	    -1,
	    Chart::Transparent, 0xffffff);

    //center the legend on the graph
    c->addLegend(160, 15, false, "timesbi.ttf", 10)->setBackground(Chart::Transparent);
    c->addTitle(title, "timesbi.ttf", 12);
    c->yAxis()->setTitle(y_title, "arialbi.ttf", 10 );
    //c->yAxis()->setLinearScale(0.0, 100.0, 10, 0);
    c->xAxis()->setWidth(2);
    c->yAxis()->setWidth(2);
    c->xAxis()->setLabels(StringArray(labels, HOURS));
    // Add a title to the secondary (right) y axis
    c->yAxis2()->setTitle("Cache Hit Ratio");
    c->yAxis2()->setLinearScale(0.0, 100.0, 20, 0);
    c->yAxis2()->setLabelFormat("{value}%");

    // set the axis, label and title colors for the primary y axis to green
    // (0x008000) to match the second data set
    c->yAxis2()->setColors(0x0080ff, 0x0080ff, 0x0080ff);
    LineLayer *layer2 = c->addLineLayer();
    layer2->setLineWidth(2);
    layer2->setUseYAxis2();
    layer2->addDataSet(DoubleArray(sum24hrs_CHR, HOURS), 0x0080ff, "Cache Hit Ratio")->setDataSymbol(Chart::CircleSymbol, 9, 0x0072cf);;

    // Add a stacked bar layer and set the layer 3D depth to 8 pixels
    BarLayer *layer = c->addBarLayer(Chart::Stack, 0);
    // Add the two data sets to the bar layer
    layer->addDataSet(DoubleArray(sum24hrs_noncache, HOURS), 0xff8080, "Origin BW");
    layer->addDataSet(DoubleArray(sum24hrs_cache, HOURS), 0x80ff80, "Bandwidth Savings");
    layer->setAggregateLabelStyle();
    layer->setDataLabelStyle();

    strcpy(filename, imgpath); // add path
    strcat(filename, imgname); // fully qualified filename
    c->makeChart(filename);
    //printf("Done creating Graph\n");
    //Free up resources
    delete c;
    for(i = 0; i < HOURS; i++){
	free((void *)labels[i]);
    }

    return;
};



////////////////////////////////////////////////////////////////////////////////////////////////
///////////////           Generic charts                   /////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////


void make_round_meter_chart( 	
		char* filename,		// Fully qualified file name
		char* title, 		// Label to be shown on the meter Ex, CPU
		int 	xsize, 		// x size in pixel
		int 	ysize,		// y size in pixel
		double  data_val, 	// Data value to be shown on the round meter
		int	green_zone,	// green zone Default 60 (range 0-60)
		int	yellow_zone,	// yellow zone 80 (range 60 - 80)
		int	red_zone)	// red zone 100, (ranze  80 - 100)
{
    // The value to display on the meter
    double value = data_val;

    // Create an AugularMeter object of size xsize x ysize pixels, with silver
    // background, black border, 2 pixel 3D depressed border and rounded corners.
    AngularMeter *m = new AngularMeter(xsize, ysize, Chart::silverColor(), 0x000000, -2);
    m->setRoundedFrame();	

    // Set the meter center at (xsize/2, ysize/2), with radius xsize/2-15 pixels, and span from -135
    // to +135 degress
    int x_center = xsize/2;
    int y_center = ysize/2;
    int radius = x_center - 15;
    m->setMeter(x_center, y_center, radius, -135, 135);

    // Meter scale is 0 - 100, with major tick every 10 units, minor tick every 5
    // units, and micro tick every 1 units
    m->setScale(0, 100, 10, 5, 1);

    // Disable default angular arc by setting its width to 0. Set 2 pixels line width
    // for major tick, and 1 pixel line width for minor ticks.
    m->setLineWidth(0, 2, 1);

    // Set the circular meter surface as metallic blue (9999DD)
    int dial_width = x_center - 10;
    m->addRing(0, dial_width, Chart::metalColor(0x9999dd));

    // Add a blue (6666FF) ring between radii 88 - 90 as decoration
    int start_decorative_ring = dial_width - 2;
    int end_decorative_ring = dial_width;
    m->addRing(start_decorative_ring, end_decorative_ring, 0x6666ff);

    // Set 0 - 60 as green (99FF99) zone, 60 - 80 as yellow (FFFF00) zone, and 80 -
    // 100 as red (FF3333) zone
    m->addZone(0, green_zone, 0x99ff99);
    m->addZone(green_zone, yellow_zone, 0xffff00);
    m->addZone(yellow_zone, red_zone, 0xff3333);

    // Add a text label centered at (100, 135) with 15 pts Arial Bold font
    int text_label_x = x_center;
    int text_label_y = y_center + 35;
    m->addText(text_label_x, text_label_y, title, "arialbd.ttf", 15, Chart::TextColor, Chart::Center);

    // Add a text box centered at (100, 165) showing the value formatted to 2 decimal
    // places, using white text on a black background, and with 1 pixel 3D depressed
    // border
    m->addText(text_label_x, text_label_y + 35, m->formatValue(value, "2"), "Arial", 8, 0xffffff,
	    Chart::Center)->setBackground(0x000000, 0x000000, -1);

    // Add a semi-transparent blue (40333399) pointer at the specified value
    m->addPointer(value, 0x40333399);

    // Output the chart
    m->makeChart(filename);

    //free up resources
    delete m;

    return;
}

void make_semi_circle_chart(
        char*   filename,       // Fully qualified file name
        char*   title,          // Label to be shown on the meter Ex, CPU
        int     xsize,          // x size in pixel
        int     ysize,          // y size in pixel
        double  data_val,       // Data value to be shown on the meter
        int     green_zone,     // green zone Default 60 (range 0-60)
        int     yellow_zone,    // yellow zone 80 (range 60 - 80)
        int     red_zone) 	// red zone 100, (ranze  80 - 100)
{
    // The value to display on the meter
    double value = data_val;
    //value = 45.23;

    // Create an AngularMeter object of size xsize x ysize pixels, with silver background
    // color, black border, 2 pixel 3D border border and rounded corners
    AngularMeter *m = new AngularMeter(xsize, ysize, 0xffffff, 0xffffff, 0);
    m->setRoundedFrame();

    // Set the meter center at (x-center, y-center), with radius xsize/2-15 pixels, and span from -90
    // to +90 degress
    int x_center = xsize/2;
    int y_center = x_center;
    int radius = x_center - 7;
    m->setMeter(x_center, y_center, radius, -90, 90);

    // Meter scale is 0 - 100, with major tick every 20 units, minor tick every 10
    // units, and micro tick every 5 units
    //m->setScale(0, 100, 20, 10, 5);
    m->setScale(0, 100, 10, 2, 0);

    // Disable default angular arc by setting its width to 0. Set 2 pixels line width
    // for major tick, and 1 pixel line width for minor ticks.
    //m->setLineWidth(2, 2, 1);
    m->setLineWidth(0, 2, 1);

    // Add a blue (6666FF) ring between radii 88 - 90 as decoration
    m->addZone(0, 100, radius+2, radius+4, 0x6666ff);

    // Set the circular meter surface as metallic blue (9999DD)
    m->addZone(0, 100, 0, radius-10,  Chart::metalColor(0x9999dd));

    // Set 0 - 60 as green (99FF99) zone, 60 - 80 as yellow (FFFF00) zone, and 80 -
    // 100 as red (FF3333) zone
    m->addZone(0, green_zone, radius - 10, radius,  0x99ff99);
    m->addZone(green_zone, yellow_zone, radius - 10, radius, 0xffff00);
    m->addZone(yellow_zone, red_zone, radius - 10, radius, 0xff3333);


    // Add a text label at (100, 60) with 15 pts Arial Bold font
    m->addText(x_center, y_center-15, title, "arialbd.ttf", 15, Chart::TextColor, Chart::Center);

    // Add a text box at top right corner (160,  10) showing the value formatted to 2 decimal
    // places, using white text on a black background, and with 1 pixel 3D depressed
    // border
    m->addText(x_center, y_center-40, m->formatValue(value, "2"), "Arial", 10, 0xffffff,
	    Chart::Center)->setBackground(0x000000, 0x000000, -1);

    // Add a semi-transparent blue (40333399) pointer at the specified value
    m->addPointer(value, 0x40333399);

    // Output the chart
    m->makeChart(filename);

    //free up resources
    delete m;
    return;
}



void make_3d_pie_chart(
        char* filename,         // Fully qualified file name
        char* title,            // Label to be shown on the top of the chart
        int title_font,         // font for the title
        int xsize,              // x size in pixel
        int ysize,              // y size in pixel
        double* data,           // data array
	int data_points,        // total data points
        char** label,           // labels
        int* colors,            // colors for different segment of the pie
        int label_format)	// 1 = circular, lebel info - on the pie
				// 2 = circular, outside of the pie,
				// 3 = side label, 4 = only legend
				// 5 = side label with legend
{
    // Create a PieChart object of size 360 x 300 pixels
    PieChart *c = new PieChart(xsize, ysize);
    c->setStartAngle(90);
    c->setColors(Chart::DataColor, IntArray(colors, data_points));
    // Add a title to the pie chart
    c->addTitle(title, "timesbi.ttf", title_font);

    int x_center, y_center, radius;
    x_center = xsize/2;
    y_center = ysize/2 + 10; // move y_center 10  pixel down to accomodate the Title

    switch(label_format){
	case 1: // circular label with label on the pie
	    radius =  ysize/2 -10; // leave 10 pixel free space
	    c->setPieSize(x_center, y_center, radius);
	    // Set the label position to -40 pixels from the perimeter of the pie
	    c->setLabelPos(-40);
	    break;
	case 2: // circular label with label out side the pie
	    radius = ysize/2 -40; // leave 40 pixel for label
	    c->setPieSize(x_center, y_center, radius);
	    // Set the sector label position to be 20 pixels from the pie. Use a join
	    // line to connect the labels to the sectors.
	    c->setLabelPos(20, Chart::LineColor);
	    break;
	case 3: // side label
	    radius =  ysize/2  -40; // leave 40 pixel for label
	    c->setPieSize(x_center, y_center, radius);
	    // Use the side label layout method
	    c->setLabelLayout(Chart::SideLayout);
	    break; 
	case 4: // only legend on the top
	    y_center = ysize/2 + 20; // move y_center 20 pixel down to accomodate the Title and lagend
	    radius =  ysize/2 - 10; // leave 10 pixel free space
	    c->setPieSize(x_center, y_center, radius);
	    //no label
	    c->setLabelStyle("", 8, Chart::Transparent);
	    //Add legend on the top just below the title
	    c->addLegend(5, 17, 0, "arialbi.ttf", 9)->setBackground(Chart::Transparent);
	    break; 
	case 5: // label and legend
	    x_center = xsize - 120 /2; // leave 120 pixel for legend on the right corner
	    radius =  ysize/2 - 10; // leave 10 pixel free space
	    c->setPieSize(x_center, y_center, radius);
	    // add a legend box where the top left corner is at (330, 50)
	    c->addLegend(xsize - 120, 60);
	    // modify the sector label format to show percentages only
	    c->setLabelFormat("{percent}%");
	    break;
	default: // label and legend 2
	    radius = ysize/2 - 40; // leave 40 pixel for label
	    c->setPieSize(x_center, y_center, radius);
	    break;
    }
    // Draw the pie in 3D
    c->set3D();
    // Set the pie data and the pie labels
    c->setData(DoubleArray(data, data_points), StringArray(label, data_points));

    // Output the chart
    c->makeChart(filename);

    //free up resources
    delete c;
    return;
}




void make_symbol_line_chart(
        char* filename,         // Fully qualified file name
        char* title,            // Label to be shown on the top of the chart
        int title_font,         // font for the title
        int xsize,              // x size in pixel
        int ysize,              // y size in pixel
        double** data,          // data array
        int num_dataset,        // how many data set, multiline or simple line
        char** legend,          //legend for each data set
        int data_points,        // data points for each data set, should be same
        char** label,           //label array to be shown for the x-axis
        char* x_title,          // Extra label on x-axis
        char* y_title,          // extra label on y-axis
        int* data_color)       //line color for each dataset
{
    // Create a XYChart object of size xsize x ysize pixels
    XYChart *c = new XYChart(xsize, ysize);
    c->setRoundedFrame();

    // Set the plotarea at (55, 40) and of size 285 x 175 pixels
    int x_margin = 55; // leave 55 pixel on the left for label
    int y_margin = 40; // leave 40 pixel on the top for title
    int xplot_area = xsize - x_margin - 10; //leave 10 pixel for right margin
    int yplot_area = ysize - y_margin - 35; //leave 35 pixel for x-lebel
    c->setPlotArea( x_margin,
	    y_margin,
	    xplot_area,
	    yplot_area,
	    c->linearGradientColor(0, x_margin, 0, ysize, 0xf9fcff, 0xaaccff),
	    -1,
	    Chart::Transparent, 0xffffff);

    // Add a title box to the chart using title_font pts Times Bold Italic font.
    c->addTitle(title, "timesbi.ttf", title_font);

    // Add a title to the y axis
    c->yAxis()->setTitle(y_title, "arialbi.ttf", 10 );

    // Set the labels on the x axis.
    c->xAxis()->setLabels(StringArray(label, data_points));

    // Display 1 out of 3 labels on the x-axis.
    c->xAxis()->setLabelStep(3);

    // Set the axes width to 2 pixels
    c->xAxis()->setWidth(2);
    c->yAxis()->setWidth(2);

    // Add a line layer to the chart
    c->yAxis()->setTickDensity(20);
    LineLayer *layer = c->addLineLayer();

    // Set the default line width to 2 pixels
    layer->setLineWidth(2);

    //Add datasets here
    for (int i = 0; i < num_dataset; i++)
	layer->addDataSet(DoubleArray(data[i], data_points), data_color[i], legend[i])
	    ->setDataSymbol(Chart::CircleSymbol, 9, data_color[i]);

    //make chart
    c->makeChart(filename);

    //free up resources
    delete c;

    return;
}


void make_simple_area_chart(
        char* filename,         // Fully qualified file name
        char* title,            // Label to be shown on the top of the chart
        int xsize,              // x size in pixel
        int ysize,              // y size in pixel
        double* data,           // Data value in an array
        int data_points,        // total data points
        char** label,           // Label array to be shown on the x-axis
        char* x_title,          // Extra label on x-axis
        char* y_title,          // extra label on y-axis
        int area_color,         // color for the shaded are
        bool transparent)       // flag, if the area is transparent
{
    // Create a XYChart object of size xsize x ysize pixels
    XYChart *c = new XYChart(xsize, ysize);

    // Set the plotarea at (55, 40) and of size 285 x 175 pixels
    int x_margin = 55; // leave 55 pixel on the left for label
    int y_margin = 40; // leave 40 pixel on the top for title 
    int xplot_area = xsize - x_margin - 10; //leave 10 pixel for right margin
    int yplot_area = ysize - y_margin - 35; //leave 35 pixel for x-lebel
    c->setPlotArea(	x_margin, 
	    y_margin, 
	    xplot_area, 
	    yplot_area, 
	    c->linearGradientColor(0, x_margin, 0, ysize, 0xf9fcff, 0xaaccff),
	    -1, 
	    Chart::Transparent, 0xffffff);

    // Add a title box to the chart using 18 pts Times Bold Italic font.
    //TextBox *title = c->addTitle(title, "timesbi.ttf", 12);
    c->addTitle(title, "timesbi.ttf", 12);

    // Add a title to the y axis
    c->yAxis()->setTitle(y_title, "arialbi.ttf", 10 );
    //Set the scale of y axis min = 0, max = max_val + 20%
    //find max val in the data stream
    double max_data_val = find_max(data, data_points);
    double upperlimit = max_data_val + (max_data_val * (0.2));
    //printf("max_data_val = %f, upperlimit = %f\n", max_data_val, upperlimit);
    //Put the upper limit to atleast 5, so that we don't see a decimal point on the y-axis
    if( upperlimit < 5 ) upperlimit = 5;
    c->yAxis()->setLinearScale(0, upperlimit);
    c->yAxis()->setRounding(true, true);

    // Set the labels on the x axis.
    c->xAxis()->setLabels(StringArray(label, data_points));

    // Display 1 out of 3 labels on the x-axis.
    //c->xAxis()->setLabelStep(3);

    // Set the axes width to 2 pixels
    c->xAxis()->setWidth(2);
    c->yAxis()->setWidth(2);

    // Add an area layer to the chart using a gradient color that changes vertically
    // from semi-transparent red (60ff0000) to semi-transparent white (60ffffff)
    c->addAreaLayer(DoubleArray(data, data_points),
	    c->linearGradientColor(0, 50, 0, ysize, area_color, 0x80ffffff));

    // Output the chart
    c->makeChart(filename);

    //free up resources
    delete c;
    return;
}

void make_soft_multibar_chart(
        char* filename,         // Fully qualified file name
        char* title,            // Label to be shown on the top of the chart
        int title_font,         // font for the title
        int xsize,              // x size in pixel
        int ysize,              // y size in pixel
        double** data,          // a set of data array
        int num_dataset,        // number of data set
        char** legend,          // one legend for each data set
        int data_points,        // how many data points to be shown on the graph
        char **label,           // Label array to be shown on the x-axis
        char* x_title,          // Extra label on x-axis
        char* y_title,          // extra label on y-axis
        int* data_colors)
{
    // Create a XYChart object of size 540 x 375 pixels
    XYChart *c = new XYChart(xsize, ysize);

    // Add a title to the chart using 18 pts Times Bold Italic font
    c->addTitle(title, "timesbi.ttf", title_font);

    // Set the plotarea at (55, 40) and of size 285 x 175 pixels
    int x_margin = 55; // leave 55 pixel on the left for label
    int y_margin = 40; // leave 40 pixel on the top for title
    int xplot_area = xsize - x_margin - 10; //leave 10 pixel for right margin
    int yplot_area = ysize - y_margin - 35; //leave 35 pixel for x-lebel
    c->setPlotArea( x_margin,
	    y_margin,
	    xplot_area,
	    yplot_area,
	    c->linearGradientColor(0, x_margin, 0, ysize, 0xf9fcff, 0xaaccff),
	    -1,
	    Chart::Transparent, 0xffffff);

    //Add a legend to the chart
    //center the legend on the screen
    c->addLegend((xsize-x_margin)/num_dataset - 20, 12, false, "timesbi.ttf", 10)->setBackground(Chart::Transparent);

    // Set the labels on the x axis.
    c->xAxis()->setLabels(StringArray(label, data_points));

    // Add a title to the y- axis
    c->yAxis()->setTitle(y_title, "arialbi.ttf", 10 );	

    c->xAxis()->setWidth(2);
    c->yAxis()->setWidth(2);
    // Add a multi-bar layer with 3 data sets
    BarLayer *layer = c->addBarLayer(Chart::Side);
    for(int i = 0; i < num_dataset; i++)
	layer->addDataSet(DoubleArray(data[0], data_points), data_colors[i], legend[i]);

    layer->setBarGap(0.2, Chart::TouchBar);
    //generate chart
    c->makeChart(filename);

    //Free up resources
    delete c;

    return;
}	



void make_soft_multibar_2input_chart(
        char* filename,         // Fully qualified file name
        char* title,            // Label to be shown on the top of the chart
	int title_font,		// font for the title
        int xsize,              // x size in pixel
        int ysize,              // y size in pixel
        double* data1,          // Data value in an array
	char * legend1,		// Legend for first dataset
        double* data2,          // Data value in an array
	char* legend2,		// legend for 2nd data set
        int data_points,        // total data points
        char **label,           // Label array to be shown on the x-axis
        char* x_title,          // Extra label on x-axis
        char* y_title,          // extra label on y-axis
        int data1_color,
        int data2_color)
{
    // Create a XYChart object of size xsize x ysize pixels
    XYChart *c = new XYChart(xsize, ysize);

    // Add a title to the chart using 12 pts Times Bold Italic font
    c->addTitle(title, "timesbi.ttf", title_font);

    // Set the plotarea at (55, 40) and of size 285 x 175 pixels
    int x_margin = 55; // leave 55 pixel on the left for label
    int y_margin = 40; // leave 40 pixel on the top for title
    int xplot_area = xsize - x_margin - 10; //leave 10 pixel for right margin
    int yplot_area = ysize - y_margin - 35; //leave 35 pixel for x-lebel
    c->setPlotArea( x_margin,
	    y_margin, 
	    xplot_area, 
	    yplot_area,
	    c->linearGradientColor(0, x_margin, 0, ysize, 0xf9fcff, 0xaaccff),
	    -1, 
	    Chart::Transparent, 0xffffff);

    //Add a legend to the chart
    //center the legend on the screen

    c->addLegend((xsize-x_margin)/2 - 30, 12, false, "timesbi.ttf", 10)->setBackground(Chart::Transparent);

    // Add a title to the y axis
    c->yAxis()->setTitle(y_title, "arialbi.ttf", 10 );

    // Set the labels on the x axis.
    c->xAxis()->setLabels(StringArray(label, data_points));
    //Set tick offset 0.5
    c->xAxis()->setTickOffset(0.5);

    // Set the axes width to 2 pixels
    c->xAxis()->setWidth(2);
    c->yAxis()->setWidth(2);

    // Add a multi-bar layer with 2 data sets and 4 pixels 3D depth
    BarLayer *layer = c->addBarLayer(Chart::Side , 4);
    layer->setBarShape(Chart::CircleShape);
    layer->addDataSet(DoubleArray(data1, data_points), data1_color, legend1);
    layer->addDataSet(DoubleArray(data2, data_points), data2_color, legend2);

    layer->setBorderColor(Chart::Transparent, Chart::glassEffect(Chart::NormalGlare, Chart::Left));
    // Configure the bars within a group to touch each others (no gap)
    layer->setBarGap(0.2, Chart::TouchBar);


    // Output the chart
    c->makeChart(filename);

    //free up resources
    delete c;
}


void make_stacked_2bar_chart(
        char* filename,         // Fully qualified file name
        char* title,            // Label to be shown on the top of the chart
        int title_font,         // font for the title
        int xsize,              // x size in pixel
        int ysize,              // y size in pixel
        double* data1,          // Data1 values
        char* legend1,          // Legend1
	double* data2,          // Data2 values
        char* legend2,          // Legend2 
        int data_points,        // total data points
        char **label,           // Label array to be shown on the x-axis
        char* x_title,          // Extra label on x-axis
        char* y_title,          // extra label on y-axis
        int data1_color, 
	int data2_color)
{
    // Create a XYChart object of size 500 x 320 pixels
    XYChart *c = new XYChart(xsize, ysize);

    // Set the plotarea at (55, 40) and of size 285 x 175 pixels
    int x_margin = 55; // leave 55 pixel on the left for label
    int y_margin = 40; // leave 40 pixel on the top for title
    int xplot_area = xsize - x_margin - 10; //leave 10 pixel for right margin
    int yplot_area = ysize - y_margin - 35; //leave 35 pixel for x-lebel
    c->setPlotArea( x_margin,
	    y_margin,
	    xplot_area,
	    yplot_area,
	    c->linearGradientColor(0, x_margin, 0, ysize, 0xf9fcff, 0xaaccff),
	    -1,
	    Chart::Transparent, 0xffffff);

    //Add a legend to the chart
    c->addLegend((xsize-x_margin)/2 - 20, 12, false, "timesbi.ttf", 10)->setBackground(Chart::Transparent);

    //Add a title to the graph
    c->addTitle(title, "timesbi.ttf", title_font);

    // Add a title to the y- axis
    c->yAxis()->setTitle(y_title, "arialbi.ttf", 10 );

    // Set the labels on the x axis.
    c->xAxis()->setLabels(StringArray(label, data_points));
    //c->xAxis()->setTickOffset(0.5);

    c->xAxis()->setWidth(2);
    c->yAxis()->setWidth(2);


    // Add a multi-bar layer with 2 data sets and 8 pixels 3D depth
    BarLayer *layer = c->addBarLayer(Chart::Stack, 8);
    layer->setBarShape(Chart::CircleShape);
    layer->addDataSet(DoubleArray(data1, data_points), data1_color, legend1);
    layer->addDataSet(DoubleArray(data2, data_points), data2_color, legend2);

    layer->setBorderColor(Chart::Transparent, Chart::glassEffect(Chart::NormalGlare, Chart::Left));

    // Enable bar label for the whole bar
    layer->setAggregateLabelStyle();
    // Enable bar label for each segment of the stacked bar
    if(data_points < 10)
	layer->setDataLabelStyle();

    layer->setBarGap(0.2, Chart::TouchBar);

    c->makeChart(filename);

    delete c;
    return;
}



void make_3d_area_chart(
        char* filename,         // Fully qualified file name
        char* title,            // Label to be shown on the top of the chart
        int title_font,         // font for the title
        int xsize,              // x size in pixel
        int ysize,              // y size in pixel
        double* data,           // data array
        int data_points,        // number of data points to be displayed
        char** label,           //label array
	int label_step,         // how frequently the labels should show on the x-axis
        char* x_title,          // Extra label on x-axis
        char* y_title,          // extra label on y-axis
        int color)
{
    // Create a XYChart object of size 300 x 300 pixels
    XYChart *c = new XYChart(xsize, ysize);
    c->setRoundedFrame();

    // Set the plotarea at (55, 40) and of size 285 x 175 pixels
    int x_margin = 55; // leave 55 pixel on the left for label
    int y_margin = 40; // leave 40 pixel on the top for title
    int xplot_area = xsize - x_margin - 10; //leave 10 pixel for right margin
    int yplot_area = ysize - y_margin - 35; //leave 35 pixel for x-lebel
    c->setPlotArea( x_margin, y_margin, xplot_area, yplot_area);

    // Add a title to the chart using 12 pts Arial Bold Italic font
    c->addTitle(title, "timesbi.ttf", title_font);

    // Add a title to the y axis
    c->yAxis()->setTitle(y_title, "arialbi.ttf", 10 );

    // Set the labels on the x axis.
    c->xAxis()->setLabels(StringArray(label, data_points));

    // Display 1 out of 60 labels on the x-axis, so that we have only 6 points on the grapha, every 10 mins
    c->xAxis()->setLabelStep(label_step);
    c->xAxis()->setLabelStyle("Arial", 8, Chart::TextColor, 0);

    // Set the axes width to 2 pixels
    c->xAxis()->setWidth(2);
    c->yAxis()->setWidth(2);

    c->yAxis()->setTickDensity(20);

    //add a area layer
    AreaLayer *layer = c->addAreaLayer();

    //add data set
    layer->addDataSet(DoubleArray(data,data_points),  
	    c->linearGradientColor(0, 55, 0, ysize, color, 0x00ffffff));

    //Set 3d
    layer->set3D();

    //generate chart
    c->makeChart(filename);

    //free up resources
    delete c;
    return;
};

void make_stacked_bar_chart(
        char* filename,         // Fully qualified file name
        char* title,            // Label to be shown on the top of the chart
        int title_font,         // font for the title
        int xsize,              // x size in pixel
        int ysize,              // y size in pixel
        double** data,          // a set of data array
        int num_dataset,        // number of data set
        char** legend,          // one legend for each data set
        int data_points,        // how many data points to be shown on the graph
        char **label,           // Label array to be shown on the x-axis
        char* x_title,          // Extra label on x-axis
        char* y_title,          // extra label on y-axis
        int* data_colors)
{
    // Create a XYChart object of size xsize x ysize pixels
    XYChart *c = new XYChart(xsize, ysize);

    // Add a title to the chart using 12 pts Times Bold Italic font
    c->addTitle(title, "timesbi.ttf", title_font);

    // Set the plotarea at (55, 40) and of size 285 x 175 pixels
    int x_margin = 55; // leave 55 pixel on the left for label
    int y_margin = 55; // leave 40 pixel on the top for title
    int xplot_area = xsize - x_margin - 10; //leave 10 pixel for right margin
    int yplot_area = ysize - y_margin - 35; //leave 35 pixel for x-lebel
    c->setPlotArea( x_margin,
	    y_margin,
	    xplot_area,
	    yplot_area,
	    c->linearGradientColor(0, x_margin, 0, ysize, 0xf9fcff, 0xaaccff),
	    -1,
	    Chart::Transparent, 0xffffff);

    //Add a legend to the chart
    //center the legend on the screen

    c->addLegend(x_margin+10, 16, false, "timesbi.ttf", 10)->setBackground(Chart::Transparent);

    // Add a title to the y axis
    c->yAxis()->setTitle(y_title, "arialbi.ttf", 10 );

    // Set the labels on the x axis.
    c->xAxis()->setLabels(StringArray(label, data_points));

    // Add a stacked bar layer and set the layer 3D depth to 8 pixels
    BarLayer *layer = c->addBarLayer(Chart::Stack, 8);

    // Add the three data sets to the bar layer
    for(int i = 0; i < num_dataset; i++){
	layer->addDataSet(DoubleArray(data[i], data_points), data_colors[i], legend[i]);
    }

    // Enable bar label for the whole bar
    layer->setAggregateLabelStyle();

    // Enable bar label for each segment of the stacked bar
    layer->setDataLabelStyle();

    // Output the chart
    c->makeChart(filename);

    //free up resources
    delete c;
    return;
}
