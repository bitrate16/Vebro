#pragma once

const wchar_t* VERSION = L"1.0";

// Hello, shadertoy.com
const char* defaultMainShader = R"glsl(#version 330 core
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

// Function for pure glsl code
void main(){vec4 color=vec4(0.0,0.,0.,1.);mainImage(color,gl_FragCoord.xy);color.rgb=clamp(color.rgb,0.,1.);color.w=1.0;out_FragColor=color;})glsl";

const char* defaultBufferShader = R"glsl(#version 330 core
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
uniform vec4      iDate;                 // Year, month, day, time (seconds)
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

// Function for pure glsl code
void main(){vec4 color=vec4(0.0,0.,0.,0.);mainImage(color,gl_FragCoord.xy);color=clamp(color,0.,1.);out_FragColor=color;})glsl";