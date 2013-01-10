#!/bin/sh
# This file was automatically generated
# by the pfSense service handler.

rc_start() {
	/usr/local/sbin/nanosistemi-postfix start

}

rc_stop() {
	/usr/local/sbin/nanosistemi-postfix stop

}

case $1 in
	start)
		rc_start
		;;
	stop)
		rc_stop
		;;
	restart)
		rc_stop
		rc_start
		;;
esac

