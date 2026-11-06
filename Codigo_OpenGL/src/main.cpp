/*
 * ============================================================================
 *  PROYECTO FINAL - COMPUTACION GRAFICA E INTERACCION HUMANO-COMPUTADORA
 *  Facultad de Ingenieria, UNAM - Semestre 2027-1
 *
 *  Metro CDMX - ESCENA 1: Recorrido por los andenes
 *
 *  Base: practicas de laboratorio stv-labopengl (Dr. Sergio Teodoro Vite):
 *        GLFW + GLAD + GLM + Assimp + stb_image + irrKlang, clases Camera,
 *        Shader y CubeMap, y los shaders de Phong, Fresnel, cubemap y picking.
 *
 *  Indice de bloques (para la explicacion):
 *    BLOQUE 0.  Librerias
 *    BLOQUE 1.  Configuracion general (ventana, rutas, constantes)
 *    BLOQUE 2.  Estado global (camaras, tiempo, escena, tren, interaccion)
 *    BLOQUE 3.  Prototipos de funciones
 *    BLOQUE 4.  main(): ciclo principal
 *    BLOQUE 5.  Inicializacion (ventana, OpenGL, shaders, modelo, cubemap)
 *    BLOQUE 6.  Preparacion de la escena (correcciones y nodos por nombre)
 *    BLOQUE 7.  Animaciones
 *                 7.1 Tren: maquina de estados con keyframes (llega, abre, sale)
 *                 7.2 Procedurales: ruedas, balanceo, alarmas, lampara con falla
 *                 7.3 Keyframes: conos del espacio exclusivo
 *                 7.4 Simples: pantallas, objeto tomado, caida
 *                 7.5 Camara cinematografica (keyframes)
 *    BLOQUE 8.  Iluminacion (seleccion de las lamparas mas cercanas)
 *    BLOQUE 9.  Camaras (primera persona + colisiones, libre, aerea)
 *    BLOQUE 10. Renderizado (picking, opacos, cielo, traslucidos, HUD)
 *    BLOQUE 11. Interaccion con objetos (seleccionar, tomar, mover, rotar, escalar)
 *    BLOQUE 12. Transicion a la escena 2 (subir / bajar del vagon)
 *    BLOQUE 13. Entrada: teclado, mouse y callbacks
 *    BLOQUE 14. Utilidades (titulo de ventana, capturas de pantalla)
 * ============================================================================
 */

// ============================================================================
// BLOQUE 0. Librerias
// ============================================================================
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <direct.h>   // _mkdir (carpeta de capturas)

#include <glad/glad.h>                  // Cargador de funciones de OpenGL
#include <GLFW/glfw3.h>                 // Ventana, teclado y mouse

#include <glm/glm.hpp>                  // Matematicas (vectores y matrices)
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <shader_m.h>                   // Clases del laboratorio
#include <camera.h>
#include <cubemap.h>

#include <modelo_escena.h>              // Clases del proyecto
#include <keyframes.h>
#include <colisiones.h>

#include <irrKlang.h>                   // Audio (igual que en las practicas)
using namespace irrklang;

// ============================================================================
// BLOQUE 1. Configuracion general
// ============================================================================
unsigned int anchoVentana = 1280;
unsigned int altoVentana = 720;
const char* TITULO = "Metro CDMX - Andenes";
int muestrasMSAA = 4;   // antialiasing (0 = apagado, mas rapido en tarjetas integradas)

// Archivos (rutas relativas a la carpeta bin/)
std::string rutaModelo = "models/Andenes_Completo.fbx";
const char* RUTA_SONIDO_TREN = "sound/tren.mp3";       // opcional
const char* RUTA_SONIDO_PUERTAS = "sound/puertas.mp3"; // opcional

// Medidas de la escena (metros). El FBX se exporta con Y hacia arriba.
const float ALTURA_OJOS = 1.70f;
const float VELOCIDAD_CAMINAR = 3.0f;
const float VELOCIDAD_CORRER = 6.5f;
const float VELOCIDAD_LIBRE = 10.0f;
const float RADIO_RUEDA = 0.42f;

const float DISTANCIA_DETALLE = 30.0f;   // LOD: lotes muy densos (rejas) solo se dibujan hasta aqui
const int TRIANGULOS_DETALLE = 20000;    // un lote con mas triangulos que esto se considera "detalle fino"
const int MAX_LUCES = 10;          // debe coincidir con el shader
const int LAMPARAS_ACTIVAS = 8;    // lamparas cercanas que iluminan (+ faro del tren)

// ============================================================================
// BLOQUE 2. Estado global
// ============================================================================
GLFWwindow* ventana = nullptr;

// ---- 2.1 Tiempo ----
float deltaTime = 0.0f, ultimoCuadro = 0.0f, tiempoTotal = 0.0f;

// ---- 2.2 Camaras ----
enum ModoCamara { CAM_PRIMERA_PERSONA = 0, CAM_LIBRE = 1, CAM_AEREA = 2, CAM_CINEMATICA = 3 };
ModoCamara modoCamara = CAM_PRIMERA_PERSONA;
ModoCamara modoAntesDeCinematica = CAM_PRIMERA_PERSONA;

Camera camara(glm::vec3(-4.5f, ALTURA_OJOS, 30.0f));   // camara del laboratorio (yaw / pitch)
glm::vec3 posJugador = glm::vec3(-4.5f, 0.0f, 30.0f);   // pies del jugador
float alturaPiso = 0.0f;                                 // 0 en el anden; cambia dentro del vagon
float faseCaminata = 0.0f;                               // para el head-bob procedural
float zoomAereo = 25.0f;

float ultimoX = 640.0f, ultimoY = 360.0f;
bool primerMouse = true;

// ---- 2.3 Shaders, modelo y cielo ----
Shader* shaderIluminacion = nullptr;
Shader* shaderCielo = nullptr;
Shader* shaderPicking = nullptr;
Shader* shaderHUD = nullptr;
ModeloEscena* estacion = nullptr;
CubeMap* cielo = nullptr;
GLuint vaoHUD = 0, vboHUD = 0;

// Ubicaciones de uniforms del shader principal (se buscan una sola vez)
struct UniformsIluminacion {
    GLint model, view, projection, matrizNormal;
    GLint colorDifuso, colorEspecular, colorEmision, brillo, opacidad, reflectividad, tipoMaterial, usaTextura;
    GLint numLuces, luzPosicion, luzColor, luzRadio;
    GLint solDireccion, solColor, luzAmbiente, posCamara, colorNiebla, densidadNiebla, alturaCorte;
} U;

// ---- 2.4 Iluminacion ----
std::vector<glm::vec3> posicionesLamparas;   // 372 lamparas del techo (del FBX)
std::vector<int> nodosLamparas;
int lamparaConFalla = -1;                    // indice en posicionesLamparas
float intensidadFalla = 1.0f;

std::vector<glm::vec3> lucesPos;             // luces que se mandan al shader este cuadro
std::vector<glm::vec3> lucesColor;
std::vector<float> lucesRadio;

// ---- 2.5 Tren ----
enum EstadoTren { TREN_AUSENTE, TREN_LLEGANDO, TREN_ABRIENDO, TREN_DETENIDO, TREN_CERRANDO, TREN_SALIENDO };
EstadoTren estadoTren = TREN_AUSENTE;
float tiempoEstado = 0.0f;
float esperaParaLlegar = 4.0f;               // segundos que tarda en aparecer el primer tren
const float TIEMPO_DETENIDO = 15.0f;
const float TIEMPO_AUSENTE = 10.0f;

int nodoTren = -1;
PistaKeyframes<float> pistaLlegada;          // K1: desplazamiento en Z al llegar
PistaKeyframes<float> pistaSalida;           // K2: desplazamiento en Z al salir
PistaKeyframes<float> pistaPuertas;          // K3: apertura de puertas (metros)
float desplazamientoTren = 0.0f, desplazamientoAnterior = 0.0f, velocidadTren = 0.0f;
float aperturaPuertas = 0.0f, anguloRuedas = 0.0f;

struct Puerta { int nodo; glm::vec3 direccion; };  // direccion de deslizamiento (espacio del padre)
std::vector<Puerta> puertas;
std::vector<int> ruedas;
std::vector<int> alarmas;
int nodoVagonInterior = -1;                  // vagon 5: el que tiene interior (escena 2)

// ---- 2.6 Conos del espacio exclusivo (K5) ----
struct Cono { int nodo; PistaKeyframes<glm::vec3> pista; };
std::vector<Cono> conos;
bool conosColocados = true;
float tiempoConos = 100.0f;                  // grande = animacion terminada

// ---- 2.7 Pantallas (animacion simple de encendido) ----
std::vector<int> imagenesPantalla;
bool pantallasEncendidas = true;
float escalaPantallas = 1.0f;

// ---- 2.8 Interaccion ----
struct ObjetoInteractivo {
    int nodo;
    std::string nombre;
    glm::vec3 pivote;              // centro inferior de la caja original
    glm::vec3 desplazamiento = glm::vec3(0.0f);
    float rotacionY = 0.0f;        // grados
    float escala = 1.0f;
    bool tomado = false;
    bool cayendo = false;
    float yDestino = 0.0f;
};
std::vector<ObjetoInteractivo> interactivos;
int seleccionado = -1;
bool solicitudPicking = false;

