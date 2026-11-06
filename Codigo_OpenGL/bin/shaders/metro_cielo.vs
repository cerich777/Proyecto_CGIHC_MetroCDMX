#version 330 core
// Skybox (cubemap de ambiente). Igual que 10_vertex_cubemap.vs del laboratorio,
// pero se dibuja al final con profundidad maxima para no tapar la escena.
layout (location = 0) in vec3 aPos;

out vec3 TexCoords;

uniform mat4 projection;
uniform mat4 view;   // vista SIN traslacion: el cielo siempre rodea a la camara

void main()
{
    TexCoords = aPos;
    vec4 pos = projection * view * vec4(aPos, 1.0);
    gl_Position = pos.xyww;   // z = w  ->  profundidad 1.0 (lo mas lejano)
}
