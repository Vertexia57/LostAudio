#version 460 core

in vec3 fragPos;
in vec2 fragTexCoord;
in vec4 fragColor;
in vec3 fragNormal;
in vec3 fragTangent;
in vec3 fragWorldNormal;

layout(location = 0) out vec4 finalColor;
layout(location = 1) out vec4 emissive;

uniform sampler2D colorTex;
uniform sampler2D emissiveTex;

void main() {
	finalColor = texture(colorTex, fragTexCoord) * fragColor;
    emissive = texture(emissiveTex, fragTexCoord) * fragColor;
}