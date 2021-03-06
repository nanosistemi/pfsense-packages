<?php
/*
	imspector.inc
	part of pfSense (http://www.pfsense.com/)
	Copyright (C) 2011 Scott Ullrich <sullrich@gmail.com>.
	Copyright (C) 2011 Bill Marquette <billm@gmail.com>.
	Copyright (C) 2007 Ryan Wagoner <rswagoner@gmail.com>.
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

	require_once("config.inc");
	require_once("functions.inc");

	/* IMSpector */

	define('IMSPECTOR_RCFILE', '/usr/local/etc/rc.d/imspector.sh');
	define('IMSPECTOR_ETC', '/usr/local/etc/imspector');
	define('IMSPECTOR_CONFIG', IMSPECTOR_ETC . '/imspector.conf');

	function imspector_notice ($msg) { syslog(LOG_NOTICE, "imspector: {$msg}"); }
	function imspector_warn ($msg) { syslog(LOG_WARNING, "imspector: {$msg}"); }

	function imspector_action ($action) {
		if (file_exists(IMSPECTOR_RCFILE))
			mwexec(IMSPECTOR_RCFILE.' '.$action);
	}	

	function imspector_running () {
		if((int)exec('pgrep imspector | wc -l') > 0)
			return true;
		return false;
	}

	function write_imspector_config($file, $text) {
		$conf = fopen($file, 'w');
		if(!$conf) {
			imspector_warn("Could not open {$file} for writing.");
			exit;
		}
		fwrite($conf, $text);
		fclose($conf);	
	}	

	function imspector_pf_rdr($iface, $port) {
		return "rdr pass on {$iface} inet proto tcp from any to any port = {$port} -> 127.0.0.1 port 16667\n";
	}

	function imspector_pf_rule($iface, $port) {
		return "pass in quick on {$iface} inet proto tcp from any to any port {$port} keep state\n";
	}		

	function imspector_proto_to_port ($proto)
	{
		switch ($proto) {
			case 'gadu-gadu':
				return 8074;
			case 'jabber':
				return 5222;
			case 'jabber-ssl':
				return 5223;
			case 'msn':
				return 1863;
			case 'icq':
				return 5190;
			case 'yahoo':
				return 5050;
			case 'irc':
				return 6667;
			default:
				return null;
		}
	}

	function validate_form_imspector($post, $input_errors) {
		if($post['iface_array'])
			foreach($post['iface_array'] as $iface)
				if($iface == 'wan')
					$input_errors[] = 'It is a security risk to specify WAN in the \'Interface\' field';
	}

	function deinstall_package_imspector() {
		imspector_action('stop');

		@unlink(IMSPECTOR_RCFILE);
		@unlink(IMSPECTOR_CONFIG);
		@unlink(IMSPECTOR_ETC . '/badwords_custom.txt');
		@unlink(IMSPECTOR_ETC . '/acl_blacklist.txt');
		@unlink(IMSPECTOR_ETC . '/acl_whitelist.txt');

		//exec('pkg_delete imspector-0.4');
	}

	function imspector_generate_rules($type) {
		
		$rules = "";
		switch ($type) {
		case 'rdr':
		case 'nat':
			$rules = "# IMSpector rdr anchor\n";
			$rules .= "rdr-anchor \"imspector\"\n";
			break;
		case 'rule':
			$rules = "# IMSpector \n";
			$rules .= "anchor \"miniupnpd\"\n";
			break;
		}

		return $rules;
	}

	function sync_package_imspector() {
		global $config;
		global $input_errors;

		config_lock();

		$imspector_config = $config['installedpackages']['imspector']['config'][0];

		/* remove existing rules */
		exec('/sbin/pfctl -a imspector -Fr');
		exec('/sbin/pfctl -a imspector -Fn');	

		$ifaces_active = '';		

		if($imspector_config['enable'] && $imspector_config['proto_array'])
			$proto_array = explode(',', $imspector_config['proto_array']);

		if($imspector_config['enable'] && $imspector_config['iface_array'])
			$iface_array = explode(',', $imspector_config['iface_array']);

		if($iface_array && $proto_array) {
			foreach($iface_array as $iface) {
				$if = convert_friendly_interface_to_real_interface_name($iface);
				/* above function returns iface if fail */
				if($if!=$iface) {
					$addr = find_interface_ip($if);
					/* non enabled interfaces are displayed in list on imspector settings page */
					/* check that the interface has an ip address before adding parameters */
					if($addr) {
						foreach($proto_array as $proto) {
							if(imspector_proto_to_port($proto))	{
								/* we can use rdr pass to auto create the filter rule */
								$pf_rules .= imspector_pf_rdr($if,imspector_proto_to_port($proto));
							}
						}
						if(!$ifaces_active)
							$ifaces_active = "{$iface}";
						else
							$ifaces_active .= ", {$iface}";
					} else {
						imspector_warn("Interface {$iface} has no ip address, ignoring");
					}
				} else {
					imspector_warn("Could not resolve real interface for {$iface}");
				}
			}

			if($pf_rules) {
				exec("echo \"{$pf_rules}\" | /sbin/pfctl -a imspector -f -");

				conf_mount_rw();

				/* generate configuration files */

				$conf['plugin_dir'] = '/usr/local/lib/imspector';

				foreach($proto_array as $proto)
					$conf[$proto . '_protocol'] = 'on';
					
				if($imspector_config['log_file']) {
					@mkdir('/var/imspector');
					$conf['file_logging_dir'] = '/var/imspector';
				}
					
				if($imspector_config['log_mysql']) {
					$conf['mysql_server'] = $imspector_config['mysql_server'];
					$conf['mysql_database'] = $imspector_config['mysql_database'];
					$conf['mysql_username'] = $imspector_config['mysql_username'];
					$conf['mysql_password'] = $imspector_config['mysql_password'];
				}

				if($imspector_config['filter_badwords']) {
					if(!empty($imspector_config["badwords_list"])) {
						$conf['badwords_filename'] = IMSPECTOR_ETC . '/badwords_custom.txt';
						write_imspector_config(IMSPECTOR_ETC . '/badwords_custom.txt', 
							str_replace("\r", '', base64_decode($imspector_config["badwords_list"])));
					} else
						$conf['badwords_filename'] = IMSPECTOR_ETC . '/badwords.txt'; 
				}

				if($imspector_config['block_files'])
					$conf['block_files'] = 'on';

				if($imspector_config['block_unlisted'])
					$conf['block_unlisted'] = 'on';

				if(!empty($imspector_config['acl_whitelist'])) {
					$conf['whitelist_filename'] = IMSPECTOR_ETC . '/acl_whitelist.txt';
					write_imspector_config(IMSPECTOR_ETC . '/acl_whitelist.txt', 
						str_replace("\r", '', base64_decode($imspector_config["acl_whitelist"])));
				}

				if(!empty($imspector_config['acl_blacklist'])) {
					$conf['blacklist_filename'] = IMSPECTOR_ETC . '/acl_blacklist.txt';
					write_imspector_config(IMSPECTOR_ETC . '/acl_blacklist.txt', 
						str_replace("\r", '', base64_decode($imspector_config["acl_blacklist"])));
				}
				
				// Handle Jabber SSL options
				if(isset($imspector_config["ssl_ca_cert"]) && $imspector_config["ssl_ca_cert"] != "none" &&
				   isset($imspector_config["ssl_server_cert"]) && $imspector_config["ssl_server_cert"] != "none") {
					$conf['ssl'] = "on";
					if(!is_dir(IMSPECTOR_ETC . "/ssl"))	
						mkdir(IMSPECTOR_ETC . "/ssl");
						
					$ca_cert = lookup_ca($imspector_config["ssl_ca_cert"]);
					if ($ca_cert != false) {
						if(base64_decode($ca_cert['prv'])) {
							file_put_contents(IMSPECTOR_ETC . "/ssl/ssl_ca_key.pem", 
								base64_decode($ca_cert['prv']));
							$conf['ssl_ca_key'] = IMSPECTOR_ETC . '/ssl/ssl_ca_key.pem';
						}
						if(base64_decode($ca_cert['crt'])) {
							file_put_contents(IMSPECTOR_ETC . "/ssl/ssl_ca_cert.pem", 
								base64_decode($ca_cert['crt']));
							$conf['ssl_ca_cert'] = IMSPECTOR_ETC . "/ssl/ssl_ca_cert.pem";
						}
						$svr_cert = lookup_cert($imspector_config["ssl_server_cert"]);
						if ($svr_cert != false) {
							if(base64_decode($svr_cert['prv'])) {
								file_put_contents(IMSPECTOR_ETC . "/ssl/ssl_server_key.pem", 
									base64_decode($svr_cert['prv']));
								$conf['ssl_key'] = IMSPECTOR_ETC . '/ssl/ssl_server_key.pem';
							}
							
						}
						$conf['ssl_cert_dir'] = IMSPECTOR_ETC . '/ssl';
					}
				} else {
					// SSL Not enabled.  Make sure Jabber-SSL is not processed.
					unset($conf['jabber-ssl']);
					unset($conf['ssl']);
				}

				if (isset($imspector_config['resonder']) && $imspector_config['resonder'] == 'on') {
					$conf['responder_filename'] = IMSPECTOR_ETC . "/responder.db";
					if (isset($imspector_config['prefix_message']) && $imspector_config['prefix_message'] != '' ) {
						$conf['response_prefix'] = base64_decode($imspector_config['prefix_message']) . " -=";
					}
					if (isset($imspector_config['notice_days']) && is_numeric($imspector_config['notice_days'])) {
						if ($imspector_config['notice_days'] != 0) {
							$conf['notice_days'] = $imspector_config['notice_days'];
						}
					} else {
							$conf['notice_days'] = 1;
					}
					$conf['notice_response'] = "Your activities are being logged";
					if (isset($imspector_config['filtered_minutes']) && is_numeric($imspector_config['filtered_minutes'])) {
						if ($imspector_config['filtered_minutes'] != 0) {
							$conf['filtered_mins'] = $imspector_config['filtered_minutes'];
						}
					} else {
							$conf['filtered_mins'] = 15;
					}
					$conf['filtered_response'] = "Your message has been filtered";
				}

				$conftext = '';
				foreach($conf as $var => $key)
					$conftext .= "{$var}={$key}\n";
				write_imspector_config(IMSPECTOR_CONFIG, $conftext);

				/* generate rc file start and stop */
				$stop = <<<EOD
if [ `pgrep imspector | wc -l` != 0  ]; then
		/usr/bin/killall imspector
		while [ `pgrep imspector | wc -l` != 0 ]; do
			sleep 1
		done	
	fi
EOD;
				$start = $stop."\n\tldconfig -m /usr/local/lib/mysql\n";
				$start .= "\t/usr/local/sbin/imspector -c \"".IMSPECTOR_CONFIG."\"";

				write_rcfile(array(
					    'file' => 'imspector.sh',
					    'start' => $start,
					    'stop' => $stop
				    )
				);

				conf_mount_ro();

				/* if imspector not running start it */
				if(!imspector_running()) {
					imspector_notice("Starting service on interface: {$ifaces_active}");
					imspector_action('start');
				}
				/* or restart imspector if settings were changed */
				elseif($_POST['iface_array']) {
					imspector_notice("Restarting service on interface: {$ifaces_active}");
					imspector_action('restart');
				}				
			}
		}

		if(!$iface_array || !$proto_array || !$pf_rules) {
			/* no parameters user does not want imspector running */
			/* lets stop the service and remove the rc file */

			if(file_exists(IMSPECTOR_RCFILE)) {
				if(!$imspector_config['enable'])
					imspector_notice('Stopping service: imspector disabled');
				else
					imspector_notice('Stopping service: no interfaces and/or protocols selected');
					
				imspector_action('stop');

				conf_mount_rw();
				unlink(IMSPECTOR_RCFILE);
				unlink(IMSPECTOR_CONFIG);
				@unlink(IMSPECTOR_ETC . '/badwords_custom.txt');
				@unlink(IMSPECTOR_ETC . '/acl_blacklist.txt');
				@unlink(IMSPECTOR_ETC . '/acl_whitelist.txt');
				conf_mount_ro();
			}			
		}	

		config_unlock();		
	}

	function imspector_get_ca_certs() {
		global $config;

		$ca_arr = array();
		$ca_arr[] = array('refid' => 'none', 'descr' => 'none');
		foreach ($config['ca'] as $ca) {
			$ca_arr[] = array('refid' => $ca['refid'], 'descr' => $ca['descr']);
		}
		return $ca_arr;
	}

	function imspector_get_server_certs() {
		global $config;
		$cert_arr = array();
		$cert_arr[] = array('refid' => 'none', 'descr' => 'none');
		
		foreach ($config['cert'] as $cert) {
			$cert_arr[] = array('refid' => $cert['refid'], 'descr' => $cert['descr']);
		}
		return $cert_arr;
	}
?>