ServerRoot "@ServerRoot@"
ServerName @/system/hostname@

@ProdDevEnv@

PidFile @rel_pidfile@

Timeout 3600
KeepAlive On
MaxKeepAliveRequests 100
KeepAliveTimeout 15

StartServers            3
MinSpareServers         2
MaxSpareServers        10
MaxClients            256
MaxRequestsPerChild     0

@HttpListen@
@HttpsListen@

TraceEnable Off

Include conf.d/*.conf

User @ApacheUser@
Group @ApacheGroup@
UseCanonicalName Off
SSLSessionCache none

DocumentRoot "@ServerRoot@/content"
CoreDumpDirectory "@WorkingDir@"

<Directory />
    Options FollowSymLinks
    AllowOverride None
    Order Deny,Allow
    Deny from all 
</Directory>

<Directory "@ServerRoot@/content">
    Options FollowSymLinks
    AllowOverride None
    Order Allow,Deny
    Allow from all
</Directory>

<Directory "@ServerRoot@/content/graphs">
    Options None
    AllowOverride None
    Header set Cache-Control no-cache
</Directory>

<Directory "@ServerRoot@/content/sysimages">
    Options SymLinksIfOwnerMatch
    AllowOverride None
</Directory>

DirectoryIndex index.html
TypesConfig @ServerRoot@/conf/mime.types
DefaultType text/plain

<IfModule mod_mime_magic.c>
    MIMEMagicFile @ServerRoot@/conf/magic
</IfModule>

HostnameLookups Off
LogLevel notice
ErrorLog @rel_logfiledir@/web_error_log
TransferLog @rel_logfiledir@/web_access_log

ServerTokens Prod
ServerSignature On

ScriptAlias /admin/ "@ServerRoot@/cgi-bin/"

<Directory "@ServerRoot@/cgi-bin">
    AllowOverride None
    Options None
    Order Allow,Deny
    Allow from all
</Directory>

RewriteEngine On

# Note there is a duplicate of this below
@XmlGwComment@RewriteRule ^/xtree(.*)$ /admin/launch?script=xg&name=$1 [PT]

#IndexOptions FancyIndexing VersionSort

AddLanguage da .dk
AddLanguage nl .nl
AddLanguage en .en
AddLanguage et .et
AddLanguage fr .fr
AddLanguage de .de
AddLanguage he .he
AddLanguage el .el
AddLanguage it .it
AddLanguage ja .ja
AddLanguage pl .po
AddLanguage ko .ko
AddLanguage pt .pt
AddLanguage nn .nn
AddLanguage no .no
AddLanguage pt-br .pt-br
AddLanguage ltz .ltz
AddLanguage ca .ca
AddLanguage es .es
AddLanguage sv .sv
AddLanguage cs .cz .cs
AddLanguage ru .ru
AddLanguage zh-CN .zh-cn
AddLanguage zh-TW .zh-tw
AddLanguage hr .hr

AddDefaultCharset @default_charset@

AddCharset ISO-8859-1  .iso8859-1  .latin1
AddCharset ISO-8859-2  .iso8859-2  .latin2 .cen
AddCharset ISO-8859-3  .iso8859-3  .latin3
AddCharset ISO-8859-4  .iso8859-4  .latin4
AddCharset ISO-8859-5  .iso8859-5  .latin5 .cyr .iso-ru
AddCharset ISO-8859-6  .iso8859-6  .latin6 .arb
AddCharset ISO-8859-7  .iso8859-7  .latin7 .grk
AddCharset ISO-8859-8  .iso8859-8  .latin8 .heb
AddCharset ISO-8859-9  .iso8859-9  .latin9 .trk
AddCharset ISO-2022-JP .iso2022-jp .jis
AddCharset ISO-2022-KR .iso2022-kr .kis
AddCharset ISO-2022-CN .iso2022-cn .cis
AddCharset Big5        .Big5       .big5
# For russian, more than one charset is used (depends on client, mostly):
AddCharset WINDOWS-1251 .cp-1251   .win-1251
AddCharset CP866       .cp866
AddCharset KOI8-r      .koi8-r .koi8-ru
AddCharset KOI8-ru     .koi8-uk .ua
AddCharset ISO-10646-UCS-2 .ucs2
AddCharset ISO-10646-UCS-4 .ucs4
AddCharset UTF-8       .utf8
AddCharset GB2312      .gb2312 .gb 
AddCharset utf-7       .utf7
AddCharset utf-8       .utf8
AddCharset big5        .big5 .b5
AddCharset EUC-TW      .euc-tw
AddCharset EUC-JP      .euc-jp
AddCharset EUC-KR      .euc-kr
AddCharset shift_jis   .sjis

AddType application/x-tar .tgz
AddType image/x-icon .ico

AddHandler type-map var

<VirtualHost _default_:@HttpsPort@>

LogLevel notice
ErrorLog @rel_logfiledir@/web_error_log
TransferLog @rel_logfiledir@/web_access_log

SSLEngine on
SSLCertificateFile @web_ssl_cert_path@/server.crt
SSLCertificateKeyFile @web_ssl_cert_path@/server.key
SSLProtocol -ALL +SSLv3 +TLSv1
SSLCipherSuite ALL:!aNULL:!ADH:!eNULL:!LOW:!EXP:RC4+RSA:+HIGH:+MEDIUM
SSLOptions +StdEnvVars

RewriteEngine On

# Note this is a duplicate of above
@XmlGwComment@RewriteRule ^/xtree(.*)$ /admin/launch?script=xg&name=$1 [PT]

@CustVirtualHost@

</VirtualHost>

@SecureRedirectComment@RewriteCond %{SERVER_PORT} !^@HttpsPort@$
@SecureRedirectComment@RewriteRule ^.*$ https://%{SERVER_NAME}:@HttpsPort@%{REQUEST_URI} [L,R]
