import struct

dt_path = r'C:\Users\Usuario\hakios-xnu-kernel\devices\j105m\bootshim\DeviceTree_patched.dec'
with open(dt_path, 'rb') as f:
    dt = f.read()

def parse_node(offset, path=''):
    num_props, num_children = struct.unpack('<II', dt[offset:offset+8])
    p = offset + 8
    node_name = 'unknown'
    props = []
    for _ in range(num_props):
        pname = dt[p:p+32].split(b'\x00')[0].decode('latin1', errors='ignore')
        plen = struct.unpack('<I', dt[p+32:p+36])[0] & 0x7fffffff
        v_off = p + 36
        val = dt[v_off:v_off+plen]
        props.append((pname, val, plen, v_off))
        if pname == 'name':
            node_name = val.split(b'\x00')[0].decode('latin1', errors='ignore')
        p += 36 + ((plen + 3) & ~3)
    
    curr = f'{path}/{node_name}'
    for pname, val, plen, v_off in props:
        if 'freq' in pname.lower() or 'timebase' in pname.lower() or 'clock' in pname.lower():
            if plen == 4:
                int_val = struct.unpack('<I', val)[0]
                print(f'{curr} -> {pname}: {int_val} ({hex(int_val)}) at {hex(v_off)}')
            elif plen == 8:
                i1, i2 = struct.unpack('<II', val)
                print(f'{curr} -> {pname}: {i1}, {i2} at {hex(v_off)}')
            else:
                print(f'{curr} -> {pname} (len {plen}) at {hex(v_off)}')
    
    for _ in range(num_children):
        p = parse_node(p, curr)
    return p

parse_node(0)
