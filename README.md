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
  --preset {genshin}    Use a predefined download address. Currently supported presets include: 'genshin' - Zip packages for Genshin Impact updates. Leave blank to manually specify the download address. (Python only)
```

### Example
```
stream --urls https://foo.com/bar.zip --log-level INFO 
```


## Build
### C++
1. Install [xmake](https://github.com/xmake-io/xmake?tab=readme-ov-file#installation) and [vcpkg](https://github.com/microsoft/vcpkg?tab=readme-ov-file#getting-started) first.
2. Set `VCPKG_ROOT` to the path of vcpkg.

3. Ubuntu should install pkg-config first
    ```shell
    sudo apt install pkg-config
    ```
4. Build
    ```shell
    git clone https://github.com/ShiinaRinne/ZipStreamLoader
    cd ZipStreamLoader/cpp
    xmake
    ```

### Python
1. Install nuikta first
    ```shell
    pip3 install nuitka
    ```
2. Linux should install patchelf first
    ```shell
    sudo apt install patchelf
    ```
3. Build
    ```shell
    git clone https://github.com/ShiinaRinne/ZipStreamLoader
    cd ZipStreamLoader/python
    nuitka3 --standalone --onefile stream.py -o stream
    ```
