#ifndef  __DASHBOARD_GRAPH__H
#define  __DASHBOARD_GRAPH__H

#include "chartdir.h"

/*
 * This chart is similar to a round meter chart described in chart director
 */
void make_round_meter_chart(
	char*   filename, 	// Fully qualified file name
	char*   title,          // Label to be shown on the meter Ex, CPU   
        int     xsize,		// x size in pixel
        int     ysize,		// y size in pixel
	double  data_val,       // Data value to be shown on the round meter
        int     green_zone,	// green zone Default 60 (range 0-60)
        int     yellow_zone,	// yellow zone 80 (range 60 - 80)
        int     red_zone);	// red zone 100, (ranze  80 - 100)

/*
 * This chart is similar to semi-circle meter chart described in chart director
 */
void make_semi_circle_chart(
	char* 	filename,	// Fully qualified file name 
	char*	title,  	// Label to be shown on the meter Ex, CPU
	int	xsize, 		// x size in pixel
	int	ysize, 		// y size in pixel
	double	data_val, 	// Data value to be shown on the meter
	int	green_zone, 	// green zone Default 60 (range 0-60)
	int	yellow_zone, 	// yellow zone 80 (range 60 - 80)
	int	red_zone);	// red zone 100, (ranze  80 - 100)

/*
 * This is a simple area chart simillar to the simple area chart described 
 * in Chart director.
 */
void make_simple_area_chart(
	char* filename,		// Fully qualified file name
	char* title,		// Label to be shown on the top of the chart
 	int xsize,		// x size in pixel
	int ysize,		// y size in pixel
	double* data,		// Data value in an array
	int data_points,	// total data points
	char** label,		// Label array to be shown on the x-axis
	char* x_title,		// Extra label on x-axis
	char* y_title,		// extra label on y-axis
	int area_color,		// color for the shaded are
	bool transparent = true);	// flag, if the area is transparent
	 

/*
 * This is a modified version of symbol line chart described in 
 * Chart Director
 */
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
        int* data_color);       //line color for each dataset


/*
 * This is a 3d PI chart similar to on described in Chart 
 * Director
 */
void make_3d_pie_chart(
	char* filename,         // Fully qualified file name
        char* title,            // Label to be shown on the top of the chart
        int title_font,         // font for the title
        int xsize,              // x size in pixel
        int ysize,              // y size in pixel
	double* data, 		// data array
	int data_points,        // total data points
	char** label,		// labels
	int* color,		// color
	int label_format);	// 1 = circular, on the pie
				// 2 = circular, outside of the pie,
				// 3 = side label, 4 = only legend
				// 5 = side label with legend

/*
 * This chart is simillar to the softmultibar chart described in the
 * chart director. It takes only 2 data sets
 */
void make_soft_multibar_2input_chart(
	char* filename,         // Fully qualified file name
        char* title,            // Label to be shown on the top of the chart
        int title_font,		// font for the title
	int xsize,              // x size in pixel
        int ysize,              // y size in pixel
	double* data1,          // Data value in an array
	char* legend1, 		// legend for first data set
	double* data2,          // Data value in an array
	char* legend2, 		// legend for second data set
	int data_points,        // total data points
	char **label,           // Label array to be shown on the x-axis
	char* x_title,          // Extra label on x-axis
	char* y_title,          // extra label on y-axis
	int data1_color,
	int data2_color);

/*
 *  This chart is similar to stacked bar chart described in 
 * Chart director, it takes only 2 data sets for stacking 
 */
void make_stacked_2bar_chart(
	char* filename,         // Fully qualified file name
        char* title,            // Label to be shown on the top of the chart
        int title_font,         // font for the title
        int xsize,              // x size in pixel
        int ysize,              // y size in pixel
	double* data1,		// Data1 values 
	char* legend1,		// Legend2
	double* data2,          // Data2 values
        char* legend2,          // Legend2 
	int data_points,        // total data points
        char **label,           // Label array to be shown on the x-axis
        char* x_title,          // Extra label on x-axis
        char* y_title,          // extra label on y-axis
	int data1_color,
	int data2_color);


/*
 * This chart is similar to soft multi bar chart described in the chart 
 * director. 
 */
void make_soft_multibar_chart(
	char* filename,         // Fully qualified file name
        char* title,            // Label to be shown on the top of the chart
        int title_font,         // font for the title
        int xsize,              // x size in pixel
        int ysize,              // y size in pixel
	double** data,		// a set of data array
	int num_dataset,	// number of data set
	char** legend, 		// one legend for each data set
	int data_points, 	// how many data points to be shown on the graph
	char **label,           // Label array to be shown on the x-axis
        char* x_title,          // Extra label on x-axis
        char* y_title,          // extra label on y-axis
	int* data_colors);


/*
 * 3d_area chart
 */
void make_3d_area_chart(
	char* filename,         // Fully qualified file name
        char* title,            // Label to be shown on the top of the chart
        int title_font,         // font for the title
        int xsize,              // x size in pixel
        int ysize,              // y size in pixel
	double* data, 		// data array
	int data_points,	// number of data points to be displayed
	char** label, 		// label array
	int label_step, 	// how frequently the labels should show on the x-axis
	char* x_title,          // Extra label on x-axis
        char* y_title,          // extra label on y-axis
        int color);


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
        int* data_colors);

#endif // __DASHBOARD_GRAPH__H

