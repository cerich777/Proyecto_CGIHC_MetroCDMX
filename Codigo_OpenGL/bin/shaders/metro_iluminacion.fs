#version 330 core
// ---------------------------------------------------------------------------
// metro_iluminacion.fs
// Dos tecnicas de iluminacion en un mismo shader (se elige por material):
//
//   1) PHONG con multiples luces puntuales + luz direccional
//      -> materiales OPACOS y PLASTICOS (concreto, muros, letreros, conos)
//      Basado en 11_PhongShaderMultLights.fs del laboratorio.
//
//   2) FRESNEL (aproximacion de Schlick) + mapeo de ambiente (cubemap)
//      -> materiales TRASLUCIDOS (cristales) y METALICOS (rieles, soportes)
//      Basado en 11_Fresnel.vs / .fs del laboratorio.
//
// Bloques:
//   A. Entradas y uniforms
//   B. Funcion de una luz puntual (Phong + atenuacion)
//   C. Funcion de la luz direccional (sol que entra por los costados)
//   D. Fresnel (Schlick)
//   E. main(): textura -> Phong -> Fresnel / reflejo -> emision -> niebla
// ---------------------------------------------------------------------------

// ---- A. Entradas y uniforms ------------------------------------------------
in vec3 posMundo;
in vec3 normalMundo;
in vec2 TexCoords;

out vec4 FragColor;

#define MAX_LUCES 10
#define MAT_OPACO      0
#define MAT_PLASTICO   1
#define MAT_METALICO   2
#define MAT_TRASLUCIDO 3
#define MAT_EMISIVO    4
#define MAT_SENAL      5
#define MAT_NEGRO      6

// Material (viene del FBX exportado desde Blender)
uniform vec3  colorDifuso;
uniform vec3  colorEspecular;
uniform vec3  colorEmision;
uniform float brillo;          // exponente especular
uniform float opacidad;
uniform float reflectividad;   // peso del reflejo del cubemap
uniform int   tipoMaterial;
uniform int   usaTextura;
uniform sampler2D texture_diffuse1;
uniform samplerCube cubemapAmbiente;

// Luces puntuales (las lamparas mas cercanas a la camara)
uniform int   numLuces;
uniform vec3  luzPosicion[MAX_LUCES];
uniform vec3  luzColor[MAX_LUCES];     // color * potencia
uniform float luzRadio[MAX_LUCES];     // alcance en metros

// Luz direccional y ambiente
uniform vec3 solDireccion;    // direccion HACIA donde viaja la luz
uniform vec3 solColor;
uniform vec3 luzAmbiente;

// Camara y niebla
uniform vec3  posCamara;
uniform vec3  colorNiebla;
uniform float densidadNiebla;
uniform float alturaCorte;     // vista aerea: se recorta todo lo que este arriba (techo)

// ---- B. Una luz puntual: Phong (ambiental ya va aparte) ---------------------
vec3 LuzPuntual(int i, vec3 N, vec3 V, vec3 baseColor)
{
    vec3  L   = luzPosicion[i] - posMundo;
    float d   = length(L);
    if (d >= luzRadio[i]) return vec3(0.0);   // fuera del alcance: no se calcula nada
    L /= d;
    // Atenuacion suave que llega a cero en el radio de la lampara
    float x   = clamp(1.0 - d / luzRadio[i], 0.0, 1.0);
    float ate = x * x;

    float cosTheta = max(dot(N, L), 0.0);              // componente difusa
    vec3  R        = reflect(-L, N);
    float cosAlpha = max(dot(V, R), 0.0);              // componente especular
    vec3  difusa   = baseColor * cosTheta;
    vec3  especular = colorEspecular * pow(cosAlpha, brillo) * step(0.0, cosTheta);
    return (difusa + especular) * luzColor[i] * ate;
}

// ---- C. Luz direccional ------------------------------------------------------
vec3 LuzDireccional(vec3 N, vec3 V, vec3 baseColor)
{
    vec3  L = normalize(-solDireccion);
    float cosTheta = max(dot(N, L), 0.0);
    vec3  R = reflect(-L, N);
    float cosAlpha = max(dot(V, R), 0.0);
    return (baseColor * cosTheta + colorEspecular * pow(cosAlpha, brillo) * step(0.0, cosTheta)) * solColor;
}

// ---- D. Fresnel (aproximacion de Schlick, como en 11_Fresnel.vs) ------------
float Fresnel(vec3 I, vec3 N, float sesgo, float escala, float potencia)
{
    return clamp(sesgo + escala * pow(1.0 + dot(I, N), potencia), 0.0, 1.0);
}

// ---- E. main -------------------------------------------------------------------
void main()
{
    if (posMundo.y > alturaCorte) discard;     // corte tipo maqueta en la vista aerea

    if (tipoMaterial == MAT_NEGRO) {
        FragColor = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }

    // Color base: textura o color del material
    vec4 base = vec4(colorDifuso, 1.0);
    if (usaTextura == 1) {
        vec4 texel = texture(texture_diffuse1, TexCoords);
        if (texel.a < 0.1) discard;             // recortes (letreros PNG con transparencia)
        base = texel;
    }

    // Los graficos del letrero conservan el contraste al pasar bajo una lampara.
    if (tipoMaterial == MAT_SENAL) {
        float d = length(posCamara - posMundo);
        float visibilidad = exp(-pow(d * densidadNiebla, 2.0));
        FragColor = vec4(mix(colorNiebla, clamp(base.rgb * 0.88 + colorEmision * base.rgb, 0.0, 1.0),
                             visibilidad), opacidad * base.a);
        return;
    }

    vec3 N = normalize(normalMundo);
    vec3 V = normalize(posCamara - posMundo);
    if (!gl_FrontFacing) N = -N;
    // Phong: ambiente + sol + luces puntuales
    vec3 color = luzAmbiente * base.rgb + LuzDireccional(N, V, base.rgb);
    for (int i = 0; i < numLuces; ++i)
        color += LuzPuntual(i, N, V, base.rgb);

    float alfa = opacidad * base.a;

    // Fresnel / mapeo de ambiente para metales y cristales
    // (el cubemap solo se lee en estos dos materiales: ahorra tiempo en el resto)
    vec3 I = -V;                                // del ojo hacia la superficie
    if (tipoMaterial == MAT_TRASLUCIDO) {
        vec3 reflejo = texture(cubemapAmbiente, reflect(I, N)).rgb;
        float F = Fresnel(I, N, 0.08, 0.92, 3.0);
        color = mix(color, reflejo, F * reflectividad + 0.15);
        alfa  = mix(alfa, 1.0, F);              // en angulos rasantes el vidrio refleja mas y se ve menos
    } else if (tipoMaterial == MAT_METALICO) {
        vec3 reflejo = texture(cubemapAmbiente, reflect(I, N)).rgb;
        float F = Fresnel(I, N, 0.25, 0.75, 4.0);
        color = mix(color, reflejo * (base.rgb + 0.35), F * reflectividad);
    }

    // Emision (lamparas, pantallas, alarmas). Con textura, la emision la "ilumina"
    // en vez de taparla (asi las pantallas muestran su imagen y no se ven blancas)
    color += (usaTextura == 1) ? colorEmision * base.rgb : colorEmision;

    // Niebla exponencial: oculta el fin del modelo y da profundidad
    float d = length(posCamara - posMundo);
    float niebla = exp(-pow(d * densidadNiebla, 2.0));
    color = mix(colorNiebla, color, clamp(niebla, 0.0, 1.0));

    FragColor = vec4(color, alfa);
}
