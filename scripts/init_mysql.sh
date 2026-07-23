#!/usr/bin/env bash
set -euo pipefail

# 作用：
#   初始化本地 MySQL 测试数据库和应用账号。
#   会创建/授权 WIMI_DB_USER，并导入 scripts/sql/init_message_mysql_test.sql，
#   因此会重置测试库中的表结构和种子数据。
# 常用方式：
#   ./scripts/init_mysql.sh
# 可选环境变量：
#   MYSQL_ROOT_CMD 指定管理命令，MYSQL_HOST/MYSQL_PORT/WIMI_DB_* 指定连接信息。

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

: "${MYSQL_ROOT_CMD:=sudo mysql}"
: "${MYSQL_HOST:=127.0.0.1}"
: "${MYSQL_PORT:=3306}"
: "${WIMI_DB:=chatServ}"
: "${WIMI_DB_USER:=zorjen}"
: "${WIMI_DB_PASSWORD:=root}"

$MYSQL_ROOT_CMD <<SQL
CREATE USER IF NOT EXISTS '$WIMI_DB_USER'@'localhost' IDENTIFIED BY '$WIMI_DB_PASSWORD';
CREATE USER IF NOT EXISTS '$WIMI_DB_USER'@'%' IDENTIFIED BY '$WIMI_DB_PASSWORD';
GRANT ALL PRIVILEGES ON $WIMI_DB.* TO '$WIMI_DB_USER'@'localhost';
GRANT ALL PRIVILEGES ON $WIMI_DB.* TO '$WIMI_DB_USER'@'%';
FLUSH PRIVILEGES;
SQL

mysql --protocol=tcp -h"$MYSQL_HOST" -P"$MYSQL_PORT" \
  -u"$WIMI_DB_USER" -p"$WIMI_DB_PASSWORD" \
  < "$ROOT_DIR/scripts/sql/init_message_mysql_test.sql"

mysql --protocol=tcp -h"$MYSQL_HOST" -P"$MYSQL_PORT" \
  -u"$WIMI_DB_USER" -p"$WIMI_DB_PASSWORD" \
  -e "SELECT COUNT(*) AS users FROM $WIMI_DB.users; SELECT COUNT(*) AS messages FROM $WIMI_DB.messages;"
