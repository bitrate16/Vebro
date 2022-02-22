![Vebro](Vebro.svg)

# Vebro
Updated & refactored version of [DesktopWallpaper](github.com/bitrate16/DesktopWallpaper) for GLSL & [shadertoy.com](https://www.shadertoy.com//)

### [Release page](https://github.com/bitrate16/Vebro/releases)

# Overview

* Based on GLSL Shaders in OpenGL
* Contains Debug console window
* Supports commandline options
* Supports Textures
* Supports mouse input (pointer location only)
* Supports [shadertoy.org](https://www.shadertoy.com) like shaders:
	* 4 Buffers (A, B, C, D)
	* 4 Inputs for main and buffer shaders (0, 1, 2, 3)
	* Standard uniforms (iTime, iTimeDelda, iResultion, [e.t.c](#shader-structure))
	* Each shader collection (Main and buffers) can be packed into [Shader Pack](#shader-packs)
* Display selection (One of displays or full total displays viewport)
* Pause / Resume modes
* Buffer clear, Time reset, Inputs reloading

### Application interface

Application is based on Windows tray menu icon item and opens single right click menu containign all controls for the application:
* Primary display - selects display for image output
* Fullscreen - selects if app should render on total avaialble displays area (If you have 2 displays, single image will cover both of them)
* Rescan display - option in case when you plug / unplug your display
* Pause - Nuff said
* Reset time - reset iTime uniform value
* Reset buffers - clear textures of all buffers (fill with black)
* Reload all inputs - force all inputs to be reloaded (example: re,load textures from disk)
* Clear all inputs - remove all inputs of Main and Buffer shaders
* FPS -  set upper limit for FPS
* Enable mouse - enable mouse input (change iMouse values)
* Close pack - close currently opened pack
* Open pack - select and open pack file
* Reload pack - reload currently opened pack
* New pack - create new unsaved pack
* Save pack - save currently opened pack
	* This opetion saves all information about current pack and opened shaders and inputs
	* Main shader and it's inputs paths are saved with paths relative to pack parent folder, same for buffer shaders
* Save pack as - save currently opened pack with other name and location
* Main shader - Main shader properties:
	* Open - Open shader from file
	* Reload - reload curretly opened main shader
	* New - Create new shader file and select location for it
	* Clear inputs - clear inputs only for this shader
	* Input 0 / 1 / 2 / 3 - select input type, **support is limited to Texture, Buffer or None**:
		* None
		* Texture
		* Buffer (A / B / C / D)
		* Audio
		* Microphone
		* Video
		* Webcam
		* Keyboard
* Buffer shader - similar to main shader, but has an option to remove shader of this buffer without render pause
* Enable debug console - show console with debug output, closing console window also closes application
* Exit - e n t e r _ THE _ V.O.I.D.

### **WARNING:** If you don't know, what is GLSL, you're welcome:
* [khronos wiki](https://www.khronos.org/opengl/wiki/Core_Language_(GLSL))
* [shadertoy How To](https://www.shadertoy.com/howto)
* [Shadertoy tutorial by vug](https://www.shadertoy.com/view/Md23DV)
* [google.com, I'm feeling lucky](https://google.com/search?q=glsl%20shadertoy%20tutorial)

# [shadertoy.com](https://www.shadertoy.com) compability

Shadertoy compability is implemented in same uniform names and similar shader structure. Basic "shader" consists of:

* Main shader (Main.glsl) that render to display and may have 0-4 inputs (Input 0 / 1 / 2 / 3).
* 4 Buffer shaders (Buffer A / B / C / D .glsl) that render to texture and is used by other (or the same) shaders, may have 0-4 inputs (Input 0 / 1 / 2 / 3).
* Each input can have a different type, but current **support is limited to Texture, Buffer or None**:
	* None
	* Texture
	* Buffer (A / B / C / D)
	* Audio
	* Microphone
	* Video
	* Webcam
	* Keyboard

Examples of Main shader only: [Example from shadertoy by iq](https://www.shadertoy.com/view/ldl3W8)

Examples of Main and Buffer shaders: [Example from shadertoy by iq](https://www.shadertoy.com/view/3dGSWR)


# Shader structure:

Shader structure is based on similar to shadertoy framework. Each shader (Main or Buffer) contains pre-defined uniforms, fully compatible with shadertoy's:
```glsl
uniform vec3      iResolution;           // Viewport resolution, pixels
uniform float     iTime;                 // Shader playback time, seconds
uniform float     iTimeDelta;            // Shader frame render time
uniform int       iFrame;                // Shader playback, frame number
uniform float     iChannelTime[4];       // Channel playback time, seconds
uniform vec3      iChannelResolution[4]; // Channel resolution, pixels
uniform vec4      iMouse;                // Mouse coords, pixels (xy - current, zw - previous if enabled)
uniform sampler2D iChannel0;             // Input channel 0
uniform sampler2D iChannel1;             // Input channel 1
uniform sampler2D iChannel2;             // Input channel 2
uniform sampler2D iChannel3;             // Input channel 3
uniform vec4      iDate;                 // year, month, day, time (seconds)
uniform float     iSampleRate;           // Sound sample rate (i.e., 44100)
```

User code can be placed into dedicated function `mainImage` from exmaple below by copy-pasting:
```glsl
void mainImage(out vec4 fragColor, in vec2 fragCoord) {
	// User code example:
    vec2 uv = fragCoord / iResolution.xy;
    vec3 col = 0.5 + 0.5 * cos(iTime + uv.xyx + vec3(0, 2, 4));
    fragColor = vec4(col, 1.0);
}
```

Advanced users may use standard GLSL entry point `main()` instead of `mainImage()`:
```glsl
void main() {
	// Pure user code goes here
	out_FragColor = /* some magic */ mainImage(out_FragColor, gl_FragCoord);
}
```

# Example shader

Full example of GLSL Vertex shader is present in this section:
```glsl
// Version definition
#version 330 core

// Required pre-defined uniforms
uniform vec3      iResolution;           // Viewport resolution, pixels
uniform float     iTime;                 // Shader playback time, seconds
uniform float     iTimeDelta;            // Shader frame render time
uniform int       iFrame;                // Shader playback, frame number
uniform float     iChannelTime[4];       // Channel playback time, seconds
uniform vec3      iChannelResolution[4]; // Channel resolution, pixels
uniform vec4      iMouse;                // Mouse coords, pixels (xy - current, zw - previous if enabled)
uniform sampler2D iChannel0;             // Input channel 0
uniform sampler2D iChannel1;             // Input channel 1
uniform sampler2D iChannel2;             // Input channel 2
uniform sampler2D iChannel3;             // Input channel 3
uniform vec4      iDate;                 // year, month, day, time (seconds)
uniform float     iSampleRate;           // Sound sample rate (i.e., 44100)

// Compability, instead of gl_FragColor
out vec4 out_FragColor;

// Function for your shadertoy code
void mainImage(out vec4 fragColor, in vec2 fragCoord) {
    // Normalized pixel coordinates (from 0 to 1)
    vec2 uv = fragCoord / iResolution.xy;
    // Time varying pixel color
    vec3 col = 0.5 + 0.5 * cos(iTime + uv.xyx + vec3(0, 2, 4));
    // Output to screen
    fragColor = vec4(col, 1.0);
}

// Function for pure glsl code, calls mainImage()
void main(){vec4 color=vec4(0.0,0.,0.,1.);mainImage(color,gl_FragCoord.xy);color.rgb=clamp(color.rgb,0.,1.);color.w=1.0;out_FragColor=color;}
```

**Note that some GLSL versions deprecate usage of gl_FragColor**


# Shader Packs

Lazy people like me may ask: "why do I have to import all shaders and inputs each time I open this 500kb app?". No, you don't have to..

Instead, a raw, but already useful format allows you to save your shader set into [JSON](https://www.w3schools.com/whatis/whatis_json.asp) configuration file similar to this one:
```json
{
  "Main": {
    "path": "main.glsl",
    "inputs": [
      {
        "type": "BUFFERA"
      }
    ]
  },
  "BufferA": {
    "path": "BufferA.glsl",
    "inputs": [
      "BufferA",
      {
        "type": "Image",
        "path": "shrek2.png"
      }
    ]
  }
}
```

In this example, Main shader has input 0 equal to Buffer A. Buffer A has input 0 equal to itself and input 1 is an image pointing to image of Shrek.

This format allows setting input equal to string with Buffer name or JSON object with `type` and optional `path` keys.

Currently supported types are:
* Buffer (BufferA, BufferB, BufferC, BufferD), **case insensitive** as string or JSON object with `type` key
* Texture as JSON object with `type` key and `path` key

Unimplemented types are ignored, however invalid type leads to an error during pack loading.

When using automatic pack saving (Save pack button in menu), all paths of shaders are calculated erlative to the parent folder of pack JSON file. 

Example:
```
Assume following:
pack.json full path: C:/aboba/bibib/pack.json
Main.glsl full path: C:/aboba/foo/Main.glsl

So, path of main shader in resulting JSON will be:
Main.glsl relative path ../foo/Main.glsl
```

# Commandline options

This application also supports configuring over command line arguments:
```
help for commandline options (use separately)
 -h, --help         display help
 --display <id>     default display ID (>= 0)
 --fullscreen       enable fullscreen mode
 --fps <fps>        set fps (1-240)
 --mouse            enable mouse input
 --pack             pack json location
 --main             main shader location
 --main:0           main shader Input 0 (type:path), exmaple: image:shrek.png
 --main:1           main shader Input 1 (type:path)
 --main:2           main shader Input 2 (type:path)
 --main:3           main shader Input 3 (type:path)
 --a                Buffer A shader location
 --a:0              Buffer A Input 0 (type:path)
 --a:1              Buffer A Input 1 (type:path)
 --a:2              Buffer A Input 2 (type:path)
 --a:3              Buffer A Input 3 (type:path)
 --b                Buffer B shader location
 --b:0              Buffer B Input 0 (type:path)
 --b:1              Buffer B Input 1 (type:path)
 --b:2              Buffer B Input 2 (type:path)
 --b:3              Buffer B Input 3 (type:path)
 --c                Buffer C shader location
 --c:0              Buffer C Input 0 (type:path)
 --c:1              Buffer C Input 1 (type:path)
 --c:2              Buffer C Input 2 (type:path)
 --c:3              Buffer C Input 3 (type:path)
 --d                Buffer D shader location
 --d:0              Buffer D Input 0 (type:path)
 --d:1              Buffer D Input 1 (type:path)
 --d:2              Buffer D Input 2 (type:path)
 --d:3              Buffer D Input 3 (type:path)
 --debug            enable debug output
```

Currently avaialble types of inputs:
* Buffers: a, b, c, d, case sensitive (exmaple: `--main:0 b`)
* Image: image:filepath.png (example: `--a:1 image:my_swamp/shrek2.png`)
* None: none, when you want to write something to have more arguments

Important note: In case when you open pack and specify shader files, direct shader file names and inputs will overwrite pack's locations, but not the pack file itself

Example usage:
```
Vebro.exe --main Main.glsl --main:0
```

# Shader pack downloading

Shader packs can be easily downloaded using [get-pack.py](https://github.com/bitrate16/Vebro/blob/main/get-pack.py) utility from commandline interface or using [get-pack.bat](https://github.com/bitrate16/Vebro/blob/main/get-pack.bat) script that wraps following command (Warning: EULA):

```batch
python get-pack.py --eula=true --clipboard-url
```

Commandline interface for get-pack.py has the following options and supports getting url or shader id from clipboard (pyperclip module required):

```
usage: get-pack.py [-h] (--url URL | --id ID | --clipboard-url | --clipboard-id) [--output OUTPUT] [--outside] [--eula EULA] [--license]

Download shadertoy.com shader

optional arguments:
  -h, --help       show this help message and exit
  --url URL        url of the shader, example https://www.shadertoy.com/view/AMSyft
  --id ID          id of the shader, example AMSyft
  --clipboard-url  get shader url from clipboard, requires pyperclip module
  --clipboard-id   get shader id from clipboard, requires pyperclip module
  --output OUTPUT  output folder and pack name
  --outside        put pack.json outside of the folder, default is shader name
  --eula EULA      End User License Agreement
  --license        License Agreement
```

Example usage with get-pack.bat:

1. Open [shadertoy.com](https://www.shadertoy.com/)
2. Find any shader that you like
3. Copy its url
4. Run get-pack.bat
5. Open pack in Vebro


# TODO

Todo section:
* Refactoring for code
* Audio, Microphone, VIdeo, Webcam, Keyboard input types
* Microphone and Webcam source selection
* Fix issue causing desktop not refresh after application exit (last rendered frame is not esased and keep showing on desktop)
* Allow inserting shader source into pack file
* Parallel rendering same shader on multiple displays using buffer mirroring
* Mouse click handling
* Shadertoy shader exporting
* ~~Shadertoy shader exporting python script~~

# Credits

* [shadertoy.com](https://www.shadertoy.com//) for motivation
* [lodepng](https://github.com/lvandeve/lodepng) for PNG decoding library
* [nlohmann/json](https://github.com/nlohmann/json) for JSON library
* Khronos Group Inc & OpenGL in common
* [bitrate16](https://github.com/bitrate16) for being myself

# License

```
MIT License

Copyright (c) 2021-2022 bitrate16

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```