# ZipStreamDownloader

In many cases, we seek to quickly load the contents of zip files or face limited disk space after downloading compressed files, leaving no space for decompression. In thie case, streaming download and decompression significantly improve the user experience

C++ and Python(Linux & Windows) version are available for download in the releases. 
For other platforms, please build it yourself.

While this repository is now ready for direct use, unforeseen errors may occur, and its use in production environments is not recommended. 
Currently, it has only been tested with Genshin Impact 4.5, where streaming download and decompression of about 70G of compressed files took about 22 minutes to complete.

If you have any suggestions or issues, feel free to post an issue.

## Usage
Just download from [release](https://github.com/ShiinaRinne/stream_zip_downloader/releases).
### Supprted parameters
```
usage: stream [-h] [--log-level {INFO,WARNING,ERROR,CRITICAL}] [--output-dir OUTPUT_DIR] [--urls URLS [URLS ...]]
                  [--preset {genshin}]

Zip Stream Downloader

options:
  -h, --help            show this help message and exit
  --log-level {INFO,WARNING,ERROR,CRITICAL}
                        Sets the log level, default is `INFO`.
  --output-dir OUTPUT_DIR
                        Sets the download directory, default is `./output`.
  --urls URLS [URLS ...]
                        The list of URLs to download.
  --preset {genshin,zzz,bh3,sr}
                        Use a predefined download address. Leave blank to manually specify the download address. (Python only)
  --type {major,patch}  Preset type, default is `major`. [More detail](https://hyp-api.mihoyo.com/hyp/hyp-connect/api/getGamePackages?launcher_id=jGHBHlcOq1) (Python only)
```

### Example
```
stream.exe --preset zzz --output-dir F:\zzz
```


## Build
### C++
1. Install [xmake](https://github.com/xmake-io/xmake?tab=readme-ov-file#installation) and [vcpkg](https://github.com/microsoft/vcpkg?tab=readme-ov-file#getting-started)  
   If you are using macOS, please install [XCode](https://apps.apple.com/us/app/xcode/id497799835)
2. Set `VCPKG_ROOT` to the path of vcpkg  

3. Install `pkg-config` (unix only)
    ```shell
    # Ubuntu
    sudo apt install pkg-config
    # macosx
    brew install pkgconf
    ```
4. Build
    ```shell
    git clone https://github.com/ShiinaRinne/ZipStreamLoader
    cd ZipStreamLoader/cpp
    xmake
    ```

### Python
1. Install pdm  
   https://pdm-project.org/zh-cn/latest/#_3
2. Install patchelf(unix only)
    ```shell
    sudo apt install patchelf
    ```
3. Build
    ```shell
    git clone https://github.com/ShiinaRinne/ZipStreamLoader
    cd ZipStreamLoader/python
    pdm install
    nuitka3 --standalone --onefile stream.py -o stream
    ```
