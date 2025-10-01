# video2ascii
Convert video files to ASCII art animations in the terminal.

## Requirements
- C++17 compiler
- CMake 3.16+
- OpenCV 4.x (core, imgproc, videoio)

## Building
```bash
mkdir build
cd build
cmake ..
cd ..
make
```

## Usage
```bash
./video2ascii <video_path> [options]
```
## Options
`--color=<mode>` — Color mode: `none`, `ansi`, `full` (default: `none`)

`--height=<n>` — Target height in characters [20, 120] (default: 60)

`--width=<n>` — Target width in characters [40, 200] (default: auto)

`--framerate=<n>` — Playback framerate [1-120] (default: auto)

## Examples
```bash
./video2ascii video.mp4
./video2ascii video.mp4 --color=full --height=80
./video2ascii video.mp4 --color=ansi --framerate=30
```