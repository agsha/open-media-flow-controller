#!/usr/bin/perl
#!/usr/bin/bash

sub main()
{
    $numargs = $#ARGV + 1;
    my $ret;
    $g_crawler_name = $ARGV[0];
    $g_base_url = $ARGV[1];
    $g_base_url =~ s/\/+$//;
    if (!($string =~ m/\/$/)) {
	if (!($g_base_url =~ m/\.html$/i) && !($g_base_url =~ m/\.htm$/i) && !($g_base_url =~ m/\.shtml$/i)){
	    if ($g_base_url =~ m/\.s3\.amazonaws\.com/i){
		if (!(($g_base_url =~ m/\/\?prefix\=/) || ($g_base_url =~ m/\/\?delimiter\=/))) {
		    $g_base_url = $g_base_url."/";
		}
	    	$g_crawl_type = "s3_crawl";
	    } else {
		$g_base_url = $g_base_url."/";
		$g_crawl_type = "dir_crawl";
	    }
	} else {
	    $g_crawl_type = "html_crawl";
	}
    }

    if (!($g_base_url =~ m/^http/i)) {
	$g_base_url = "http://".$g_base_url;
    }
    $g_host_name = get_domain_from_url($g_base_url);
    $g_base_url = "\"".$g_base_url."\"";
    $g_link_level = $ARGV[2];
    $g_link_level = $g_link_level + 1; #level 0 means download base dir.
    $g_accept_list = trim_spaces($ARGV[3]);
    $g_output_path = $ARGV[4];
    $g_additional_args = $ARGV[6];
    $g_base_output_path = $g_output_path;
    if (!(-e $g_output_path)) {
	system ("mkdir $g_output_path");
    }
    $g_output_path = $g_output_path."/".$g_crawler_name;
    if (!(-e $g_output_path)) {
	system ("mkdir $g_output_path");
    }

    $g_client_domain = "127.0.0.1";

    #64 ports can be configured in nvsd. pickup the first non zero port.
    my $cnt = 1;
    for ($cnt = 1; $cnt <= 64; $cnt++) {
	$g_server_port = `/opt/tms/bin/mdreq -vv query get - /nkn/nvsd/http/config/server_port/$cnt/port`;
	if ($g_server_port) {
	    last;
	}
    }

    if (!$g_server_port) {
	$g_server_port = 80;
    }

    $g_client_domain = $g_client_domain.":".$g_server_port;
    $ENV{'http_proxy'} = "$g_client_domain";

    #actions are given as last argument.
    for ($action_index = 7; $action_index < $numargs; $action_index++) {
	$g_actions[$num_action] = $ARGV[$action_index];
	$num_actions++;
    }

    init();
    #special char to flush output.
    $| = 1;
    $pid = $$;
    printf ("crawler_pid: %d\n", $pid);

    trigger_wget ();

    $ret = parse_wget_output_and_generate_cur_list();
    if ($ret == 1 || $g_urls_found) {
	if (-e $g_prev_only_urilist_file_name) {
	    diff_cur_prev_lists($g_cur_only_urilist_sorted_filename, $g_prev_only_urilist_file_name);
	    generate_cur_del_list($diff_output_filename);
	}
	`cp $g_cur_list_sorted_filename $add_list_filename`;
	update_overall_master_delete_file();
	run_actions();
	if ($g_crawl_finished) {
	    printf ("<%s>: crawl completed\n", $g_crawler_name);
	}
	move_cur_list_prev_list($g_cur_list_sorted_filename, $g_cur_only_urilist_sorted_filename);
    } 
    if ($ret != 1) {
	if (!($wget_error_message eq "")) {
	    printf ("<%s>: $wget_error_message\n", $g_crawler_name);
	}
	if ($g_urls_found) {
	    if (!$g_crawl_finished) {
		printf ("<%s>: Crawl partially completed\n", $g_crawler_name);
	    }
	} else {
	    if (!($wget_error_message eq "WGET_URL_NOT_FOUND") && 
		    !($wget_error_message eq "WGET_RESP_HDR_NOT_FOUND") &&
		    !($wget_error_message =~ m/WGET_INVALID_STATUS_CODE/)) {
		printf ("<%s>: Origin is down\n", $g_crawler_name);
	    }
	}
    }
    cleanup_files ($diff_output_filename, $del_list_filename, $g_cur_only_urilist_file_name, $wget_op_filename, $g_cur_list_file_name, $g_cur_list_sorted_filename, $g_input_config_file);
}

