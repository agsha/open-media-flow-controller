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
	var st1 = '/cgi-bin/mfllog_analysis.pl?seed=1&type=domain';

	var result = validate_al_input();
	if (result > 0)
		return;

	var s1 = st1 + '&h=' + document.getElementById('la_thour').value + 
	'&m=' + document.getElementById('la_tmin').value + '&n=' + document.getElementById('la_records').value;
//	alert(s1);
	if1.src = s1;
 
}

function reset_la_param()
{
	document.getElementById('la_thour').value = 0;
	document.getElementById('la_tmin').value = 30;
	document.getElementById('la_records').value = 20;

}

