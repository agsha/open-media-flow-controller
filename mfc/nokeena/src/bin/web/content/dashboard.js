//
// Filename:  dashboard.js
// Date:      2009/04/10
// Author:    Sasmita
//
// (C) Copyright 2008-2009 Nokeena Networks, Inc.
// All rights reserved.
//

var update_interval = 10;
var seed = 100;
var datas = null;
var timeout_state = null;
var gc;

function startupdate()
{

	if(document.getElementById('page').value == "dashboard")
		startupdate_dashboard();
	else if (document.getElementById('page').value == "disk_cache") 	
		startupdate_disk_cache();
	//else if (document.getElementById('page').value == "log_analysis")
        //        startupdate_log_analysis();
	else if (document.getElementById('page').value == "cache_hit_rate")
		startupdate_cache_hit_rate();
}


function startupdate_namespace_report()
 {
	var iframe = document.getElementById('nsr_fr');
	iframe.src = '/admin/dashboard_load.pl?file=ns_bandwidth&seed=' + seed;

	timeout_state = setTimeout('startupdate_namespace_report()', update_interval*1000);
        seed = seed + 10;
}


function startupdate_dashboard()
 {
	var iframe = document.getElementById('info_fr');
	iframe.src = '/admin/dashboard_load.pl?file=info&type=html&seed=' + seed;
	datas = document.getElementById('cpu_img');     //cpu usage
        datas.src = '/admin/dashboard_load.pl?file=average_cpu&seed=' + seed ;

        datas = document.getElementById('openconnection_img'); // open connection
        datas.src = '/admin/dashboard_load.pl?file=open_connections&seed=' + seed ;
        datas = document.getElementById('media_delivery_bd_img');   // bandwidth
        datas.src = '/admin/dashboard_load.pl?file=media_delivery_bandwidth&seed=' + seed ;

	datas = document.getElementById('weekly_bw_savings_img'); // weekly bandwidth saving
        datas.src = '/admin/dashboard_load.pl?file=weekly_bw_savings&seed=' + seed ;
        datas = document.getElementById('cache_tier_throughput_img');   // Cache tier throughput
        datas.src = '/admin/dashboard_load.pl?file=cache_tier_throughput&seed=' + seed ;

	timeout_state = setTimeout('startupdate_dashboard()', update_interval*1000);
        seed = seed + 10;
}


function startupdate_disk_cache()
{
	datas = document.getElementById('disk_usage'); // open connection
        datas.src = '/admin/dashboard_load.pl?file=disk_usage&seed=' + seed ;
        datas = document.getElementById('cache_th_img');   // bandwidth
        datas.src = '/admin/dashboard_load.pl?file=disk_throughput&seed=' + seed ;

	timeout_state = setTimeout('startupdate_disk_cache()', update_interval*1000);
        seed = seed + 10;
}

function startupdate_log_analysis()
{
	//var iframe = document.getElementById('log_analysis_fr');
	//iframe.src = '/admin/dashboard_load.pl?file=domain.min&type=html&seed=' + seed;
	datas = document.getElementById('filesize_analysis_img'); // open connection
        datas.src = '/admin/dashboard_load.pl?file=filesize_analyzer&seed=' + seed ;
        datas = document.getElementById('httpcode_analysis_img');   // bandwidth
        datas.src = '/admin/dashboard_load.pl?file=http_response_analyzer&seed=' + seed ;

        timeout_state = setTimeout('startupdate_log_analysis()', update_interval*1000);
        seed = seed + 10;

}

function startupdate_cache_hit_rate()
{
	datas = document.getElementById('ch_rate_bw_img');
	datas.src = '/admin/dashboard_load.pl?file=ch_ratio_bw&seed=' + seed ;

	datas = document.getElementById('ns_bandwidth');
	datas.src = '/admin/dashboard_load.pl?file=ns_bandwidth&seed=' + seed ;

	datas = document.getElementById('ns_chr');
	datas.src = '/admin/dashboard_load.pl?file=ns_chr&seed=' + seed ;

	timeout_state = setTimeout('startupdate_cache_hit_rate()', update_interval*1000);
	seed = seed + 10;
}


function buttoncheck()
{
        update_interval = document.getElementById("db_refreshinterval").value;
        if (update_interval < 10) {
                 update_interval = 10; 
                 document.getElementById("refreshinterval").value=10;
        }
        if(document.getElementById("updatebutton").value == "Reload") {
          document.getElementById("updatebutton").value = "Pause";
          startupdate();
          } 
        else {
          document.getElementById("updatebutton").value = "Reload";
          clearTimeout(timeout_state);
          timeout_state = null;
          }
}



function pause_refresh()
{
	clearTimeout(timeout_state);
	timeout_state = null;
	gc = document.getElementById('pausebutton');
	gc.className = 'ajaxButtonDisabled';
	gc = document.getElementById('reloadbutton');
	gc.className = 'ajaxButton';
	gc = null;
}


function resume_refresh()
{
	update_interval = document.getElementById('db_refreshinterval').value;
	gc = document.getElementById('reloadbutton');
	gc.className = 'ajaxButtonDisabled';
	gc = document.getElementById('pausebutton');
	gc.className = 'ajaxButton';
	startupdate_dashboard();
	gc = null;
	startupdate_dashboard();
}

function validate_al_input()
{
	var hour = document.getElementById('la_thour').value;
	var min = document.getElementById('la_tmin').value;
	var recs = document.getElementById('la_records').value;

	//alert("in validate");
	var msg = "";
	var msg2 = "Invalid input.\n";
	if (hour < 0 || hour > 24){ 
		msg = msg + 'Input hours should be in range [0 to 24]\n';
		alert(msg2+msg);
		return 1;
	}
	if (min < 0 || min > 59){
		msg = msg + 'Input minutes should be in range [0 to 59]\n';
                alert(msg2+msg);
                return 1;
	}
	if (recs < 0 || recs > 40){
		document.getElementById('la_records').value = 1;
		msg = msg + 'Input records should be in range [1 to 40]\n';
                alert(msg2+msg);
                return 1;
	}
	return 0;
}

function load_la_page()
{
	var if1 = document.getElementById('log_analysis_fr');
	var if2 = document.getElementById('filesize_fr');
	var if3 = document.getElementById('respcode_fr');
	var st1 = '/admin/accesslog.pl?seed=1&type=domain';
	var st2 = '/admin/accesslog.pl?seed=1&type=file_size';
	var st3 = '/admin/accesslog.pl?seed=1&type=resp_code';

	var result = validate_al_input();
	if (result > 0)
		return;

	var s1 = st1 + '&h=' + document.getElementById('la_thour').value + 
	'&m=' + document.getElementById('la_tmin').value + '&n=' + document.getElementById('la_records').value;
	if1.src = s1;
 
	var s2 = st2 + '&h=' + document.getElementById('la_thour').value + '&m=' + document.getElementById('la_tmin').value;
	if2.src = s2;

	var s3 = st3 + '&h=' + document.getElementById('la_thour').value + '&m=' + document.getElementById('la_tmin').value;
	if3.src = s3

}

function reset_la_param()
{
	document.getElementById('la_thour').value = 0;
	document.getElementById('la_tmin').value = 30;
	document.getElementById('la_records').value = 20;

}

