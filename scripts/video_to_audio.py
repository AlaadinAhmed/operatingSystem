#!/usr/bin/env python3
"""
Video to Audio Downloader
Downloads video from URL and extracts audio in various formats.
Requires: yt-dlp, ffmpeg
Install: pip install yt-dlp
"""

import subprocess
import sys
import os
import argparse
import shutil

def check_dependencies():
    """Check if required tools are installed."""
    for tool in ['yt-dlp', 'ffmpeg']:
        if not shutil.which(tool):
            print(f"Error: {tool} is not installed.")
            print(f"Install with: {'pip install yt-dlp' if tool == 'yt-dlp' else 'sudo pacman -S ffmpeg'}")
            sys.exit(1)

def download_audio(url: str, output_format: str = 'wav', output_dir: str = '.', 
                   sample_rate: int = 48000, channels: int = 2) -> str:
    """
    Download video and extract audio in specified format.
    
    Args:
        url: Video URL (YouTube, Vimeo, etc.)
        output_format: Audio format (wav, mp3, flac, etc.)
        output_dir: Output directory
        sample_rate: Audio sample rate in Hz (default: 48000 for HDA compatibility)
        channels: Number of audio channels (default: 2 for stereo)
    
    Returns:
        Path to the downloaded audio file
    """
    os.makedirs(output_dir, exist_ok=True)
    
    # Output template
    output_template = os.path.join(output_dir, '%(title)s.%(ext)s')
    
    # yt-dlp command with audio extraction
    cmd = [
        'yt-dlp',
        '-x',  # Extract audio
        '--audio-format', output_format,
        '--audio-quality', '0',  # Best quality
        '-o', output_template,
        '--postprocessor-args', 
        f'ffmpeg:-ar {sample_rate} -ac {channels}',  # Resample
        '--no-playlist',  # Single video only
        '--print', 'after_move:filepath',  # Print final path
        url
    ]
    
    print(f"Downloading: {url}")
    print(f"Format: {output_format}, {sample_rate}Hz, {channels}ch")
    
    result = subprocess.run(cmd, capture_output=True, text=True)
    
    if result.returncode != 0:
        print(f"Error: {result.stderr}")
        sys.exit(1)
    
    # Get the output file path from yt-dlp
    output_path = result.stdout.strip().split('\n')[-1]
    print(f"Saved: {output_path}")
    
    return output_path

def main():
    parser = argparse.ArgumentParser(
        description='Download video and extract audio',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog='''
Examples:
  %(prog)s https://youtube.com/watch?v=...
  %(prog)s https://youtube.com/watch?v=... -f mp3
  %(prog)s https://youtube.com/watch?v=... -o ./audio -r 44100
        '''
    )
    
    parser.add_argument('url', help='Video URL')
    parser.add_argument('-f', '--format', default='wav',
                        choices=['wav', 'mp3', 'flac', 'ogg', 'aac'],
                        help='Output audio format (default: wav)')
    parser.add_argument('-o', '--output', default='.',
                        help='Output directory (default: current)')
    parser.add_argument('-r', '--rate', type=int, default=48000,
                        help='Sample rate in Hz (default: 48000)')
    parser.add_argument('-c', '--channels', type=int, default=2,
                        choices=[1, 2], help='Channels: 1=mono, 2=stereo (default: 2)')
    
    args = parser.parse_args()
    
    check_dependencies()
    download_audio(args.url, args.format, args.output, args.rate, args.channels)

if __name__ == '__main__':
    main()
