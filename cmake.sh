#!/bin/bash
cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=1 -S ./ -B build
if [ ! -h compile_commands.json ]
then
	ln -s build/compile_commands.json .
fi
