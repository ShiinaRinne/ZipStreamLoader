# -*- coding: utf-8 -*-
import requests
from typing import List
import sys
import argparse
import pyperclip

def parse_presets(game, download_type):
    game_id = {
        'bh3':'osvnlOc0S8',
        'genshin':'1Z8W5NHUQb',
        'sr':'64kMb5iAWu',
        'zzz':'x6znKlJ0xK'
    }
    data = requests.get(f"https://hyp-api.mihoyo.com/hyp/hyp-connect/api/getGamePackages?game_ids[]={game_id[game]}&launcher_id=jGHBHlcOq1").json()

    main = data["data"]["game_packages"][0]["main"]

    try:
        zip_list = main["major"]["game_pkgs"] if download_type == "major" else main["patches"][0]["game_pkgs"] # TODO: patch version
    except IndexError:
        print("未找到指定类型的下载链接")
        input("按任意键退出...")
        sys.exit(1)

    return [pkg["url"] for pkg in zip_list]
      

def parse_args():
    parser = argparse.ArgumentParser(description="Zip Stream Downloader")
    presets = ['genshin', 'zzz', 'bh3', 'sr']
    download_type = ['major', 'patch']
    parser.add_argument("--preset", default=None, choices=presets, help="使用预定义的下载地址。留空则需手动指定下载地址。")
    parser.add_argument("--type", default="major", choices=download_type, help="下载类型，默认为major.")
    return parser.parse_args()


if __name__ == "__main__":
    args = parse_args()
    if args.preset is not None:
        urls:List[str] = parse_presets(args.preset, args.type)
    else:
        print("需要指定下载目标")
        args.preset=input("请输入预设下载目标(genshin/zzz/bh3/sr): ")
        urls:List[str] = parse_presets(args.preset, args.type)

    url_str = "\r\n".join(urls)
    pyperclip.copy(url_str)
    print(f"{url_str} \r\n 以上为待下载列表，已复制到剪贴板，请自行粘贴到浏览器下载。")
    input("按任意键退出...")
 