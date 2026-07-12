const fs = require('fs');
const path = require('path');

const defaultConfigPath = path.resolve(__dirname, '../conf/verify.json');
const configPath = process.env.WIM_VERIFY_CONFIG || defaultConfigPath;
let config = JSON.parse(fs.readFileSync(configPath, 'utf8'));

function envOrConfig(envName, value) {
  return process.env[envName] !== undefined ? process.env[envName] : value;
}

function optionalSecret(value) {
  return value === '' || value === null || value === undefined ? undefined : value;
}

let email_user = envOrConfig('WIM_VERIFY_EMAIL_USER', config.email.user);
let email_pass = optionalSecret(envOrConfig('WIM_VERIFY_EMAIL_PASS', config.email.pass));
let mysql_host = envOrConfig('WIM_VERIFY_MYSQL_HOST', config.mysql.host);
let mysql_port = Number(envOrConfig('WIM_VERIFY_MYSQL_PORT', config.mysql.port));
let mysql_passwd = optionalSecret(
  envOrConfig('WIM_VERIFY_MYSQL_PASSWORD', config.mysql.passwd)
);
let redis_host = envOrConfig('WIM_VERIFY_REDIS_HOST', config.redis.host);
let redis_port = Number(envOrConfig('WIM_VERIFY_REDIS_PORT', config.redis.port));
let redis_passwd = optionalSecret(
  envOrConfig('WIM_VERIFY_REDIS_PASSWORD', config.redis.passwd)
);
let code_prefix = 'code_';

module.exports = {
  email_pass,
  email_user,
  mysql_host,
  mysql_port,
  mysql_passwd,
  redis_host,
  redis_port,
  redis_passwd,
  code_prefix,
};
