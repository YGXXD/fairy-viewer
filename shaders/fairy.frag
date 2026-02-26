#version 450

/*
    shadertoy.com input variables
    uniform vec3      iResolution;           // viewport resolution (in pixels)
    uniform float     iTime;                 // shader playback time (in seconds)
    uniform float     iTimeDelta;            // render time (in seconds)
    uniform float     iFrameRate;            // shader frame rate
    uniform int       iFrame;                // shader playback frame
    uniform float     iChannelTime[4];       // channel playback time (in seconds)
    uniform vec3      iChannelResolution[4]; // channel resolution (in pixels)
    uniform vec4      iMouse;                // mouse pixel coords. xy: current (if MLB down), zw: click
    uniform samplerXX iChannel0..3;          // input channel. XX = 2D/Cube
    uniform vec4      iDate;                 // (year, month, day, time in seconds)
*/

layout(set = 0, binding = 0) uniform input00 {
    vec3 iResolution;
} s0b0;

layout(set = 0, binding = 1) uniform input01 {
    float iTime;
} s0b1;

layout(set = 0, binding = 2) uniform input02 {
    float iTimeDelta;
} s0b2;

layout(set = 0, binding = 3) uniform input03 {
    float iFrameRate;
} s0b3;

layout(set = 0, binding = 4) uniform input04 {
    int iFrame;
} s0b4;

layout(set = 0, binding = 5) uniform input05 {
    vec4 iMouse;
} s0b5;

layout(set = 0, binding = 6) uniform input06 {
    vec4 iDate;
} s0b6;

vec3 iResolution = s0b0.iResolution;
float iTime = s0b1.iTime;
float iTimeDelta = s0b2.iTimeDelta;
float iFrameRate = s0b3.iFrameRate;
int iFrame = s0b4.iFrame;
vec4 iMouse = s0b5.iMouse;
vec4 iDate = s0b6.iDate;

void mainImage(out vec4 fragColor, in vec2 fragCoord)
{
    fragColor = vec4(0.0);
}

layout(location = 0) out vec4 outColor;

void main() 
{
    vec2 fragCoord = vec2(gl_FragCoord.x, iResolution.y - gl_FragCoord.y);
    mainImage(outColor, fragCoord);
}
