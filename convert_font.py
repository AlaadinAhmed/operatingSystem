import sys

def convert_to_header(input_file, output_file, array_name):
    with open(input_file, 'rb') as f:
        data = f.read()

    with open(output_file, 'w') as f:
        f.write(f'#ifndef {array_name.upper()}_H\n')
        f.write(f'#define {array_name.upper()}_H\n\n')
        f.write('#include <stdint.h>\n\n')
        f.write(f'static const unsigned char {array_name}[] = {{\n')
        
        for i, byte in enumerate(data):
            f.write(f'0x{byte:02x}, ')
            if (i + 1) % 16 == 0:
                f.write('\n')
        
        f.write('\n};\n\n')
        f.write(f'static const unsigned int {array_name}_len = {len(data)};\n\n')
        f.write('#endif\n')

if __name__ == '__main__':
    convert_to_header('Roboto-Regular.ttf', 'src/drivers/roboto_font.h', 'roboto_font')
