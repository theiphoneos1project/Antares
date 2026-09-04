<img align="left" width="100" height="100" src="icons/Icon.ico" alt="logo" style="float: left;"/>

<h3 align="left">
    Antares 
    <br />
    iPhone OS 1.0 - 1.1.5 untethered jailbreak for iPhone and iPod touch.
    <div align="right" style="float: top;" />
    <br />
</h3> 

## Information

> [!WARNING]
> There is no warranty for this software. Use it at your own risk.

- **macOS**, **Linux**, and **Windows** are supported
    - Older versions of macOS may have trouble detecting the device in normal mode. Putting the device into recovery mode manually should allow you to jailbreak successfully.
    - Antares was tested on Linux Mint. Mileage may vary with other distributions of Linux.
    - **Windows 10** and **11** are supported. You will need to manually rebind the USB drivers to `libusbK` with [Zadig](https://zadig.akeo.ie).
    - Hacktivation is supported with Antares, tested on both the original iPhone and iPod touch on every version of iPhone OS 1.

## How to Use
Download the relevant build for your operating system and architecture from the [Releases](https://github.com/theiphoneos1project/Antares/releases) tab.

- **macOS**:
    - For the CLI, make sure to run `chmod +x /path/to/binary` after downloading. If possible, run via `sudo` in order to force Antares's USB implementation to take priority over macOS. 
    - For the GUI, make sure to try running it for the first time, allowing it to run in Privacy & Security, and then running it again.
- **Linux**:
    - Make sure `libwxgtk3.2-dev` is installed from your package manager.
    - For both the CLI and GUI, make sure to run `chmod +x /path/to/binary` after downloading. Make sure to run via `sudo` in order for Antares to be able to temporarily disable `usbmuxd` and replace it with its own implementation while it is running.
- **Windows:**
    - Make sure to also install [Zadig](https://zadig.akeo.ie). Plug in your device. In Zadig, tick the "List All Devices" flag in the "Options" tab. Select the iPhone/iPod touch (Interface 1) and rebind the driver to `libusbK`. Put the device into recovery mode via Antares and make sure to also rebind the driver to `libusbK` there. Keep in mind that if you switch devices, you will have to run the same steps for that device as well. Then, if you switch back to the original device, the same proceduce needs to be performed again. If you have only one device on iPhone OS 1, you will not have to run the procedure multiple times.

## How to Compile Manually
Ensure you have [CMake](https://cmake.org), [vcpkg](https://vcpkg.io/en/), [7z](https://www.7-zip.org/download.html) and [Python](https://www.python.org) installed.

To compile the ramdisk, please run:
```
cd core/ramdisk
python3 get_dependencies.py
python3 get_ziphone_ramdisk.py
python3 repack_ramdisk.py
```

### macOS (arm64)
```bash
cmake -B build/macos-arm64 -DCMAKE_OSX_ARCHITECTURES=arm64 -DVCPKG_TARGET_TRIPLET=arm64-osx -DVCPKG_OVERLAY_TRIPLETS=triplets -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake

cmake --build build/macos-arm64 --target dist --clean-first
```

### macOS (x64)
```bash
cmake -B build/macos-x64 -DCMAKE_OSX_ARCHITECTURES=x86_64 -DVCPKG_TARGET_TRIPLET=x64-osx -DVCPKG_OVERLAY_TRIPLETS=triplets -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake

cmake --build build/macos-x64 --target dist --clean-first
```

### Linux
Make sure `wxWidgets` is installed from your package manager. The Linux build script does not rely on compiling `wxWidgets` as a dependency since that takes quite a long time on slow machines.
```bash
cmake -B build/linux-x64 -DVCPKG_TARGET_TRIPLET=x64-linux -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake -DwxWidgets_CONFIG_EXECUTABLE=$(which wx-config)

cmake --build build/linux-x64 --target dist --clean-first
```

### Windows
Ensure you have [Visual Studio 17 2022](https://visualstudio.microsoft.com/vs/older-downloads/) installed. Make sure your environment is set up correctly. The build instructions assume you are using a PowerShell session with the Visual Studio Developer tools set up correctly. If `$VCPKG_ROOT` is not set up correctly, try entering the direct path to your `vcpkg` root.

```powershell
cmake -B build\windows -G "Visual Studio 17 2022" -A x64 -DVCPKG_TARGET_TRIPLET=x64-windows -DVCPKG_OVERLAY_TRIPLETS=triplets -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT\scripts\buildsystems\vcpkg.cmake

cmake --build build\windows --target dist --clean-first
```

## Credits
- [EthanArbuckle](https://github.com/EthanArbuckle) - original jailbreak ([iOS1.0-Jailbreak](https://github.com/EthanArbuckle/iOS1.0-Jailbreak)) which Antares is a fork of
- [Zibri](https://github.com/Zibri) - creator of ZiPhone, which Antares's ramdisk and kernelcaches are based on
- lex - contributor to ZiPhone and creator of [Whitera1n](https://theapplewiki.com/wiki/Whitera1n). Incredibly helpful [information](https://web.archive.org/web/20190228192547/http://whitera1n.com/ramdiskhackwriteup) about kernelcaches on the original iPod touch on iPhone OS 1.1-1.1.5
- [forcequitOS](https://github.com/forcequitOS) - incredibly helpful in testing and developing iPod touch support, figuring out bugs with the kernelcache
- [iphone-elite](https://code.google.com/archive/p/iphone-elite/) - lockdownd patching for hacktivation with iPatcher

## Copyright
This project is licensed under [MIT](LICENSE).

###### Copyright (c) 2026 Nightwind
