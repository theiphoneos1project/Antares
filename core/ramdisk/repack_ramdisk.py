import shutil
import subprocess
import pathlib

SCRIPT_DIR        = pathlib.Path(__file__).parent

HFSPLUS_TOOL      = SCRIPT_DIR / "xpwn" / "bin" / "hfsplus"
RAMDISK_ROOT_PATH = SCRIPT_DIR / "ramdisk_root"
RAMDISK_IMG_PATH  = SCRIPT_DIR / "ramdisk.img"
ZIBRI_DAT_PATH    = SCRIPT_DIR / "zibri.dat"
FILES_PATH        = SCRIPT_DIR / "files"

def clean_dsstore(root: pathlib.Path) -> None:
    for pattern in (".DS_Store", "._*"):
        for f in root.rglob(pattern):
            f.unlink()
            print(f"[+] Removed {f.relative_to(root)}")

def add_folder(local_dir: pathlib.Path, hfs_dest: str) -> None:
    for local_file in local_dir.rglob("*"):
        if not local_file.is_file():
            continue
        relative = local_file.relative_to(local_dir)
        hfs_path = f"{hfs_dest}/{relative.as_posix()}"
        subprocess.run(
            [HFSPLUS_TOOL.as_posix(), RAMDISK_IMG_PATH.as_posix(), "rm", hfs_path],
            capture_output=True
        )
        subprocess.run(
            [HFSPLUS_TOOL.as_posix(), RAMDISK_IMG_PATH.as_posix(), "add",
             local_file.as_posix(), hfs_path],
            check=True
        )
        print(f"    [ok] {hfs_path}")

def add_file(local_path: pathlib.Path, hfs_path: str) -> None:
    result = subprocess.run(
        [HFSPLUS_TOOL.as_posix(), RAMDISK_IMG_PATH.as_posix(), "add", local_path.as_posix(), hfs_path],
        check=True,
        text=True
    )
    if result.returncode != 0:
        print(result.stdout)
        print(result.stderr)
        raise subprocess.CalledProcessError(result.returncode, result.args)
    print(f"    [ok] {hfs_path}")

def replace_file(local_path: pathlib.Path, hfs_path: str) -> None:
    subprocess.run(
        [HFSPLUS_TOOL.as_posix(), RAMDISK_IMG_PATH.as_posix(), "rm", hfs_path]
    )

    result = subprocess.run([HFSPLUS_TOOL.as_posix(), RAMDISK_IMG_PATH.as_posix(), "add", local_path.as_posix(), hfs_path], text=True)
    if result.returncode != 0:
        print(result.stdout)
        print(result.stderr)
        raise subprocess.CalledProcessError(result.returncode, result.args)
    print(f"    [ok] {hfs_path}")

def remove_folder(hfs_path: str) -> None:
    result = subprocess.run(
        [HFSPLUS_TOOL.as_posix(), RAMDISK_IMG_PATH.as_posix(), "ls", hfs_path],
        capture_output=True, text=True
    )
    for line in result.stdout.splitlines():
        stripped = line.strip()
        if not stripped or stripped.endswith(':'):
            continue
        filename = stripped.split()[-1]
        child = f"{hfs_path}/{filename}"
        subprocess.run(
            [HFSPLUS_TOOL.as_posix(), RAMDISK_IMG_PATH.as_posix(), "rm", child],
            capture_output=True
        )
        print(f"    [rm] {child}")
    
    subprocess.run(
        [HFSPLUS_TOOL.as_posix(), RAMDISK_IMG_PATH.as_posix(), "rm", hfs_path],
        capture_output=True 
    )

def main() -> None:
    clean_dsstore(RAMDISK_ROOT_PATH)
    shutil.copy2(ZIBRI_DAT_PATH, RAMDISK_IMG_PATH)

    CHANGED_FILES = [
        (RAMDISK_ROOT_PATH / "ZiPhone Ramdisk" / "etc" / "profile", "/etc/profile"),
        (FILES_PATH / "profile", "/etc/profile")
    ]

    print(f"[+] Adding {len(CHANGED_FILES)} modified file(s) to ramdisk...")
    for (local, hfs) in CHANGED_FILES:
        print(f"  -> {hfs}")
        replace_file(local, hfs)

    remove_folder("/zib")
    add_folder(FILES_PATH, "/tmp")

    print(f"[+] Successfully repacked ramdisk.")

if __name__ == "__main__":
    main()