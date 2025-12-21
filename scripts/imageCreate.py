import struct

def create_bmp(filename, width, height):
    # BMP Header: 14 bytes
    # 'BM' (2), Size (4), Reserved (4), Offset to pixels (4)
    file_type = b'BM'
    offset = 14 + 40  # File Header + Info Header
    
    # Each row must be a multiple of 4 bytes
    row_size = (width * 3 + 3) & ~3 
    pixel_data_size = row_size * height
    file_size = offset + pixel_data_size
    
    file_header = struct.pack('<2sIHHI', file_type, file_size, 0, 0, offset)

    # DIB Info Header: 40 bytes (BITMAPINFOHEADER)
    # Size (4), Width (4), Height (4), Planes (2), BPP (2), Compression (4)...
    info_header = struct.pack('<IIIHHIIIIII', 
        40, width, height, 1, 24, 0, pixel_data_size, 2835, 2835, 0, 0)

    with open(filename, 'wb') as f:
        f.write(file_header)
        f.write(info_header)
        
        # Pixel data (Blue, Green, Red)
        for y in range(height):
            row = bytearray()
            for x in range(width):
                # Creating a simple color gradient
                r = (x * 255 // width)
                g = (y * 255 // height)
                b = 255 - r
                row.extend([b, g, r])
            
            # Add padding to make row a multiple of 4 bytes
            row.extend([0] * (row_size - len(row)))
            f.write(row)

if __name__ == "__main__":
    create_bmp("logo.bmp", 64, 64)
    print("logo.bmp created successfully.")
