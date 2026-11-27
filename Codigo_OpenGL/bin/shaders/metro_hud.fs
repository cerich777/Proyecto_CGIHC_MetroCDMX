#version 330 core
out vec4 FragColor;
in vec2 uv;

uniform vec4 color;
uniform int usaTextura;
uniform sampler2D textoHUD;
uniform int lupa;          // 1 = lente circular del mapa
uniform vec4 uvLupa;       // xy: esquina en la textura, zw: tamano de la region

void main()
{
    if (lupa == 1) {
        vec2 c = uv * 2.0 - 1.0;
        float r = length(c);
        if (r > 1.0) discard;
        if (r > 0.94) { FragColor = vec4(0.10, 0.12, 0.16, 1.0); return; }   // aro de la lupa
        FragColor = texture(textoHUD, uvLupa.xy + uv * uvLupa.zw);
        return;
    }
    FragColor = usaTextura == 1 ? texture(textoHUD, uv) * vec4(1.0, 1.0, 1.0, color.a) : color;   // alfa global para fundidos
}
