#version 330 core
// ---------------------------------------------------------------------------
// metro_iluminacion.vs
// Vertex shader comun para todos los materiales de la escena.
// Basado en 11_PhongShaderMultLights.vs del laboratorio; aqui se trabaja en
// coordenadas de MUNDO para que las luces del techo y el cubemap coincidan.
// ---------------------------------------------------------------------------
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform mat3 matrizNormal;   // transpuesta de la inversa de 'model' (se calcula en la CPU)

out vec3 posMundo;
out vec3 normalMundo;
out vec2 TexCoords;

void main()
{
    vec4 pos = model * vec4(aPos, 1.0);
    posMundo    = pos.xyz;
    normalMundo = normalize(matrizNormal * aNormal);
    TexCoords   = aTexCoords;
    gl_Position = projection * view * pos;
}