// ---- 2.9 Colisiones y escenas ----
MundoColision colisionAnden;
MundoColision colisionVagon;
bool aBordo = false;               // true = vista previa de la escena 2 (dentro del vagon)
float fundido = 0.0f;              // 0 = sin negro, 1 = pantalla negra
int fundidoDireccion = 0;          // +1 oscureciendo, -1 aclarando
bool accionPendienteSubir = false; // que hacer cuando el fundido llegue a negro

// ---- 2.10 Camara cinematografica (K4) ----
PistaKeyframes<glm::vec3> pistaCamPos;
PistaKeyframes<glm::vec3> pistaCamMira;
float tiempoCinematica = 0.0f;

// ---- 2.11 Audio ----
ISoundEngine* motorSonido = nullptr;

// ---- 2.12 Captura automatica (opcion --captura, sirve para el reporte) ----
std::string archivoCapturaAuto;
float tiempoCapturaAuto = 0.0f;
int ultimosDibujos = 0;
long long ultimosTriangulos = 0;   // llamadas de dibujo del ultimo cuadro (diagnostico)

// ============================================================================
// BLOQUE 3. Prototipos de funciones
// ============================================================================
bool Inicializar();
void PrepararEscena();
void ActualizarAnimaciones(float dt);
void ActualizarTren(float dt);
void ActualizarProcedurales(float dt);
void ActualizarConos(float dt);
void ActualizarSimples(float dt);
void ActualizarCinematica(float dt);
void SeleccionarLuces(const glm::vec3& desde);
void ActualizarCamara(float dt);
void ObtenerMatrices(glm::mat4& proyeccion, glm::mat4& vista);
void Renderizar();
void RenderizarPicking(const glm::mat4& proyeccion, const glm::mat4& vista);
void DibujarHUD();
void ActualizarInteraccion(float dt);
void IntentarSubirOBajar();
void ActualizarFundido(float dt);
void ProcesarEntrada(float dt);
void callbackTamano(GLFWwindow* w, int ancho, int alto);
void callbackMouse(GLFWwindow* w, double x, double y);
void callbackScroll(GLFWwindow* w, double dx, double dy);
void callbackBotonMouse(GLFWwindow* w, int boton, int accion, int mods);
void callbackTecla(GLFWwindow* w, int tecla, int scancode, int accion, int mods);
void ActualizarTitulo();
void GuardarCaptura(const std::string& archivo);
void ReproducirSonido(const char* ruta);

// ============================================================================
// BLOQUE 4. main(): ciclo principal
// ============================================================================
int main(int argc, char** argv)
{
    // Opciones de linea de comandos (solo para pruebas y capturas del reporte)
    //   --modelo ruta.fbx      usa otro archivo
    //   --captura salida.bmp   guarda una imagen despues de N segundos y cierra
    //   --t segundos           tiempo simulado antes de la captura
    //   --cam x y z yaw pitch  posicion inicial de la camara
    //   --modo n               0 primera persona, 1 libre, 2 aerea
    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        if (a == "--modelo" && i + 1 < argc) rutaModelo = argv[++i];
        else if (a == "--captura" && i + 1 < argc) archivoCapturaAuto = argv[++i];
        else if (a == "--t" && i + 1 < argc) tiempoCapturaAuto = (float)atof(argv[++i]);
        else if (a == "--res" && i + 2 < argc) { anchoVentana = atoi(argv[i + 1]); altoVentana = atoi(argv[i + 2]); i += 2; }
        else if (a == "--sinmsaa") muestrasMSAA = 0;
        else if (a == "--modo" && i + 1 < argc) modoCamara = (ModoCamara)atoi(argv[++i]);
        else if (a == "--cam" && i + 5 < argc) {
            posJugador = glm::vec3((float)atof(argv[i + 1]), 0.0f, (float)atof(argv[i + 3]));
            camara.Position = glm::vec3(posJugador.x, (float)atof(argv[i + 2]), posJugador.z);
            camara.Yaw = (float)atof(argv[i + 4]);
            camara.Pitch = (float)atof(argv[i + 5]);
            i += 5;
        }
    }

    if (!Inicializar()) return -1;
    PrepararEscena();

    // Modo captura: simula el tiempo pedido con pasos fijos, dibuja y guarda
    if (!archivoCapturaAuto.empty()) {
        const float paso = 1.0f / 30.0f;
        for (float t = 0.0f; t < tiempoCapturaAuto; t += paso) {
            tiempoTotal += paso;
            ActualizarAnimaciones(paso);
            if (modoCamara != CAM_LIBRE && modoCamara != CAM_PRIMERA_PERSONA) ActualizarCamara(paso);
        }
        estacion->ActualizarJerarquia();
        // Medicion de rendimiento: dibuja 30 cuadros y reporta el promedio
        double inicio = glfwGetTime();
        double tCpu = 0.0;
        for (int i = 0; i < 30; i++) {
            double t0 = glfwGetTime();
            estacion->ActualizarJerarquia();
            Renderizar();
            tCpu += glfwGetTime() - t0;
            glFinish();
        }
        std::cout << "> CPU por cuadro: " << tCpu / 30.0 * 1000.0 << " ms, dibujos=" << ultimosDibujos << " triangulos=" << ultimosTriangulos << std::endl;
        std::cout << "> Tiempo promedio por cuadro: " << (glfwGetTime() - inicio) / 30.0 * 1000.0 << " ms" << std::endl;
        GuardarCaptura(archivoCapturaAuto);
        glfwTerminate();
        return 0;
    }

    // Ciclo principal: entrada -> animacion -> render
    while (!glfwWindowShouldClose(ventana)) {
        float ahora = (float)glfwGetTime();
        deltaTime = std::min(ahora - ultimoCuadro, 0.1f);   // evita saltos si la ventana se congela
        ultimoCuadro = ahora;
        tiempoTotal += deltaTime;

        ProcesarEntrada(deltaTime);
        ActualizarAnimaciones(deltaTime);
        ActualizarInteraccion(deltaTime);
        ActualizarCamara(deltaTime);
        ActualizarFundido(deltaTime);
        estacion->ActualizarJerarquia();

        Renderizar();
        ActualizarTitulo();

        glfwSwapBuffers(ventana);
        glfwPollEvents();
    }

    if (motorSonido) motorSonido->drop();
    glfwTerminate();
    return 0;
}

// ============================================================================
// BLOQUE 5. Inicializacion
// ============================================================================
bool Inicializar()
{
    // ---- 5.1 GLFW y ventana (contexto OpenGL 3.3 core, como en las practicas) ----
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_SAMPLES, muestrasMSAA);   // antialiasing
    if (!archivoCapturaAuto.empty()) glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);

    ventana = glfwCreateWindow(anchoVentana, altoVentana, TITULO, NULL, NULL);
    if (ventana == NULL) {
        std::cout << "No se pudo crear la ventana de GLFW" << std::endl;
        glfwTerminate();
        return false;
    }
    glfwMakeContextCurrent(ventana);
    glfwSwapInterval(1);
    glfwSetFramebufferSizeCallback(ventana, callbackTamano);
    glfwSetCursorPosCallback(ventana, callbackMouse);
    glfwSetScrollCallback(ventana, callbackScroll);
    glfwSetMouseButtonCallback(ventana, callbackBotonMouse);
    glfwSetKeyCallback(ventana, callbackTecla);
    glfwSetInputMode(ventana, GLFW_CURSOR, GLFW_CURSOR_DISABLED);   // mouse para mirar

    // ---- 5.2 GLAD y estado de OpenGL ----
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cout << "No se pudo inicializar GLAD" << std::endl;
        return false;
    }
    std::cout << "> OpenGL " << glGetString(GL_VERSION) << " - " << glGetString(GL_RENDERER) << std::endl;
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_MULTISAMPLE);
    glDisable(GL_CULL_FACE);   // los modelos de Blender tienen planos de una sola cara

    // ---- 5.3 Shaders ----
    shaderIluminacion = new Shader("shaders/metro_iluminacion.vs", "shaders/metro_iluminacion.fs");
    shaderCielo = new Shader("shaders/metro_cielo.vs", "shaders/metro_cielo.fs");
    shaderPicking = new Shader("shaders/15_picking.vs", "shaders/15_picking.fs");
    shaderHUD = new Shader("shaders/metro_hud.vs", "shaders/metro_hud.fs");

    GLuint id = shaderIluminacion->ID;
    auto ubic = [id](const char* n) { return glGetUniformLocation(id, n); };
    U = { ubic("model"), ubic("view"), ubic("projection"), ubic("matrizNormal"),
          ubic("colorDifuso"), ubic("colorEspecular"), ubic("colorEmision"), ubic("brillo"), ubic("opacidad"),
          ubic("reflectividad"), ubic("tipoMaterial"), ubic("usaTextura"),
          ubic("numLuces"), ubic("luzPosicion"), ubic("luzColor"), ubic("luzRadio"),
          ubic("solDireccion"), ubic("solColor"), ubic("luzAmbiente"), ubic("posCamara"),
          ubic("colorNiebla"), ubic("densidadNiebla"), ubic("alturaCorte") };
    shaderIluminacion->use();
    shaderIluminacion->setInt("texture_diffuse1", 0);
    shaderIluminacion->setInt("cubemapAmbiente", 1);

    // ---- 5.4 Modelo de la estacion (exportado desde Blender en FBX) ----
    estacion = new ModeloEscena(rutaModelo);
    if (!estacion->cargado) {
        std::cout << "\nNo se encontro el modelo: " << rutaModelo
                  << "\nExporta Andenes_Completo.blend como FBX en bin/models/ (ver LEEME.md)." << std::endl;
        return false;
    }

    // ---- 5.5 Cubemap de ambiente (cielo + reflejos), clase del laboratorio ----
    cielo = new CubeMap();
    std::vector<std::string> caras {
        "textures/cubemap_cielo/posx.png", "textures/cubemap_cielo/negx.png",
        "textures/cubemap_cielo/posy.png", "textures/cubemap_cielo/negy.png",
        "textures/cubemap_cielo/posz.png", "textures/cubemap_cielo/negz.png" };
    cielo->loadCubemap(caras);

    // ---- 5.6 Geometria del HUD (un cuadrado que se escala) ----
    float cuadrado[] = { -1, -1, 1, -1, 1, 1, -1, -1, 1, 1, -1, 1 };
    glGenVertexArrays(1, &vaoHUD);
    glGenBuffers(1, &vboHUD);
    glBindVertexArray(vaoHUD);
    glBindBuffer(GL_ARRAY_BUFFER, vboHUD);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cuadrado), cuadrado, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glBindVertexArray(0);

    // ---- 5.7 Audio (opcional: solo suena si existen los archivos) ----
    motorSonido = createIrrKlangDevice();

    camara.MouseSensitivity = 0.12f;
    camara.ProcessMouseMovement(0, 0);   // recalcula Front con el yaw/pitch inicial
    return true;
}

