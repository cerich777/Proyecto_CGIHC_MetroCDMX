#version 330 core
// Paisaje en movimiento: se recorre la coordenada U con el tiempo
out vec4 FragColor;
in vec2 TexCoords;

uniform sampler2D paisaje;
uniform float desplazamiento;   // crece con el tiempo
uniform float sentido;          // -1 / +1: cada lado ve el paisaje en su direccion

void main()
{
    // 0.35 = parte de la panoramica que cabe en el plano; v se limita a la franja del horizonte
    float u = TexCoords.x * 0.35 * sentido + desplazamiento;
    float v = 0.30 + TexCoords.y * 0.40;
    FragColor = vec4(texture(paisaje, vec2(u, 1.0 - v)).rgb, 1.0);
}
