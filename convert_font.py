import sys
import readline
import glob

def complete(text, state):
    return (glob.glob(text+'*')+[None])[state]

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
    readline.set_completer_delims(' \t\n;')
    readline.parse_and_bind("tab: complete")
    readline.set_completer(complete)

    input_file = input('Font file to convert (e.g., Roboto-Regular.ttf): ')
    if not input_file:
        input_file = 'Roboto-Regular.ttf'
        print(f"Using default: {input_file}")

    output_file = input('Output header file (e.g., resources/font/roboto_font.h): ')
    if not output_file:
        output_file = 'resource/font/roboto_font.h'
        print(f"Using default: {output_file}")

    array_name = input("Array name (e.g., roboto_font): ")
    if not array_name:
        array_name = 'roboto_font'
        print(f"Using default: {array_name}")

    convert_to_header(input_file, output_file, array_name)