// ============================================================================
// BLOQUE 6. Preparacion de la escena
// ----------------------------------------------------------------------------
// El FBX conserva los NOMBRES de los objetos de Blender. Aqui se buscan los
// nodos importantes por nombre; por eso NO se deben renombrar en Blender.
// ============================================================================
void PrepararEscena()
{
    ModeloEscena& E = *estacion;

    // ---- 6.0 Correcciones del modelo ----
    // En Andenes_Completo.blend el bloque de grava "Grava_Via_1_17" quedo en el
    // origen (encima de la via central). Le corresponde la via exterior,
    // entre Grava_Via_1_16 (z = -45) y Grava_Via_1_18 (z = -65).
    // Se recoloca aqui; si se corrige en Blender, este bloque ya no hace nada.
    int grava = E.Buscar("Grava_Via_1_17");
    if (grava >= 0 && glm::length(glm::vec3(E.nodos[grava].local[3])) < 0.01f)
        E.nodos[grava].local[3] = glm::vec4(-12.5f, -0.72f, -55.0f, 1.0f);
    E.ActualizarJerarquia();
    E.CalcularCajas();

    // ---- 6.1 Tren: raiz, puertas, ruedas y vagon con interior ----
    nodoTren = E.Buscar("TREN_RAIZ");
    nodoVagonInterior = E.Buscar("VAGON_05_RAIZ");
    for (int i : E.BuscarContiene("_Puerta_")) {
        const std::string& n = E.nodos[i].nombre;
        if (n.size() < 4 || n.substr(n.size() - 4) != "_Der") continue;
        int izq = E.Buscar(n.substr(0, n.size() - 4) + "_Izq");
        if (izq < 0) continue;
        // Direccion de apertura: de la hoja izquierda hacia la derecha, en el espacio del padre
        glm::vec3 pDer = glm::vec3(E.nodos[i].local[3]);
        glm::vec3 pIzq = glm::vec3(E.nodos[izq].local[3]);
        glm::vec3 dir = glm::normalize(pDer - pIzq);
        puertas.push_back({ i, dir });
        puertas.push_back({ izq, -dir });
    }
    for (int i : E.BuscarContiene("_Llanta_")) if (!E.nodos[i].mallas.empty()) ruedas.push_back(i);

    // Alarmas de los letreros "Proxima salida"
    for (int i : E.BuscarContiene("|alarma")) alarmas.push_back(i);

    // ---- 6.2 Keyframes del tren (K1, K2, K3) ----
    pistaLlegada.Agregar(0.0f, 320.0f);
    pistaLlegada.Agregar(6.0f, 90.0f, LINEAL);        // entra a velocidad constante
    pistaLlegada.Agregar(13.0f, 0.0f, SALIDA);        // frena hasta detenerse
    pistaSalida.Agregar(0.0f, 0.0f);
    pistaSalida.Agregar(8.0f, -110.0f, ENTRADA);      // arranca suave
    pistaSalida.Agregar(12.0f, -340.0f, LINEAL);      // y se aleja
    pistaPuertas.Agregar(0.0f, 0.0f);
    pistaPuertas.Agregar(0.4f, 0.03f, ENTRADA);       // se despegan
    pistaPuertas.Agregar(1.6f, 0.62f, ENTRADA_SALIDA);// se deslizan
    if (nodoTren >= 0) E.nodos[nodoTren].visible = false;

    // ---- 6.3 Lamparas del techo -> posibles luces ----
    for (int i : E.BuscarPrefijo("Lampara_Estacion_", true)) {
        nodosLamparas.push_back(i);
        posicionesLamparas.push_back(glm::vec3(E.nodos[i].global[3]) - glm::vec3(0.0f, 0.25f, 0.0f));
    }
    if (!posicionesLamparas.empty()) lamparaConFalla = (int)(posicionesLamparas.size() * 37 / 100);

    // ---- 6.4 Conos del espacio exclusivo (K5): posicion "retirada" junto al muro ----
    int k = 0;
    for (int i : E.BuscarPrefijo("Cono_Exclusivo_", true)) {
        Cono c;
        c.nodo = i;
        glm::vec3 pos = glm::vec3(E.nodos[i].global[3]);
        glm::vec3 retirado = glm::vec3(9.3f - pos.x, 0.0f, 2.0f + 0.6f * k) ;  // apilados junto al borde
        float d = 0.35f * k;                                                     // salen uno tras otro
        c.pista.Agregar(0.0f, retirado);
        c.pista.Agregar(d + 0.01f, retirado);
        c.pista.Agregar(d + 0.5f, retirado + glm::vec3(0, 0.35f, 0), SALIDA);   // se levanta
        c.pista.Agregar(d + 2.0f, glm::vec3(0, 0.35f, 0), ENTRADA_SALIDA);      // se traslada
        c.pista.Agregar(d + 2.5f, glm::vec3(0.0f), ENTRADA);                    // se baja
        conos.push_back(c);
        k++;
    }

    // ---- 6.5 Pantallas: la malla de la imagen (material "Imagen...") ----
    for (int p : E.BuscarPrefijo("Pantalla_", true))
        E.RecorrerSubarbol(p, [&](int n) {
            for (unsigned int m : E.nodos[n].mallas)
                if (E.materiales[E.mallas[m].material].nombre.find("Imagen") != std::string::npos) {
                    imagenesPantalla.push_back(n);
                    break;
                }
        });

    // ---- 6.6 Objetos interactivos (se pueden tomar, mover, rotar y escalar) ----
    const char* prefijos[] = { "Pantalla_", "Proxima_", "Terminal_", "Exclusivo_", "Direccion_Indios", "Emergencia_" };
    for (const char* pre : prefijos)
        for (int i : E.BuscarPrefijo(pre, true)) {
            ObjetoInteractivo o;
            o.nodo = i;
            o.nombre = E.nodos[i].nombre;
            const Nodo& n = E.nodos[i];
            o.pivote = glm::vec3((n.aabbMin.x + n.aabbMax.x) * 0.5f, n.aabbMin.y, (n.aabbMin.z + n.aabbMax.z) * 0.5f);
            E.nodos[i].idInteractivo = (int)interactivos.size();
            interactivos.push_back(o);
        }

    // ---- 6.7 Colisiones del anden ----
    // Zonas caminables: los dos andenes (sin rebasar la linea amarilla)
    colisionAnden.zonasCaminables.push_back({ -10.0f, -2.25f, -110.0f, 110.0f });
    colisionAnden.zonasCaminables.push_back({ 2.25f, 10.0f, -110.0f, 110.0f });
    // Obstaculos: todo lo que este parado sobre el anden a la altura del cuerpo
    const char* ignorar[] = { "Piso_", "Franja_", "Base_", "Grava_", "Durmiente_", "Riel_", "Panel_Techo",
                              "Lampara_", "Viga_", "Trabe_", "Marco_Hueco", "Muro_", "TREN_RAIZ", "Cono_", "Pantalla_" };
    for (int h : E.nodos[0].hijos) {
        const Nodo& n = E.nodos[h];
        bool omitir = false;
        for (const char* pre : ignorar) if (n.nombre.compare(0, strlen(pre), pre) == 0) omitir = true;
        if (omitir || n.aabbMin.x > n.aabbMax.x) continue;
        glm::vec3 tam = n.aabbMax - n.aabbMin;
        bool sobreAnden = std::fabs(n.aabbMin.x + n.aabbMax.x) * 0.5f < 10.5f;
        if (sobreAnden && n.aabbMin.y < 1.5f && n.aabbMax.y > 0.2f && tam.x < 12.0f && tam.z < 30.0f)
            colisionAnden.obstaculos.push_back({ n.aabbMin, n.aabbMax });
    }

    // ---- 6.8 Camara cinematografica (K4): recorrido para el video ----
    pistaCamPos.Agregar(0.0f, glm::vec3(-6.0f, 6.0f, 108.0f));
    pistaCamPos.Agregar(6.0f, glm::vec3(-6.0f, 2.2f, 60.0f), ENTRADA_SALIDA);
    pistaCamPos.Agregar(12.0f, glm::vec3(0.0f, 2.8f, 10.0f), ENTRADA_SALIDA);
    pistaCamPos.Agregar(18.0f, glm::vec3(6.0f, 1.8f, -40.0f), ENTRADA_SALIDA);
    pistaCamPos.Agregar(24.0f, glm::vec3(12.0f, 8.0f, -100.0f), ENTRADA_SALIDA);
    pistaCamMira.Agregar(0.0f, glm::vec3(0.0f, 1.0f, 40.0f));
    pistaCamMira.Agregar(6.0f, glm::vec3(0.0f, 1.0f, 20.0f), ENTRADA_SALIDA);
    pistaCamMira.Agregar(12.0f, glm::vec3(6.0f, 1.5f, -20.0f), ENTRADA_SALIDA);
    pistaCamMira.Agregar(18.0f, glm::vec3(0.0f, 1.2f, -70.0f), ENTRADA_SALIDA);
    pistaCamMira.Agregar(24.0f, glm::vec3(0.0f, 0.0f, 0.0f), ENTRADA_SALIDA);

    // ---- 6.9 Lotes (rendimiento) ----
    // Se unen en pocos VAOs por material todas las mallas que se mueven juntas.
    // Cada nodo que se anima por su cuenta (tren, puertas, ruedas, alarmas,
    // conos, pantallas, objetos interactivos, lampara con falla) es una
    // "referencia": sus piezas se agrupan en su propio espacio y el lote se
    // dibuja con la matriz global de ese nodo, asi se sigue animando.
    std::vector<char> dinamico(E.nodos.size(), 0);   // nada se excluye
    std::vector<int> referencias;
    if (nodoTren >= 0) referencias.push_back(nodoTren);
    for (const Puerta& p : puertas) referencias.push_back(p.nodo);
    for (int r : ruedas) referencias.push_back(r);
    for (int a : alarmas) referencias.push_back(a);
    for (int n : imagenesPantalla) referencias.push_back(n);
    for (const Cono& c : conos) referencias.push_back(c.nodo);
    for (const ObjetoInteractivo& o : interactivos) referencias.push_back(o.nodo);
    if (lamparaConFalla >= 0) referencias.push_back(nodosLamparas[lamparaConFalla]);
    E.ConstruirLotes(dinamico, referencias, 20.0f);

    camara.Position = glm::vec3(posJugador.x, camara.Position.y, posJugador.z);
    std::cout << "> Escena: puertas=" << puertas.size() << " ruedas=" << ruedas.size()
              << " lamparas=" << posicionesLamparas.size() << " alarmas=" << alarmas.size()
              << " conos=" << conos.size() << " pantallas=" << imagenesPantalla.size()
              << " interactivos=" << interactivos.size() << " obstaculos=" << colisionAnden.obstaculos.size()
              << std::endl;
}

