import requests
import os
import zlib
from typing import List
from loguru import logger
import lzma
import sys
import bz2


logger.remove()
logger.add("info.log", level='INFO', encoding='utf8')
logger.add("error.log", level='WARNING', encoding='utf8')


class ZipContentFetcher:
    def __init__(self, urls):
        self.urls = urls
        self.url_generator = self.url_generator_func()
        self.current_stream = None
        self.switch_stream()

    def url_generator_func(self):
        for url in self.urls:
            yield url

    def switch_stream(self):
        url = next(self.url_generator, None)
        if url is None:
            self.current_stream = None  # 所有URL都已处理完毕
        else:
            r = requests.get(url, stream=True)
            r.raise_for_status()
            self.current_stream = r.raw
            logger.info(f"开始处理分卷: {url}")

    def read(self, length):
        if self.current_stream is None:
            return b''  # 所有数据都已读取完毕

        data = self.current_stream.read(length)
        if len(data) < length:
            self.switch_stream()  # 当前文件已读完，切换到下一个
            logger.info(f"切换分卷, 偏移: {hex(length - len(data))}")
            data += self.read(length - len(data))
            
        return data

def download_and_unzip(zip_list:List[str],extract_to='./output/stream', enable_crc32_check: bool = False):
    if not os.path.exists(extract_to):
        os.makedirs(extract_to) 
        
    fetcher = ZipContentFetcher(zip_list)
    while True:
        signature = fetcher.read(4)
        if not signature or signature == b'PK\x01\x02':
            logger.info("文件读取结束")
            break
        
        version = fetcher.read(2)
        flags = fetcher.read(2)
        compression = fetcher.read(2)
        mod_time = fetcher.read(2)
        modeDate = fetcher.read(2)
        crc32_expected = int.from_bytes(fetcher.read(4), byteorder='little')
        compressedSize = fetcher.read(4)
        uncompressedSize = fetcher.read(4)
        file_name_length = int.from_bytes(fetcher.read(2), byteorder='little')
        extra_field_length = int.from_bytes(fetcher.read(2), byteorder='little')

        file_name = fetcher.read(file_name_length).decode('gbk', 'ignore')
        extra_field = fetcher.read(extra_field_length)
        
        logger.info(f"正在处理: {file_name}, 预期文件大小: {int.from_bytes(compressedSize, byteorder='little')}")
        compressData = fetcher.read(int.from_bytes(compressedSize, byteorder='little'))
        
        
        output_file_path = os.path.join(extract_to, file_name)
        if file_name.endswith('/'):
            os.makedirs(output_file_path, exist_ok=True)
            continue
        
        decompressed_data = b''
        if compression == b'\x00\x00':  # No Compression (Stored)
            logger.info(f"正在解压: {file_name}, 格式: No Compression (Stored)")
            decompressed_data = compressData
        elif compression == b'\x08\x00':  # Deflate
            logger.info(f"正在解压: {file_name}, 格式: Deflate")
            decompressed_data = zlib.decompress(compressData, -zlib.MAX_WBITS)
        elif compression == b'\x0c\x00':  # BZIP2
            logger.info(f"正在解压: {file_name}, 格式: BZIP2")
            decompressed_data = bz2.decompress(compressData)
        elif compression == b'\x0e\x00':  # LZMA
            logger.info(f"正在解压: {file_name}, 格式: LZMA")
            decompressed_data = lzma.decompress(compressData)
        
        with open(output_file_path, 'wb') as f:
            f.write(decompressed_data)
            logger.info(f"{file_name} 解压完成")

        crc32_calculated = zlib.crc32(decompressed_data) & 0xffffffff
        if crc32_calculated == crc32_expected:
            logger.info(f"{file_name} CRC32校验成功.")
        else:
            logger.error(f"{file_name} CRC32校验失败. 计算的CRC32: {crc32_calculated}, 期望的CRC32: {crc32_expected}")
            



genshin_list = os.listdir(r"E:\mhy")
genshin_list = [f"http://localhost:5000/{file}" for file in genshin_list]

# sdk: https://sdk-static.mihoyo.com/hk4e_cn/mdk/launcher/api/resource?key=eYd89JmJ&launcher_id=18

if __name__ == "__main__":
    logger.add(sys.stdout, level='INFO')
    download_and_unzip(genshin_list, r'E:\Genshin Impact\Genshin Impact Game', enable_crc32_check=True)
    