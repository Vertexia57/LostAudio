#version 460 core
#define PI 3.141592653589

in vec3 fragPos;
in vec2 fragTexCoord;
in vec4 fragColor;
in vec3 fragNormal;
in vec3 fragTangent;
in vec3 fragWorldNormal;

in mat4 fragModel;
in mat3 TBN;

layout (location=0) out vec4 colorOut;
layout (location=1) out vec4 emissive;

uniform vec2 resolution;
uniform float radius = 16.0f;
uniform int axis;

uniform sampler2D colorTex;
uniform sampler2D emissiveTex;

vec4 calcEmissive() 
{
    // // GAUSSIAN BLUR SETTINGS {{{
    // float Directions = directions; // BLUR DIRECTIONS (Default 16.0 - More is better but slower)
    // float Quality = quality; // BLUR QUALITY (Default 4.0 - More is better but slower)
    // float Size = radius; // BLUR SIZE (Radius)

    // // GAUSSIAN BLUR SETTINGS }}}
   
    // vec2 Radius = Size/vec2(resolution.x, resolution.y);
    
    // // Normalized pixel coordinates (from 0 to 1)
    // vec2 uv = fragTexCoord;

    // // Pixel colour
    // vec4 EmissiveColor = texture(emissiveTex, uv);
    // vec4 EmissiveBlurColour = EmissiveColor;

    // // Blur calculations
    // for( float d = 0.0; d < 2.0f * PI; d += 2.0f * PI / Directions)
    // {
	// 	for(float i = 1.0 / Quality; i <= 1.0; i += 1.0 / Quality)
    //     {
	// 	    EmissiveBlurColour += texture(emissiveTex, uv + vec2(cos(d), sin(d)) * Radius * i);		
    //     }
    // }
    
    // // Output to screen
    // EmissiveBlurColour /= Quality * Directions - 15.0;
    // return max(EmissiveColor, EmissiveBlurColour);

    float r = radius;
    float x, y, rr = r*r, d, w, w0;
    vec2 p = fragTexCoord;
    vec4 col = vec4(0.0, 0.0, 0.0, 0.0);
    w0 = 0.5135 / pow(r, 0.96);
    if (axis == 0) for(d = 1.0 / resolution.x, x =- r, p.x += x * d; x <= r; x++, p.x += d) {w = w0 * exp((-x * x) / (2.0 * rr)); col += texture2D(emissiveTex, p) * w; }
    if (axis == 1) for(d = 1.0 / resolution.y, y =- r, p.y += y * d; y <= r; y++, p.y += d) {w = w0 * exp((-y * y) / (2.0 * rr)); col += texture2D(emissiveTex, p) * w; }
    return col;
}

void main()
{
    if (axis == 2) colorOut = min(texture(colorTex, fragTexCoord) + texture(emissiveTex, fragTexCoord), vec4(1.0f));
    else
    {   
        colorOut = texture(colorTex, fragTexCoord);
        emissive = min(calcEmissive(), vec4(1.0f));
    }
}