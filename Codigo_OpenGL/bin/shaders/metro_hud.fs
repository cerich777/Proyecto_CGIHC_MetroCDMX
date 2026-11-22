#version 330 core
out vec4 FragColor;
in vec2 uv;

uniform vec4 color;
uniform int usaTextura;
uniform sampler2D textoHUD;

void main()
{
    FragColor = usaTextura == 1 ? texture(textoHUD, uv) : color;
}
