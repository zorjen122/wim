#!/bin/bash

# 指定目标文件夹
TARGET_DIR="$1"

# 查找所有 .cpp 文件并重命名为 .cc
find "$TARGET_DIR" -type f -name "*.cpp" | while read -r file; do
    # 获取文件所在目录和文件名（不含扩展名）
    dir=$(dirname "$file")
    base=$(basename "$file" .cpp)
    # 重命名文件
    mv "$file" "$dir/$base.cc"
done

echo "重命名完成！"