# stream zip downloader

Many times, we don't need all the content of the zip file, or the disk space is small and there is no remaining space to decompress after downloading the compressed file. 
In this case, the streaming download and decompression feature will disrupt this process

Although this repository is now ready for direct use (tested in Genshin Impact,~69.7G compressed files can be downloaded and decompressed through streaming, which takes about 22 minutes to complete), unexpected bugs may occur and it is not recommended to use it in a production environment. 
If you have any suggestions or questions, please feel free to post an issue

In the future, this project may be rewritten using C++

## Usage
Edit the code file and replace the download link. It now supports `zip` and `multipart zip volumes`
