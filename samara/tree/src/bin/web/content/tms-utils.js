//
//

/**
 * Sets a Cookie with the given name and value.
 *
 * name       Name of the cookie
 * value      Value of the cookie
 * [expires]  Expiration date of the cookie (default: end of current session)
 * [path]     Path where the cookie is valid (default: path of calling document)
 * [domain]   Domain where the cookie is valid
 *              (default: domain of calling document)
 * [secure]   Boolean value indicating if the cookie transmission requires a
 *              secure transmission
 */
function setCookie(name, value, expires, path, domain, secure)
{
    document.cookie= name + "=" + escape(value) +
        ((expires) ? "; expires=" + expires.toGMTString() : "") +
        ((path) ? "; path=" + path : "") +
        ((domain) ? "; domain=" + domain : "") +
        ((secure) ? "; secure" : "");
}

/**
 * Gets the value of the specified cookie.
 *
 * name  Name of the desired cookie.
 *
 * Returns a string containing value of specified cookie,
 *   or null if cookie does not exist.
 */
function getCookie(name)
{
    var dc = document.cookie;
    var prefix = name + "=";
    var begin = dc.indexOf("; " + prefix);
    if (begin == -1)
    {
        begin = dc.indexOf(prefix);
        if (begin != 0) return null;
    }
    else
    {
        begin += 2;
    }
    var end = document.cookie.indexOf(";", begin);
    if (end == -1)
    {
        end = dc.length;
    }
    return unescape(dc.substring(begin + prefix.length, end));
}

/**
 * Deletes the specified cookie.
 *
 * name      name of the cookie
 * [path]    path of the cookie (must be same as path used to create cookie)
 * [domain]  domain of the cookie (must be same as domain used to create cookie)
 */
function deleteCookie(name, path, domain)
{
    if (getCookie(name))
    {
        document.cookie = name + "=" + 
            ((path) ? "; path=" + path : "") +
            ((domain) ? "; domain=" + domain : "") +
            "; expires=Thu, 01-Jan-70 00:00:01 GMT";
    }
}

function refresh(refresh_cookie)
{
    var count = getCookie(refresh_cookie + "_ar_next_count");
    var interval = getCookie(refresh_cookie + "_ar_interval");

    deleteCookie(refresh_cookie + "_ar_next_count");

    if (count) {
        var cookie_expiry = new Date();
        cookie_expiry.setTime(cookie_expiry.getTime() + (interval * 1000 * 2));

        setCookie(refresh_cookie + "_ar_count", count, cookie_expiry);
    }

    window.location.reload(false);
}

var refresh_timer_id = null;

function startRefresh(refresh_cookie, interval, duration)
{
    var count = duration / interval;

    setCookie(refresh_cookie + "_ar_next_count", count);
    setCookie(refresh_cookie + "_ar_interval", interval);
    if (document.auto_refresh) {
        document.auto_refresh.start.disabled = true;
        document.auto_refresh.stop.disabled = false;
    }
    refresh(refresh_cookie);
}

function stopRefresh(refresh_cookie, reload)
{
    deleteCookie(refresh_cookie + "_ar_count");
    deleteCookie(refresh_cookie + "_ar_next_count");
    deleteCookie(refresh_cookie + "_ar_interval");
    if(document.auto_refresh != null) {
       document.auto_refresh.start.disabled = false;
       document.auto_refresh.stop.disabled = true;
    }
    if (refresh_timer_id) {
        clearTimeout(refresh_timer_id);
        refresh_timer_id = null;
    }
    if (reload) {
        window.location.reload(false);
    }
}

function doRefresh(refresh_cookie)
{
    var count = getCookie(refresh_cookie + "_ar_count");
        
    var from_self = doPageLocation();

    if (!from_self || !count) {
        stopRefresh(refresh_cookie, false);
    }

    deleteCookie(refresh_cookie + "_ar_count");
    count--;

    if (count > 0) {
        if (document.auto_refresh) {
            document.auto_refresh.start.disabled = true;
            document.auto_refresh.stop.disabled = false;
        }

        setCookie(refresh_cookie + "_ar_next_count", count);
        var interval = getCookie(refresh_cookie + "_ar_interval");
        if (interval) {
            var refresh_str = 'refresh(' + '"' + refresh_cookie + '"' + ')';
            setTimeout(refresh_str, interval * 1000);
        }
    } else {
        stopRefresh(refresh_cookie, false);
    }
}

