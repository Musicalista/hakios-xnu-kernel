import struct
import subprocess
import os

kc_path = r'C:\Users\Usuario\hakios-xnu-kernel\sysimage_armv7\kernelcache.dec'
with open(kc_path, 'rb') as f:
    data = f.read()

magic, cputype, cpusubtype, filetype, ncmds, sizeofcmds, flags = struct.unpack('<7I', data[:28])
print(f"Magic: {hex(magic)}, ncmds: {ncmds}")

offset = 28
target_va = 0x8038783c
slice_file = r'C:\Users\Usuario\hakios-xnu-kernel\tools\trap_slice.bin'

for _ in range(ncmds):
    cmd, cmdsize = struct.unpack('<2I', data[offset:offset+8])
    if cmd == 1: # LC_SEGMENT
        segname = data[offset+8:offset+24].split(b'\x00')[0].decode('ascii', errors='ignore')
        vmaddr, vmsize, fileoff, filesize = struct.unpack('<4I', data[offset+24:offset+40])
        print(f"Seg {segname}: vmaddr={hex(vmaddr)}, vmsize={hex(vmsize)}, fileoff={hex(fileoff)}, filesize={hex(filesize)}")
        if vmaddr <= target_va < vmaddr + vmsize:
            file_pos = fileoff + (target_va - vmaddr)
            print(f"Target {hex(target_va)} is in segment {segname} at file offset {hex(file_pos)}")
            start_va = target_va - 0x40
            start_off = fileoff + (start_va - vmaddr)
            with open(slice_file, 'wb') as sf:
                sf.write(data[start_off:start_off+0x80])
            print(f"Wrote slice from {hex(start_va)} to {slice_file}")
            
            # Disassemble slice with arm-none-eabi-objdump
            objdump = r'C:\Program Files (x86)\Arm GNU Toolchain arm-none-eabi\14.2 rel1\bin\arm-none-eabi-objdump.exe'
            res = subprocess.run([
                objdump, '-D', '-b', 'binary', '-m', 'arm', '-M', 'force-thumb',
                f'--adjust-vma={start_va}', slice_file
            ], capture_output=True, text=True)
            print(res.stdout)
    offset += cmdsize