// ============================================================================
// BLOQUE 7. Animaciones
// ============================================================================
void ActualizarAnimaciones(float dt)
{
    ActualizarTren(dt);          // 7.1
    ActualizarProcedurales(dt);  // 7.2
    ActualizarConos(dt);         // 7.3
    ActualizarSimples(dt);       // 7.4
    ActualizarCinematica(dt);    // 7.5
}

// ---- 7.1 Tren: maquina de estados con keyframes --------------------------------
//   AUSENTE -> LLEGANDO (K1) -> ABRIENDO (K3) -> DETENIDO -> CERRANDO (K3 al reves)
//          -> SALIENDO (K2) -> AUSENTE ...
void CambiarEstadoTren(EstadoTren nuevo)
{
    estadoTren = nuevo;
    tiempoEstado = 0.0f;
    if (nuevo == TREN_LLEGANDO) ReproducirSonido(RUTA_SONIDO_TREN);
    if (nuevo == TREN_ABRIENDO || nuevo == TREN_CERRANDO) ReproducirSonido(RUTA_SONIDO_PUERTAS);
}

void ActualizarTren(float dt)
{
    if (nodoTren < 0) return;
    ModeloEscena& E = *estacion;
    tiempoEstado += dt;

    switch (estadoTren) {
    case TREN_AUSENTE:
        desplazamientoTren = 320.0f;
        if (tiempoEstado >= esperaParaLlegar) { CambiarEstadoTren(TREN_LLEGANDO); esperaParaLlegar = TIEMPO_AUSENTE; }
        break;
    case TREN_LLEGANDO:
        desplazamientoTren = pistaLlegada.Evaluar(tiempoEstado);
        if (tiempoEstado >= pistaLlegada.Duracion()) CambiarEstadoTren(TREN_ABRIENDO);
        break;
    case TREN_ABRIENDO:
        aperturaPuertas = pistaPuertas.Evaluar(tiempoEstado);
        if (tiempoEstado >= pistaPuertas.Duracion()) CambiarEstadoTren(TREN_DETENIDO);
        break;
    case TREN_DETENIDO:
        if (aBordo) tiempoEstado = 0.0f;   // no se va mientras el usuario esta dentro
        if (tiempoEstado >= TIEMPO_DETENIDO) CambiarEstadoTren(TREN_CERRANDO);
        break;
    case TREN_CERRANDO:
        aperturaPuertas = pistaPuertas.Evaluar(pistaPuertas.Duracion() - tiempoEstado);  // K3 al reves
        if (tiempoEstado >= pistaPuertas.Duracion()) { aperturaPuertas = 0.0f; CambiarEstadoTren(TREN_SALIENDO); }
        break;
    case TREN_SALIENDO:
        desplazamientoTren = pistaSalida.Evaluar(tiempoEstado);
        if (tiempoEstado >= pistaSalida.Duracion()) CambiarEstadoTren(TREN_AUSENTE);
        break;
    }

    // Velocidad (m/s) para las animaciones procedurales
    velocidadTren = (dt > 0.0f) ? (desplazamientoTren - desplazamientoAnterior) / dt : 0.0f;
    desplazamientoAnterior = desplazamientoTren;

    // El tren "aparece" y "desaparece": solo se dibuja cuando esta en la estacion
    E.nodos[nodoTren].visible = (estadoTren != TREN_AUSENTE);

    // Puertas: traslacion en el espacio del vagon (modelado jerarquico)
    for (const Puerta& p : puertas)
        E.nodos[p.nodo].animPadre = glm::translate(glm::mat4(1.0f), p.direccion * aperturaPuertas);
}

// ---- 7.2 Animaciones procedurales (calculadas con formulas, sin cuadros clave) ----
void ActualizarProcedurales(float dt)
{
    ModeloEscena& E = *estacion;
    float rapidez = std::fabs(velocidadTren);

    // P1. Ruedas: angulo = distancia / radio  (w = v / r)
    anguloRuedas += velocidadTren * dt / RADIO_RUEDA;
    for (int r : ruedas)
        E.nodos[r].animLocal = glm::rotate(glm::mat4(1.0f), anguloRuedas, glm::vec3(0.0f, 0.0f, 1.0f));

    // P2. Balanceo y vibracion del tren: senoidales cuya amplitud depende de la velocidad
    if (nodoTren >= 0) {
        float a = glm::clamp(rapidez / 20.0f, 0.0f, 1.0f);
        float rebote = a * 0.015f * std::sin(tiempoTotal * 23.0f) + a * 0.01f * std::sin(tiempoTotal * 7.3f);
        float alabeo = a * glm::radians(0.6f) * std::sin(tiempoTotal * 1.7f);
        glm::vec3 centro(0.0f, 0.6f, 0.0f);
        glm::mat4 m = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, rebote, desplazamientoTren));
        m = glm::translate(m, centro);
        m = glm::rotate(m, alabeo, glm::vec3(0.0f, 0.0f, 1.0f));   // gira sobre su eje largo
        m = glm::translate(m, -centro);
        E.nodos[nodoTren].animPadre = m;
    }

    // P3. Alarmas de "Proxima salida": parpadeo rojo cuando el tren entra o sale
    bool alerta = estadoTren == TREN_LLEGANDO || estadoTren == TREN_CERRANDO || estadoTren == TREN_SALIENDO;
    float pulso = alerta ? (std::sin(tiempoTotal * 10.0f) > 0.0f ? 1.0f : 0.0f) : 0.0f;
    for (int n : alarmas) E.nodos[n].emisionExtra = glm::vec3(1.6f, 0.05f, 0.02f) * pulso;

    // P4. Lampara con falla: parpadeo pseudoaleatorio (suma de senos de distinta frecuencia)
    if (lamparaConFalla >= 0) {
        float s = std::sin(tiempoTotal * 13.0f) + std::sin(tiempoTotal * 31.7f) + std::sin(tiempoTotal * 3.1f);
        intensidadFalla = (s > 1.2f) ? 0.05f : (s > 0.6f ? 0.5f : 1.0f);
        // la lampara apagada pierde su brillo propio (emision negativa compensa la del material)
        E.nodos[nodosLamparas[lamparaConFalla]].emisionExtra = glm::vec3(-5.0f) * (1.0f - intensidadFalla);
    }

    // P5 (head-bob) se calcula en la camara de primera persona (BLOQUE 9)
}

// ---- 7.3 Keyframes: conos del espacio exclusivo (K5) ---------------------------
void ActualizarConos(float dt)
{
    tiempoConos += dt;
    for (Cono& c : conos) {
        float t = std::min(tiempoConos, c.pista.Duracion());
        // colocar = reproducir la pista hacia adelante; retirar = hacia atras
        glm::vec3 d = conosColocados ? c.pista.Evaluar(t) : c.pista.Evaluar(c.pista.Duracion() - t);
        estacion->nodos[c.nodo].animPadre = glm::translate(glm::mat4(1.0f), d);
    }
}

