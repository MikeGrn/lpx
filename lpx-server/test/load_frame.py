import urllib.request
import urllib.response
import struct


response = urllib.request.urlopen("http://localhost:8888/stream?stream_time=1529488179412403&frame_time=0")
contents = response.read()
response.close()

frames_cnt = struct.unpack("<I", contents[0:4])
print(frames_cnt)

index_len = len(str("0"))
name_len = index_len + 1
header = name_len + 8
offset = 4
to = offset + header
(name, size) = struct.unpack("<" + str(name_len) + "sQ", contents[offset:to])
print(name.decode("ascii"))
print(size)

frameFd = open("/home/azhidkov/tmp/out.bmp", "wb")
n=frameFd.write(contents[to:to + size])
