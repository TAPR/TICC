#!/usr/bin/env python3

###############################  N8UR multiTICC  ###############################
#
#	Copyright 2019 by John Ackermann, N8UR jra@febo.com https://febo.com
#	Version number can be found in the ashglobal.py file
#
#	This program is free software; you can redistribute it and/or modify
#	it under the terms of the GNU General Public License as published by
#	the Free Software Foundation; either version 2 of the License, or
#	(at your option) any later version.
#
#	This program is distributed in the hope that it will be useful,
#	but WITHOUT ANY WARRANTY; without even the implied warranty of
#	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#	GNU General Public License for more details.
#
################################################################################

############################   multi-ticc.py    ################################

import sys
import time
import queue
import threading
import socket
import serial         # install python3-serial
import argparse       # install python3-argparse

version = '20191217.1'

HOST = '0.0.0.0'

def get_data(s,q):
    min_line_length = 18  # serial line shorter than this is bogus
    # get first possibly partial line and throw away
    line = s.readline()
    while True:
        try:
            line = s.readline().decode('ascii').strip()
            # ignore comments and short lines
            if (not line.startswith('#')) and (len(line) >= min_line_length):
                if not q.full():
                    q.put(line)
        except serial.SerialException:
            print("Oops serial port error:", sys.exc_info()[0])
            sys.exit(1)

def ch_handler(channels,inqueue,qlist):
    while True:
        line = inqueue.get()
        if line is None:
            break
        if not qlist[0].full():
            qlist[0].put(line)
        if not qlist[1].full():
            qlist[1].put(line)
        for index,x in enumerate(channels):
            if x in line:
                if not qlist[index + 2].full():
                    qlist[index + 2].put(line)
        inqueue.task_done()
    
def demuxed_port_listener(base_tcp_port,index,q,channels):
    tcp_port = base_tcp_port + index
    print("Listening and ready to send",
                channels[index - 2],"on port",tcp_port)

    while True:
        s = socket.socket(socket.AF_INET,socket.SOCK_STREAM)
        s.bind((HOST, tcp_port))
        s.listen(1)
        while True:
            conn, addr = s.accept()
            print("Client connection accepted from", addr)
            try:
                # empty the queue so we don't have gaps in data
                while not q.empty():
                    q.get(False)
                while True:
                    line = q.get()
                    if line is None:
                        break
                    line = line + '\n'
                    line = bytes(line,'ascii')
                    conn.send(line)
                    q.task_done()
            except socket.error as msg:
                print("Client connection closed by",addr)
                conn.close()
            conn.close()

def muxed_port_listener(tcp_port,q):
    print("Listening and ready to send multiplexed data on port",tcp_port)

    while True:
        s = socket.socket(socket.AF_INET,socket.SOCK_STREAM)
        s.bind((HOST, tcp_port))
        s.listen(1)
        while True:
            conn, addr = s.accept()
            print("Client connection accepted from", addr)
            try:
                # empty the queue so we don't have gaps in data
                while not q.empty():
                    q.get(False)
                while True:
                    line = q.get()
                    line = line + '\n'
                    line = bytes(line,'ascii')
                    conn.send(line)
                    q.task_done()
            except socket.error as msg:
                print("Client connection closed by",addr)
                conn.close()
            conn.close()

def sorted_port_listener(tcp_port,q):
    print("Listening and ready to send sorted data on port",tcp_port)

    while True:
        s = socket.socket(socket.AF_INET,socket.SOCK_STREAM)
        s.bind((HOST, tcp_port))
        s.listen(1)
        while True:
            conn, addr = s.accept()
            print("Client connection accepted from", addr)
            try:
                # empty the queue so we don't have gaps in data
                while not q.empty():
                    q.get(False)
                while True:
                    # this is a priority (sorted) queue, so keep a
                    # minimum size so we can successfully sort the output
                    if q.qsize() > 20:
                        line = q.get()
                        line = line + '\n'
                        line = bytes(line,'ascii')
                        conn.send(line)
                        q.task_done()
            except socket.error as msg:
                print("Client connection closed by",addr)
                conn.close()
            conn.close()

###############################################################################
# Main routine
###############################################################################
def main():

    print("N8UR multi_ticc.py -- version",version)
    # get params; use default if not specified on command line
    a = argparse.ArgumentParser()
    a.add_argument('--comports',nargs='*',type=str,
        default = ['/dev/ttyTICC0','/dev/ttyTICC1',
            '/dev/ttyTICC2','/dev/ttyTICC3'])
    a.add_argument('--chnames',nargs='*',type=str,
        default = ['chA','chB','chC','chD','chE','chF','chG','chH'])
    a.add_argument('--baseport',type=int,default = 9190)
    a.add_argument('--baudrate',type=int,default = 115200)

    args = a.parse_args()
    hwports = args.comports
    channels = args.chnames
    base_tcp_port = args.baseport
    baudrate = str(args.baudrate)    # convert to string for serial object

    # open serial ports using a thread for each; dump into serqueue
    serports = []
    serthreads = []
    serqueue = queue.Queue(20)
    for x in hwports:
        try:
            print("Attempting to open", x, "...")
            p = serial.Serial(x,baudrate)
            serports.append(p)
            thread = threading.Thread(target=get_data, 
                args=(p,serqueue),daemon=True)
            thread.start()
            serthreads.append(thread)
        except serial.SerialException:
            print("Oops... error", sys.exc_info()[0],
                    "occured opening", x,"!")
            sys.exit(1)

    # make output queues
    ch_queues = []
    # First is a multiplexed queue with all data, FIFO
    ch_queues.append(queue.Queue(5))

    # Next is a priority queue to dump channel data in time
    # sequence so we make it a bit bigger to allow for sorting
    ch_queues.append(queue.PriorityQueue(50))

    # then add a queue for each channel
    for x in channels:
        ch_queues.append(queue.Queue(5))

    # start the handler that dumps data into proper queue
    ch_queues_thread = threading.Thread(target=ch_handler,
        args=(channels,serqueue,ch_queues),daemon=True)
    ch_queues_thread.start()

    # listen and dump data to tcp_port on connection
    sockthreads = []
    for index,x in enumerate(ch_queues):
        if index == 0:
            thread = threading.Thread(target=muxed_port_listener, 
                args=(base_tcp_port,x),daemon=True)
        if index == 1:
            thread = threading.Thread(target=sorted_port_listener, 
                args=(base_tcp_port + 1,x),daemon=True)
        if index > 1:
            thread = threading.Thread(target=demuxed_port_listener, 
                args=(base_tcp_port,index,x,channels),daemon=True)
        thread.start()
        sockthreads.append(thread)

    while True:
        try:
            time.sleep(0.01)
        except KeyboardInterrupt:
            print("\nExiting...")
            for x in serports:
                x.close()
            sys.exit(0)

    for x in serports:
        x.close()
    sys.exit(0)

if __name__ == '__main__':
    main()

