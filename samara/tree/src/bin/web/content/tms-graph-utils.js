//
//

/* ============================================================================
 * Utilities used by tms-graph-utils.tem.
 */

/* ------------------------------------------------------------------------- */
function ajaxGraphInit(maxGroups, maxGraphs)
{
    ajaxUsed =            array_2d(maxGroups, maxGraphs);
    ajaxWidthStyle =      array_2d(maxGroups, maxGraphs);
    ajaxWidthParam =      array_2d(maxGroups, maxGraphs);
    ajaxWidthDiff =       array_2d(maxGroups, maxGraphs);
    ajaxClearDataUrls =   array_2d(maxGroups, maxGraphs);
    ajaxDestUrls =        array_2d(maxGroups, maxGraphs);
    ajaxLastUpdateGraph = array_2d(maxGroups, maxGraphs);

    ajaxUpdate =          new Array(maxGroups);
    ajaxTimeout =         new Array(maxGroups);
    ajaxRefreshInterval = new Array(maxGroups);
    ajaxLastUpdateGroup = new Array(maxGroups);

    for (var n = 0; n != maxGroups; ++n) {
        for (var i = 0; i != maxGraphs; ++i) {
            ajaxUsed[n][i] = false;
            ajaxWidthStyle[n][i] = 'percent';
            ajaxWidthParam[n][i] = 100;
            ajaxWidthDiff[n][i] = 80;
            ajaxDestUrls[n][i] = null;
            ajaxLastUpdateGraph[n][i] = 0;
        }

        ajaxUpdate[n] = false;
        ajaxTimeout[n] = null;
        ajaxRefreshInterval[n] = 10;
        ajaxClearDataUrls[n] = "";
        ajaxLastUpdateGroup[n] = 0;
    }
}


/* ------------------------------------------------------------------------- */
/* We want to get the "physical" width of the window, NOT that of the
 * "document" (which includes the whole scrollable area).  Unfortunately,
 * browsers are inconsistent about which variables you can get this out of.
 */
function getWindowWidth()
{
  var windowWidth = 0;

  if (typeof(window.innerWidth) == 'number') {
    // All non-IE browsers
    windowWidth = window.innerWidth;
  }
  else if (document.documentElement && 
           document.documentElement.clientWidth) {
    // Some flavors of IE give us the right number from here...
    windowWidth = document.documentElement.clientWidth;
  }
  else {
    // ...and others from here.
    windowWidth = document.body.clientWidth;
  }

  return windowWidth;
}


/* ------------------------------------------------------------------------- */
function graphCalcWidth(grp, idx)
{
    var sidebarWidth = 0, newWidth = 0;
    var sidebarDiv = document.getElementById('pageSidebar');
    var graph = document.getElementById('displayImg' + grp + '_' + idx);

    switch (ajaxWidthStyle[grp][idx]) {
    default:
        /*
         * Treat this the same as 'percent' so fall through...
         */

    case 'percent':
        /*
         * As we calculate the amount of space available, use the
         * "width differential", which takes into account the extra space
         * needed for the vertical legend, margins, etc.
         */
        sidebarWidth = 0;
        if (sidebarDiv) {
            sidebarWidth = sidebarDiv.offsetLeft + sidebarDiv.offsetWidth;
        }
        newWidth = getWindowWidth() - sidebarWidth -
                   ajaxWidthDiff[grp][idx];
        if (ajaxWidthParam[grp][idx] != 100) {
            newWidth = Math.round(newWidth * ajaxWidthParam[grp][idx] / 100);
        }

        // Clamp at a minimum width.
        if (!(newWidth > 100)) {
            newWidth = 100;
        }
        break;

    case 'fixed':
        newWidth = ajaxWidthParam[grp][idx];
        break;
    }

    return newWidth;
}


/* ------------------------------------------------------------------------- */
/* Handle a window resize.  Note that we are NOT called with a group or
 * index; we just resize everything.
 */
function resizeHandler()
{
    for (var grp = 0; grp != maxGroups; ++grp) {
        groupRefresh(grp, true);
    }
}


/* ------------------------------------------------------------------------- */
function pauseAjax(grp)
{
    if (ajaxUpdate[grp]) {
        ajaxUpdate[grp] = false;
        o = document.getElementById('updateTime' + grp);
        if (o) {
            o.innerHTML += ' (Paused)';
        }
    }

    if (ajaxTimeout[grp] != null) {
        clearTimeout(ajaxTimeout[grp]);
        ajaxTimeout[grp] = null;
    }

    gc = document.getElementById('ajaxPauseButton' + grp);
    gc.className = 'ajaxButtonDisabled';
    gc = document.getElementById('ajaxResumeButton' + grp);
    gc.className = 'ajaxButton';
}


/* ------------------------------------------------------------------------- */
function resumeAjax(grp)
{
    if (!(ajaxUpdate[grp])) {
        ajaxUpdate[grp] = true;
        gc = document.getElementById('ajaxPauseButton' + grp);
        gc.className = 'ajaxButton';
        gc = document.getElementById('ajaxResumeButton' + grp);
        gc.className = 'ajaxButtonDisabled';
        groupRefresh(grp, true);
    }
}


