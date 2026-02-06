#version 460 core
layout (local_size_x = 16, local_size_y = 16) in;
layout (rgba32f, binding = 0) uniform image2D screenTex;

uniform vec2 resolution;
uniform vec3 cameraPosition;
uniform mat3 cameraRotation;
uniform int  MAX_TRACE_BOUNCES;
uniform int  MAX_TRACE_PER_PIXEL;
uniform float fov;
uniform int numSpheres;

struct Sphere {
    vec4  positionRadius;           // position (xyz) + radius (w)
    vec4  baseColor;                // baseColor (xyz) + padding (w)
    vec4  emissionColorStrength;    // emissionColor (xyz) + emissionStrength (w)
};

layout (std430, binding = 0) readonly buffer SphereBuffer {
    Sphere spheres[];
};

float FOV = tan(radians(fov) * 0.5);

struct Ray {
    vec3 origin;
    vec3 direction;
};

struct SurfaceMaterial
{
	vec3  baseColor;
    vec3  emissionColor;
    float emissionStrength;
};

struct HitResult {
    bool  hit;
    float dist;
    vec3  position;
    vec3  normal;
    SurfaceMaterial material;
};



float randomValueInt(inout uint state) 
{
    state = state * 747796405 + 2891336453;
    uint result = ((state >> ((state >> 28u) + 4)) ^ state) * 277803737;
    result = (result >> 22) ^ result;
    return result / 4294967296.0;
}

float randomValueNormalDist(inout uint state) 
{
    float theta = 2 * 3.1415926 * randomValueInt(state);
    float rho = sqrt(-2 * log(randomValueInt(state)));
    return rho * cos(theta); 
}

vec3 randomValueVec3(inout uint state) 
{
    float x = randomValueNormalDist(state);
    float y = randomValueNormalDist(state);
    float z = randomValueNormalDist(state);
    return normalize(vec3(x, y, z));
}

vec3 randomValueVec3Hemisphere(inout uint state, vec3 normal) 
{
    vec3 randomDir = randomValueVec3(state);
    return randomDir * sign(dot(randomDir, normal));
}

HitResult RaySphereIntersection(Ray ray, vec3 sphereCenter, float sphereRadius) 
{
    HitResult hitResult;
    hitResult.hit = false;
    
    vec3 offsetRayOrigin = ray.origin - sphereCenter;

    float a = dot(ray.direction, ray.direction);
    float b = 2.0 * dot(offsetRayOrigin, ray.direction);
    float c = dot(offsetRayOrigin, offsetRayOrigin) - sphereRadius * sphereRadius;

    float discriminant = b * b - 4.0 * a * c;

    if(discriminant >= 0) 
    {
        float dist = (-b - sqrt(discriminant)) / (2.0 * a);

        if(dist > 0) 
        {
            hitResult.hit = true;
            hitResult.dist = dist;
            hitResult.position = ray.origin + ray.direction * dist;
            hitResult.normal = normalize(hitResult.position - sphereCenter);
        }
    }
    
    return hitResult;
}

HitResult CalculateRayCollision(Ray ray)
{
    // Find closest sphere hit
    HitResult hitResult;
    hitResult.hit = false;
    hitResult.dist = 1e10;
    hitResult.position = vec3(0.0);
    hitResult.normal = vec3(0.0);
    hitResult.material.baseColor = vec3(0.0);
    hitResult.material.emissionColor = vec3(0.0);
    hitResult.material.emissionStrength = 0.0;

    for (int i = 0; i < numSpheres; ++i) {
        vec3 spherePos = spheres[i].positionRadius.xyz;
        float sphereRadius = spheres[i].positionRadius.w;
        
        HitResult hit = RaySphereIntersection(ray, spherePos, sphereRadius);
        if (hit.hit && hit.dist < hitResult.dist && hit.dist > 0.001) {  // Avoid self-intersection
            hitResult = hit;
            hitResult.material.baseColor = spheres[i].baseColor.xyz;
            hitResult.material.emissionColor = spheres[i].emissionColorStrength.xyz;
            hitResult.material.emissionStrength = spheres[i].emissionColorStrength.w;
        }
    }

    return hitResult;
}

vec3 Trace(Ray ray, inout uint state) 
{
    vec3 incomingLight = vec3(0.0);
    vec3 rayColor = vec3(1.0);

    for(int i = 0; i < MAX_TRACE_BOUNCES; i++)
    {
        HitResult hitResult = CalculateRayCollision(ray);
        if(hitResult.hit) 
        {
            ray.origin = hitResult.position + hitResult.normal * 0.01;  // Offset to avoid self-intersection
            ray.direction = randomValueVec3Hemisphere(state, hitResult.normal);

            SurfaceMaterial material = hitResult.material;
            vec3 emittedLight = material.emissionColor * material.emissionStrength;
            incomingLight += emittedLight * rayColor;
            rayColor *= material.baseColor;
            
            // Russian roulette: stop tracing if ray color becomes too dark
            float maxComponent = max(max(rayColor.r, rayColor.g), rayColor.b);
            if (maxComponent < 0.1) break;
        }
        else 
        {
            // Add background color when ray doesn't hit anything
            incomingLight += vec3(0.1) * rayColor;  // Ambient light
            break;
        }
    }

    return incomingLight;
}


void main()
{
    ivec2 pixelCoords = ivec2(gl_GlobalInvocationID.xy);    // Pixel index 0 to 1920 for fHD
    vec2  uvCoords = (pixelCoords / resolution) * 2.0 - 1.0; // Screen coordinates from -1 to 1
    
    // Better seed for random number generator per pixel
    uint rngState = uint(pixelCoords.x * 73856093 ^ pixelCoords.y * 19349663);

    float aspectRation = resolution.x / resolution.y;       // Aspect ration correction for non-square screens
    uvCoords.x *= aspectRation;                             // Correct the UV coordinates for the aspect ratio (prevents stretching)

    Ray ray;
    ray.origin = cameraPosition;
    ray.direction = normalize(cameraRotation * vec3(uvCoords * FOV, 1.0)); 

    vec3 totalIncomingLight = vec3(0.0);

    for(int rayIndex = 0; rayIndex < MAX_TRACE_PER_PIXEL; rayIndex++) {
        totalIncomingLight += Trace(ray, rngState);
    }

    vec3 pixelColor = totalIncomingLight / float(MAX_TRACE_PER_PIXEL); // Average the color from multiple rays per pixel

    imageStore(screenTex, pixelCoords, vec4(pixelColor, 1.0));
}