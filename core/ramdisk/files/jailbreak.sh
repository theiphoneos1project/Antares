export PATH="/bin:/sbin:/usr/bin:/usr/local/bin:/usr/sbin"

echo "[+] Starting jailbreak..."

if ! grep -q "/dev/disk0s1 / hfs rw 0 1" /mnt1/etc/fstab; then
	echo "[+] Copying fstab to root filesystem..."
	cp /tmp/fstab /mnt1/etc/fstab
else
	echo "[+] The root filesystem is already mounted as read write. Skipping..."
fi

if [ ! -f /mnt1/bin/chmod ]; then
	echo "[+] Bootstrapping..."

	cp /bin/chmod /mnt1/bin/chmod
	cp /bin/sh /mnt1/bin/sh
	cp /tmp/ls /mnt1/bin/ls

	cp /usr/lib/libarmfp.dylib /mnt1/usr/lib/libarmfp.dylib
	cp /tmp/libintl.8.dylib /mnt1/usr/lib/libintl.8.dylib

	chmod +x /mnt1/bin/ls
else
	echo "[+] Device is already minimally bootstrapped. Skipping..."
fi

if ! grep -q "com.apple.afc2" /mnt1/System/Library/Lockdown/Services.plist; then
	echo "[+] Enabling Apple File Conduit \"2\"..."
	cp /tmp/Services.plist /mnt1/System/Library/Lockdown/Services.plist
else
	echo "[+] Apple File Conduit \"2\" is already enabled. Skipping..."
fi

if [ ! -f /mnt1/etc/syslog.conf ]; then
	echo "[+] Enabling logging to /var/log/syslog"
	cp /tmp/com.apple.syslogd.plist /mnt1/System/Library/LaunchDaemons/com.apple.syslogd.plist
	cp /tmp/syslog.conf /mnt1/etc/syslog.conf
else
	echo "[+] Logging to /var/log/syslog already enabled. Skipping..."
fi

if [ ! -f /mnt1/usr/sbin/PXLdaemon ]; then
	echo "[+] Adding PXL daemon..."

	if [ ! -d /mnt1/etc/init.d ]; then
		echo "[+] /etc/init.d doesn't exist, creating..."
		mkdir /mnt1/etc/init.d
	fi

	cp /tmp/hackinit.sh /mnt1/etc/hackinit.sh
	cp /tmp/pxl.sh /mnt1/etc/init.d/pxl.sh
	cp /tmp/PXLdaemon /mnt1/usr/sbin/PXLdaemon
	cp /tmp/com.apple.update.plist.hackinit /mnt1/System/Library/LaunchDaemons/com.apple.update.plist

	chmod +x /mnt1/usr/sbin/PXLdaemon
else
	echo "[+] PXLdaemon already installed. Skipping..."
fi

sleep 5

echo "[+] Unmounting filesystems..."
umount /mnt1 >/dev/null
umount /mnt2 >/dev/null
fsck_hfs /dev/disk0s1 >/dev/null
fsck_hfs /dev/disk0s2 >/dev/null

echo "[+] Done setting up."

echo
echo "======================================="
echo "           !!! Attention !!!           "
echo "If you want to fully bootstrap your device, make sure to install the BSD base package."
echo "======================================="   
echo
