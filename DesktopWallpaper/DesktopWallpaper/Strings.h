#pragma once

// Hello, shadertoy.com
const char* defaultShader = R"glsl(
// Standard header

#version 330 core

uniform float iTime;
uniform float iTimeDelta;
uniform int   iFrame;
uniform vec3  iResolution;
uniform vec4  iMouse;

out vec4 fragColor;

// End Standard header


void mainImage( out vec4 fragColor, in vec2 fragCoord )
{
    // Normalized pixel coordinates (from 0 to 1)
    vec2 uv = fragCoord/iResolution.xy;

    // Time varying pixel color
    vec3 col = 0.5 + 0.5*cos(iTime+uv.xyx+vec3(0,2,4));

    // Output to screen
    fragColor = vec4(col,1.0);
}


// Standard footer

void main()
{
    mainImage(fragColor, gl_FragCoord.xy);
}

// End Standard footer
)glsl";

const char* whiteShader = R"glsl(
// Standard header

#version 330 core

uniform float iTime;
uniform float iTimeDelta;
uniform int   iFrame;
uniform vec3  iResolution;
uniform vec4  iMouse;

out vec4 fragColor;

// End Standard header


void mainImage( out vec4 fragColor, in vec2 fragCoord )
{
    fragColor = vec4(1.0);
}


// Standard footer

void main()
{
    mainImage(fragColor, gl_FragCoord.xy);
}

// End Standard footer
)glsl";

const char* sineShader = R"glsl(
// Standard header

#version 330 core

uniform float iTime;
uniform float iTimeDelta;
uniform int   iFrame;
uniform vec3  iResolution;
uniform vec4  iMouse;

out vec4 fragColor;

// End Standard header


void mainImage( out vec4 fragColor, in vec2 fragCoord )
{
	vec2 to = vec2(fragCoord.x, iResolution.y * (sin(iTime * 0.5 + 3.14 * fragCoord.x / iResolution.x) * 0.5 + 0.5));
    if (distance(fragCoord, to) < 10.0)
        fragColor = vec4(1.0);
    else
        fragColor = vec4(0.0);
}


// Standard footer

void main()
{
    mainImage(fragColor, gl_FragCoord.xy);
}

// End Standard footer
)glsl";

const char* mouseShader = R"glsl(
// Standard header

#version 330 core

uniform float iTime;
uniform float iTimeDelta;
uniform int   iFrame;
uniform vec3  iResolution;
uniform vec4  iMouse;

out vec4 fragColor;

// End Standard header


void mainImage( out vec4 fragColor, in vec2 fragCoord )
{
	if (distance(fragCoord, iMouse.xy) < 10.0)
        fragColor = vec4(1.0);
    else
        fragColor = vec4(0.0);
}


// Standard footer

void main()
{
    mainImage(fragColor, gl_FragCoord.xy);
}

// End Standard footer
)glsl";