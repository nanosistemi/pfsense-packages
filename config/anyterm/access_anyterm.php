<?php
/*
    access_anyterm.php
	pfSense package (http://www.pfSense.com)
	Copyright (C) 2009 Scott Ullrich <sullrich@pfsense.org>
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:

    1. Redistributions of source code must retain the above copyright notice,
       this list of conditions and the following disclaimer.

    2. Redistributions in binary form must reproduce the above copyright
       notice, this list of conditions and the following disclaimer in the
       documentation and/or other materials provided with the distribution.

    THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
    INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
    AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
    AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
    OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
    SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
    INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
    CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
    ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
    POSSIBILITY OF SUCH DAMAGE.
*/

require("guiconfig.inc");

if($config['installedpackages']['anyterm']['config'][0]['stunnelport']) {
	$port = $config['installedpackages']['anyterm']['config'][0]['stunnelport'];
	$httpors = "https";
} else {
	$port = $config['installedpackages']['anyterm']['config'][0]['port'];
	$httpors = "http";
}

if($config['installedpackages']['anyterm']['config'][0]['stunnelipaddr'])
	$location = "{$config['installedpackages']['anyterm']['config'][0]['stunnelipaddr']}:{$port}/anyterm.html";
else
	$location = "{$_SERVER['SERVER_ADDR']}:{$port}/anyterm.html";

Header("Location: {$httpors}://{$location}");

?>