/* ------------------------------------------------------------------------- */
function clearData(grp)
{
    window.location.assign(ajaxClearDataUrls[grp]);
}


/* ------------------------------------------------------------------------- */
/* Call when a graph is updated.  We'll decide whether it's time to update
 * the group's timestamp, and do it if so.
 */
function maybeUpdateGroupTimestamp(grp, idx)
{
    var unUpdated = false;
    var date = new Date();        // Will hold current date/time.
    var now = date.getTime();

    ajaxLastUpdateGraph[grp][idx] = now;

    for (var testIdx = 0; testIdx != maxGraphs; ++testIdx) {
        if (ajaxUsed[grp][testIdx] && 
            ajaxLastUpdateGraph[grp][testIdx] <= ajaxLastUpdateGroup[grp]) {
            unUpdated = true;
        }
    }

    if (unUpdated) {
        /*
         * There is still at least one graph in this group waiting
         * to be updated
         */
    }
    else {
        timestamp = document.getElementById('timeStamp' + grp + '_' + idx);
        var o2 = document.getElementById('updateTime' + grp);
        if (o2) {
            if (ajaxUpdate[grp]) {
                o2.innerHTML = timestamp.innerHTML;
            }
            else {
                o2.innerHTML = timestamp.innerHTML + ' (Paused)';
            }
        }
        ajaxLastUpdateGroup[grp] = now;
    }
}


/* ------------------------------------------------------------------------- */
function newGraphLoaded(o, grp, idx)
{
    var newGraph = document.getElementById('displayImg' + grp + '_' + idx);
    if (newGraph && o && o.name == ('stagedImage' + grp + '_' + idx)) {
        // copy graph to display div
        newGraph.src = o.src

        maybeUpdateGroupTimestamp(grp, idx);
    }

    var hasLoadGraphError = document.getElementById('loadGraphError' +
                                                    grp + '_' + idx);
    if (!hasLoadGraphError) {
        var gmsg = document.getElementById('graphMsg' + grp + '_' + idx);
        if (gmsg) {
            gmsg.innerHTML = '';
        }
    }
}


/* ------------------------------------------------------------------------- */
function newGraphLoadedHack()
{
    // Nothing to do
}


/* ------------------------------------------------------------------------- */
function newGraphLoadedFailed(o, grp, idx)
{
     newGraphLoaded(o, grp, idx);
     // update msg
     var gmsg = document.getElementById('graphMsg' + grp + '_' + idx);
     if (gmsg) {
         gmsg.innerHTML = '<p>No data available for graph at present</p>';
     }
}


/* ------------------------------------------------------------------------- */
/* Refresh the graph, and reschedule ourselves to run after another refresh
 * interval has passed.
 */
function graphRefresh(grp, idx)
{
    var url = ajaxDestUrls[grp][idx];
    var newWidth = graphCalcWidth(grp, idx);
    var fixedUrl = url + '&var_width=' + newWidth;

    // Need to replace HTML entities before AJAX can load the URL
    fixedUrl = fixedUrl.replace(/&amp;/g, '&');

    // Fetch the URL, and update the target DIV with it.
    getAjaxText (fixedUrl, 'graphStaging' + grp + '_' + idx);
}


/* ------------------------------------------------------------------------- */
/* Refresh all graphs in the group, and if auto-refresh is specified for
 * this group, reschedule ourselves to run again, after another refresh
 * interval has passed.
 */
function groupRefresh(grp, refreshGraphs)
{
    if (refreshGraphs == true) {
        for (var idx = 0; idx < maxGraphs; ++idx) {
            if (ajaxUsed[grp][idx]) {
                graphRefresh(grp, idx);
            }
        }
    }

    if (ajaxTimeout[grp] != null) {
        clearTimeout(ajaxTimeout[grp]);
    }

    if (ajaxUpdate[grp] == true) {
        ajaxTimeout[grp] = setTimeout('groupRefresh(' + grp + ',true)', 
                                      ajaxRefreshInterval[grp]);
    }
}


/* ------------------------------------------------------------------------- */
function ajaxWriteControls(grp, clearDataDisabled)
{
    document.write('<p>Updated: <span id="updateTime' + grp + '" ' + 
                   'style="display:inline;"></span></p>');

    document.write('<p><table width="100%">' +
                   '<td align="left"> ' +
                   '<span id="ajaxPauseButton' + grp + '" ' +
                   'class="ajaxButton" onclick="pauseAjax(' + grp + ')">' +
                   'Pause</span>');

    document.write('<span id="ajaxResumeButton' + grp + '" ' +
                   'class="ajaxButtonDisabled" ' +
                   'onclick="resumeAjax(' + grp + ')">Resume</span>');
    if (clearDataDisabled) {
        ajaxClearButtonSettings = 'class="ajaxButtonDisabled" '
    }
    else {
        ajaxClearButtonSettings= 'class="ajaxButton" onclick="clearData(' +
                                 grp + ')"'
    }

    if (ajaxClearDataUrls[grp] != "") {
      document.write('<td align="right"> ' +
                     '<span id="ajaxClearButton' + grp + '" ' +
                     ajaxClearButtonSettings + '>' +
                     'Clear&nbsp;Data</span>');
    }

    document.write('</table></p>');
}
