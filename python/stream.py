import requests
import os
import zlib
from typing import List
from loguru import logger
import lzma
import sys
import bz2
import argparse
import asyncio
import aiofiles
import struct

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


async def unzip(compression: bytes, compressData: bytes, file_name: str, output_file_path: str, crc32_expected: int):
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
    
    async with aiofiles.open(output_file_path, 'wb') as f:
        await f.write(decompressed_data)
        logger.info(f"{file_name} 解压完成")
        
    
    crc32_calculated = zlib.crc32(decompressed_data) & 0xffffffff
    if crc32_calculated == crc32_expected:
        logger.info(f"{file_name} CRC32校验成功.")
    else:
        logger.error(f"{file_name} CRC32校验失败. 计算的CRC32: {crc32_calculated}, 期望的CRC32: {crc32_expected}")


async def download(zip_list:List[str]):
    fetcher = ZipContentFetcher(zip_list)
    while True:
        signature = fetcher.read(4)
        if not signature or signature == b'PK\x01\x02':
            logger.info("文件读取结束")
            break
        
        header_data = fetcher.read(26)
        version, flags, compression, mod_time, mod_date, crc32_expected, compressed_size, uncompressed_size, file_name_length, extra_field_length = struct.unpack('<2s2s2s2s2sIIIHH', header_data)

        file_name = fetcher.read(file_name_length).decode('gbk', 'ignore')
        extra_field = fetcher.read(extra_field_length)
        
        logger.info(f"正在处理: {file_name}, 预期文件大小: {compressed_size}")
        compressData = fetcher.read(compressed_size)
        
        
        output_file_path = os.path.join(args.output_dir, file_name)
        if not os.path.exists(os.path.dirname(output_file_path)):
            os.makedirs(os.path.dirname(output_file_path))
            continue
        
        await unzip(compression, compressData, file_name, output_file_path, crc32_expected)
        

def parse_presets(preset):
    match preset:
        case "genshin":
            data = requests.get("https://sdk-static.mihoyo.com/hk4e_cn/mdk/launcher/api/resource?key=eYd89JmJ&launcher_id=18").json()
            game_list = data["data"]["game"]["latest"]["segments"]
            return [z["path"] for z in game_list]
        case _:
            return None
      

def parse_args():
    parser = argparse.ArgumentParser(description="Zip Stream Downloader")
    parser.add_argument("--log-level", default="INFO", choices=["INFO", "WARNING", "ERROR", "CRITICAL"], help="设置日志级别，默认为 INFO ")
    parser.add_argument("--output-dir", default="./output", help="设置下载目录，默认为./output")
    parser.add_argument("--urls", nargs='+', help="需要下载的链接列表。")
    
    presets = ['genshin']
    parser.add_argument("--preset", default=None, choices=presets,
                        help="使用预定义的下载地址。"
                             "当前支持的预设包括: "
                             "'genshin' - 原神更新时的zip包。"
                             "留空则需手动指定下载地址。")

    return parser.parse_args()


logger.add("info.log", level='INFO', encoding='utf8')
logger.add("error.log", level='WARNING', encoding='utf8')

args = None

if __name__ == "__main__":
    args = parse_args()
    
    logger.remove()
    logger.add(sys.stdout, level=args.log_level.upper())
    logger.add("info.log", level=args.log_level.upper())
    
    if args.preset is not None:
        urls:List[str] = parse_presets(args.preset)
    else:
        if args.urls is None:
            logger.error("需要指定下载地址")
            sys.exit(1)
        urls = args.urls
        
    if not os.path.exists(args.output_dir):
        os.makedirs(args.output_dir)

    asyncio.run(download(urls))
    