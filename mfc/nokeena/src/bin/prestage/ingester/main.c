#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <libgen.h>
#include "network.h"

int ingester_log_level = LOG_DEBUG;


struct ingester_conf {
    int	    max_fd;
};

struct ingester_stats {
    uint64_t	tot_bytes;
};



struct ingester_conf selfconf;
struct ingester_stats selfstats;

pthread_t pthr_net;

int check_pid(const char *pid_file);
void *net_thread(void *args);
int daemonize(void);
int micro_parser(struct network *net);
void start_net_thread(void);
int connect_out(struct in_addr dest, char *request,
		char *nsname, char *host, char *uri,
		char *path, int64_t size);
int __remove_net(struct network *net);
int create_control_uds(void);
int add_prestage_entry(struct network *net);
int add_upload_entry(struct network *net);
int delete_upload_entry(struct network *net);
int clean_file(struct network *net);
void net_free(struct network *net);

#include "common.c"


int
add_prestage_entry(struct network *net)
{
    char abs_path[256], *fname = NULL, ps_entry[2048];
    const char *host = NULL;
    char sub_dir[256], ps_fname[256], *dir_name = NULL, url[1024];
    FILE *fp = NULL;

    bzero(abs_path, sizeof(abs_path));
    snprintf(abs_path, sizeof(abs_path),"%s", net->abs_path);
    fname = basename(abs_path);
    if (fname == NULL) {
	log_to_syslog(LOG_NOTICE, "basename failed (%d)", errno);
	return 1;
    }

    bzero(sub_dir, sizeof(sub_dir));
    snprintf(sub_dir, sizeof(sub_dir),"%s", net->abs_path);
    dir_name = dirname(sub_dir);
    if (dir_name == NULL) {
	log_to_syslog(LOG_NOTICE, "dirname failed (%d)", errno);
	return 1;
    }
    bzero(ps_fname, sizeof(ps_fname));
    snprintf(ps_fname, sizeof(ps_fname), "%s/prestage.info", dir_name);
    if (net->host && (strcmp(net->host, "127.0.0.1"))) {
	host = net->host;
    } else {
	host = "*";
    }
    bzero(url, sizeof(url));
    snprintf(url, sizeof(url), "http://%s%s", host, net->uri);
    bzero(ps_entry, sizeof(ps_entry));
    snprintf(ps_entry, sizeof(ps_entry), "%s %ld %s %s\n",
	    net->time, net->size, fname, url);
    fp = fopen(ps_fname, "a+");
    if (fp == NULL) {
	log_to_syslog(LOG_NOTICE, "fopen failed- %s (%d)", ps_fname, errno);
	return 1;
    }
    fseek(fp, 0, SEEK_END);
    fwrite(ps_entry, 1, strlen(ps_entry), fp);
    fclose(fp);
    return 0;
}
int
add_upload_entry(struct network *net)
{
    char abs_path[256], *fname = NULL, ps_entry[2048];
    const char *host = NULL;
    char sub_dir[256], ps_fname[256], *dir_name = NULL, url[1024];
    FILE *fp = NULL;

    bzero(abs_path, sizeof(abs_path));
    snprintf(abs_path, sizeof(abs_path),"%s", net->abs_path);
    fname = basename(abs_path);
    if (fname == NULL) {
	log_to_syslog(LOG_NOTICE, "basename failed");
	return 1;
    }

    bzero(sub_dir, sizeof(sub_dir));
    snprintf(sub_dir, sizeof(sub_dir),"%s", net->abs_path);
    dir_name = dirname(sub_dir);
    if (dir_name == NULL) {
	log_to_syslog(LOG_NOTICE, "dirname failed");
	return 1;
    }
    bzero(ps_fname, sizeof(ps_fname));
    snprintf(ps_fname, sizeof(ps_fname), "%s/upload.info", dir_name);
    if (net->host && (strcmp(net->host, "127.0.0.1"))) {
	host = net->host;
    } else {
	host = "*";
    }
    bzero(url, sizeof(url));
    snprintf(url, sizeof(url), "http://%s%s", host, net->uri);
    bzero(ps_entry, sizeof(ps_entry));
    snprintf(ps_entry, sizeof(ps_entry), "%s %ld %s %s\n",
	    net->time, net->size, fname, url);
    fp = fopen(ps_fname, "a+");
    if (fp == NULL) {
	log_to_syslog(LOG_NOTICE, "fopen failed- %d, %s", errno, ps_entry);
	return 1;
    }
    fseek(fp, 0, SEEK_END);
    fwrite(ps_entry, 1, strlen(ps_entry), fp);
    fclose(fp);
    return 0;
}

