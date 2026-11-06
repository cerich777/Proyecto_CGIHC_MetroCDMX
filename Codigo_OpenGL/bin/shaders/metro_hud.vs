#version 330 core
// HUD 2D (mira al centro y fundido a negro). Las coordenadas ya vienen en
// espacio de pantalla normalizado (-1..1), por eso no hay matrices.
layout (location = 0) in vec2 aPos;

uniform vec2 escala;    // para que la mira no se deforme con el ancho de la ventana

void main()
{
    gl_Position = vec4(aPos * escala, 0.0, 1.0);
}
