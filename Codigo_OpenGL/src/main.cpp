/*
 * ============================================================================
 *  PROYECTO FINAL - COMPUTACION GRAFICA E INTERACCION HUMANO-COMPUTADORA
 *  Facultad de Ingenieria, UNAM - Semestre 2027-1
 *
 *  Metro CDMX - Recorrido por los andenes y la planta alta
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
 *                 7.6 Torniquetes de planta alta (proximidad)
 *    BLOQUE 8.  Iluminacion (seleccion de las lamparas mas cercanas)
 *    BLOQUE 9.  Camaras (primera persona + colisiones, libre, aerea)
 *    BLOQUE 10. Renderizado (picking, opacos, cielo, traslucidos, HUD)
 *    BLOQUE 11. Interaccion con objetos (seleccionar, tomar, mover, rotar, escalar)
 *    BLOQUE 12. Transiciones entre plantas y al vagon
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
#include <array>
#include <memory>
#include <initializer_list>
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
#include <torniquetes.h>
#include <flujo_pasajeros.h>

#include <irrKlang.h>                   // Audio (igual que en las practicas)
using namespace irrklang;

// ============================================================================
// BLOQUE 1. Configuracion general
// ============================================================================
unsigned int anchoVentana = 1280;
unsigned int altoVentana = 720;
const char* TITULO = "Metro CDMX - Universidad";
int muestrasMSAA = 4;   // antialiasing (0 = apagado, mas rapido en tarjetas integradas)

// Archivos (rutas relativas a la carpeta bin/)
std::string rutaModelo = "models/Andenes_Completo.fbx";
const char* RUTA_PLANTA_ALTA = "models/Estacion_Planta_Alta.fbx";
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
std::unique_ptr<ModeloEscena> estacion;
std::unique_ptr<ModeloEscena> escenaAndenEnEspera, interiorGuardado;
FlujoPasajeros pasajeros;
glm::vec3 regresoAlAnden;
float giroAlAnden = 0.0f, inclinacionAlAnden = 0.0f;
glm::vec3 centroVagon;
GLint uniformPiel = -1, uniformPose = -1;
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
std::vector<ObjetoInteractivo> interactivos, objetosGuardadosAnden, objetosGuardadosAlta;
int seleccionado = -1;
bool solicitudPicking = false;

// ---- 2.9 Colisiones y escenas ----
MundoColision colisionAnden;
MundoColision colisionVagon;
MundoColision colisionPlantaAlta;
Torniquetes torniquetesAlta;
bool plantaAlta = false;
bool plantaAltaDisponible = false;
bool avisoCargaPlanta = false;

// Numeracion por hilera desde -Z: primero izquierda (X menor), luego derecha.
// La planta alta numera sus barandales por lado, no alternados.
struct AccesoEscalera {
    glm::vec3 boca{0.0f};
    float haciaPasillo = 0.0f;   // signo en Z; 0 indica que no hay geometria valida
    float semiancho = 0.0f;
    glm::vec3 Descanso() const { return boca + glm::vec3(0, 0, haciaPasillo * 1.05f); }
};
const std::array<const char*, 8> nombresHuecosAlta = {
    "Esc_Sup_05", "Esc_Sup_01", "Esc_Sup_06", "Esc_Sup_02",
    "Esc_Sup_07", "Esc_Sup_03", "Esc_Sup_08", "Esc_Sup_04"
};
std::array<AccesoEscalera, 8> accesosAnden, accesosAlta;
int escaleraPendiente = -1;
int escaleraInicio = -1;         // opcion de captura: iniciar junto a un hueco de arriba
bool aBordo = false;               // true = vista previa de la escena 2 (dentro del vagon)
float fundido = 0.0f;              // 0 = sin negro, 1 = pantalla negra
int fundidoDireccion = 0;          // 1 oscureciendo, 2 negro ya presentado, -1 aclarando
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
bool Inicializar(bool oculta);
void PrepararEscena();
bool PrepararPlantaAlta();
bool CargarPlanta(bool arriba);
void ColocarEnEscalera(int i);
int EscaleraCercana();
bool ProbarEscaleras();
bool ProbarTorniquetes();
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
void DibujarPaisaje(const glm::mat4& proyeccion, const glm::mat4& vista);
void RenderizarPicking(const glm::mat4& proyeccion, const glm::mat4& vista);
void DibujarHUD();
void ActualizarInteraccion(float dt);
void IntentarSubirOBajar();
void ActualizarFundido(float dt);
void EntrarEscenaVagon();
void SalirEscenaVagon();
bool ProbarPasajeros();
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
    bool probarEscaleras = false, probarTorniquetes = false, probarPasajeros = false, iniciarVagon = false;
    bool camaraInicialIndicada = false;
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
        else if (a == "--planta-alta" && i + 1 < argc) escaleraInicio = atoi(argv[++i]) - 1;
        else if (a == "--probar-escaleras") probarEscaleras = true;
        else if (a == "--probar-torniquetes") probarTorniquetes = true;
        else if (a == "--probar-pasajeros") probarPasajeros = true;
        else if (a == "--interior-tren") iniciarVagon = true;
        else if (a == "--modo" && i + 1 < argc) modoCamara = (ModoCamara)atoi(argv[++i]);
        else if (a == "--cam" && i + 5 < argc) {
            camaraInicialIndicada = true;
            posJugador = glm::vec3((float)atof(argv[i + 1]), 0.0f, (float)atof(argv[i + 3]));
            camara.Position = glm::vec3(posJugador.x, (float)atof(argv[i + 2]), posJugador.z);
            camara.Yaw = (float)atof(argv[i + 4]);
            camara.Pitch = (float)atof(argv[i + 5]);
            i += 5;
        }
    }

    if (!Inicializar(probarEscaleras || probarTorniquetes || probarPasajeros || !archivoCapturaAuto.empty())) return -1;
    auto cerrar = [] {
        pasajeros.Liberar();
        estacion.reset(); interiorGuardado.reset(); escenaAndenEnEspera.reset();
        if (motorSonido) motorSonido->drop();
        glfwTerminate();
    };
    if (!pasajeros.Cargar()) std::cerr << "No se pudieron cargar los FBX de pasajeros.\n";
    if (!CargarPlanta(false)) { cerrar(); return 1; }
    plantaAltaDisponible = std::ifstream(RUTA_PLANTA_ALTA).good();
    if (escaleraInicio >= 0 && escaleraInicio < (int)accesosAnden.size()) {
        if (!CargarPlanta(true)) { cerrar(); return 1; }
        if (camaraInicialIndicada) alturaPiso = accesosAlta[escaleraInicio].boca.y;
        else {
            ColocarEnEscalera(escaleraInicio);
            camara.Yaw += 180.0f;   // para la captura, mirar hacia el hueco
            camara.Pitch = -14.0f;
            camara.ProcessMouseMovement(0, 0);
        }
    }
    if (probarEscaleras || probarTorniquetes || probarPasajeros) {
        bool ok = !probarEscaleras || ProbarEscaleras();
        if (ok && probarTorniquetes) ok = (plantaAlta || CargarPlanta(true)) && ProbarTorniquetes();
        if (ok && probarPasajeros) ok = ProbarPasajeros();
        cerrar();
        return ok ? 0 : 1;
    }

    // Modo captura: simula el tiempo pedido con pasos fijos, dibuja y guarda
    if (!archivoCapturaAuto.empty()) {
        const float paso = 1.0f / 30.0f;
        for (float t = 0.0f; t < tiempoCapturaAuto; t += paso) {
            tiempoTotal += paso;
            ActualizarAnimaciones(paso);
            if (modoCamara != CAM_LIBRE && modoCamara != CAM_PRIMERA_PERSONA) ActualizarCamara(paso);
        }
        if (iniciarVagon && !plantaAlta) {
            const Camera vistaPedida=camara;
            EntrarEscenaVagon();
            if (!aBordo) { cerrar(); return 1; }
            if (camaraInicialIndicada) camara=vistaPedida;
            for (int i=0;i<45;++i) ActualizarAnimaciones(paso);
        }
        // Un framebuffer propio permite capturar aunque la ventana este oculta.
        GLuint fbo=0,color=0,profundidad=0;
        glGenFramebuffers(1,&fbo); glBindFramebuffer(GL_FRAMEBUFFER,fbo);
        glGenRenderbuffers(1,&color); glBindRenderbuffer(GL_RENDERBUFFER,color);
        glRenderbufferStorage(GL_RENDERBUFFER,GL_RGBA8,anchoVentana,altoVentana);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_RENDERBUFFER,color);
        glGenRenderbuffers(1,&profundidad); glBindRenderbuffer(GL_RENDERBUFFER,profundidad);
        glRenderbufferStorage(GL_RENDERBUFFER,GL_DEPTH_COMPONENT24,anchoVentana,altoVentana);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_RENDERBUFFER,profundidad);
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE) {
            std::cerr << "No se pudo preparar la captura.\n";
            cerrar(); return 1;
        }
        glViewport(0,0,anchoVentana,altoVentana);
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
        glBindFramebuffer(GL_FRAMEBUFFER,0);
        glDeleteRenderbuffers(1,&profundidad); glDeleteRenderbuffers(1,&color);
        glDeleteFramebuffers(1,&fbo);
        cerrar();
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
        if (!aBordo) ActualizarInteraccion(deltaTime);
        ActualizarCamara(deltaTime);
        ActualizarFundido(deltaTime);
        if (!estacion) break;
        estacion->ActualizarJerarquia();

        Renderizar();
        ActualizarTitulo();

        glfwSwapBuffers(ventana);
        glfwPollEvents();
    }

    cerrar();
    return 0;
}

// ============================================================================
// BLOQUE 5. Inicializacion
// ============================================================================
bool Inicializar(bool oculta)
{
    // ---- 5.1 GLFW y ventana (contexto OpenGL 3.3 core, como en las practicas) ----
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_SAMPLES, muestrasMSAA);   // antialiasing
    if (oculta) glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);

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
    glPolygonOffset(-1.0f, -2.0f);   // solo para graficos sobre una cara de letrero

    // ---- 5.3 Shaders ----
    shaderIluminacion = new Shader("shaders/metro_iluminacion.vs", "shaders/metro_iluminacion.fs");
    shaderCielo = new Shader("shaders/metro_cielo.vs", "shaders/metro_cielo.fs");
    shaderPicking = new Shader("shaders/15_picking.vs", "shaders/15_picking.fs");
    shaderHUD = new Shader("shaders/metro_hud.vs", "shaders/metro_hud.fs");

    GLuint id = shaderIluminacion->ID;
    auto ubic = [id](const char* n) { return glGetUniformLocation(id, n); };
    uniformPiel=ubic("usaPiel"); uniformPose=ubic("pose[0]");
    U = { ubic("model"), ubic("view"), ubic("projection"), ubic("matrizNormal"),
          ubic("colorDifuso"), ubic("colorEspecular"), ubic("colorEmision"), ubic("brillo"), ubic("opacidad"),
          ubic("reflectividad"), ubic("tipoMaterial"), ubic("usaTextura"),
          ubic("numLuces"), ubic("luzPosicion"), ubic("luzColor"), ubic("luzRadio"),
          ubic("solDireccion"), ubic("solColor"), ubic("luzAmbiente"), ubic("posCamara"),
          ubic("colorNiebla"), ubic("densidadNiebla"), ubic("alturaCorte") };
    shaderIluminacion->use();
    shaderIluminacion->setInt("texture_diffuse1", 0);
    shaderIluminacion->setInt("cubemapAmbiente", 1);

    // ---- 5.4 La planta activa se carga con CargarPlanta (BLOQUE 12) ----

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
// Solo las caras informativas reciben este sombreado; alarmas y soportes
// siguen usando los materiales del modelo.
void MaterialesDeSenales(ModeloEscena& E, std::initializer_list<const char*> prefijos, bool soloNivelSuperior = true)
{
    std::map<unsigned int, unsigned int> materiales;
    std::vector<char> tratada(E.mallas.size(), 0);
    struct MallaMixta { unsigned int malla; int nodo; float centroZ; bool letrasClaras; };
    std::vector<MallaMixta> mallasMixtas;
    for (const char* prefijo : prefijos) for (int raiz : E.BuscarPrefijo(prefijo, soloNivelSuperior)) {
        const Nodo& letrero = E.nodos[raiz];
        const float centroZ = (letrero.aabbMin.z + letrero.aabbMax.z) * 0.5f;
        const bool emergencia = letrero.nombre.compare(0, 11, "Emergencia_") == 0;
        E.RecorrerSubarbol(raiz, [&](int i) {
            const Nodo& n = E.nodos[i];
            if (n.nombre.find("alarma") != std::string::npos) return;
            for (unsigned int mi : n.mallas) {
                if (tratada[mi]) continue;
                tratada[mi] = 1;
                Malla& m = E.mallas[mi];
                MaterialEscena mat = E.materiales[m.material];
                if (mat.tipo == MAT_METALICO || (mat.tipo == MAT_TRASLUCIDO && !mat.texturaDifusa)) continue;
                if (emergencia && !mat.texturaDifusa) continue;
                if (!emergencia && m.numIndices > 600) {
                    glm::vec3 mn, mx;
                    ModeloEscena::CajaMundo(m, n.global, mn, mx);
                    if (mx.z - mn.z < 0.14f && mx.y - mn.y < 0.45f && mx.y - mn.y > 0.04f) {
                        if (mat.texturaDifusa)
                            mallasMixtas.push_back({ mi, i, centroZ, letrero.nombre.compare(0, 10, "Exclusivo_") == 0 });
                        else E.SepararLetras(mi, n.global, centroZ);
                    }
                }
                auto it = materiales.find(m.material);
                if (it == materiales.end()) {
                    mat.tipo = MAT_SENAL;
                    mat.emision = glm::vec3(0.0f);
                    mat.opacidad = 1.0f;
                    mat.decal = mat.texturaDifusa != 0;
                    it = materiales.emplace(m.material, (unsigned int)E.materiales.size()).first;
                    E.materiales.push_back(mat);
                }
                m.material = it->second;
                m.translucida = false;
            }
        });
    }
    for (const MallaMixta& mixta : mallasMixtas)
        E.SepararLetrasDeIcono(mixta.malla, E.nodos[mixta.nodo].global, mixta.centroZ, mixta.letrasClaras);
}

void PrepararAccesosAnden(ModeloEscena& E)
{
    accesosAnden = {};
    for (int i = 0; i < (int)accesosAnden.size(); ++i) {
        std::string numero = "0" + std::to_string(i + 1);
        int escalera = E.Buscar("Escalera_" + numero);
        if (escalera >= 0) {
            const Nodo& n = E.nodos[escalera];
            if (n.aabbMax.z > n.aabbMin.z) {
                glm::vec3 centro = (n.aabbMin + n.aabbMax) * 0.5f;
                float salida = centro.z < 0.0f ? -1.0f : 1.0f;
                accesosAnden[i] = { { centro.x, 0.0f, salida < 0 ? n.aabbMin.z : n.aabbMax.z },
                                    salida, (n.aabbMax.x - n.aabbMin.x) * 0.5f };
            }
        }
        Caja marco{ glm::vec3(FLT_MAX), glm::vec3(-FLT_MAX) };
        for (const char* lado : { "I", "D", "A", "B" }) {
            int idx = E.Buscar("Marco_Hueco_" + numero + "_" + lado);
            if (idx < 0) continue;
            marco.min = glm::min(marco.min, E.nodos[idx].aabbMin);
            marco.max = glm::max(marco.max, E.nodos[idx].aabbMax);
        }
        if (marco.min.x < marco.max.x)
            E.AgregarBloqueNegro("Fondo_Escalera_" + numero,
                                 { marco.min.x, marco.max.y - 0.02f, marco.min.z },
                                 { marco.max.x, marco.max.y + 0.14f, marco.max.z });
    }

    // El borde inferior de los muros queda a y=0; la grava termina a y=-0.64.
    // Se prolongan solo por debajo de la reja, a ambos lados de las vias.
    for (float x : { -15.0f, 15.0f })
        E.AgregarBloqueNegro("Zocalo_Via", { x - 0.16f, -0.82f, -110.0f },
                                         { x + 0.16f, 0.025f, 110.0f });
    E.ActualizarJerarquia();
}

std::vector<EscaleraPasajero> EscalerasParaPasajeros(const std::array<AccesoEscalera,8>& origen)
{
    std::vector<EscaleraPasajero> resultado;
    for (const auto& a:origen) resultado.push_back({a.boca,a.Descanso()});
    return resultado;
}

void RegistrarInteractivos(ModeloEscena& escena, std::initializer_list<const char*> prefijos,
                           const std::vector<ObjetoInteractivo>& guardados, bool soloRaices)
{
    interactivos.clear();
    for (const char* prefijo : prefijos) {
        for (int indice : escena.BuscarPrefijo(prefijo, soloRaices)) {
            Nodo& nodo = escena.nodos[indice];
            if (nodo.aabbMin.x > nodo.aabbMax.x || nodo.idInteractivo >= 0) continue;

            ObjetoInteractivo objeto;
            const auto anterior = std::find_if(guardados.begin(), guardados.end(),
                [&](const ObjetoInteractivo& guardado) { return guardado.nombre == nodo.nombre; });
            if (anterior != guardados.end()) objeto = *anterior;
            objeto.nodo = indice;
            objeto.nombre = nodo.nombre;
            objeto.pivote = {
                (nodo.aabbMin.x + nodo.aabbMax.x) * 0.5f,
                nodo.aabbMin.y,
                (nodo.aabbMin.z + nodo.aabbMax.z) * 0.5f
            };
            nodo.idInteractivo = static_cast<int>(interactivos.size());
            interactivos.push_back(std::move(objeto));
        }
    }
}

bool PrepararPlantaAlta()
{
    ModeloEscena& E = *estacion;
    E.CalcularCajas();
    colisionPlantaAlta = {};
    colisionPlantaAlta.radioJugador = 0.24f;  // los carriles abiertos miden unos 0.60 m libres
    accesosAlta = {};
    if (E.Buscar("Estacion_Base") < 0) return false;

    // Piso de la nave principal; los corredores laterales solapan su entrada
    // con esta zona para que no haya un borde invisible entre rectangulos.
    colisionPlantaAlta.zonasCaminables.push_back({ -19.05f, 6.15f, -69.65f, 69.90f });
    for (int i : E.BuscarPrefijo("Pasillo_")) {
        const Nodo& n = E.nodos[i];
        bool izquierdo = n.aabbMax.x < 0.0f;
        colisionPlantaAlta.zonasCaminables.push_back({
            n.aabbMin.x + (izquierdo ? 0.30f : -0.85f), n.aabbMax.x + (izquierdo ? 0.85f : -0.30f),
            n.aabbMin.z + 0.18f, n.aabbMax.z - 0.18f
        });
    }

    for (int i = 0; i < (int)accesosAlta.size(); ++i) {
        int indice = E.Buscar(nombresHuecosAlta[i]);
        if (indice < 0) return false;
        const Nodo& n = E.nodos[indice];
        if (n.aabbMin.z >= n.aabbMax.z) return false;
        const glm::vec3 mn = n.aabbMin, mx = n.aabbMax;
        glm::vec3 centro = (mn + mx) * 0.5f;
        // En este modelo las U de los barandales abren hacia el centro de la nave.
        float salida = centro.z < 0.0f ? 1.0f : -1.0f;
        accesosAlta[i] = { { centro.x, 0.019f, salida > 0 ? mx.z : mn.z },
                          salida, (mx.x - mn.x) * 0.5f };
        colisionPlantaAlta.obstaculos.push_back({ mn, mx });
        E.AgregarBloqueNegro("Fondo_" + std::string(nombresHuecosAlta[i]),
                             { mn.x - 0.35f, -0.70f, mn.z - 0.35f },
                             { mx.x + 0.35f, -0.035f, mx.z + 0.35f });
    }

    std::vector<int> referencias = torniquetesAlta.Preparar(E, colisionPlantaAlta);

    // Los gabinetes y barandales se quedan fijos. Las hojas ya tienen cajas moviles.
    for (size_t i = 0; i < E.nodos.size(); ++i) {
        const Nodo& n = E.nodos[i];
        if (n.mallas.empty() || n.nombre.compare(0, 8, "Esc_Sup_") == 0 ||
            std::find(referencias.begin(), referencias.end(), (int)i) != referencias.end()) continue;
        glm::vec3 tam = n.aabbMax - n.aabbMin;
        if (n.aabbMin.y < 1.5f && n.aabbMax.y > 0.25f && tam.x < 8.0f && tam.z < 11.0f)
            colisionPlantaAlta.obstaculos.push_back({ n.aabbMin, n.aabbMax });
    }
    for (const auto& acceso : accesosAlta)
        if (!colisionPlantaAlta.Libre(acceso.Descanso())) return false;

    // Las luminarias de arriba estan agrupadas en mallas largas; se muestrean
    // las dos hileras de la nave sin traer luces de los andenes.
    posicionesLamparas.clear();
    lamparaConFalla = -1;
    for (float x : { -14.0f, -2.0f }) for (float z = -58.0f; z <= 60.0f; z += 5.0f)
        posicionesLamparas.push_back({ x, 3.82f, z });
    MaterialesDeSenales(E, { "Letrero_Andenes_", "Letrero_Salida_", "Letrero_Est_Fumar_" }, false);
    RegistrarInteractivos(E, {"Letrero_Andenes_", "Letrero_Salida_", "Mural"}, objetosGuardadosAlta, false);
    for (const auto& objeto : interactivos) referencias.push_back(objeto.nodo);
    pasajeros.PrepararAlta(E,colisionPlantaAlta,torniquetesAlta,EscalerasParaPasajeros(accesosAlta));
    E.ActualizarJerarquia();
    E.ConstruirLotes(std::vector<char>(E.nodos.size(), 0), referencias, 20.0f);
    return true;
}

void PrepararEscena()
{
    ModeloEscena& E = *estacion;
    // Al volver se reconstruyen los indices del FBX. Los relojes del anden se conservan.
    puertas.clear(); ruedas.clear(); alarmas.clear(); conos.clear(); imagenesPantalla.clear();
    interactivos.clear(); posicionesLamparas.clear(); nodosLamparas.clear();
    colisionAnden = {};
    lamparaConFalla = -1;
    pistaLlegada.cuadros.clear(); pistaSalida.cuadros.clear(); pistaPuertas.cuadros.clear();
    pistaCamPos.cuadros.clear(); pistaCamMira.cuadros.clear();

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
    std::vector<PuertaPasajero> accesosTren;
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
        accesosTren.push_back({.25f*(E.nodos[i].aabbMin+E.nodos[i].aabbMax+
                                     E.nodos[izq].aabbMin+E.nodos[izq].aabbMax)});
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
    RegistrarInteractivos(E, {"Pantalla_", "Proxima_", "Terminal_", "Exclusivo_", "Direccion_Indios", "Emergencia_"},
                         objetosGuardadosAnden, true);

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

    PrepararAccesosAnden(E);
    pasajeros.PrepararAnden(E,colisionAnden,std::move(accesosTren),EscalerasParaPasajeros(accesosAnden));
    MaterialesDeSenales(E, { "Terminal_", "Proxima_", "Exclusivo_", "Direccion_Indios", "Emergencia_" });

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
    if (aBordo) { pasajeros.ActualizarVagon(dt); return; }
    if (plantaAlta) {
        torniquetesAlta.Actualizar(*estacion, colisionPlantaAlta, posJugador, dt, pasajeros.SensoresAlta());
        pasajeros.ActualizarAlta(colisionPlantaAlta,dt);
        return;     // los relojes del anden se pausan mientras se recorre arriba
    }
    ActualizarTren(dt);          // 7.1
    ActualizarProcedurales(dt);  // 7.2
    ActualizarConos(dt);         // 7.3
    ActualizarSimples(dt);       // 7.4
    ActualizarCinematica(dt);    // 7.5
    if (dt>0.0f)
        pasajeros.ActualizarAnden(colisionAnden,
            (estadoTren==TREN_LLEGANDO && tiempoEstado>=9.0f) || estadoTren==TREN_ABRIENDO,
            estadoTren==TREN_DETENIDO,estadoTren==TREN_AUSENTE,desplazamientoTren,dt);
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
    if (aBordo) {
        for (int i=-3;i<=3;++i) {
            lucesPos.push_back({centroVagon.x+2.05f*i,2.13f,centroVagon.z});
            lucesColor.push_back({.44f,.47f,.51f});
            lucesRadio.push_back(2.8f);
        }
        // Simulacion de movimiento: lamparas del tunel que pasan por las
        // ventanas de ambos lados (se desplazan hacia -X a 15 m/s, cada 12 m)
        float corrimiento = std::fmod(tiempoTotal * 15.0f, 12.0f);
        for (int i = 0; i < 3; ++i) {
            float x = centroVagon.x + 12.0f - corrimiento - 12.0f * i;
            float lado = (i % 2 == 0) ? 2.2f : -2.2f;
            lucesPos.push_back({x, 1.6f, centroVagon.z + lado});
            lucesColor.push_back({1.1f, .95f, .7f});
            lucesRadio.push_back(5.0f);
        }
        return;
    }
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
        float potencia = plantaAlta ? 0.67f : (k == lamparaConFalla ? 1.3f * intensidadFalla : 1.3f);
        lucesPos.push_back(posicionesLamparas[k]);
        lucesColor.push_back(colorLampara * potencia);
        lucesRadio.push_back(9.0f);
    }

    // Faro del tren: va en la cabina que entra primero (frente en -Z)
    if (!plantaAlta && nodoTren >= 0 && estadoTren != TREN_AUSENTE) {
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
        if (fundidoDireccion != 0) mov = glm::vec3(0.0f);   // durante la transicion no se camina

        if (glm::length(mov) > 0.0f) {
            glm::vec3 deseada = posJugador + glm::normalize(mov) * velocidad * dt;
            const MundoColision& mundo = aBordo ? colisionVagon : plantaAlta ? colisionPlantaAlta : colisionAnden;
            // Pasos cortos para que correr no atraviese barandales o cajas estrechas.
            glm::vec3 nueva = posJugador;
            int pasos = std::max(1, (int)std::ceil(glm::length(deseada - posJugador) / 0.20f));
            glm::vec3 paso = (deseada - posJugador) / (float)pasos;
            for (int i = 0; i < pasos; ++i) nueva = mundo.Mover(nueva, nueva + paso);
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
        if (fundidoDireccion != 0) break;
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
        glm::vec3 centro = posJugador + glm::vec3(0.0f, alturaPiso + 1.0f, 0.0f);
        glm::vec3 ojo = centro + glm::normalize(glm::vec3(1.0f, 1.15f, 1.0f)) * 80.0f;
        proyeccion = glm::ortho(-zoomAereo * aspecto, zoomAereo * aspecto, -zoomAereo, zoomAereo, 0.1f, 400.0f);
        vista = glm::lookAt(ojo, centro, glm::vec3(0, 1, 0));
    } else {
        proyeccion = glm::perspective(glm::radians(camara.Zoom + 15.0f), aspecto, 0.05f, 1000.0f);
        vista = camara.GetViewMatrix();
        if (aBordo) {
            // Simulacion de movimiento: el vagon se mece y vibra (senoidales)
            float t = tiempoTotal;
            float rebote = 0.010f * std::sin(t * 21.0f) + 0.006f * std::sin(t * 6.7f);
            float alabeo = glm::radians(0.8f) * std::sin(t * 1.3f);
            glm::mat4 vaiven = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, rebote, 0.0f));
            vaiven = glm::rotate(vaiven, alabeo, glm::vec3(1.0f, 0.0f, 0.0f));   // eje largo del tren
            vista = vista * glm::inverse(vaiven);
        }
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
    if (mat.decal) glEnable(GL_POLYGON_OFFSET_FILL);
    glDrawElements(GL_TRIANGLES, numIndices, GL_UNSIGNED_INT, 0);
    if (mat.decal) glDisable(GL_POLYGON_OFFSET_FILL);
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

void DibujarPasajeros(const Frustum& fr, const glm::vec3& ojo)
{
    if (!pasajeros.Disponible()) return;
    const auto& grupo = aBordo ? pasajeros.vagon : plantaAlta ? pasajeros.arriba : pasajeros.anden;
    glUniform1i(uniformPiel,GL_TRUE);
    for (const auto& persona:grupo) {
        if (!persona.visible || (!aBordo && glm::distance(ojo,persona.pos)>65.0f) ||
            !fr.CajaVisible(persona.pos-glm::vec3(.8f,1,.8f),persona.pos+glm::vec3(.8f,2.0f,.8f))) continue;
        const auto& cuerpo=pasajeros.modelos[persona.tipo];
        const auto& fbx=*cuerpo.fbx;
        auto ubicacion=cuerpo.Ubicar(persona);
        auto pose=cuerpo.Pose(persona,ubicacion);
        for (const auto& nodo:fbx.nodos) if (!nodo.mallas.empty()) {
            auto inv=glm::inverse(nodo.global);
            auto paleta=pose;
            for (auto& m:paleta) m=inv*m*nodo.global;
            glUniformMatrix4fv(uniformPose,NUM_ARTICULACIONES,GL_FALSE,glm::value_ptr(paleta[0]));
            for (auto i:nodo.mallas) {
                const auto& m=fbx.mallas[i];
                DibujarConMaterial(fbx.materiales[m.material],ubicacion*nodo.global,glm::vec3(0),m.vao,m.numIndices);
            }
        }
    }
    glUniform1i(uniformPiel,GL_FALSE);
}

// ---------------------------------------------------------------------------
// Paisaje en movimiento (simula que el tren avanza)
// Dos planos largos afuera de las ventanas con una foto panoramica. Cada
// cuadro se recorre la coordenada U de la textura: el paisaje "pasa".
// Imagen: "Urban Street 01", Poly Haven (CC0) - bin/textures/paisaje.jpg
// ---------------------------------------------------------------------------
void DibujarPaisaje(const glm::mat4& proyeccion, const glm::mat4& vista)
{
    static Shader* shaderPaisaje = nullptr;
    static GLuint vao = 0, vbo = 0, textura = 0;
    if (!shaderPaisaje) {
        shaderPaisaje = new Shader("shaders/metro_paisaje.vs", "shaders/metro_paisaje.fs");
        int w, h, c;
        unsigned char* datos = stbi_load("textures/paisaje.jpg", &w, &h, &c, 3);
        glGenTextures(1, &textura);
        glBindTexture(GL_TEXTURE_2D, textura);
        if (datos) glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, datos);
        stbi_image_free(datos);
        glGenerateMipmap(GL_TEXTURE_2D);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);   // la panoramica se repite sin corte
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        // Un plano a lo largo de X: posicion (x, y, z) + uv
        float L = 80.0f, abajo = -3.0f, arriba = 9.0f;
        float v[] = { -L, abajo, 0, 0, 0,   L, abajo, 0, 1, 0,   L, arriba, 0, 1, 1,
                      -L, abajo, 0, 0, 0,   L, arriba, 0, 1, 1, -L, arriba, 0, 0, 1 };
        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(v), v, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    }
    shaderPaisaje->use();
    shaderPaisaje->setMat4("projection", proyeccion);
    shaderPaisaje->setMat4("view", vista);
    shaderPaisaje->setFloat("desplazamiento", tiempoTotal * 0.03f);   // velocidad del paisaje
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textura);
    shaderPaisaje->setInt("paisaje", 0);
    glBindVertexArray(vao);
    for (int lado = -1; lado <= 1; lado += 2) {   // un plano a cada lado del vagon
        glm::mat4 m = glm::translate(glm::mat4(1.0f), glm::vec3(centroVagon.x, 0.0f, centroVagon.z + lado * 1.6f));
        shaderPaisaje->setMat4("model", m);
        shaderPaisaje->setFloat("sentido", (float)lado);
        glDrawArrays(GL_TRIANGLES, 0, 6);
    }
    glBindVertexArray(0);
}

void Renderizar()
{
    if (fundido >= 1.0f) {
        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        return;
    }
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
    if (aBordo) glUniform3f(U.luzAmbiente, .43f, .44f, .47f);
    else if (plantaAlta) glUniform3f(U.luzAmbiente, 0.47f, 0.48f, 0.51f);
    else glUniform3f(U.luzAmbiente, 0.30f, 0.31f, 0.34f);
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
    DibujarPasajeros(fr,posVista);
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

    // ---- 10.4b Paisaje en movimiento (solo dentro del vagon) ----
    if (aBordo) DibujarPaisaje(proyeccion, vista);

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
        GLint modoPoligono[2];
        glGetIntegerv(GL_POLYGON_MODE, modoPoligono);
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        shaderHUD->setVec4("color", glm::vec4(0.0f, 0.0f, 0.0f, fundido));
        shaderHUD->setVec2("escala", glm::vec2(1.0f, 1.0f));
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glPolygonMode(GL_FRONT_AND_BACK, modoPoligono[0]);
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
        Nodo& nodo = E.nodos[o.nodo];
        if (nodo.padre >= 0) {
            const glm::mat4& padre = E.nodos[nodo.padre].global;
            m = glm::inverse(padre) * m * padre;
        }
        nodo.animPadre = m;
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
// BLOQUE 12. Transiciones entre plantas y al vagon
// ----------------------------------------------------------------------------
// Cuando el tren esta detenido con las puertas abiertas y el usuario esta
// junto a una puerta, [F] hace un fundido a negro y lo pasa al interior del
// vagon 5 (el que tiene asientos y pasamanos). Este es el CONECTOR con la
// escena 2: cuando exista, en EntrarEscenaVagon() se cargara esa escena.
// ============================================================================
bool CargarPlanta(bool arriba)
{
    if (estacion) {
        if (seleccionado >= 0 && interactivos[seleccionado].tomado) TomarOSoltar();
        (plantaAlta ? objetosGuardadosAlta : objetosGuardadosAnden) = interactivos;
    }
    seleccionado = -1;
    solicitudPicking = false;
    estacion.reset();   // libera RAM, VAOs, buffers y texturas antes de abrir el otro FBX
    estacion = std::make_unique<ModeloEscena>(arriba ? RUTA_PLANTA_ALTA : rutaModelo);
    if (!estacion->cargado || (arriba && !PrepararPlantaAlta())) {
        std::cerr << "No se pudo cargar " << (arriba ? RUTA_PLANTA_ALTA : rutaModelo) << std::endl;
        estacion.reset();
        return false;
    }
    plantaAlta = arriba;
    if (!arriba) {
        PrepararEscena();
        ActualizarAnimaciones(0.0f);
    }
    ActualizarInteraccion(0.0f);
    estacion->ActualizarJerarquia();
    return true;
}

void ColocarEnEscalera(int i)
{
    const AccesoEscalera& acceso = (plantaAlta ? accesosAlta : accesosAnden)[i];
    alturaPiso = acceso.boca.y;
    posJugador = acceso.Descanso();
    faseCaminata = 0.0f;
    modoCamara = CAM_PRIMERA_PERSONA;
    camara.Position = posJugador + glm::vec3(0, ALTURA_OJOS, 0);
    camara.Yaw = acceso.haciaPasillo > 0 ? 90.0f : -90.0f;
    camara.Pitch = 0.0f;
    camara.ProcessMouseMovement(0, 0);
    primerMouse = true;
}

int EscaleraCercana()
{
    if (modoCamara != CAM_PRIMERA_PERSONA || aBordo || !plantaAltaDisponible || fundidoDireccion != 0) return -1;
    const auto& accesos = plantaAlta ? accesosAlta : accesosAnden;
    for (int i = 0; i < (int)accesos.size(); ++i) {
        const AccesoEscalera& a = accesos[i];
        glm::vec3 diferencia = posJugador - a.boca;
        float longitudinal = diferencia.z * a.haciaPasillo;
        if (a.haciaPasillo != 0.0f && longitudinal >= -0.08f && longitudinal <= 2.65f &&
            std::fabs(diferencia.x) <= a.semiancho + 0.40f) return i;
    }
    return -1;
}

bool JuntoAPuerta()
{
    if (plantaAlta || aBordo || estadoTren != TREN_DETENIDO) return false;
    for (const Puerta& p : puertas) {
        const auto& n=estacion->nodos[p.nodo];
        for (unsigned int i:n.mallas) {
            glm::vec3 a,b;
            ModeloEscena::CajaMundo(estacion->mallas[i],n.global,a,b);
            glm::vec3 w=.5f*(a+b);
            if (w.x*posJugador.x>0 && DistanciaXZ(w,posJugador)<1.95f) return true;
        }
    }
    return false;
}

void EntrarEscenaVagon()
{
    if (aBordo || plantaAlta) return;
    if (!interiorGuardado) {
        auto interior=std::make_unique<ModeloEscena>("models/Tren_Completo123.fbx");
        if (!interior->cargado) return;
        int id=interior->Buscar("VAGON_05_RAIZ");
        if (id<0) return;
        interior->CalcularCajas();
        // El FBX recibido conserva la escala de exportacion de Blender.
        // Tres asientos homologan sus ejes y unidades con el interior en metros.
        auto centroAsiento=[&](const char* nombre,glm::vec3& p) {
            int i=interior->Buscar(nombre);
            if (i<0) return false;
            const Nodo& n=interior->nodos[i];
            p=.5f*(n.aabbMin+n.aabbMax);
            return true;
        };
        glm::vec3 a,b,c;
        if (!centroAsiento("Asiento_03",a) || !centroAsiento("Asiento_13",b) ||
            !centroAsiento("Asiento_20",c)) return;
        float largo=glm::length(b-a);
        if (largo<.01f) return;
        glm::vec3 x=(b-a)/largo;
        glm::vec3 lateral=c-a-x*glm::dot(c-a,x);
        if (glm::length(lateral)<.01f) return;
        glm::vec3 z=glm::normalize(lateral);
        glm::vec3 y=glm::cross(z,x);
        const glm::mat3 ejes=glm::transpose(glm::mat3(x,y,z))*(7.549f/largo);
        glm::mat4 normalizar(ejes);
        normalizar[3]=glm::vec4(glm::vec3(68.083f,.476f,-.866f)-ejes*a,1);
        interior->nodos[0].local=normalizar*interior->nodos[0].local;
        interior->ActualizarJerarquia();
        interior->CalcularCajas();
        const Nodo& v=interior->nodos[id];
        centroVagon=.5f*(v.aabbMin+v.aabbMax);
        std::vector<char> fuera(interior->nodos.size(),1);
        std::vector<char> visible(fuera.size(),0);
        for (int n=id;n>=0;n=interior->nodos[n].padre) fuera[n]=0,visible[n]=1;
        interior->RecorrerSubarbol(id,[&](int n){fuera[n]=0;visible[n]=1;});
        for (size_t i=0;i<visible.size();++i) interior->nodos[i].visible=visible[i];
        interior->ConstruirLotes(fuera,{},12.0f);
        interiorGuardado=std::move(interior);
    }
    regresoAlAnden=posJugador;
    giroAlAnden=camara.Yaw; inclinacionAlAnden=camara.Pitch;
    if (seleccionado>=0 && interactivos[seleccionado].tomado) TomarOSoltar();
    seleccionado=-1; solicitudPicking=false;
    escenaAndenEnEspera=std::move(estacion);
    estacion=std::move(interiorGuardado);
    colisionVagon={};
    colisionVagon.radioJugador=.23f;
    colisionVagon.zonasCaminables.push_back({65.40f,79.22f,-.60f,.60f});
    alturaPiso=.012f;
    posJugador=glm::vec3(66.05f,alturaPiso,0);
    camara.Position=posJugador+glm::vec3(0,ALTURA_OJOS,0);
    camara.Yaw=0; camara.Pitch=-4; camara.ProcessMouseMovement(0,0);
    pasajeros.PrepararVagon(*estacion);
    aBordo = true;
    modoCamara = CAM_PRIMERA_PERSONA;
    std::cout << "> Interior del vagon 5: Tren_Completo123.fbx" << std::endl;
}

void SalirEscenaVagon()
{
    if (!aBordo) return;
    interiorGuardado=std::move(estacion);
    estacion=std::move(escenaAndenEnEspera);
    aBordo = false;
    alturaPiso = 0.0f;
    posJugador = regresoAlAnden;
    camara.Position=posJugador+glm::vec3(0,ALTURA_OJOS,0);
    camara.Yaw=giroAlAnden; camara.Pitch=inclinacionAlAnden; camara.ProcessMouseMovement(0,0);
    std::cout << "> De regreso en el anden" << std::endl;
}

void IntentarSubirOBajar()
{
    if (plantaAlta || fundidoDireccion != 0 || nodoVagonInterior < 0) return;
    if (aBordo || JuntoAPuerta()) { fundidoDireccion = 1; accionPendienteSubir = !aBordo; }
}

void ActualizarFundido(float dt)
{
    if (fundidoDireccion == 0) return;
    if (fundidoDireccion == 2) {
        // Ya hubo un cuadro negro. La carga puede tardar sin congelar la imagen a medio fundir.
        if (escaleraPendiente >= 0) {
            const bool subir = !plantaAlta;
            if (!CargarPlanta(subir)) {
                avisoCargaPlanta = true;
                if (!subir || !CargarPlanta(false)) {
                    glfwSetWindowShouldClose(ventana, true);
                    return;
                }
                plantaAltaDisponible = false;
            }
            ColocarEnEscalera(escaleraPendiente);
            std::cout << "> Escalera " << escaleraPendiente + 1 << ": "
                      << (plantaAlta ? nombresHuecosAlta[escaleraPendiente] : "anden") << std::endl;
            escaleraPendiente = -1;
        } else if (accionPendienteSubir) EntrarEscenaVagon();
        else SalirEscenaVagon();
        fundidoDireccion = -1;
        return;
    }
    fundido = glm::clamp(fundido + fundidoDireccion * dt * 1.6f, 0.0f, 1.0f);
    if (fundido >= 1.0f) fundidoDireccion = 2;
    if (fundido <= 0.0f) fundidoDireccion = 0;
}

// Verifica las rutas usando la misma tecla, fundido y carga que el juego.
bool ProbarEscaleras()
{
    if (!plantaAltaDisponible || plantaAlta) return false;
    auto fallo = [](int i, const char* causa) {
        std::cerr << "Escalera " << i + 1 << ": " << causa << std::endl;
        return false;
    };
    for (int i = 0; i < (int)accesosAnden.size(); ++i) {
        ColocarEnEscalera(i);
        if (!colisionAnden.Libre(posJugador) || EscaleraCercana() != i) return fallo(i, "sin acceso desde el anden");
        const glm::vec3 partida = posJugador;
        for (bool arriba : { true, false }) {
            callbackTecla(ventana, GLFW_KEY_X, 0, GLFW_PRESS, 0);
            if (escaleraPendiente != i) return fallo(i, "X no activo el acceso correcto");
            for (int paso = 0; paso < 20 && fundidoDireccion != 0; ++paso) {
                ActualizarFundido(0.10f);
                if (!estacion) return fallo(i, "no se cargo el modelo");
            }
            if (plantaAlta != arriba || EscaleraCercana() != i) return fallo(i, "destino incorrecto");
            const MundoColision& mundo = arriba ? colisionPlantaAlta : colisionAnden;
            if (!mundo.Libre(posJugador)) return fallo(i, "el descanso tiene una obstruccion");
        }
        if (glm::length(posJugador - partida) > 0.01f) return fallo(i, "no regreso al mismo descanso");
    }
    std::cout << "> Verificadas las ocho escaleras: 16 transiciones en total." << std::endl;
    return true;
}

// Recorre los carriles con las mismas cajas y la misma animacion del juego.
bool ProbarTorniquetes()
{
    if (torniquetesAlta.carriles.size() != 20) {
        std::cerr << "Se esperaban 20 torniquetes en la planta alta" << std::endl;
        return false;
    }

    auto& mundo = colisionPlantaAlta;
    auto& puertas = torniquetesAlta;
    constexpr float dt = 1.0f / 60.0f;
    const glm::vec3 lejos(-6.0f, 0.0f, 0.0f);
    auto fallo = [](size_t i, const char* causa) {
        std::cerr << "Torniquete " << i + 1 << ": " << causa << std::endl;
        return false;
    };
    for (size_t i = 0; i < puertas.carriles.size(); ++i) {
        const auto& t = puertas.carriles[i];
        const glm::vec3 centro(t.centro.x, 0.0f, t.centro.y);
        for (float sentido : { -1.0f, 1.0f }) {
            if (mundo.Libre(centro)) return fallo(i, "cerrado debe bloquear el carril");
            glm::vec3 p = centro - glm::vec3(0, 0, sentido * 2.80f);
            if (!mundo.Libre(p)) return fallo(i, "no hay espacio para acercarse");
            bool abrio = false, esperaEnMedio = false;
            float rapidez = sentido > 0 ? VELOCIDAD_CAMINAR : VELOCIDAD_CORRER;
            for (int f = 0; f < 180 && (p.z - centro.z) * sentido < 2.80f; ++f) {
                puertas.Actualizar(*estacion, mundo, p, dt);
                abrio |= t.avance >= 0.99f;
                if (!esperaEnMedio && std::fabs(p.z - centro.z) < 0.16f) {
                    esperaEnMedio = true;
                    for (int j = 0; j < 75; ++j) puertas.Actualizar(*estacion, mundo, p, dt);
                    if (t.avance < 0.99f || !mundo.Libre(p)) return fallo(i, "se cerro con el jugador en medio");
                }
                p = mundo.Mover(p, p + glm::vec3(0, 0, rapidez * dt * sentido));
            }
            if (!abrio || !esperaEnMedio || (p.z - centro.z) * sentido < 2.80f)
                return fallo(i, "no se puede atravesar el carril");
            for (int j = 0; j < 75; ++j) puertas.Actualizar(*estacion, mundo, lejos, dt);
            if (t.avance != 0.0f || mundo.Libre(centro)) return fallo(i, "no cerro al salir");
        }
    }

    // Las cuatro conexiones entre la nave y los corredores siguen siendo transitables.
    const auto& zonas = mundo.zonasCaminables;
    if (zonas.size() != 5) return false;
    for (size_t i = 1; i < zonas.size(); ++i) {
        const auto& z = zonas[i];
        const bool izquierdo = z.xMax < 0.0f;
        const float signo = izquierdo ? -1.0f : 1.0f;
        glm::vec3 p(izquierdo ? -17.6f : 4.7f, 0.0f, (z.zMin + z.zMax) * 0.5f);
        const float salida = izquierdo ? z.xMin + 1.0f : z.xMax - 1.0f;
        for (int paso = 0; paso < 500 && (salida - p.x) * signo > 0.0f; ++paso)
            p = mundo.Mover(p, p + glm::vec3(signo * 0.10f, 0, 0));
        if ((salida - p.x) * signo > 0.0f) {
            std::cerr << "El corredor " << i << " no conecta con la nave" << std::endl;
            return false;
        }
    }
    std::cout << "> Verificados 20 torniquetes en ambos sentidos y 4 corredores." << std::endl;
    return true;
}

// ============================================================================
bool ProbarInteraccionAlta()
{
    auto mural = std::find_if(interactivos.begin(), interactivos.end(),
        [](const ObjetoInteractivo& objeto) { return objeto.nombre == "Mural"; });
    auto letrero = std::find_if(interactivos.begin(), interactivos.end(),
        [](const ObjetoInteractivo& objeto) { return objeto.nombre == "Letrero_Andenes_01"; });
    if (mural == interactivos.end() || letrero == interactivos.end()) return false;
    const Nodo& nodo = estacion->nodos[mural->nodo];
    glm::vec3 centro = (nodo.aabbMin + nodo.aabbMax) * 0.5f;
    const Camera original = camara;
    camara.Position = centro + glm::vec3(0, 0, centro.z > 0 ? -3.0f : 3.0f);
    camara.Yaw = centro.z > 0 ? 90.0f : -90.0f;
    camara.Pitch = 0;
    camara.ProcessMouseMovement(0, 0);

    GLuint fbo = 0, color = 0, profundidad = 0;
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glGenRenderbuffers(1, &color);
    glBindRenderbuffer(GL_RENDERBUFFER, color);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, anchoVentana, altoVentana);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, color);
    glGenRenderbuffers(1, &profundidad);
    glBindRenderbuffer(GL_RENDERBUFFER, profundidad);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, anchoVentana, altoVentana);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, profundidad);
    bool ok = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
    if (ok) {
        glViewport(0, 0, anchoVentana, altoVentana);
        callbackBotonMouse(ventana, GLFW_MOUSE_BUTTON_LEFT, GLFW_PRESS, 0);
        Renderizar();
        ok = seleccionado == nodo.idInteractivo;
    }
    if (ok) {
        const glm::vec3 partida(nodo.global[3]);
        callbackTecla(ventana, GLFW_KEY_G, 0, GLFW_PRESS, 0);
        ActualizarInteraccion(0.1f);
        estacion->ActualizarJerarquia();
        ok = mural->tomado && glm::distance(glm::vec3(nodo.global[3]), partida) > 0.05f;
        callbackTecla(ventana, GLFW_KEY_BACKSPACE, 0, GLFW_PRESS, 0);
        ActualizarInteraccion(0);
        estacion->ActualizarJerarquia();
        ok = ok && glm::distance(glm::vec3(nodo.global[3]), partida) < 0.001f;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteRenderbuffers(1, &color);
    glDeleteRenderbuffers(1, &profundidad);
    glDeleteFramebuffers(1, &fbo);
    camara = original;
    seleccionado = -1;
    solicitudPicking = false;
    if (ok) std::cout << "> Planta alta: mural seleccionado, tomado y restaurado." << std::endl;
    return ok;
}

bool ProbarPasajeros()
{
    if (!pasajeros.Disponible() || (!plantaAlta && !CargarPlanta(true))) return false;
    if (!ProbarInteraccionAlta()) {
        std::cerr << "No se pudo seleccionar o manipular el mural de planta alta." << std::endl;
        return false;
    }
    posJugador={-6.0f,0,0};  // el jugador no abre los torniquetes durante esta prueba
    for (int i=0;i<3600;++i) ActualizarAnimaciones(1.0f/30.0f);
    std::cout << "> Pasajeros arriba: recargas=" << pasajeros.recargas << " taquilla=" << pasajeros.compras
              << " entradas=" << pasajeros.entradas << " salidas=" << pasajeros.salidas
              << " escaleras=" << pasajeros.escaleras << std::endl;
    for (const auto& p:pasajeros.arriba)
        std::cout << "  ruta " << (&p-pasajeros.arriba.data()) << " parada=" << p.parada << "/" << p.plan.size()
                  << " posicion=(" << p.pos.x << "," << p.pos.z << ")" << std::endl;
    bool ok=pasajeros.recargas>0 && pasajeros.compras>0 && pasajeros.entradas>=4 &&
            pasajeros.salidas>=2 && pasajeros.escaleras>=4;
    if (!CargarPlanta(false)) return false;
    estadoTren=TREN_AUSENTE; tiempoEstado=0; esperaParaLlegar=1.0f;
    int duranteFrenado=0;
    for (int i=0;i<810;++i) {
        ActualizarAnimaciones(1.0f/30.0f);
        if (estadoTren==TREN_LLEGANDO) duranteFrenado=pasajeros.esperan;
        for (const auto& p:pasajeros.anden) if (p.visible && p.tipo==ADULTO && pasajeros.EsExclusivo(p.pos)) {
            std::cerr << "Adulto en el area exclusiva." << std::endl;
            ok=false;
        }
    }
    int pasean=0;
    for (const auto& p:pasajeros.anden) pasean+=p.viaje==PASEO;
    std::cout << "> Pasajeros anden: se acercan frenando=" << duranteFrenado
              << " abordaron=" << pasajeros.subidas << " siguen paseando=" << pasean << std::endl;
    ok &= duranteFrenado>0 && pasajeros.subidas>0 && pasean>0;
    estacion->ActualizarJerarquia();
    bool accesoJugador=false;
    for (const Puerta& p:puertas) {
        const auto& n=estacion->nodos[p.nodo];
        if (n.mallas.empty()) continue;
        glm::vec3 mn,mx;
        ModeloEscena::CajaMundo(estacion->mallas[n.mallas.front()],n.global,mn,mx);
        glm::vec3 d=.5f*(mn+mx);
        posJugador={std::copysign(2.72f,d.x),0,d.z};
        if (colisionAnden.Libre(posJugador) && JuntoAPuerta()) { accesoJugador=true; break; }
    }
    if (!accesoJugador) { std::cerr << "No hay una puerta accesible con F.\n"; return false; }
    auto antes=posJugador;
    IntentarSubirOBajar();
    for (int i=0;i<60;++i) ActualizarFundido(1.0f/30.0f);
    if (!aBordo) return false;
    for (int i=0;i<90;++i) ActualizarAnimaciones(1.0f/30.0f);
    int sentados=0, agarrados=0, dePie=0;
    for (const auto& p:pasajeros.vagon) {
        sentados+=p.accion==SENTARSE && p.mezcla>.95f;
        agarrados+=p.accion==AGARRARSE;
        dePie+=p.accion==QUIETO;
    }
    std::cout << "> Pasajeros vagon: sentados=" << sentados << " pasamanos=" << agarrados
              << " de pie=" << dePie << std::endl;
    ok &= sentados>4 && agarrados==2 && dePie>0;
    IntentarSubirOBajar();
    for (int i=0;i<60;++i) ActualizarFundido(1.0f/30.0f);
    ok &= !aBordo && glm::distance(posJugador,antes)<.01f;
    return ok;
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
    if (fundidoDireccion != 0 || (aBordo && nuevo != CAM_PRIMERA_PERSONA)) return;   // dentro del vagon solo primera persona
    if (modoCamara == CAM_LIBRE && nuevo == CAM_PRIMERA_PERSONA)
        camara.Position = posJugador + glm::vec3(0.0f, ALTURA_OJOS, 0.0f);
    modoCamara = nuevo;
    glfwSetInputMode(ventana, GLFW_CURSOR, nuevo == CAM_AEREA ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED);
    primerMouse = true;
}

void callbackTecla(GLFWwindow* w, int tecla, int scancode, int accion, int mods)
{
    if (accion != GLFW_PRESS || (fundidoDireccion != 0 && tecla != GLFW_KEY_ESCAPE)) return;
    switch (tecla) {
    case GLFW_KEY_ESCAPE: glfwSetWindowShouldClose(w, true); break;
    // Camaras
    case GLFW_KEY_1: CambiarModoCamara(CAM_PRIMERA_PERSONA); break;
    case GLFW_KEY_2: CambiarModoCamara(CAM_LIBRE); break;
    case GLFW_KEY_3: CambiarModoCamara(CAM_AEREA); break;
    case GLFW_KEY_C:
        if (!plantaAlta && !aBordo && modoCamara != CAM_CINEMATICA) {
            modoAntesDeCinematica = (modoCamara == CAM_AEREA) ? CAM_PRIMERA_PERSONA : modoCamara;
            CambiarModoCamara(CAM_PRIMERA_PERSONA);
            modoCamara = CAM_CINEMATICA;
            tiempoCinematica = 0.0f;
        }
        break;
    // Tren y escena
    case GLFW_KEY_ENTER:   // llamar al tren ahora
        if (!plantaAlta && estadoTren == TREN_AUSENTE) CambiarEstadoTren(TREN_LLEGANDO);
        break;
    case GLFW_KEY_F: IntentarSubirOBajar(); break;
    case GLFW_KEY_X:
        escaleraPendiente = EscaleraCercana();
        if (escaleraPendiente >= 0) fundidoDireccion = 1;
        break;
    case GLFW_KEY_V: if (!plantaAlta) { conosColocados = !conosColocados; tiempoConos = 0.0f; } break;
    case GLFW_KEY_P: pantallasEncendidas = !pantallasEncendidas; break;
    // Interaccion
    case GLFW_KEY_G: if (!aBordo) TomarOSoltar(); break;
    case GLFW_KEY_BACKSPACE: if (!aBordo) RestaurarObjeto(); break;
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
    if (accion != GLFW_PRESS || aBordo || fundidoDireccion != 0) return;
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
    if (fundidoDireccion == 0 && (modoCamara == CAM_PRIMERA_PERSONA || modoCamara == CAM_LIBRE))
        camara.ProcessMouseMovement(dx, dy);
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
    t << (aBordo ? "Metro CDMX - Vagon 5" : plantaAlta ? "Metro CDMX - Planta alta" : "Metro CDMX - Andenes")
      << "  |  " << (int)(cuadros / acumulado) << " FPS  |  Camara: " << camaras[modoCamara];
    if (!plantaAlta) t << "  |  Tren: " << trenes[estadoTren];
    int escalera = EscaleraCercana();
    if (escalera >= 0)
        t << (plantaAlta ? "  |  [X] Bajar a los andenes" : "  |  [X] Subir a planta alta")
          << " (Escalera " << escalera + 1 << ")";
    if (avisoCargaPlanta) t << "  |  No se pudo cargar la otra planta";
    if (aBordo) t << "  |  Interior del vagon - [F] Bajar";
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
