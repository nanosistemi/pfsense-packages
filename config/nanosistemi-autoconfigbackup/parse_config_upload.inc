<?php

if(file_exists("/usr/local/pkg/nanosistemi-autoconfigbackup.inc")) {
	require_once("/usr/local/pkg/nanosistemi-autoconfigbackup.inc");
	upload_config();
}

?>