// ---- 7.4 Animaciones simples (transformaciones basicas a velocidad constante) ----
void ActualizarSimples(float dt)
{
    // S1. Pantallas: encendido / apagado escalando la imagen (escala lineal)
    float objetivo = pantallasEncendidas ? 1.0f : 0.0f;
    float paso = 1.5f * dt;
    escalaPantallas += glm::clamp(objetivo - escalaPantallas, -paso, paso);
    float s = std::max(escalaPantallas, 0.001f);
    for (int n : imagenesPantalla)
        estacion->nodos[n].animLocal = glm::scale(glm::mat4(1.0f), glm::vec3(s, s, s));
    // S2 (objeto tomado gira) y S3 (caida al soltar) estan en el BLOQUE 11
}

// ---- 7.5 Camara cinematografica (K4) ---------------------------------------------
void ActualizarCinematica(float dt)
{
    if (modoCamara != CAM_CINEMATICA) return;
    tiempoCinematica += dt;
    camara.Position = pistaCamPos.Evaluar(tiempoCinematica);
    glm::vec3 mira = glm::normalize(pistaCamMira.Evaluar(tiempoCinematica) - camara.Position);
    camara.Yaw = glm::degrees(std::atan2(mira.z, mira.x));
    camara.Pitch = glm::degrees(std::asin(glm::clamp(mira.y, -1.0f, 1.0f)));
    camara.ProcessMouseMovement(0, 0);
    if (tiempoCinematica >= pistaCamPos.Duracion()) {
        modoCamara = modoAntesDeCinematica;
        camara.Position = posJugador + glm::vec3(0.0f, ALTURA_OJOS, 0.0f);
    }
}

// ============================================================================
// BLOQUE 8. Iluminacion
// ----------------------------------------------------------------------------
// El techo tiene 372 lamparas, pero el shader admite 10 luces. Cada cuadro
// se eligen las LAMPARAS_ACTIVAS mas cercanas al punto de vista; con eso
// siempre se ilumina bien la zona donde esta el usuario.
// ============================================================================
void SeleccionarLuces(const glm::vec3& desde)
{
    lucesPos.clear(); lucesColor.clear(); lucesRadio.clear();
    const glm::vec3 colorLampara(1.0f, 0.95f, 0.85f);   // blanco calido

    std::vector<std::pair<float, int>> distancias;
    distancias.reserve(posicionesLamparas.size());
    for (size_t i = 0; i < posicionesLamparas.size(); i++) {
        glm::vec3 d = posicionesLamparas[i] - desde;
        distancias.push_back({ glm::dot(d, d), (int)i });
    }
    int n = std::min((int)distancias.size(), LAMPARAS_ACTIVAS);
    std::partial_sort(distancias.begin(), distancias.begin() + n, distancias.end());
    for (int i = 0; i < n; i++) {
        int k = distancias[i].second;
        float potencia = (k == lamparaConFalla) ? 1.3f * intensidadFalla : 1.3f;
        lucesPos.push_back(posicionesLamparas[k]);
        lucesColor.push_back(colorLampara * potencia);
        lucesRadio.push_back(9.0f);
    }

    // Faro del tren: va en la cabina que entra primero (frente en -Z)
    if (nodoTren >= 0 && estadoTren != TREN_AUSENTE) {
        lucesPos.push_back(glm::vec3(0.0f, 1.0f, -104.5f + desplazamientoTren));
        lucesColor.push_back(glm::vec3(1.0f, 0.95f, 0.8f) * 3.0f);
        lucesRadio.push_back(25.0f);
    }
}