int
delete_upload_entry(struct network *net)
{
    char abs_path[256], *fname = NULL, ps_entry[2048], line[2048];
    const char *host = NULL;
    char sub_dir[256], ps_fname[256], tmp_fname[256],  *dir_name = NULL, url[1024];
    FILE *fp = NULL, *fp_tmp = NULL;

    bzero(abs_path, sizeof(abs_path));
    snprintf(abs_path, sizeof(abs_path),"%s", net->abs_path);
    fname = basename(abs_path);
    if (fname == NULL) {
	return 1;
    }

    bzero(sub_dir, sizeof(sub_dir));
    snprintf(sub_dir, sizeof(sub_dir),"%s", net->abs_path);
    dir_name = dirname(sub_dir);
    if (dir_name == NULL) {
	return 1;
    }
    bzero(ps_fname, sizeof(ps_fname));
    snprintf(ps_fname, sizeof(ps_fname), "%s/upload.info", dir_name);
    bzero(tmp_fname, sizeof(tmp_fname));
    snprintf(tmp_fname, sizeof(tmp_fname), "%s/upload_tmp.info", dir_name);

    if (net->host && (strcmp(net->host, "127.0.0.1"))) {
	host = net->host;
    } else {
	host = "*";
    }
    bzero(url, sizeof(url));
    snprintf(url, sizeof(url), "http://%s%s", host, net->uri);

    bzero(ps_entry, sizeof(ps_entry));
    snprintf(ps_entry, sizeof(ps_entry), "%s %ld %s %s\n",
	    net->time, net->size, fname, url);
    fp = fopen(ps_fname, "a+");
    unlink(tmp_fname);
    fp_tmp = fopen(tmp_fname, "w+");
    if (fp == NULL || fp_tmp == NULL) {
	log_to_syslog(LOG_NOTICE, "fopen failed- %d", errno);
	if (fp) fclose(fp);
	if (fp_tmp) fclose(fp_tmp);
	return 1;
    }
    fseek(fp, 0, SEEK_SET);
    fseek(fp_tmp, 0, SEEK_SET);
    bzero(line, sizeof(line));
    while ( fgets ( line , sizeof(line), fp ) != NULL )
    {
	log_to_syslog(ingester_log_level, "DEL:searching the entry %s (%ld)"
		"for deletion %s (%ld)",
		ps_entry, strlen(ps_entry), line, strlen(line));
	if (0 == strcmp(line, ps_entry)) {
	    log_to_syslog(ingester_log_level,"FOUND the entry for %s for deletion", ps_entry);
	    bzero(line, sizeof(line));
	    continue;
	}
	fwrite(line, 1, strlen(line), fp_tmp);
	bzero(line, sizeof(line));
    }
    fclose(fp);
    fclose(fp_tmp);

    rename(tmp_fname, ps_fname);
    return 0;
}

int
clean_file(struct network *net)
{
    //delete_upload_entry(net);
    if (net->req_status == 0 && net->status == 200) {
	add_prestage_entry(net);
	log_to_syslog(LOG_NOTICE, "namespace %s : ingested file %s",
		net->ns?: "no namespace", net->uri?:"no filename");
    } else {
	/* if req_status is EINVAL, then this is a case of partial ingestion */
	log_to_syslog(LOG_NOTICE, "namespace %s : ingestion failed (%d:%d) for file %s",
		net->ns?:"(none)", net->status, net->req_status, net->uri?:"(no uri)");
    }

    return 0;
}