function doPageLocation()
{
    var location = getCookie("page_location");
    var from_self = (location == document.location);

    setCookie("page_location", document.location);

    return from_self;
}

function setClusterNoRedirect(nrv)
{
    setCookie("cluster_master_redirect", nrv);
}

function clearClusterNoRedirect()
{
    deleteCookie("cluster_master_redirect")
}

// browser-independant way to get an xmlHTTP object
function getAjaxRequest () {
    var xmlhttp = false;

    // look for one of the many IE versions of XMLHTTP
    try { xmlhttp = new ActiveXObject("Msxml2.XMLHTTP.5.0");
    } catch (e) {
        try { xmlhttp = new ActiveXObject("Msxml2.XMLHTTP.4.0");
        } catch (e) {
            try { xmlhttp = new ActiveXObject("MSXML2.XMLHTTP.3.0");
            } catch (e) {
                try { xmlhttp = new ActiveXObject("Msxml2.XMLHTTP");
                } catch (e) {
                    try { xmlhttp = new ActiveXObject("Microsoft.XMLHTTP");
                    } catch (E) { 
                        xmlhttp = false;
                    }
                }
            }
        }
    }

    
    if ( !xmlhttp && typeof XMLHttpRequest != 'undefined' )
                {   
            // Try for REAL browser version
                        xmlhttp = new XMLHttpRequest();
                }
    return xmlhttp;
}

/*
 * getSjaxFile(): Returns the file contents of the specified URL
 *                as xmlHTTP object.
 *                This function is synchronous i.e. the caller is blocked
 *                until the request is complete.
 * Usage: getSjaxFile(URL);
 * Example: xmlhttp = getSjaxFile("http://www.tallmaple.com");
 */
function getSjaxFile(url)
{
    if (!url) {
        return false;
    }

    xmlhttp = getAjaxRequest();

    if (xmlhttp) {
        xmlhttp.open("GET", url, false);                             
        xmlhttp.send(null);
        //log ('<br> URL:' + url + '<br>Text:' + xmlhttp.responseText); 
        return xmlhttp;                                         
    }
    return false;
}

var stopAjax = false;
var graph = true;

/*
 * getAjaxText(): make a request to the specified URL, and overwrite the
 *                innerHTML of the specified target with the result.
 */
function getAjaxText (url, target)
{
    var xmlhttp = false;
    var ret = '';

    // Append a random param to the url just to defeat IE browser caching of
    // repeated loads.
        url = url + '&r=' + new Date().getTime();
        if (stopAjax==false)
        {
        // log('preparing to get '+url);
        xmlhttp = getAjaxRequest();
                xmlhttp.open( "GET", url, true );

                xmlhttp.onreadystatechange=function() 
                {
            var obj = document.getElementById(target);

                        if (xmlhttp.readyState==4 && xmlhttp.status == 200) 
                        {
                ret = xmlhttp.responseText;

                // Search for magic string -- see comments in login.tem
                // Failure to find this means we're OK
                if(ret.search(/WEB_USER_AUTH_REQUIRED_AfmiothminilNi/) < 0) {
                    // log ('&nbsp;&nbsp;' + url + ' state 4; target '+ target+'; obj='+obj+'; did not find login'); 
                    obj.innerHTML=ret;
                    // hack hack
                    if(target.substring(0, 12) =='graphStaging') {
                        setTimeout(newGraphLoadedHack, 3000);
                    }
                    if(target.match('state_*')) { log(target + ' <- ' + ret); }
                } else {
                    // we've timed out our login and can't get the text
                    // log ('&nbsp;&nbsp;' + url + ' state 4; did find login; stopping'); 
                    stopAjax = true;
                    // obj.innerHTML='';  there won't BE a target div on the login page
                }
                // log('&nbsp;&nbsp; '+target+' onchange = '+obj.onchange);
                if(obj.onchange != undefined) {
                    // log('&nbsp;&nbsp; '+target+' type is '+typeof(obj.onchange) + ' calling onchange');
                    if(typeof(obj.onchange) == 'string') {
                        // log('&nbsp;&nbsp; '+target+' evaling string');
                        eval(obj.onchange);
                    } else {
                        // log('&nbsp;&nbsp; '+target+' calling direct');
                        obj.onchange();
                    }
                }
            }
        }
                xmlhttp.send(null);
        } else {
        // log('getAjaxRequest: doing nothing because stopAjax is true.  url=' + url);
        document.location = '/admin/launch?script=rh&template=logout&action=logout&unknown=t';
    }
}

