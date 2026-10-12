import socket
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.bind(("0.0.0.0", 9999))
print("Đang lắng nghe UDP port 9999... Bấm Ctrl+C để dừng")
while True:
  data, addr = sock.recvfrom(2048)
  print(f"[RX từ {addr[0]}]: {data.decode('utf-8')}")