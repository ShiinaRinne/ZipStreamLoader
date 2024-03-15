# stream zip downloader

In many cases, we seek to quickly load the contents of zip files or face limited disk space after downloading compressed files, leaving no space for decompression. In thie case, streaming download and decompression significantly improve the user experience

C++ and Python(Linux & Windows) version are available for download in the releases. 
For other platforms, please build it yourself.

While this repository is now ready for direct use, unforeseen errors may occur, and its use in production environments is not recommended. 
Currently, it has only been tested with Genshin Impact 4.5, where streaming download and decompression of about 70G of compressed files took about 22 minutes to complete.

If you have any suggestions or issues, please feel free to post an issue.

## Usage
Just download from [release](https://github.com/ShiinaRinne/stream_zip_downloader/releases).
### supprted parameters
```
usage: stream [-h] [--log-level {INFO,WARNING,ERROR,CRITICAL}] [--output-dir OUTPUT_DIR] [--urls URLS [URLS ...]]
                 [--disable-crc32] [--preset {genshin}]

Zip Stream Downloader

options:
  -h, --help            show this help message and exit
  --log-level {INFO,WARNING,ERROR,CRITICAL}
                        Sets the log level, default is `INFO`.
  --output-dir OUTPUT_DIR
                        Sets the download directory, default is `./output`.
  --urls URLS [URLS ...]
                        The list of URLs to download.
  --disable-crc32       Disables CRC32 check, off by default.
  --preset {genshin}    Use a predefined download address. Currently supported presets include: 'genshin' - Zip packages for Genshin Impact updates. Leave blank to manually specify the download address. (Python only)
```

### example
```
stream --urls https://example.com/example.zip --log-level INFO 
```