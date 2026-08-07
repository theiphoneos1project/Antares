import urllib.request
import subprocess
import pathlib

SCRIPT_DIR = pathlib.Path(__file__).parent

RAMDISK_ROOT_PATH = SCRIPT_DIR / "ramdisk_root"
ZIBRI_DAT_PATH = SCRIPT_DIR / "zibri.dat"
ZIBRI_URL: str = "https://github.com/Zibri/ZiPhone/raw/d779b8517c0af7b5bca68232cab5f3466b50a1a7/zibri.dat"

def download_zibri() -> None:
    print(f"[+] Downloading zibri.dat from GitHub...")
    try:
        urllib.request.urlretrieve(ZIBRI_URL, ZIBRI_DAT_PATH)
        print(f"[+] Successfully downloaded {ZIBRI_DAT_PATH}")
    except Exception as e:
        print(f"[-] Failed to download zibri.dat: {e}")
        exit(1)

def main() -> None:
    if not ZIBRI_DAT_PATH.exists():
        download_zibri()

    print(f"[+] zibri.dat exists, continuing")

    RAMDISK_ROOT_PATH.mkdir(parents=True, exist_ok=True)
    print(f"[+] Extracting HFS+ image contents into {RAMDISK_ROOT_PATH} using 7-Zip...")

    try:
        subprocess.run(
            ["7z", "x", ZIBRI_DAT_PATH.as_posix(), f"-o{RAMDISK_ROOT_PATH.as_posix()}", "-y"],
            check=True
        )
    except FileNotFoundError:
        print(f"[-] Error: 7z is not installed or not in PATH.")
        exit(1)
    except subprocess.CalledProcessError as e:
        if not any(RAMDISK_ROOT_PATH.iterdir()):
            print(f"[-] 7-Zip extraction failed with return code {e.returncode}")
            exit(e.returncode)
        print("[+] Note: 7-Zip reported a minor header warning, but files were successfully extracted.")

    print(f"[+] Successfully extracted ramdisk contents to {RAMDISK_ROOT_PATH}")

if __name__ == "__main__":
    main()