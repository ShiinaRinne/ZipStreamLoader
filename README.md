# stream zip downloader

Many times, we don't need all the content of the zip file, or the disk space is small and there is no remaining space to decompress after downloading the compressed file. 
In this case, the streaming download and decompression feature will disrupt this process

Although this repository is now ready for direct use (tested in Genshin Impact,~69.7G compressed files can be downloaded and decompressed through streaming, which takes about 22 minutes to complete), unexpected bugs may occur and it is not recommended to use it in a production environment. 
If you have any suggestions or questions, please feel free to post an issue

In the future, this project may be rewritten using C++

## Usage
```
python stream.py
```
I have also provided a release version, which can be used directly without installing python


### supprted parameters
```
python stream.py -h
usage: stream.py [-h] [--log-level {INFO,WARNING,ERROR,CRITICAL}] [--output-dir OUTPUT_DIR] [--urls URLS [URLS ...]]
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
  --preset {genshin}    Use a predefined download address. Currently supported presets include: 'genshin' - Zip packages for Genshin Impact updates. Leave blank to manually specify the download address.
```

### example
```
python stream.py --log-level INFO --disable-crc32 --preset genshin
```