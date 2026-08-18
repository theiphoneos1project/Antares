export PATH="/bin:/sbin:/usr/bin:/usr/local/bin:/usr/sbin"

SYSTEM_VERSION=/mnt1/System/Library/CoreServices/SystemVersion.plist

patch_bspatch() {
    local patch="${1}" 
    local md5="${2}"

    if md5sum /mnt1/usr/libexec/lockdownd | grep -q "${md5}"; then
        echo "[+] Patching lockdownd (hacktivating)..."
        bspatch /mnt1/usr/libexec/lockdownd /mnt1/usr/libexec/lockdownd.patched "${patch}"
        mv /mnt1/usr/libexec/lockdownd.patched /mnt1/usr/libexec/lockdownd
        chmod 755 /mnt1/usr/libexec/lockdownd
        chown root:wheel /mnt1/usr/libexec/lockdownd
    else
        echo "[+] lockdownd is already patched. Skipping..."
    fi
}

# Sourced from https://appledb.dev/device/iPhone-2G.html and https://appledb.dev/device/iPod1,1.html
if grep -qE "<string>(1A543a|1C25|1C28|3A109a|3B48b|4A93|4A102|4B1)</string>" "${SYSTEM_VERSION}"; then
    echo "[+] Patching lockdownd (activation)..."
    ipatcher -l /mnt1/usr/libexec/lockdownd
fi

if grep -qE "<string>(3A100a|3A101a)</string>" "${SYSTEM_VERSION}"; then
    patch_bspatch "/mnt2/1.1activation.bspatch" "2877d3d910820b67daf67dba3d6296ec"
fi

if grep -q "<string>3A110a</string>" "${SYSTEM_VERSION}"; then
    patch_bspatch "/mnt2/1.1.1activation.bspatch" "0ef900923d425e1917699ab54a5b60b4"
fi