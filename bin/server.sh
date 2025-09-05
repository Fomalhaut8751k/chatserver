#!/bin/bash

gdb -ex "set args 127.0.0.1 6000" \
    -ex "set pagination off" \
    ChatServer