/*
 * getSjaxText(): make a request to the specified URL, and overwrite the
 *                innerHTML of the specified target with the result.
 *                While getAjaxText does it asynchronously, this function
 *                does it synchronously i.e. the caller is blocked
 *                until the request is complete.
 * Usage: getSjaxText(URL, HTML_DIV_ID);
 * Example: getSjaxText(
 *   "http://tb6/admin/launch?script=rh&template=get-setup-upgrade-complete",
 *   "upgrade_done");
 */
function getSjaxText(url, target)
{
    var xmlhttp = false;
    var ret = '';
    var obj = document.getElementById(target);

    // Append a random param to the url just to defeat IE browser caching of
    // repeated loads.
    url = url + '&r=' + new Date().getTime();
    if (stopAjax==false)
    {
        // log('preparing to get '+url);
        xmlhttp = getSjaxFile(url);

        if (xmlhttp.readyState==4 && xmlhttp.status == 200) {
            ret = xmlhttp.responseText;

            // Search for magic string -- see comments in login.tem
            // Failure to find this means we're OK
            if(ret.search(/WEB_USER_AUTH_REQUIRED_AfmiothminilNi/) < 0) {
                // log ('&nbsp;&nbsp;' + url + ' state 4; target '+ target+'; obj='+obj+'; did not find login'); 
                obj.innerHTML=ret;
                // hack hack
                if(target.substring(0, 12) =='graphStaging') {
                    setTimeout(newGraphLoadedHack, 3000);
                }
                if(target.match('state_*')) {
                    log(target + ' <- ' + ret);
                }
            } else {
                // we've timed out our login and can't get the text
                // log ('&nbsp;&nbsp;' + url + ' state 4; did find login; stopping'); 
                stopAjax = true;
                // obj.innerHTML='';  there won't BE a target div on the login page
            }
        }
    } else {
        // log('getSjaxFile: doing nothing because stopAjax is true.  url=' + url);
        document.location = '/admin/launch?script=rh&template=logout&action=logout&unknown=t';
    }
}

function checkTime(i)
{
    if (10>i) {
        i='0' + i;
    }
    return i;
}

function mkTimestamp()
{
    var today=new Date();
    var h=today.getHours();
    var m=today.getMinutes();
    var s=today.getSeconds();
    // add a zero in front of single digit numbers
    m=checkTime(m);
    s=checkTime(s);
    return h+':'+m+':'+s;
}

function log(s) {
    //    return; // comment out this line to turn on JS debugging.
    // be sure to add <div id="debug"></div> to the page being debugged
    obj = document.getElementById('debug');
    if(obj != null) {
        obj.innerHTML += mkTimestamp() + ': ' + s + '<br>';
    }
    return;
}

function array_2d(major, minor)
{
    var arr = new Array(major);
    for (var i = 0; i < major; ++i) {
        arr[i] = new Array(minor);
    }

    return arr;
}

//
// this sets the active stylesheet, allowing flipping between styles without
// reloading the page.  To activate, uncomment this block and the block in
// tms-layout.tem around line 138, in <TAGEND TMS-PAGE>
//
// function setActiveStyleSheet(title) {
//    var i, a, main;
//    for(i=0; (a = document.getElementsByTagName("link")[i]); i++) {
//      if(a.getAttribute("rel").indexOf("style") != -1
//        && a.getAttribute("title")) {
//       a.disabled = true;
//       if(a.getAttribute("title") == title) a.disabled = false;
//     }
//   }
// }
