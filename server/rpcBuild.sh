#!/bin/bash

# 检查是否传入了 Proto 文件路径
if [ -z "$1" ] || [ -z "$2" ]; then
  echo "Usage: $0 <path/to/proto/file.proto> <output_directory>"
  exit 1
fi

# 设置环境变量
PROTOC_PATH="/usr/local/bin"
GRPC_PLUGIN_PATH="/usr/local/bin"
PROTO_FILE="$1"  # 使用传入的第一个参数作为 Proto 文件路径
OUTPUT_DIR="$2"  # 使用传入的第二个参数作为输出目录

# 获取 Proto 文件所在的目录
PROTO_DIR=$(dirname "$PROTO_FILE")
# 获取 Proto 文件的文件名（不带路径）
PROTO_FILENAME=$(basename "$PROTO_FILE")

# 创建输出目录（如果不存在）
mkdir -p "$OUTPUT_DIR"

echo "Generating gRPC code for $PROTO_FILE..."
"${PROTOC_PATH}/protoc" -I"$PROTO_DIR" --grpc_out="$OUTPUT_DIR" --plugin=protoc-gen-grpc="${GRPC_PLUGIN_PATH}/grpc_cpp_plugin" "$PROTO_FILENAME"

echo "Generating C++ code for $PROTO_FILE..."
"${PROTOC_PATH}/protoc" -I"$PROTO_DIR" --cpp_out="$OUTPUT_DIR" "$PROTO_FILENAME"

echo "Done. Generated files are in $OUTPUT_DIR."