// ============================================================================
// BLOQUE 9. Camaras
// ============================================================================
void ActualizarCamara(float dt)
{
    float velocidad = (glfwGetKey(ventana, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) ? VELOCIDAD_CORRER : VELOCIDAD_CAMINAR;
    auto tecla = [](int k) { return glfwGetKey(ventana, k) == GLFW_PRESS; };
    bool hayInteraccion = seleccionado >= 0;   // con objeto seleccionado, las flechas lo mueven a el

    switch (modoCamara) {
    // ---- 9.1 Primera persona: camina sobre el anden con colisiones ----
    case CAM_PRIMERA_PERSONA:
    case CAM_AEREA: {
        glm::vec3 adelante, derecha;
        if (modoCamara == CAM_PRIMERA_PERSONA) {
            adelante = glm::normalize(glm::vec3(camara.Front.x, 0.0f, camara.Front.z));  // sin volar
            derecha = glm::normalize(glm::cross(adelante, glm::vec3(0, 1, 0)));
        } else {
            adelante = glm::vec3(0, 0, -1);   // en la vista aerea se mueve en ejes del mundo
            derecha = glm::vec3(1, 0, 0);
        }
        glm::vec3 mov(0.0f);
        if (tecla(GLFW_KEY_W)) mov += adelante;
        if (tecla(GLFW_KEY_S)) mov -= adelante;
        if (tecla(GLFW_KEY_D)) mov += derecha;
        if (tecla(GLFW_KEY_A)) mov -= derecha;
        if (!hayInteraccion && modoCamara == CAM_AEREA) {
            if (tecla(GLFW_KEY_UP)) mov += adelante;
            if (tecla(GLFW_KEY_DOWN)) mov -= adelante;
        }
        if (fundido > 0.0f) mov = glm::vec3(0.0f);   // durante la transicion no se camina

        if (glm::length(mov) > 0.0f) {
            glm::vec3 deseada = posJugador + glm::normalize(mov) * velocidad * dt;
            const MundoColision& mundo = aBordo ? colisionVagon : colisionAnden;
            glm::vec3 nueva = mundo.Mover(posJugador, deseada);
            faseCaminata += glm::length(nueva - posJugador) * 2.2f;
            posJugador = nueva;
        }
        // P5. Head-bob: la cabeza sube y baja al caminar (animacion procedural)
        float balanceo = 0.035f * std::sin(faseCaminata * 2.0f);
        camara.Position = glm::vec3(posJugador.x, alturaPiso + ALTURA_OJOS + balanceo, posJugador.z);
        break;
    }
    // ---- 9.2 Camara libre (dron): vuela sin colisiones ----
    case CAM_LIBRE: {
        float v = VELOCIDAD_LIBRE * (tecla(GLFW_KEY_LEFT_SHIFT) ? 3.0f : 1.0f) * dt;
        if (tecla(GLFW_KEY_W)) camara.Position += camara.Front * v;
        if (tecla(GLFW_KEY_S)) camara.Position -= camara.Front * v;
        if (tecla(GLFW_KEY_D)) camara.Position += camara.Right * v;
        if (tecla(GLFW_KEY_A)) camara.Position -= camara.Right * v;
        if (tecla(GLFW_KEY_E)) camara.Position.y += v;
        if (tecla(GLFW_KEY_Q)) camara.Position.y -= v;
        break;
    }
    case CAM_CINEMATICA:
        break;   // la mueve el BLOQUE 7.5
    }
}

// Matrices de proyeccion y vista segun la camara activa
void ObtenerMatrices(glm::mat4& proyeccion, glm::mat4& vista)
{
    float aspecto = (float)anchoVentana / (float)std::max(altoVentana, 1u);
    if (modoCamara == CAM_AEREA) {
        // ---- 9.3 Vista aerea isometrica: proyeccion ortografica que sigue al jugador ----
        glm::vec3 centro = posJugador + glm::vec3(0.0f, 1.0f, 0.0f);
        glm::vec3 ojo = centro + glm::normalize(glm::vec3(1.0f, 1.15f, 1.0f)) * 80.0f;
        proyeccion = glm::ortho(-zoomAereo * aspecto, zoomAereo * aspecto, -zoomAereo, zoomAereo, 0.1f, 400.0f);
        vista = glm::lookAt(ojo, centro, glm::vec3(0, 1, 0));
    } else {
        proyeccion = glm::perspective(glm::radians(camara.Zoom + 15.0f), aspecto, 0.05f, 1000.0f);
        vista = camara.GetViewMatrix();
    }
}

// ============================================================================
// BLOQUE 10. Renderizado
// ============================================================================

// Pruebas de visibilidad (frustum culling): no se dibuja lo que queda fuera de la vista
struct Frustum {
    glm::vec4 planos[6];
    void Extraer(const glm::mat4& m) {
        for (int i = 0; i < 3; i++) {
            planos[i * 2 + 0] = glm::vec4(m[0][3] + m[0][i], m[1][3] + m[1][i], m[2][3] + m[2][i], m[3][3] + m[3][i]);
            planos[i * 2 + 1] = glm::vec4(m[0][3] - m[0][i], m[1][3] - m[1][i], m[2][3] - m[2][i], m[3][3] - m[3][i]);
        }
    }
    bool CajaVisible(const glm::vec3& mn, const glm::vec3& mx) const {
        for (const glm::vec4& p : planos) {
            glm::vec3 v(p.x > 0 ? mx.x : mn.x, p.y > 0 ? mx.y : mn.y, p.z > 0 ? mx.z : mn.z);
            if (glm::dot(glm::vec3(p), v) + p.w < 0.0f) return false;
        }
        return true;
    }
};

// Algo que hay que dibujar: una malla suelta de un nodo (lote = -1) o un lote estatico
struct Dibujo { int nodo; int malla; int lote; float distancia; };

// Caja (min, max) transformada por una matriz -> caja en mundo
void TransformarCaja(const glm::vec3& a, const glm::vec3& b, const glm::mat4& M, glm::vec3& mn, glm::vec3& mx)
{
    mn = glm::vec3(FLT_MAX); mx = glm::vec3(-FLT_MAX);
    for (int c = 0; c < 8; c++) {
        glm::vec3 w = glm::vec3(M * glm::vec4((c & 1) ? b.x : a.x, (c & 2) ? b.y : a.y, (c & 4) ? b.z : a.z, 1.0f));
        mn = glm::min(mn, w); mx = glm::max(mx, w);
    }
}

// Visibilidad y emision heredadas de los padres (si el tren se oculta, se oculta todo)
static std::vector<char> visibleEfectivo;
static std::vector<glm::vec3> emisionEfectiva;

void CalcularHerencia()
{
    ModeloEscena& E = *estacion;
    visibleEfectivo.resize(E.nodos.size());
    emisionEfectiva.resize(E.nodos.size());
    for (size_t i = 0; i < E.nodos.size(); i++) {
        const Nodo& n = E.nodos[i];
        bool vPadre = n.padre < 0 ? true : visibleEfectivo[n.padre] != 0;
        visibleEfectivo[i] = (vPadre && n.visible) ? 1 : 0;
        emisionEfectiva[i] = n.emisionExtra + (n.padre < 0 ? glm::vec3(0.0f) : emisionEfectiva[n.padre]);
    }
    // Resaltado del objeto seleccionado: pulso azul (procedural)
    if (seleccionado >= 0) {
        float p = 0.25f + 0.2f * std::sin(tiempoTotal * 6.0f);
        E.RecorrerSubarbol(interactivos[seleccionado].nodo,
                           [&](int n) { emisionEfectiva[n] += glm::vec3(0.1f, 0.25f, 0.6f) * p; });
    }
}

// Manda al shader el material y la matriz de modelo, y dibuja un VAO
void DibujarConMaterial(const MaterialEscena& mat, const glm::mat4& modelo, const glm::vec3& emisionExtra,
                        GLuint vao, GLsizei numIndices)
{
    glUniformMatrix4fv(U.model, 1, GL_FALSE, glm::value_ptr(modelo));
    glm::mat3 normal = glm::transpose(glm::inverse(glm::mat3(modelo)));
    glUniformMatrix3fv(U.matrizNormal, 1, GL_FALSE, glm::value_ptr(normal));

    glm::vec3 emision = glm::max(mat.emision + emisionExtra, glm::vec3(0.0f));
    glUniform3fv(U.colorDifuso, 1, glm::value_ptr(mat.difuso));
    glUniform3fv(U.colorEspecular, 1, glm::value_ptr(mat.especular));
    glUniform3fv(U.colorEmision, 1, glm::value_ptr(emision));
    glUniform1f(U.brillo, mat.brillo);
    glUniform1f(U.opacidad, mat.opacidad);
    glUniform1f(U.reflectividad, mat.reflectividad);
    glUniform1i(U.tipoMaterial, (int)mat.tipo);
    glUniform1i(U.usaTextura, mat.texturaDifusa ? 1 : 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, mat.texturaDifusa);

    glBindVertexArray(vao);
    glDrawElements(GL_TRIANGLES, numIndices, GL_UNSIGNED_INT, 0);
}

void Dibujar(const Dibujo& d)
{
    ModeloEscena& E = *estacion;
    if (d.lote >= 0) {
        const Lote& L = E.lotes[d.lote];
        glm::mat4 modelo = (L.nodoReferencia >= 0) ? E.nodos[L.nodoReferencia].global : glm::mat4(1.0f);
        glm::vec3 extra = (L.nodoReferencia >= 0) ? emisionEfectiva[L.nodoReferencia] : glm::vec3(0.0f);
        DibujarConMaterial(E.materiales[L.material], modelo, extra, L.vao, L.numIndices);
    } else {
        const Malla& m = E.mallas[d.malla];
        DibujarConMaterial(E.materiales[m.material], E.nodos[d.nodo].global, emisionEfectiva[d.nodo], m.vao, m.numIndices);
    }
}

void Renderizar()
{
    glm::mat4 proyeccion, vista;
    ObtenerMatrices(proyeccion, vista);
    ModeloEscena& E = *estacion;
    CalcularHerencia();

    // ---- 10.1 Picking (solo cuando el usuario hizo clic) ----
    if (solicitudPicking) { RenderizarPicking(proyeccion, vista); solicitudPicking = false; }

    glClearColor(0.55f, 0.62f, 0.70f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // ---- 10.2 Uniforms comunes: camara, luces, ambiente y niebla ----
    glm::vec3 ojo = (modoCamara == CAM_AEREA) ? posJugador + glm::vec3(0, 3, 0) : camara.Position;
    SeleccionarLuces(ojo);
    shaderIluminacion->use();
    glUniformMatrix4fv(U.projection, 1, GL_FALSE, glm::value_ptr(proyeccion));
    glUniformMatrix4fv(U.view, 1, GL_FALSE, glm::value_ptr(vista));
    glm::vec3 posVista = (modoCamara == CAM_AEREA) ? glm::vec3(glm::inverse(vista)[3]) : camara.Position;
    glUniform3fv(U.posCamara, 1, glm::value_ptr(posVista));
    glUniform1i(U.numLuces, (int)lucesPos.size());
    if (!lucesPos.empty()) {
        glUniform3fv(U.luzPosicion, (GLsizei)lucesPos.size(), glm::value_ptr(lucesPos[0]));
        glUniform3fv(U.luzColor, (GLsizei)lucesColor.size(), glm::value_ptr(lucesColor[0]));
        glUniform1fv(U.luzRadio, (GLsizei)lucesRadio.size(), lucesRadio.data());
    }
    glUniform3f(U.solDireccion, -0.35f, -0.8f, 0.25f);
    glUniform3f(U.solColor, 0.55f, 0.52f, 0.47f);
    glUniform3f(U.luzAmbiente, 0.30f, 0.31f, 0.34f);
    glUniform3f(U.colorNiebla, 0.55f, 0.62f, 0.70f);
    glUniform1f(U.densidadNiebla, modoCamara == CAM_AEREA ? 0.0f : 0.0065f);
    glUniform1f(U.alturaCorte, modoCamara == CAM_AEREA ? 2.9f : 1000.0f);   // sin techo en la vista aerea
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_CUBE_MAP, cielo->textureID);

    // ---- 10.3 Recolectar lo visible y dibujar OPACOS ----
    //  a) lotes estaticos (estacion y tren en bloque)
    //  b) nodos dinamicos sueltos (puertas, ruedas, objetos interactivos...)
    //  Se descarta lo que esta fuera de la vista (frustum culling) y lo que
    //  se veria diminuto (tamano / distancia muy pequeno). Los opacos se
    //  dibujan de cerca a lejos: asi la prueba de profundidad evita pintar
    //  pixeles que de todas formas quedarian tapados.
    Frustum fr;
    fr.Extraer(proyeccion * vista);
    std::vector<Dibujo> opacos, traslucidos;
    float umbralTamano = (modoCamara == CAM_AEREA) ? 0.0f : 0.012f;
    auto visible = [&](const glm::vec3& mn, const glm::vec3& mx, float& distancia) {
        if (!fr.CajaVisible(mn, mx)) return false;
        glm::vec3 masCercano = glm::clamp(posVista, mn, mx);
        distancia = glm::length(masCercano - posVista);
        return glm::length(mx - mn) >= umbralTamano * distancia;
    };
    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);
    for (size_t li = 0; li < E.lotes.size(); li++) {
        const Lote& L = E.lotes[li];
        if (L.nodoReferencia >= 0 && !visibleEfectivo[L.nodoReferencia]) continue;
        glm::mat4 M = (L.nodoReferencia >= 0) ? E.nodos[L.nodoReferencia].global : glm::mat4(1.0f);
        glm::vec3 mn, mx;
        float dist;
        TransformarCaja(L.aabbMin, L.aabbMax, M, mn, mx);
        if (!visible(mn, mx, dist)) continue;
        // LOD por distancia: el detalle fino lejano no se nota y cuesta mucho
        if (modoCamara != CAM_AEREA && L.numIndices / 3 > TRIANGULOS_DETALLE && dist > DISTANCIA_DETALLE) continue;
        Dibujo d = { -1, -1, (int)li, dist };
        (L.translucido ? traslucidos : opacos).push_back(d);
    }
    for (size_t i = 0; i < E.nodos.size(); i++) {
        const Nodo& n = E.nodos[i];
        if (!visibleEfectivo[i] || n.mallas.empty() || E.enLote[i]) continue;
        for (unsigned int mi : n.mallas) {
            const Malla& m = E.mallas[mi];
            glm::vec3 mn, mx;
            float dist;
            ModeloEscena::CajaMundo(m, n.global, mn, mx);
            if (!visible(mn, mx, dist)) continue;
            Dibujo d = { (int)i, (int)mi, -1, dist };
            (m.translucida ? traslucidos : opacos).push_back(d);
        }
    }
    std::sort(opacos.begin(), opacos.end(), [](const Dibujo& a, const Dibujo& b) { return a.distancia < b.distancia; });
    for (const Dibujo& d : opacos) Dibujar(d);
    ultimosDibujos = (int)(opacos.size() + traslucidos.size());
    ultimosTriangulos = 0;
    for (const Dibujo& d : opacos) ultimosTriangulos += (d.lote >= 0 ? E.lotes[d.lote].numIndices : E.mallas[d.malla].numIndices) / 3;
    for (const Dibujo& d : traslucidos) ultimosTriangulos += (d.lote >= 0 ? E.lotes[d.lote].numIndices : E.mallas[d.malla].numIndices) / 3;

    // ---- 10.4 Cielo (cubemap): al final de los opacos, en la profundidad maxima ----
    glDepthFunc(GL_LEQUAL);
    shaderCielo->use();
    glm::mat4 vistaSinTraslacion = glm::mat4(glm::mat3(vista));
    if (modoCamara == CAM_AEREA) proyeccion = glm::perspective(glm::radians(50.0f), (float)anchoVentana / altoVentana, 0.1f, 1000.0f);
    shaderCielo->setMat4("projection", proyeccion);
    shaderCielo->setMat4("view", vistaSinTraslacion);
    shaderCielo->setInt("skybox", 1);
    glBindVertexArray(cielo->VAO);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glDepthFunc(GL_LESS);
    if (modoCamara == CAM_AEREA) ObtenerMatrices(proyeccion, vista);

    // ---- 10.5 TRASLUCIDOS: de atras hacia adelante, sin escribir profundidad ----
    std::sort(traslucidos.begin(), traslucidos.end(),
              [](const Dibujo& a, const Dibujo& b) { return a.distancia > b.distancia; });
    shaderIluminacion->use();
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    for (const Dibujo& d : traslucidos) Dibujar(d);
    glDepthMask(GL_TRUE);

    // ---- 10.6 HUD: mira y fundido ----
    DibujarHUD();
    glBindVertexArray(0);
}

