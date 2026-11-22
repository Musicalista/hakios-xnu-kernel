import struct

dt_path = r'C:\Users\Usuario\hakios-xnu-kernel\devices\j105m\bootshim\DeviceTree_patched.dec'
with open(dt_path, 'rb') as f:
    dt = f.read()

count = 0
def parse_node(offset, path=''):
    global count
    num_props, num_children = struct.unpack('<II', dt[offset:offset+8])
    p = offset + 8
    node_name = 'unknown'
    for _ in range(num_props):
        pname = dt[p:p+32].split(b'\x00')[0].decode('latin1', errors='ignore')
        raw_len = struct.unpack('<I', dt[p+32:p+36])[0]
        v_off = p + 36
        if raw_len & 0x80000000:
            print(f'Node {path}: prop "{pname}" at offset {hex(p+32)} has RAW_LEN = {hex(raw_len)}')
            count += 1
        if pname == 'name':
            node_name = dt[v_off:v_off+(raw_len & 0x7fffffff)].split(b'\x00')[0].decode('latin1', errors='ignore')
        plen = raw_len & 0x7fffffff
        p += 36 + ((plen + 3) & ~3)
    for _ in range(num_children):
        p = parse_node(p, f'{path}/{node_name}')
    return p

parse_node(0)
print(f'Total properties with bit 31 set: {count}')
