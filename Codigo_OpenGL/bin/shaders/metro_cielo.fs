#version 330 core
// Skybox: igual que 10_fragment_cubemap.fs del laboratorio
out vec4 FragColor;

in vec3 TexCoords;

uniform samplerCube skybox;

void main()
{
    FragColor = texture(skybox, TexCoords);
}
