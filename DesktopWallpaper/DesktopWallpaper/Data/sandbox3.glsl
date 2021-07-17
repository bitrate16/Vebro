#version 330 core

uniform vec3      iResolution;
uniform float     iTime;
uniform float     iTimeDelta;
uniform int       iFrame;
uniform float     iChannelTime[4];
uniform vec3      iChannelResolution[4];
uniform vec4      iMouse;
uniform sampler2D iChannel0;
uniform sampler2D iChannel1;
uniform sampler2D iChannel2;
uniform sampler2D iChannel3;
uniform vec4      iDate;
uniform float     iSampleRate;

#define fragCoord gl_FragCoord.xy

out vec4 fragColor;

void main() {
	fragColor = texture(iChannel0, fragCoord.xy / iResolution.xy) + texture(iChannel1, fragCoord.xy / iResolution.xy);
}