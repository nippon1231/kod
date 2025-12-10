#!/usr/bin/env python3
# Generate levelMap data files

MAP_WIDTH = 191
MAP_HEIGHT = 20

# Level 2
with open("src/lvl2_data.c", "w") as f:
    f.write('#include <genesis.h>\n')
    f.write('#include "lvl1.h"\n\n')
    f.write(f'const u16 levelMap2[MAP_WIDTH*MAP_HEIGHT] = {{\n')
    
    # Lines 0-12: all zeros
    for line in range(13):
        values = ','.join(['0'] * MAP_WIDTH)
        f.write(f'{values},\n')
    
    # Lines 13-19: all ones
    for line in range(13, 20):
        values = ','.join(['1'] * MAP_WIDTH)
        if line == 19:  # last line, no comma
            f.write(f'{values}\n')
        else:
            f.write(f'{values},\n')
    
    f.write('};\n')

print("lvl2_data.c generated")

# Level 3
with open("src/lvl3_data.c", "w") as f:
    f.write('#include <genesis.h>\n')
    f.write('#include "lvl1.h"\n\n')
    f.write(f'const u16 levelMap3[MAP_WIDTH*MAP_HEIGHT] = {{\n')
    
    # Lines 0-12: all zeros
    for line in range(13):
        values = ','.join(['0'] * MAP_WIDTH)
        f.write(f'{values},\n')
    
    # Lines 13-19: all ones
    for line in range(13, 20):
        values = ','.join(['1'] * MAP_WIDTH)
        if line == 19:  # last line, no comma
            f.write(f'{values}\n')
        else:
            f.write(f'{values},\n')
    
    f.write('};\n')

print("lvl3_data.c generated")
