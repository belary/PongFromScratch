#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
文档同步脚本 - 将 Markdown 文档转换为 TXT 格式

用途：将 docs 目录中的 .md 文件复制为 .txt 文件
      这样可以在 Google Drive 等不支持 Markdown 的平台直接查看

使用方法：
    1. 双击运行此脚本（Windows）
    2. 或在命令行运行：python sync_docs.py

每次 .md 文件更新后运行此脚本即可同步
"""

import os
import shutil
from pathlib import Path
from datetime import datetime


def sync_markdown_to_txt(source_dir: str = "docs", target_dir: str = "docs") -> None:
    """
    将指定目录中的 .md 文件同步为 .txt 文件

    Args:
        source_dir: 源目录（包含 .md 文件）
        target_dir: 目标目录（存放 .txt 文件）
    """
    source_path = Path(source_dir)
    target_path = Path(target_dir)

    # 检查源目录是否存在
    if not source_path.exists():
        print(f"[错误] 源目录不存在: {source_dir}")
        return

    print("=" * 50)
    print("   文档同步 - Markdown 转 TXT")
    print("=" * 50)
    print()

    count = 0
    md_files = list(source_path.glob("*.md"))

    if not md_files:
        print(f"[信息] 在 {source_dir} 中未找到 .md 文件")
        return

    # 遍历所有 .md 文件
    for md_file in md_files:
        # 获取文件名（不含扩展名）
        filename = md_file.stem

        # 源文件和目标文件路径
        source_file = md_file
        target_file = target_path / f"{filename}.txt"

        # 复制文件
        print(f"[复制] {filename}.md --> {filename}.txt")
        try:
            shutil.copy2(source_file, target_file)
            count += 1
        except Exception as e:
            print(f"[错误] 复制失败: {filename}.md - {e}")

    print()
    print("=" * 50)
    print(f"[完成] 已同步 {count} 个文档")
    print("=" * 50)
    print()
    print(f"文档位置: {target_path.absolute()}")
    print()


def main():
    """主函数"""
    # 获取脚本所在目录
    script_dir = Path(__file__).parent

    # 切换到脚本目录
    os.chdir(script_dir)

    # 执行同步
    sync_markdown_to_txt()

    # 询问是否打开目标目录（仅在 Windows 上）
    if os.name == 'nt':
        try:
            choice = input("是否打开 docs 目录？(Y/N): ").strip().upper()
            if choice == 'Y':
                os.startfile("docs")
        except:
            pass


if __name__ == "__main__":
    main()
