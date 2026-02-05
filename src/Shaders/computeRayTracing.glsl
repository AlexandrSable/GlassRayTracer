#version 460 core
layout (local_size_x = 16, local_size_y = 16) in;
layout (rgba32f, binding = 0) uniform image2D screenTex;

void main()
{
    ivec2 pixelCoords = ivec2(gl_GlobalInvocationID.xy);
    vec3 color = vec3(float(pixelCoords.x) / 800.0, float(pixelCoords.y) / 600.0, 0.5);
    imageStore(screenTex, pixelCoords, vec4(color, 1.0));
}