int micro_parser(struct network *net)
{
    FILE *stream;
    char *line = NULL;
    int n;
    char msg[64];
    size_t len = 0;
    size_t hdr_len = 0;

    stream = fmemopen(net->buf, MAX_BUF, "r");

    // Get the status line
    n = getline(&line, &len, stream);
    hdr_len += n;
    //log_to_syslog(ingester_log_level, "RESP Line: %s\n", line);
    sscanf(line, "HTTP/1.1 %d %s\r\n", &net->status, msg);
    //log_to_syslog(ingester_log_level, "STATUS: %d\n", net->status);
    free(line);


    line = NULL;
    len = 0;
    do {
	n = getline(&line, &len, stream);
	hdr_len += n;
	if (n == 2) {
	    free(line);
	    break;
	}

	if (strcasestr(line, "Content-Length")) {
	    //sscanf(line, "Content-Length: %d\r\n", &net->expected_len);
	    //log_to_syslog(ingester_log_level, "Content-Length: %d\n", net->expected_len);
	    continue;
	}

	//log_to_syslog(ingester_log_level, "RESP LINE: %s\n", line);

    } while(1);

    net->header = hdr_len;
    fclose(stream);
    return 0;
}


int daemonize(void)
{
    if (0 != fork()) exit(0);
    if (-1 == setsid()) exit(0);

    return 0;
}




void start_net_thread(void)
{
    pthread_create(&pthr_net, NULL, net_thread, NULL);
    return;
}

int
check_pid(const char *pid_file)
{
    char	sz_buf [48];
    int	n_size = 0;
    int	ingester_pid = 0;
    FILE	*fp_pidfile = NULL;
    /* Sanity test */
    if (!pid_file) return 1;

    /* Now open the pid file */
    fp_pidfile = fopen (pid_file, "r");
    if (NULL == fp_pidfile)
    {
	/* No pid file hence no process */
	return 0;
    }
    /* Now read the PID */
    memset (sz_buf, 0, sizeof(sz_buf));
    n_size = fread (sz_buf, sizeof(char), sizeof(sz_buf), fp_pidfile);
    if (n_size <= 0)
    {
	/* Empty PID file - should never happen */
	fclose(fp_pidfile);
	return 3;
    }

    /* Close the FILE */
    fclose(fp_pidfile);

    /* We assume the buffer has the PID */
    ingester_pid = atoi(sz_buf);
    if (ingester_pid > 0 && ingester_pid != getpid()) {
	/* a ingester is already running into the system */
	exit(1);
    }
    return 0;
}

int main(int argc, char **argv)
{
    int ret = 0;

    check_pid("/coredump/ingester/ingester.pid");
    if ((ret = parse_args(argc, argv)) != argc) {
	return -1;
    }

    /* integrated with pm, so we must not be daemoning */
    //daemonize();

    start_net_thread();

    sleep(1);
    openlog("ingester", LOG_PID|LOG_NDELAY, LOG_DAEMON);
    ret = create_control_uds();
    if (ret != 0) {
	log_to_syslog(LOG_NOTICE, "Unable to create UDS (err =%d)", ret);
	return EINVAL;
    }

    system("/usr/bin/find /nkn/ftphome -name upload.info -type f -exec rm {} \\;");
    pthread_join(pthr_net, NULL);
    return 0;
}



void *net_thread(void *args)
{
    int i;
    int ret;
    struct epoll_event *events;
    int nfd;

    (void) args;

    epfd = epoll_create(selfconf.max_fd);
    if (epfd < 0) {
	return NULL;
    }

    events = calloc(selfconf.max_fd, sizeof(struct epoll_event));
    if (events == NULL) {
	return NULL;
    }

    //struct network *net = NULL;

    for (;;) {
	nfd = epoll_wait(epfd, events, selfconf.max_fd, -1);
	for (i = 0; i < nfd; i++) {
	    struct socku *sku = NULL;
	    sku = (struct socku *)events[i].data.ptr;
	    ret = 0;
	    sku_get(sku);
	    if (events[i].events & EPOLLIN) {
		ret = (sku->_epollin) ? sku->_epollin(sku) : 0;
	    }
	    if ((ret == 0) && (events[i].events & EPOLLOUT)) {
		ret = (sku->_epollout) ? sku->_epollout(sku) : 0;
	    }
	    if ((ret == 0) && (events[i].events & EPOLLERR)) {
		ret = (sku->_epollerr) ? sku->_epollerr(sku) : 0;
	    }
	    if ((ret == 0) && (events[i].events & EPOLLHUP)) {
		ret = (sku->_epollhup) ? sku->_epollhup(sku) : 0;
	    }
	    if (!ret) {
		// Otherwise, the sku was freed
		sku_put(sku);
	    }
	}

    }

    return NULL;
}


