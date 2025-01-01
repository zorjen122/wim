#!/bin/bash

service mysql start &
service redis-server start &

wait
