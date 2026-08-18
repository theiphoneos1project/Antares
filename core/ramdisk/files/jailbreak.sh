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

sleep 5

echo "[+] Unmounting filesystems..."
umount /mnt1 >/dev/null
umount /mnt2 >/dev/null
fsck_hfs /dev/disk0s1 >/dev/null
fsck_hfs /dev/disk0s2 >/dev/null
