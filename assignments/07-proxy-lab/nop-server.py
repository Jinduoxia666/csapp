#!/usr/bin/python

# nop-server.py —— 用于并发测试，制造队头阻塞。
# 接受一个连接后，
# 持续空转，不返回响应。
#
# 用法：nop-server.py <端口>
#
import socket
import sys

# 创建 IPv4 流式 socket
serversocket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
serversocket.bind(('', int(sys.argv[1])))
serversocket.listen(5)

while 1:
  channel, details = serversocket.accept()
  while 1:
    continue