// Picking por color (igual que la practica 13): cada objeto interactivo se dibuja
// con un color unico y se lee el pixel que esta bajo la mira o el mouse.
void RenderizarPicking(const glm::mat4& proyeccion, const glm::mat4& vista)
{
    ModeloEscena& E = *estacion;
    glDisable(GL_BLEND);
    glDisable(GL_MULTISAMPLE);   // sin antialiasing: cada pixel debe tener un color de ID exacto
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    shaderPicking->use();
    shaderPicking->setMat4("projection", proyeccion);
    shaderPicking->setMat4("view", vista);

    // Primero lo no interactivo en negro, para que tape lo que esta detras de una pared
    for (size_t i = 0; i < E.nodos.size(); i++) {
        if (!visibleEfectivo[i] || E.nodos[i].mallas.empty()) continue;
        int id = -1;
        for (int p = (int)i; p >= 0 && id < 0; p = E.nodos[p].padre) id = E.nodos[p].idInteractivo;
        int codigo = id + 1;   // 0 = nada
        shaderPicking->setVec4("PickingColor", glm::vec4((codigo & 0xFF) / 255.0f, ((codigo >> 8) & 0xFF) / 255.0f, 0.0f, 1.0f));
        shaderPicking->setMat4("model", E.nodos[i].global);
        for (unsigned int mi : E.nodos[i].mallas) {
            if (E.mallas[mi].translucida && id < 0) continue;
            glBindVertexArray(E.mallas[mi].vao);
            glDrawElements(GL_TRIANGLES, E.mallas[mi].numIndices, GL_UNSIGNED_INT, 0);
        }
    }
    glFlush();
    glFinish();

    double mx = anchoVentana / 2.0, my = altoVentana / 2.0;
    if (modoCamara == CAM_AEREA) glfwGetCursorPos(ventana, &mx, &my);
    unsigned char px[4] = { 0, 0, 0, 0 };
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glReadPixels((int)mx, (int)(altoVentana - my), 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, px);
    glEnable(GL_MULTISAMPLE);
    int codigo = px[0] + px[1] * 256;
    seleccionado = codigo - 1;
    if (seleccionado >= (int)interactivos.size()) seleccionado = -1;
    if (seleccionado >= 0) std::cout << "> Seleccionado: " << interactivos[seleccionado].nombre << std::endl;
}

void DibujarHUD()
{
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    shaderHUD->use();
    glBindVertexArray(vaoHUD);
    float ar = (float)altoVentana / (float)anchoVentana;

    // Mira en forma de cruz (solo en primera persona y camara libre)
    if (modoCamara == CAM_PRIMERA_PERSONA || modoCamara == CAM_LIBRE) {
        glm::vec4 color = seleccionado >= 0 ? glm::vec4(0.3f, 0.7f, 1.0f, 0.9f) : glm::vec4(1.0f, 1.0f, 1.0f, 0.75f);
        shaderHUD->setVec4("color", color);
        shaderHUD->setVec2("escala", glm::vec2(0.012f * ar, 0.0018f));
        glDrawArrays(GL_TRIANGLES, 0, 6);
        shaderHUD->setVec2("escala", glm::vec2(0.0018f * ar, 0.012f));
        glDrawArrays(GL_TRIANGLES, 0, 6);
    }
    // Fundido a negro (transicion entre escenas)
    if (fundido > 0.0f) {
        shaderHUD->setVec4("color", glm::vec4(0.0f, 0.0f, 0.0f, fundido));
        shaderHUD->setVec2("escala", glm::vec2(1.0f, 1.0f));
        glDrawArrays(GL_TRIANGLES, 0, 6);
    }
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
}

// ============================================================================
// BLOQUE 11. Interaccion con objetos
// ----------------------------------------------------------------------------
//   Clic izquierdo  seleccionar (picking)     Clic derecho  soltar seleccion
//   G               tomar / soltar            Flechas       trasladar
//   RePag / AvPag   subir / bajar             R / T         rotar
//   + / -           escalar                   Retroceso     regresar a su lugar
// La transformacion se aplica en animPadre:  T(desp) * T(pivote) * Ry * S * T(-pivote)
// ============================================================================
void ActualizarInteraccion(float dt)
{
    ModeloEscena& E = *estacion;
    auto tecla = [](int k) { return glfwGetKey(ventana, k) == GLFW_PRESS; };

    if (seleccionado >= 0) {
        ObjetoInteractivo& o = interactivos[seleccionado];
        float v = 1.5f * dt;
        glm::vec3 adelante = glm::normalize(glm::vec3(camara.Front.x, 0.0f, camara.Front.z));
        glm::vec3 derecha = glm::normalize(glm::cross(adelante, glm::vec3(0, 1, 0)));
        if (modoCamara == CAM_AEREA) { adelante = glm::vec3(0, 0, -1); derecha = glm::vec3(1, 0, 0); }
        if (tecla(GLFW_KEY_UP)) o.desplazamiento += adelante * v;
        if (tecla(GLFW_KEY_DOWN)) o.desplazamiento -= adelante * v;
        if (tecla(GLFW_KEY_RIGHT)) o.desplazamiento += derecha * v;
        if (tecla(GLFW_KEY_LEFT)) o.desplazamiento -= derecha * v;
        if (tecla(GLFW_KEY_PAGE_UP)) o.desplazamiento.y += v;
        if (tecla(GLFW_KEY_PAGE_DOWN)) o.desplazamiento.y -= v;
        if (tecla(GLFW_KEY_R)) o.rotacionY += 90.0f * dt;
        if (tecla(GLFW_KEY_T)) o.rotacionY -= 90.0f * dt;
        if (tecla(GLFW_KEY_EQUAL) || tecla(GLFW_KEY_KP_ADD)) o.escala = std::min(o.escala * (1.0f + dt), 3.0f);
        if (tecla(GLFW_KEY_MINUS) || tecla(GLFW_KEY_KP_SUBTRACT)) o.escala = std::max(o.escala * (1.0f - dt), 0.2f);
    }

    for (ObjetoInteractivo& o : interactivos) {
        if (o.tomado) {
            // S2. El objeto tomado flota frente a la camara y gira (rotacion constante)
            glm::vec3 frente = camara.Position + camara.Front * 1.6f - glm::vec3(0.0f, 0.6f, 0.0f);
            o.desplazamiento = frente - o.pivote;
            o.rotacionY += 45.0f * dt;
        } else if (o.cayendo) {
            // S3. Al soltarlo cae en linea recta hasta el piso (traslacion constante)
            o.desplazamiento.y -= 4.0f * dt;
            if (o.desplazamiento.y <= o.yDestino) { o.desplazamiento.y = o.yDestino; o.cayendo = false; }
        }
        glm::mat4 m = glm::translate(glm::mat4(1.0f), o.desplazamiento + o.pivote);
        m = glm::rotate(m, glm::radians(o.rotacionY), glm::vec3(0, 1, 0));
        m = glm::scale(m, glm::vec3(o.escala));
        m = glm::translate(m, -o.pivote);
        E.nodos[o.nodo].animPadre = m;
    }
}

void TomarOSoltar()
{
    if (seleccionado < 0) return;
    ObjetoInteractivo& o = interactivos[seleccionado];
    if (!o.tomado) {
        for (ObjetoInteractivo& otro : interactivos) otro.tomado = false;
        o.tomado = true;
        o.cayendo = false;
    } else {
        o.tomado = false;
        o.cayendo = true;
        o.yDestino = alturaPiso - o.pivote.y;   // su base termina sobre el piso
    }
}

void RestaurarObjeto()
{
    if (seleccionado < 0) return;
    ObjetoInteractivo& o = interactivos[seleccionado];
    o.desplazamiento = glm::vec3(0.0f); o.rotacionY = 0.0f; o.escala = 1.0f; o.tomado = false; o.cayendo = false;
}

// ============================================================================
// BLOQUE 12. Transicion a la escena 2 (subir / bajar del vagon)
// ----------------------------------------------------------------------------
// Cuando el tren esta detenido con las puertas abiertas y el usuario esta
// junto a una puerta, [F] hace un fundido a negro y lo pasa al interior del
// vagon 5 (el que tiene asientos y pasamanos). Este es el CONECTOR con la
// escena 2: cuando exista, en EntrarEscenaVagon() se cargara esa escena.
// ============================================================================
bool JuntoAPuerta()
{
    if (estadoTren != TREN_DETENIDO) return false;
    for (const Puerta& p : puertas) {
        glm::vec3 w = glm::vec3(estacion->nodos[p.nodo].global[3]);
        if (glm::length(glm::vec2(w.x - posJugador.x, w.z - posJugador.z)) < 1.8f) return true;
    }
    return false;
}