int connect_out(struct in_addr dest, char *request,
		char *nsname, char *host, char *uri,
		char *path, int64_t size)
{
    int ret = 0, fd;
    struct network *net = NULL;
    time_t time_now ;
    struct tm curr_time;
    char time_buf[256] = {0};

    if (size <= 0) {
	return -1;
    }
    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd <= 0) {
	log_to_syslog(ingester_log_level, "error: fd > max_fd [fd = %d, max_fd = %d]\n",
	       fd, max_fd);
	return -5;
    }

    net = malloc(sizeof(struct network));
    if (net == NULL) {
	close(fd);
	return 1;
    }
    bzero(net, sizeof(struct network));
    net->sku_fd = fd;
    // Plugin the sip/dip details here.
    net->sip.sin_addr.s_addr = 0;
    net->sip.sin_family = AF_INET;
    net->sip.sin_port = htons(0);
    net->dip.sin_addr = dest;
    net->dip.sin_family = AF_INET;
    net->dip.sin_port = htons(dport);

    net->request = strdup(request);
    net->ns = strdup(nsname);
    net->host = strdup(host);
    net->uri = strdup(uri);
    net->size = size;
    net->expected_len = size;
    net->abs_path = strdup(path);
    /* set this to 1 to start with error to avoid false sucess */
    net->req_status = 1;

    time(&time_now); 
    localtime_r(&time_now, &curr_time);

    strftime(time_buf, sizeof(time_buf), "%d-%b-%Y %H:%M:%S", &curr_time);
    net->time = strdup(time_buf);

    if ((ret = add_net(net))) {
	log_to_syslog(ingester_log_level, "failed to add epoll for fd = %d (%d)\n", fd, ret);
	close(fd);
	net_free(net);
	return ret;
    }

    ret = add_upload_entry(net);
    if (ret != 0) {
	close(fd);
	net_free(net);
	return ret;
    }
    return 0;
}

void net_free(struct network *net)
{
    (net->ns) ? free(net->ns) : 0;
    (net->host) ? free(net->host) : 0;
    (net->uri) ? free(net->uri) : 0;
    (net->abs_path) ? free(net->abs_path) : 0;
    (net->request) ? free(net->request) : 0;
    (net->time) ? free(net->time) : 0;

    free(net);
}

int add_net(struct network *net)
{
    int ret = 0;
    char *c = 0, *d = 0;
    uint16_t cp, dp;

    if (!epfd || !net || (-1 == net->sku_fd)) {
	ret = -1;
	log_to_syslog(ingester_log_level,"epfd = %d, net = %p, fd = %d\n", epfd, net, net->sku_fd);
	goto bail_2;
    }

    net_get((struct socku*) net);

    net->epollin = net_epollin;
    net->epollout = net_epollout;
    net->epollerr = net_epollerr;
    net->epollhup = net_epollhup;

    c = strdup(inet_ntoa(net->sip.sin_addr));
    cp = ntohs(net->sip.sin_port);
    d = strdup(inet_ntoa(net->dip.sin_addr));
    dp = ntohs(net->dip.sin_port);


    if (!epfd || !net || (-1 == net->sku_fd)) {
	ret = -1;
	log_to_syslog(ingester_log_level, "epfd = %d, net = %p, fd = %d\n", epfd, net, net->sku_fd);
	abort();
	goto bail;
    }


    ret = connect(net->sku_fd, (struct sockaddr*)&(net->dip), sizeof(struct sockaddr_in));
    if (ret < 0) {
	ret = -errno;
	if (ret == -EINPROGRESS) {
	    net->sku_state = CONNECTING;
	    ret = 0;
	    goto bail;
	}
	else {
	    perror("connect ");
	    log_to_syslog(ingester_log_level, "connect() error: fd = %d\n", net->sku_fd);
	    goto bail;
	}
    }

    net->sku_state = CONNECTING;

    net_epollout((struct socku *) net);

    if (setnonblock(net->sku_fd) < 0) {
	perror("fcntl");
	ret = -1;
	log_to_syslog(ingester_log_level, "fcntl: epfd = %d, net = %p, fd = %d\n", epfd, net, net->sku_fd);
	abort();
	goto bail;
    }

    net->event.events = EPOLLIN;
    net->event.data.ptr = net;

    if ((ret = epoll_ctl(epfd, EPOLL_CTL_ADD, net->sku_fd, &net->event)) < 0) {
	log_to_syslog(ingester_log_level, "epoll set insertion error: fd = %d\n", net->sku_fd);
	goto bail;
    }


bail:
    if (c) free(c);
    if (d) free(d);
    net_put((struct socku*) net);
bail_2:
    return ret;
}



