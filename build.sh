#!/bin/bash

rm -rf mySeverLog
rm -rf client
make clean
make server
make client
./server