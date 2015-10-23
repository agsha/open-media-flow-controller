defaults
{
        instances               = 60
        log_type                = SYSLOG authpriv
        log_on_success          = HOST PID
        log_on_failure          = HOST
        cps                     = 25 30
}

service telnet
{
        flags			= REUSE @AfFlag@
        socket_type		= stream        
        wait			= no
        user			= root
        server			= /usr/sbin/in.telnetd
        log_on_failure		+= USERID
        disable			= @TelnetDisabled@
}

service ftp
{
        flags                   = @AfFlag@
        socket_type		= stream
        wait			= no
        user			= root
        server			= /usr/sbin/vsftpd
        nice			= 10
        disable			= @FtpDisabled@
}