sub init()
{
    $g_cur_list_file_name = $g_output_path."/"."cur_list.txt";
    $g_cur_only_urilist_file_name = $g_output_path."/"."cur_uri_list.txt";
    $g_prev_list_file_name = $g_output_path."/"."prev_list.txt";
    $g_prev_only_urilist_file_name = $g_output_path."/"."prev_uri_list.txt";
    $diff_output_filename = $g_output_path."/"."diff_output.txt";
    $wget_op_filename = $g_output_path."/"."out_file.txt";
    $del_list_filename = $g_output_path."/"."dellist.xml";

    $add_list_filename = $g_output_path."/"."addlist.xml";
    $masterdel_list_filename = $g_output_path."/"."masterdellist.xml";
    $masterdel_list_asx_filename = $g_output_path."/"."masterdellistasx.xml";
    $add_list_asx_filename = $g_output_path."/"."addlistasx.xml";

    $g_cur_list_sorted_filename = $g_output_path."/"."cur_url_list_sorted.txt";
    $g_cur_only_urilist_sorted_filename = $g_output_path."/"."cur_only_url_list_sorted.txt";
}

sub trigger_wget ()
{
    #wget -r -N -np -l 10 --spider http://10.102.0.160 --header=cache-control:max-age=0

    $command = "";
    if ($g_base_url eq "") {
	exit();
    } else {
	if ($g_crawl_type eq "dir_crawl") {
	    $command = $command."/opt/nkn/bin/wget -r -P $g_output_path --ignore-case  --header=\"X-NKN-Internal: crawl-req\" --connect-timeout=10 --dns-timeout=10  --read-timeout=60 --tries=3 --inet4-only --spider --server-response -nd -np -nv $g_additional_args -o $wget_op_filename $g_base_url";
	} elsif ($g_crawl_type eq "html_crawl") {
	    $command = $command."/opt/nkn/bin/wget -r -P $g_output_path --ignore-case  --header=\"X-NKN-Internal: crawl-req\" --connect-timeout=10 --dns-timeout=10  --read-timeout=60 --tries=3 --inet4-only --spider --server-response -nd -nv $g_additional_args -o $wget_op_filename $g_base_url";
	} elsif ($g_crawl_type eq "s3_crawl") {
	    $command = $command."/opt/nkn/bin/wget -r -P $g_output_path --ignore-case  --header=\"X-NKN-Internal: crawl-req\" --connect-timeout=10 --dns-timeout=10  --read-timeout=60 --tries=3 --inet4-only --server-response -nd -np -nv -o $wget_op_filename $g_base_url";
	}
    }
    
    if ($g_additional_args eq "") {
	$command = $command." --header=\"Host: $g_host_name\"";
    }

    if (!($g_accept_list eq "")) {
	$command = $command." --accept $g_accept_list ";
    }

    if (!($g_link_level eq "")) {
	$command = $command." -l $g_link_level";
    }

    `$command`;
}

