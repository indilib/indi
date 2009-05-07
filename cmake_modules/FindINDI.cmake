



<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN"
"http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html xmlns="http://www.w3.org/1999/xhtml" xml:lang="en" lang="en">
<!-- ViewVC - http://viewvc.org/
by Greg Stein - mailto:gstein@lyra.org -->
<!--
~ SourceForge.net: Create, Participate, Evaluate
~ Copyright (c) 1999-2008 SourceForge, Inc. All rights reserved.
-->
<head>
<meta http-equiv="content-type" content="text/html; charset=utf-8" />
<meta name="description" content="The world's largest development and download repository of Open Source code and applications" />
<meta name="keywords" content="Open Source, Development, Developers, Projects, Downloads, OSTG, VA Software, SF.net, SourceForge" />
<title>SourceForge.net Repository - [indi] Log of /trunk/indi-sbig/cmake_modules/FindINDI.cmake</title>
<meta name="generator" content="ViewVC 1.0.5" />
<link rel="stylesheet" type="text/css" href="http://static.sourceforge.net/css/phoneix/style.php?secure=0&amp;20080417-1657" media="all" />
<link rel="stylesheet" type="text/css" href="http://static.sourceforge.net/css/phoneix/table.php?secure=0&amp;20080417-1657" media="all" />
<link rel="stylesheet" type="text/css" href="/viewvc-static/styles.css" media="all" />
<style type="text/css">
#breadcrumb {margin: 0; padding: 0;}
#breadcrumb li {list-style: none; padding: 0 .2em 0 0; display: inline; }
#breadcrumb li:before {color: #aaa; content: "/"}
#breadcrumb li:first-child:before {content: ""}
h2 {margin: 0 0 -.5em;}
table th.vc_header {background-color: #fff;}
table th.vc_header_sort a {text-decoration: none; color: #000 !important;}
hr {background: #ccc; border: none;}
/* grr ie */
#bd .titlebar, table tr th {_background: url("http://c.fsdn.com/sf/images/phoneix/gloss_ieblows.png");}
table tbody tr td, .yui-gf.title .yui-u.first h2 {_background-image: none;}
#bd { _background: #fff url("http://c.fsdn.com/sf/images/phoneix/grad_ieblows.png") repeat-x 0 6px;}
#hd .yui-u.first h1 { _background: transparent; _filter:progid:DXImageTransform.Microsoft.AlphaImageLoader (src='http://c.fsdn.com/sf/images/phoneix/sf_phoneix.png',sizingMethod='');}
#hd {_background: none;}
</style>
<!-- BEGIN: AdSolution-Tag 4.2: Global-Code [PLACE IN HTML-HEAD-AREA!] -->
<!-- DoubleClick Random Number -->
<script language="JavaScript" type="text/javascript">
dfp_ord=Math.random()*10000000000000000;
dfp_tile = 1;
</script>
<!-- End DoubleClick Random Number -->
<script type="text/javascript">
var google_page_url = 'http://sourceforge.net/projects/indi/';
var sourceforge_project_name = 'indi';
var sourceforge_project_description = '';
</script>
<!-- END: AdSolution-Tag 4.2: Global-Code -->
<!-- End OSDN NavBar gid: -->
<!-- after META tags -->
<script type="text/javascript">
var gaJsHost = (("https:" == document.location.protocol) ? "https://ssl." : "http://www.");
document.write(unescape("%3Cscript src='" + gaJsHost + "google-analytics.com/ga.js' type='text/javascript'%3E%3C/script%3E"));
</script>
</head>
<body>
<div id="doc3" class="yui-t6 login">
<div id="hd">
<div class="yui-gf">
<div class="yui-u first">
<h1>
<a href="http://sourceforge.net/" title="">
SourceForge.net
</a>
</h1>
<ul class="jump">
<li>
<a href="#content">
Jump to main content</a>
</li>
</ul>
</div>
<div class="yui-u">
<a href="http://sourceforge.net/community/">
Community</a><a href="http://jobs.sourceforge.net">Jobs</a><a href="http://sourceforge.net/support/getsupport.php" title="Get help and support on SourceForge.net">Help
</a>
<form action="http://sourceforge.net/search/" method="get" name="searchform">
<input type="hidden" name="type_of_search" value="soft" />
<input type="text" value="" name="words" tabindex="1" />
<input type="submit" value="Search" tabindex="0" />
</form>
</div>
</div>
</div>
<div id="bd">
<!-- begin content -->
<a name="content">
</a>
<!-- Breadcrumb Trail -->
<br />

<ul id="breadcrumb">
<li class="begin"> <a href="http://sourceforge.net/">SF.net</a></li>
<li> <a href="http://sourceforge.net/softwaremap/">Projects</a></li>
<li> SCM Repositories</li>

<li>
<a href="/viewvc/indi/?pathrev=200">

indi</a>
</li>

<li>
<a href="/viewvc/indi/trunk/?pathrev=200">

trunk</a>
</li>

<li>
<a href="/viewvc/indi/trunk/indi-sbig/?pathrev=200">

indi-sbig</a>
</li>

<li>
<a href="/viewvc/indi/trunk/indi-sbig/cmake_modules/?pathrev=200">

cmake_modules</a>
</li>

<li>


FindINDI.cmake
</li>

</ul>

<div id="yui-main">
<div class="yui-b">
<h1>
SCM Repositories -
<a href="http://sourceforge.net/projects/indi">indi</a>
</h1>

<p style="margin:0;">

<a href="/viewvc/indi/trunk/indi-sbig/cmake_modules/?pathrev=200"><img src="/viewvc-static/images/back_small.png" width="16" height="16" alt="Parent Directory" /> Parent Directory</a>




</p>

<hr />
<table class="auto track" style="width: 30em;">





<tr>
<td>Sticky Revision:</td>
<td><form method="get" action="/viewvc/indi" style="display: inline">
<div style="display: inline">
<input type="hidden" name="orig_pathrev" value="200" /><input type="hidden" name="orig_pathtype" value="FILE" /><input type="hidden" name="orig_view" value="log" /><input type="hidden" name="orig_path" value="trunk/indi-sbig/cmake_modules/FindINDI.cmake" /><input type="hidden" name="view" value="redirect_pathrev" />

<input type="text" name="pathrev" value="200" size="6"/>

<input type="submit" value="Set" />
</div>
</form>

<form method="get" action="/viewvc/indi/trunk/indi-sbig/cmake_modules/FindINDI.cmake" style="display: inline">
<div style="display: inline">
<input type="hidden" name="view" value="log" /><input type="hidden" name="pathrev" value="205" />

<input type="submit" value="Set to 205" />
(<i>Current path doesn't exist after revision <strong>205</strong></i>)

</div>
</form>

</td>
</tr>
</table>
</div>
</div>
<div class="yui-b">
<div id="fad83">
<!-- DoubleClick Ad Tag -->
<script type="text/javascript">
//<![CDATA[
document.write('<script src="http://ad.doubleclick.net/adj/ostg.sourceforge/pg_viewvc_p88_shortrec;pg=viewvc;tile='+dfp_tile+';tpc=indi;ord='+dfp_ord+'?" type="text/javascript"><\/script>');
dfp_tile++;
//]]>
</script>
<!-- End DoubleClick Ad Tag -->
</div>
</div>
<br style="clear:both; margin-bottom: 1em; display: block;" />
 








<div>
<hr />

<a name="rev186"></a>


Revision <a href="/viewvc/indi?view=rev&amp;revision=186"><strong>186</strong></a> -

(<a href="/viewvc/indi/trunk/indi-sbig/cmake_modules/FindINDI.cmake?revision=186&amp;view=markup&amp;pathrev=200">view</a>)

(<a href="/viewvc/indi/trunk/indi-sbig/cmake_modules/FindINDI.cmake?revision=186&amp;pathrev=200">download</a>)

(<a href="/viewvc/indi/trunk/indi-sbig/cmake_modules/FindINDI.cmake?annotate=186&amp;pathrev=200">annotate</a>)



- <a href="/viewvc/indi/trunk/indi-sbig/cmake_modules/FindINDI.cmake?view=log&amp;r1=186&amp;pathrev=200">[select for diffs]</a>




<br />

Modified

<em>Mon Oct 27 05:08:28 2008 UTC</em> (6 months, 1 week ago) by <em>slovin</em>









<br />File length: 1822 byte(s)






<br />Diff to <a href="/viewvc/indi/trunk/indi-sbig/cmake_modules/FindINDI.cmake?r1=177&amp;r2=186&amp;pathrev=200">previous 177</a>







<pre class="vc_log">Renaming to indi-sbig which is the official release name</pre>
</div>



<div>
<hr />

<a name="rev177"></a>


Revision <a href="/viewvc/indi?view=rev&amp;revision=177"><strong>177</strong></a> -

(<a href="/viewvc/indi/trunk/indi-sbig/cmake_modules/FindINDI.cmake?revision=177&amp;view=markup&amp;pathrev=200">view</a>)

(<a href="/viewvc/indi/trunk/indi-sbig/cmake_modules/FindINDI.cmake?revision=177&amp;pathrev=200">download</a>)

(<a href="/viewvc/indi/trunk/indi-sbig/cmake_modules/FindINDI.cmake?annotate=177&amp;pathrev=200">annotate</a>)



- <a href="/viewvc/indi/trunk/indi-sbig/cmake_modules/FindINDI.cmake?view=log&amp;r1=177&amp;pathrev=200">[select for diffs]</a>




<br />

Modified

<em>Sat Oct 25 21:23:35 2008 UTC</em> (6 months, 1 week ago) by <em>slovin</em>

<br />Original Path: <a href="/viewvc/indi/trunk/indi_sbig/cmake_modules/FindINDI.cmake?view=log&amp;pathrev=177"><em>trunk/indi_sbig/cmake_modules/FindINDI.cmake</em></a>









<br />File length: 1822 byte(s)






<br />Diff to <a href="/viewvc/indi/trunk/indi-sbig/cmake_modules/FindINDI.cmake?r1=163&amp;r2=177&amp;pathrev=200">previous 163</a>







<pre class="vc_log">build updates</pre>
</div>



<div>
<hr />

<a name="rev163"></a>


Revision <a href="/viewvc/indi?view=rev&amp;revision=163"><strong>163</strong></a> -

(<a href="/viewvc/indi/trunk/indi-sbig/cmake_modules/FindINDI.cmake?revision=163&amp;view=markup&amp;pathrev=200">view</a>)

(<a href="/viewvc/indi/trunk/indi-sbig/cmake_modules/FindINDI.cmake?revision=163&amp;pathrev=200">download</a>)

(<a href="/viewvc/indi/trunk/indi-sbig/cmake_modules/FindINDI.cmake?annotate=163&amp;pathrev=200">annotate</a>)



- <a href="/viewvc/indi/trunk/indi-sbig/cmake_modules/FindINDI.cmake?view=log&amp;r1=163&amp;pathrev=200">[select for diffs]</a>




<br />

Modified

<em>Mon Oct 20 13:49:41 2008 UTC</em> (6 months, 2 weeks ago) by <em>slovin</em>

<br />Original Path: <a href="/viewvc/indi/trunk/indi_sbig/cmake_modules/FindINDI.cmake?view=log&amp;pathrev=163"><em>trunk/indi_sbig/cmake_modules/FindINDI.cmake</em></a>









<br />File length: 1507 byte(s)






<br />Diff to <a href="/viewvc/indi/trunk/indi-sbig/cmake_modules/FindINDI.cmake?r1=129&amp;r2=163&amp;pathrev=200">previous 129</a>







<pre class="vc_log">Build updates</pre>
</div>



<div>
<hr />

<a name="rev129"></a>


Revision <a href="/viewvc/indi?view=rev&amp;revision=129"><strong>129</strong></a> -

(<a href="/viewvc/indi/trunk/indi-sbig/cmake_modules/FindINDI.cmake?revision=129&amp;view=markup&amp;pathrev=200">view</a>)

(<a href="/viewvc/indi/trunk/indi-sbig/cmake_modules/FindINDI.cmake?revision=129&amp;pathrev=200">download</a>)

(<a href="/viewvc/indi/trunk/indi-sbig/cmake_modules/FindINDI.cmake?annotate=129&amp;pathrev=200">annotate</a>)



- <a href="/viewvc/indi/trunk/indi-sbig/cmake_modules/FindINDI.cmake?view=log&amp;r1=129&amp;pathrev=200">[select for diffs]</a>




<br />

Added

<em>Fri Oct  3 12:29:26 2008 UTC</em> (7 months ago) by <em>slovin</em>

<br />Original Path: <a href="/viewvc/indi/trunk/indi_sbig/cmake_modules/FindINDI.cmake?view=log&amp;pathrev=129"><em>trunk/indi_sbig/cmake_modules/FindINDI.cmake</em></a>







<br />File length: 1446 byte(s)










<pre class="vc_log">Initial import</pre>
</div>

 



 <hr />
<p><a name="diff"></a>
This form allows you to request diffs between any two revisions of this file.
For each of the two "sides" of the diff,

enter a numeric revision.

</p>
<form method="get" action="/viewvc/indi/trunk/indi-sbig/cmake_modules/FindINDI.cmake" id="diff_select">
<table cellpadding="2" cellspacing="0" class="auto">
<tr>
<td>&nbsp;</td>
<td>
<input type="hidden" name="pathrev" value="200" /><input type="hidden" name="view" value="diff" />
Diffs between

<input type="text" size="12" name="r1"
value="186" />

and

<input type="text" size="12" name="r2" value="129" />

</td>
</tr>
<tr>
<td>&nbsp;</td>
<td>
Type of Diff should be a
<select name="diff_format" onchange="submit()">
<option value="h" selected="selected">Colored Diff</option>
<option value="l" >Long Colored Diff</option>
<option value="u" >Unidiff</option>
<option value="c" >Context Diff</option>
<option value="s" >Side by Side</option>
</select>
<input type="submit" value=" Get Diffs " />
</td>
</tr>
</table>
</form>


<form method="get" action="/viewvc/indi/trunk/indi-sbig/cmake_modules/FindINDI.cmake">
<div>
<hr />
<a name="logsort"></a>
<input type="hidden" name="view" value="log" /><input type="hidden" name="pathrev" value="200" />
Sort log by:
<select name="logsort" onchange="submit()">
<option value="cvs" >Not sorted</option>
<option value="date" selected="selected">Commit date</option>
<option value="rev" >Revision</option>
</select>
<input type="submit" value=" Sort " />
</div>
</form>


</td></tr></table>
<hr />
<div class="yui-g">
<div class="yui-u first">
Powered by <a href="http://viewvc.tigris.org/">ViewVC 1.0.5</a>
</div>
<div class="yui-u" style="text-align: right">
<a href="/viewvc-static/help_log.html">ViewVC Help</a></strong>
</div>
</div>
</div>
<div id="ft">
<div class="yui-g divider">
<div class="yui-u first copyright">
&copy;Copyright 1999-2008 -
<a href="http://sourceforge.com" title="Network which provides and promotes Open Source software downloads, development, discussion and news.">SourceForge</a>, Inc., All Rights Reserved
</div>
<div class="yui-u">
<a href="http://sourceforge.net/community/forum/forum.php?id=12">Feedback</a> <a href="http://sourceforge.net/tos/tos.php">Legal</a> <a href="http://sourceforge.net/support">Help</a>
</div>
</div>
</div>
</div>
<script type="text/javascript">
var pageTracker = _gat._getTracker("UA-32013-37");
pageTracker._setDomainName(".sourceforge.net");
pageTracker._trackPageview();
</script>
</body>
</html>