// Always called in locked context
int __remove_net(struct network *net)
{
    int ret = 0;
    if (!epfd || !net || (-1 == net->sku_fd)) {
	ret = -1;
	goto bail;
    }

    if ((ret = epoll_ctl(epfd, EPOLL_CTL_DEL, net->sku_fd, NULL)) < 0) {
	log_to_syslog(ingester_log_level, "EPOLL_CTL_DEL failed: %d\n", ret);
    }

    net->sku_state = CLOSED;

    close(net->sku_fd);
    net->sku_fd = -1;
    net->epollin = 0;
    net->epollout = 0;
    net->epollerr = 0;
    net->epollhup = 0;

    net_put((struct socku *) net);

    net_free(net);

bail:
    return ret;
}



int net_epollin(struct socku *sku)
{
    struct network *net = net_sku(sku);
    int ret = 0;

    if (net->sku_state == CONNECTING) {
	goto bail;
    } else if (net->sku_state == CONNECTED) {

	ret = recv(net->sku_fd, net->buf, MAX_BUF, 0);

	if (ret <= 0) {
	    if (errno == EAGAIN) {
		net->event.events = (EPOLLIN);
		net->event.data.ptr = net;

		ret = epoll_ctl(epfd, EPOLL_CTL_MOD, net->sku_fd, &net->event);
		if (ret < 0) {
		    /* error in setting epoll_ctl() */
		    ret = 0;
		    goto bail;
		}
	    } else {
		/* ret == 0, success, else error occured */
		if (ret < 0) {
		    net->req_status = errno;
		    perror("recv");
		    log_to_syslog(LOG_NOTICE, "recv() failed");
		} else {
		    if (net->rxlen - net->header == net->expected_len) {
			net->req_status = 0;
		    } else {
			net->req_status = EINVAL;
		    }
		}
		goto bail_free;
	    }
	} else {
	    if (!net->header) {
		net->header = 1;
		micro_parser(net);
	    }
	}
	net->rxlen += ret;
	selfstats.tot_bytes += ret;
	ret = 0;

	if (net->rxlen - net->header == net->expected_len) {
	    net->req_status = 0;
	    goto bail_free;
	} else {
	    goto bail;
	}
    }
bail_free:
    clean_file(net);
    ret = net_epollerr(sku);

bail:
    return ret;
}

int net_epollout(struct socku *sku)
{
    struct network *net = net_sku(sku);
    char *buf;
    int ret = 0; //, n = 0;
    //char *host = 0;
#if 1
    if (net->sku_state == CONNECTING) {
	net->sku_state = CONNECTED;
	buf = net->request;
	net->buflen = strlen(net->request);

	if (net->txlen != net->buflen) {
	    ret = send(net->sku_fd, &buf[net->txlen], net->buflen - net->txlen, 0);
	    if (ret <= 0) {
		if (errno == -EAGAIN) {
		    perror("1. send");
		} else {
		    perror("2. send");
		    //close(net->fd);
		    return 0;
		}
	    }
	    net->txlen = ret;
	}
    }
#endif
    return ret;
}

int net_epollerr(struct socku *sku)
{
    int ret = 0;
    struct network *net = net_sku(sku);
    if ((ret = __remove_net(net)) < 0) {
	return ret;
    }

    return 1;
}

int net_epollhup(struct socku *sku)
{
    (void) sku;
    log_to_syslog(LOG_NOTICE, "%s: recvd reset", __FUNCTION__);
    return 0;
}


int
log_to_syslog(int level, const char *format, ...)
{
#define MAX_LOG_LINE 1023
    char log_buf[MAX_LOG_LINE];
    va_list vformat_args;

    snprintf(log_buf, sizeof(log_buf), "%s", format);
    va_start(vformat_args, format);
    vsyslog(level, log_buf, vformat_args);
    va_end(vformat_args);
    return 0;
}

//////////////////////////////////////////////////////////////////
// LOGGER FUNCTIONS
//////////////////////////////////////////////////////////////////

