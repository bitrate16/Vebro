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
	float c = cos(iTime * 0.1);
	float s = sin(iTime * 0.1);
	mat2 rot = mat2(c, -s, s, c);
	
	vec2 frag = iResolution.xy * 0.5 + (fragCoord.xy - iResolution.xy * 0.5) * rot * 2.;
	vec2 uv = frag / iResolution.xy;
	
	vec4 a = texture(iChannel0, fract(sign(mod(uv * 1.1, 2.) - 1.) * uv * 1.1))*0.99;
	vec4 b = texture(iChannel1, clamp(uv, 0.0, 1.0));
	
	fragColor = b.a * b + (1. - b.a) * a;
}