#!/data/data/com.termux/files/usr/bin/python
# -*- coding: utf-8 -*-

import os
import sys
from subprocess import call


def shellExec(cmd):
    errCode = call(cmd, shell=True)
    if errCode != 0:
        fatalError('failed executing: ' + cmd)


def fatalError(message):
    print('[ERROR] ' + message)
    sys.exit()


os.chdir('/storage/emulated/legacy/tmp/dirtycowy/')

print('Kompilacja dirtycowy.c...')
shellExec('gcc -pthread dirtycowy.c -o dirtycowy')
print('Kompilacja run-as.c...')
shellExec('gcc run-as.c -o run-as.fixed')

print('done')
