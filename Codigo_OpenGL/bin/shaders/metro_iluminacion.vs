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
layout (location = 3) in ivec4 aArticulaciones;
layout (location = 4) in vec4 aPesos;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform mat3 matrizNormal;   // transpuesta de la inversa de 'model' (se calcula en la CPU)
uniform bool usaPiel;
uniform mat4 pose[12];

out vec3 posMundo;
out vec3 normalMundo;
out vec2 TexCoords;

void main()
{
    mat4 piel = mat4(1.0);
    float peso = dot(aPesos, vec4(1.0));
    if (usaPiel && peso > 0.0)
        piel = (pose[aArticulaciones.x] * aPesos.x + pose[aArticulaciones.y] * aPesos.y +
                pose[aArticulaciones.z] * aPesos.z + pose[aArticulaciones.w] * aPesos.w) / peso;
    vec4 pos = model * piel * vec4(aPos, 1.0);
    posMundo    = pos.xyz;
    normalMundo = normalize(matrizNormal * mat3(piel) * aNormal);
    TexCoords   = aTexCoords;
    gl_Position = projection * view * pos;
}
