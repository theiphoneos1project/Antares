import pathlib
import urllib.request
import platform
import zipfile
import stat

SCRIPT_DIR = pathlib.Path(__file__).parent
BASE_URL = "https://github.com/LukeZGD/daibutsuCFW/releases/download/latest/"

XPWN_PATH = SCRIPT_DIR / "xpwn"

OLD_KCACHE_PATH = SCRIPT_DIR / "old_kernelcache"
OLD_KCACHE_URL: str = "https://github.com/Zibri/ZiPhone/blob/d7dca81b2707fe962f116b23bface18de88f4351/victor.dat"

NEW_KCACHE_PATH = SCRIPT_DIR / "new_kernelcache"
NEW_KCACHE_URL: str = "https://github.com/Zibri/ZiPhone/blob/d7dca81b2707fe962f116b23bface18de88f4351/igor.dat"

def get_asset_name() -> str:
    system = platform.system()
    machine = platform.machine().lower()

    if system == "Windows":
        return "xpwn_win.zip"
    elif system == "Darwin":
        if machine == "arm64":
            return "xpwn_macos-arm64.zip"
        else:
            return "xpwn_macos-x86_64.zip"
    elif system == "Linux":
        if machine == "aarch64":
            return "xpwn_linux-aarch64.zip"
        else:
            return "xpwn_linux-x86_64.zip"
    else:
        print(f"[-] Unsupported OS: {system}")
        exit(1)

def download_old_kcache() -> None:
    print(f"[+] Downloading victor.dat from GitHub...")
    try:
        urllib.request.urlretrieve(OLD_KCACHE_URL, OLD_KCACHE_PATH)
        print(f"[+] Successfully downloaded {OLD_KCACHE_PATH}")
    except Exception as e:
        print(f"[-] Failed to download victor.dat: {e}")
        exit(1)

def download_new_kcache() -> None:
    print(f"[+] Downloading igor.dat from GitHub...")
    try:
        urllib.request.urlretrieve(NEW_KCACHE_URL, NEW_KCACHE_PATH)
        print(f"[+] Successfully downloaded {NEW_KCACHE_PATH}")
    except Exception as e:
        print(f"[-] Failed to download igor.dat: {e}")
        exit(1)

def main() -> None:
    if XPWN_PATH.exists() and OLD_KCACHE_PATH.exists() and NEW_KCACHE_PATH.exists():
        print(f"[!] Dependencies have already been fetched!")
        return

    if not XPWN_PATH.exists():
        asset_name = get_asset_name()
        download_url = BASE_URL + asset_name
        zip_path = SCRIPT_DIR / asset_name

        print(f"[+] Detected System: {platform.system()} ({platform.machine()})")
        print(f"[+] Downloading {asset_name} from GitHub...")

        try:
            urllib.request.urlretrieve(download_url, zip_path)
        except Exception as e:
            print(f"[-] Failed to download {asset_name}: {e}")
            exit(1)

        print(f"[+] Extracting {asset_name}...")

        try:
            with zipfile.ZipFile(zip_path, 'r') as zip_ref:
                zip_ref.extractall(XPWN_PATH)

            if platform.system() != "Windows":
                for file_path in XPWN_PATH.rglob("*"):
                    if file_path.is_file():
                        current_permissions = file_path.stat().st_mode
                        file_path.chmod(current_permissions | stat.S_IXUSR | stat.S_IXGRP | stat.S_IXOTH)

            zip_path.unlink()
            print(f"[+] Successfully fetched xpwn dependencies!")
        except Exception as e:
            print(f"[-] Failed to extract {asset_name}: {e}")
            exit(1)

    if not OLD_KCACHE_PATH.exists():
        download_old_kcache()

    if not NEW_KCACHE_PATH.exists():
        download_new_kcache()

if __name__ == "__main__":
    main()