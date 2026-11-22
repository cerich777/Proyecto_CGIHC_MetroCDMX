#version 330 core
// HUD 2D (mira al centro y fundido a negro). Las coordenadas ya vienen en
// espacio de pantalla normalizado (-1..1), por eso no hay matrices.
layout (location = 0) in vec2 aPos;

uniform vec2 escala;    // para que la mira no se deforme con el ancho de la ventana
uniform vec2 desplazamiento;
out vec2 uv;

void main()
{
    uv = vec2((aPos.x + 1.0) * 0.5, (1.0 - aPos.y) * 0.5);
    gl_Position = vec4(aPos * escala + desplazamiento, 0.0, 1.0);
}
