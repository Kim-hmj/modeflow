#!/bin/bash

rm /data/date.log
while true 
do
    date >> /data/date.log
    date
    sync
    sleep 1
done
