#version 450

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform shaderInputs {
    vec3 iResolution;
} ub;

vec3 iResolution = ub.iResolution;

void mainImage(out vec4 fragColor, in vec2 fragCoord)
{
    vec2 uv = fragCoord / iResolution.xy;
    fragColor = vec4(uv, 0.0, 1.0);
}

void main() 
{
    vec2 fragCoord = vec2(gl_FragCoord.x, iResolution.y - gl_FragCoord.y);
    mainImage(outColor, fragCoord);
}