void EntrarEscenaVagon()
{
    // Interior del vagon 5 calculado con su caja envolvente (el tren esta detenido)
    estacion->ActualizarJerarquia();
    estacion->CalcularCajas();
    const Nodo& v = estacion->nodos[nodoVagonInterior];
    colisionVagon.zonasCaminables.clear();
    colisionVagon.zonasCaminables.push_back({ v.aabbMin.x + 0.25f, v.aabbMax.x - 0.25f, v.aabbMin.z + 1.2f, v.aabbMax.z - 1.2f });
    alturaPiso = -0.12f;
    posJugador = glm::vec3((v.aabbMin.x + v.aabbMax.x) * 0.5f, 0.0f, (v.aabbMin.z + v.aabbMax.z) * 0.5f);
    aBordo = true;
    modoCamara = CAM_PRIMERA_PERSONA;
    std::cout << "> Escena 2 (vista previa): dentro del vagon" << std::endl;
}

void SalirEscenaVagon()
{
    aBordo = false;
    alturaPiso = 0.0f;
    posJugador = glm::vec3(-3.2f, 0.0f, glm::clamp(posJugador.z, -100.0f, 100.0f));
    if (!colisionAnden.Libre(posJugador)) posJugador.x = -4.0f;
    std::cout << "> De regreso en el anden" << std::endl;
}

void IntentarSubirOBajar()
{
    if (fundidoDireccion != 0 || nodoVagonInterior < 0) return;
    if (aBordo || JuntoAPuerta()) { fundidoDireccion = 1; accionPendienteSubir = !aBordo; }
}

void ActualizarFundido(float dt)
{
    if (fundidoDireccion == 0) return;
    fundido += fundidoDireccion * dt * 1.6f;
    if (fundido >= 1.0f) {   // pantalla negra: aqui se cambia de escena
        fundido = 1.0f;
        if (accionPendienteSubir) EntrarEscenaVagon(); else SalirEscenaVagon();
        fundidoDireccion = -1;
    }
    if (fundido <= 0.0f) { fundido = 0.0f; fundidoDireccion = 0; }
}

// ============================================================================
// BLOQUE 13. Entrada: teclado, mouse y callbacks
// ============================================================================
void ProcesarEntrada(float dt)
{
    // El movimiento continuo (WASD, flechas) se lee en los BLOQUES 9 y 11
}

void CambiarModoCamara(ModoCamara nuevo)
{
    if (aBordo && nuevo != CAM_PRIMERA_PERSONA) return;   // dentro del vagon solo primera persona
    if (modoCamara == CAM_LIBRE && nuevo == CAM_PRIMERA_PERSONA)
        camara.Position = posJugador + glm::vec3(0.0f, ALTURA_OJOS, 0.0f);
    modoCamara = nuevo;
    glfwSetInputMode(ventana, GLFW_CURSOR, nuevo == CAM_AEREA ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED);
    primerMouse = true;
}

void callbackTecla(GLFWwindow* w, int tecla, int scancode, int accion, int mods)
{
    if (accion != GLFW_PRESS) return;
    switch (tecla) {
    case GLFW_KEY_ESCAPE: glfwSetWindowShouldClose(w, true); break;
    // Camaras
    case GLFW_KEY_1: CambiarModoCamara(CAM_PRIMERA_PERSONA); break;
    case GLFW_KEY_2: CambiarModoCamara(CAM_LIBRE); break;
    case GLFW_KEY_3: CambiarModoCamara(CAM_AEREA); break;
    case GLFW_KEY_C:
        if (!aBordo && modoCamara != CAM_CINEMATICA) {
            modoAntesDeCinematica = (modoCamara == CAM_AEREA) ? CAM_PRIMERA_PERSONA : modoCamara;
            CambiarModoCamara(CAM_PRIMERA_PERSONA);
            modoCamara = CAM_CINEMATICA;
            tiempoCinematica = 0.0f;
        }
        break;
    // Tren y escena
    case GLFW_KEY_ENTER:   // llamar al tren ahora
        if (estadoTren == TREN_AUSENTE) CambiarEstadoTren(TREN_LLEGANDO);
        break;
    case GLFW_KEY_F: IntentarSubirOBajar(); break;
    case GLFW_KEY_X: conosColocados = !conosColocados; tiempoConos = 0.0f; break;
    case GLFW_KEY_P: pantallasEncendidas = !pantallasEncendidas; break;
    // Interaccion
    case GLFW_KEY_G: TomarOSoltar(); break;
    case GLFW_KEY_BACKSPACE: RestaurarObjeto(); break;
    // Vista de alambre (como en las practicas)
    case GLFW_KEY_M: glPolygonMode(GL_FRONT_AND_BACK, GL_LINE); break;
    case GLFW_KEY_N: glPolygonMode(GL_FRONT_AND_BACK, GL_FILL); break;
    // Captura de pantalla para el reporte
    case GLFW_KEY_F12: {
        _mkdir("capturas");
        char nombre[64];
        snprintf(nombre, sizeof(nombre), "capturas/captura_%04d.bmp", (int)(tiempoTotal * 10));
        GuardarCaptura(nombre);
        break;
    }
    }
}

void callbackBotonMouse(GLFWwindow* w, int boton, int accion, int mods)
{
    if (accion != GLFW_PRESS) return;
    if (boton == GLFW_MOUSE_BUTTON_LEFT) solicitudPicking = true;
    if (boton == GLFW_MOUSE_BUTTON_RIGHT) {
        if (seleccionado >= 0 && interactivos[seleccionado].tomado) TomarOSoltar();
        seleccionado = -1;
    }
}

void callbackMouse(GLFWwindow* w, double xpos, double ypos)
{
    if (primerMouse) { ultimoX = (float)xpos; ultimoY = (float)ypos; primerMouse = false; }
    float dx = (float)xpos - ultimoX;
    float dy = ultimoY - (float)ypos;
    ultimoX = (float)xpos;
    ultimoY = (float)ypos;
    if (modoCamara == CAM_PRIMERA_PERSONA || modoCamara == CAM_LIBRE) camara.ProcessMouseMovement(dx, dy);
}

void callbackScroll(GLFWwindow* w, double dx, double dy)
{
    if (modoCamara == CAM_AEREA) zoomAereo = glm::clamp(zoomAereo - (float)dy * 2.0f, 6.0f, 120.0f);
    else camara.ProcessMouseScroll((float)dy);
}

void callbackTamano(GLFWwindow* w, int ancho, int alto)
{
    if (ancho == 0 || alto == 0) return;   // ventana minimizada
    anchoVentana = ancho;
    altoVentana = alto;
    glViewport(0, 0, ancho, alto);
}

// ============================================================================
// BLOQUE 14. Utilidades
// ============================================================================
void ActualizarTitulo()
{
    static float acumulado = 0.0f;
    static int cuadros = 0;
    acumulado += deltaTime;
    cuadros++;
    if (acumulado < 0.25f) return;

    const char* camaras[] = { "Primera persona", "Camara libre", "Vista aerea", "Cinematica" };
    const char* trenes[] = { "en camino", "llegando", "abriendo puertas", "detenido", "cerrando puertas", "saliendo" };
    std::ostringstream t;
    t << TITULO << "  |  " << (int)(cuadros / acumulado) << " FPS  |  Camara: " << camaras[modoCamara]
      << "  |  Tren: " << trenes[estadoTren];
    if (aBordo) t << "  |  ESCENA 2 (vista previa): dentro del vagon - [F] Bajar";
    else if (JuntoAPuerta()) t << "  |  [F] Subir al vagon";
    if (seleccionado >= 0) t << "  |  Objeto: " << interactivos[seleccionado].nombre << " ([G] tomar)";
    glfwSetWindowTitle(ventana, t.str().c_str());
    acumulado = 0.0f;
    cuadros = 0;
}

// Guarda lo que hay en pantalla como imagen BMP (sin librerias extra)
void GuardarCaptura(const std::string& archivo)
{
    int w = anchoVentana, h = altoVentana;
    int fila = (w * 3 + 3) & ~3;
    std::vector<unsigned char> px(fila * h);
    glPixelStorei(GL_PACK_ALIGNMENT, 4);
    glReadPixels(0, 0, w, h, GL_BGR, GL_UNSIGNED_BYTE, px.data());

    unsigned char cab[54] = { 'B', 'M' };
    int tamano = 54 + (int)px.size();
    auto escribir32 = [&](int pos, int v) { for (int i = 0; i < 4; i++) cab[pos + i] = (unsigned char)(v >> (8 * i)); };
    escribir32(2, tamano); escribir32(10, 54); escribir32(14, 40);
    escribir32(18, w); escribir32(22, h);
    cab[26] = 1; cab[28] = 24;
    escribir32(34, (int)px.size());
    std::ofstream f(archivo, std::ios::binary);
    f.write((char*)cab, 54);
    f.write((char*)px.data(), px.size());
    std::cout << "> Captura guardada: " << archivo << std::endl;
}

void ReproducirSonido(const char* ruta)
{
    if (!motorSonido) return;
    std::ifstream existe(ruta);
    if (existe.good()) motorSonido->play2D(ruta, false);
}