sub parse_wget_output_and_generate_cur_list()
{
    open WGETOP_HDL, $wget_op_filename or die $!;
    open CUR_URLLIST_HDL, ">$g_cur_list_file_name" or die $!;
    open CUR_ONLYURLLIST_HDL, ">$g_cur_only_urilist_file_name" or die $!;
    my $url = 0;
    my $content_length = 0;
    my $line = "";
    my $etag = "NULL";
    my $lm = "NULL";
    my @temp1;
    my $ret = 0;
    $g_crawl_finished = 0;
    $first_line = 1;

    if ($g_crawl_type eq "s3_crawl") {
	close CUR_ONLYURLLIST_HDL;
	close CUR_URLLIST_HDL;
	close WGETOP_HDL;
	my $ret_py = `python /opt/nkn/bin/generate_aws_file_list.py $g_host_name $wget_op_filename $g_cur_list_file_name $g_cur_only_urilist_file_name`;
	@temp = split(/:/, $ret_py);
	$wget_error_message = @temp[0];
	if ($wget_error_message eq "WGET_URL_FOUND") {
	   $ret = 1;
	   $g_urls_found = 1;
	   $wget_state = @temp[2];
	   if ($wget_state eq "1\n") {
		$g_crawl_finished = 1;
	   }
	} else {
	   $ret = -1;
	   $g_urls_found = 0;
	}

	if ($ret == 1 || $g_urls_found == 1) {
	    open TMPHDL, ">$g_cur_list_sorted_filename" or die $!;
	    $date = `date`;
	    chomp($date);
	    printf TMPHDL "$date\n";
	    close TMPHDL;
	    `sort $g_cur_list_file_name > $g_cur_list_sorted_filename`;
	    `sort $g_cur_only_urilist_file_name > $g_cur_only_urilist_sorted_filename`;
    	}
	return($ret);
    }
 
    while (<WGETOP_HDL>) {
	$line = $_;
	$line = trim($line);
	if ($first_line) {
	    if ($line eq "" || !($line =~ m/HTTP\//i)) {
		next;
	    }
	    if ($line =~ m/HTTP\//i) {
		@temp1 = split(/\s/, $line);
	        $resp_code = @temp1[1]; 
		if ($resp_code != 200) {
		    if ($resp_code == 404) {
			$wget_error_message = "WGET_URL_NOT_FOUND";
		    } else {
			$wget_error_message = "WGET_INVALID_STATUS_CODE_$resp_code";
		    }
		    $ret = -1;
		    $first_line = 0;
		    last;
		}
	    }
	    $first_line = 0;
	} else {
	    if ($line =~ m/Content-Length:/) {
	        @temp = split (/:/, $line);
	        $content_length = @temp[1];
	    } elsif ($line =~ m/Etag/i) {
	        @temp = split (/:/, $line);
	        $etag = trim(@temp[1]);
	    } elsif ($line =~ m/Last-Modified/i) {
	        @temp = split (/: /, $line);
	        $lm = @temp[1];
	    } elsif ($line =~ m/URL/) {
	        #2012-01-23 07:34:45 URL:http://10.157.42.19/wget_test1/
	        #2012-01-23 07:34:45 URL: http://10.157.42.19/wget_test1/2.ABC 200 OK
	        #URL: https://10.157.42.168/https_test/2.wmv
	        if ($line =~ m/http:\/\//i) {
	            @temp = split("http://", $line);
	    	@temp1 = split (/\s+/, @temp[1]);
	    	$url = trim(@temp1[0]);
	    	$url = "http://".$url;
	    	if ($url =~ m/\/$/) {
	    	    next;
	    	}
	        } else {
	            @temp = split("https://", $line);
	    	@temp1 = split (/\s+/, @temp[1]);
	    	$url = trim(@temp1[0]);
	    	$url = "http://".$url;
	    	if ($url =~ m/\/$/) {
	    	    next;
	    	}
	        }
	        if ($url =~ m/C=.;O=./) {
	            next;
	        }
	        $g_urls_found = 1;
	        my $old_domain = get_domain_from_url($url);
	        print CUR_ONLYURLLIST_HDL "<url>$url</url>\n";
	        print CUR_URLLIST_HDL "<url>$url</url> <domain>$old_domain</domain> <etag>$etag</etag> <cl>$content_length</cl> <lm>$lm</lm>\n";
	        $url = 0;
	        $content_length = 0;
	        $etag = "NULL";
	        $lm = "NULL";
	        $ret = 0;
	    } elsif ($line =~ m/Read error/i) {
	        $wget_error_message = "WGET_READ_ERROR";
	        $ret = -1;
	    } elsif ($line =~ m/No data received/i) {
	        $wget_error_message = "WGET_NO_DATA_RECEIVED";
	        $ret = -1;
	    } elsif ($line =~ m/FINISHED/i) {
	        $g_crawl_finished = 1;
	        if ($ret != -1) {
	    	$ret = 1;
	        }
	    }
	}
    }
    if ($first_line == 1) {
	$wget_error_message = "WGET_RESP_HDR_NOT_FOUND";
	$ret = -1;
    }
    close CUR_ONLYURLLIST_HDL;
    close CUR_URLLIST_HDL;
    close WGETOP_HDL;

    if ($ret == 1 || $g_urls_found == 1) {
	open TMPHDL, ">$g_cur_list_sorted_filename" or die $!;
	$date = `date`;
	chomp($date);
	printf TMPHDL "$date\n";
	close TMPHDL;
	`sort $g_cur_list_file_name > $g_cur_list_sorted_filename`;
	`sort $g_cur_only_urilist_file_name > $g_cur_only_urilist_sorted_filename`;
    }
    return($ret);
}

sub diff_cur_prev_lists()
{
    $cur_list = $_[0];
    $prev_list= $_[1];

    `diff $prev_list $cur_list > $diff_output_filename`;
}

sub generate_cur_del_list () {
    my $ret;
    $diff_output_file = $_[0];
    chomp($diff_output_file);

    my $add_header = 0;
    my $del_header = 0;

    open DIFFOP_HDL, "$diff_output_file" or die $!;

    while (<DIFFOP_HDL>) {
	$line = trim($_);
	chomp($line);
	if (!($line =~ m/url/)) {
	    next;
	}
	if ($line =~ m/^</) {
	    $line =~ s/^<//;
	    $ret = check_if_duplicate_entry($line);
	    if ($ret == 0) {
		$line1 = trim($line);
		`grep \"$line1\" $g_prev_list_file_name >> $del_list_filename`;
	    }
	}
    }

    close DIFFOP_HDL;
}

sub cleanup_files ()
{
    foreach $file (@_) {
	`rm -rf $file`;
    }
}

sub move_cur_list_prev_list()
{
    $cur_list = $_[0];
    $cur_only_urilist = $_[1];
    if (-e $cur_list) {
	`mv $cur_list $g_prev_list_file_name`;
    }

    if (-e $cur_only_urilist) {
        `mv $cur_only_urilist $g_prev_only_urilist_file_name`;
    }
}


sub trim($)
{
    my $string = shift;
    $string =~ s/^\s+//;
    $string =~ s/\s+$//;
    $string =~ s/"//g;
    return $string;
}

sub trim_spaces($)
{
    my $string = shift;
    $string =~ s/\s//g;
    $string =~ s/\,+$//;
    return $string;
}

sub generate_asx_related_files()
{
    
}

sub run_actions()
{
    my @temp = "";
    my @temp1 = "";
    for ($i = 0; $i < $num_actions; $i++) {
	if ($g_actions[i]) {
	    @temp = split(/,/, $g_actions[i]);
	    if (@temp[0] =~ m/auto_generate/) {
		handle_action_auto_gen(@temp[1], @temp[2]);
	    }
	}
    }
}

sub handle_action_auto_gen()
{
    my $src, $dest;
    $src_str = $_[0];
    $dest_str = $_[1];
    @temp1 = split(/src:/, $src_str);
    $src = trim(@temp1[1]);
    @temp1 = split(/dest:/, $dest_str);
    $dest = trim(@temp1[1]);
    $dest = lc($dest);
    autogen_dest_files($src, $dest);
}

sub autogen_dest_files()
{
    $src = $_[0];
    $dest = $_[1];
    $dest =~ s/^\.+//;
    if (-e $del_list_filename) {
	$del_filename = $g_output_path."/"."masterdellist"."$dest".".xml";
	`cat $del_list_filename | grep -i \".$src<\" | perl -pi -e 's/($src)</\$1.$dest</i' >> $del_filename`;
    }
    if (-e $add_list_filename) {
	$add_filename = $g_output_path."/"."addlist"."$dest".".xml";
	`cat $add_list_filename | grep -i \".$src<\" | perl -pi -e 's/($src)</\$1.$dest</i' > $add_filename`;
    }
}

sub update_overall_master_delete_file()
{
    if (-e $del_list_filename) {
	`cat $del_list_filename >> $masterdel_list_filename`;
    }
}

sub get_domain_from_url()
{
    $url = $_[0];
    my $protocol;
    my $domainName;
    $url =~ m|(\w+)://([^/:]+)(:\d+)?/(.*)|;  # use m|...| so that we do not need to use a lot of "\/" 
    $protocol = $1;
    $domainName = $2;
    $port = $3;
    $domainName = $domainName.$port;
    return $domainName;
}

sub check_if_duplicate_entry()
{
    my $line = $_[0];
    my @temp = split("</url>", $line);
    my $url = @temp[0]."</url>";
    my $ret = `grep \"$url\" $masterdel_list_filename | wc -l`;
    return $ret;
}

main()
