#include <random>
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
#include <windows.h>  // carteles de texto dentro de la ventana

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
#include <queue>
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
const char* RUTA_SONIDO_TREN = "sound/tren.mp3";       // llegada del tren (se corta cuando se detiene)
const char* RUTA_SONIDO_AMBIENTE = "sound/ambiente.mp3"; // ambiente del metro (en bucle)
const char* RUTA_SONIDO_CIERRE = "sound/puertas_cerrando.mp3"; // aviso y cierre de puertas
const char* RUTA_SONIDO_TONO = "sound/tono.mp3";       // tono dentro del vagon (se corta a 1 s)
const char* RUTA_SONIDO_BIP = "sound/bip_tarjeta.mp3"; // la tarjeta toca el lector del torniquete
const float AJUSTE_CIERRE = -1.2f;  // s que se adelanta el audio de cierre (negativo = empieza despues; si acaba antes que las puertas, bajar)
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
const int LAMPARAS_ACTIVAS = 9;    // lamparas cercanas que iluminan (+ faro del tren)

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
GLuint vaoHUD = 0, vboHUD = 0, texturaCartel = 0;
std::string cartelActual;
float cartelCambio = 0;   // momento del ultimo cambio de texto (para el fundido)
enum Nivel { NIVEL_NOVATO, NIVEL_MEDIO, NIVEL_EXPERTO, NIVEL_LIBRE };
int nivel = -1; // selector inicial
bool tarjetaRecargada = false, misionCumplida = false;
// Animacion de recarga de la tarjeta (modelo Tarjeta_Metro.fbx en primera persona)
const float DURACION_MAQUINA = 8.6f, DURACION_TAQUILLA = 6.2f;
bool recargaEnMaquina = true;   // maquina (insertar, monedas, pantalla) o taquilla (tarjeta al vidrio y billete)
#define DURACION_RECARGA (recargaEnMaquina ? DURACION_MAQUINA : DURACION_TAQUILLA)
float recargaAnim = -1.0f;   // segundos desde que empezo; -1 = sin animacion
float saldoTarjeta = 0.0f;
int recargaPaso = 0;
#define MONTO_RECARGA (nivel == NIVEL_LIBRE ? 10.0f : 50.0f)   // libre: 1 moneda de $10; experto: 5 monedas o un billete de $50
#define MONEDAS_RECARGA (nivel == NIVEL_LIBRE ? 1 : 5)
int numFilasMaquina = 0;
// Validacion en el torniquete: acerca la tarjeta al lector y cruza (camara bloqueada)
const float DURACION_VALIDA = 4.6f;
float validaAnim = -1.0f; int validaTorn = -1; bool validado = false;
glm::vec3 validaInicio(0), validaParada(0), validaFin(0), lectorMundo(0);
float validaYaw = 0; bool validaSalida = false; int torniqueteCerca = -1;
const float DURACION_SALIDA = 1.5f;   // siguiente pausa explicativa ([Enter] para continuar)
static const float PAUSAS_MAQUINA[] = { 0.0f, 1.9f, 4.6f, 6.4f }, PAUSAS_TAQUILLA[] = { 0.0f, 1.5f, 3.8f };
static const char* TEXTOS_MAQUINA[] = {
    "CÓMO RECARGAR EN LA MÁQUINA - 1 DE 4\nInserta tu tarjeta en la ranura \"ENTRADA DE TARJETA\", a la izquierda y debajo\nde la pantalla. La máquina lee la tarjeta y muestra el saldo que tienes.",
    "CÓMO RECARGAR EN LA MÁQUINA - 2 DE 4\nIngresa el dinero por la ranura de monedas o de billetes.\nOjo: la máquina NO da cambio. Recargaremos $50 con cinco monedas de $10.",
    "CÓMO RECARGAR EN LA MÁQUINA - 3 DE 4\nRevisa en la pantalla el monto ingresado y toca el botón \"RECARGAR\"\npara confirmar.",
    "CÓMO RECARGAR EN LA MÁQUINA - 4 DE 4\nLa máquina abona el saldo y devuelve tu tarjeta: ¡no la olvides!\nCada viaje en Metro cuesta $5.00." };
static const char* TEXTOS_TAQUILLA[] = {
    "CÓMO RECARGAR EN LA TAQUILLA - 1 DE 3\nPega tu tarjeta al lector de la ventanilla y dile a la persona\nde la taquilla cuánto quieres recargar.",
    "CÓMO RECARGAR EN LA TAQUILLA - 2 DE 3\nEntrega el dinero por la ranura del vidrio.\nA diferencia de la máquina, aquí sí te pueden dar cambio.",
    "CÓMO RECARGAR EN LA TAQUILLA - 3 DE 3\nLa persona de la taquilla abona el saldo y el lector lo confirma.\nRetira tu tarjeta. Cada viaje en Metro cuesta $5.00." };
float avisoBloqueoHasta = 0.0f;
std::vector<glm::vec3> puntosRecarga;
glm::vec3 centroMaquinas(0), centroTaquillas(0);   // para colocar al experto en medio de la entrada
int pasoNovato = 0; glm::vec3 inicioNovato(0);      // tutorial guiado del novato
GLuint texturaMenu = 0; int anchoCartel = 1000;
float menuEscala = 1, menuX0 = 0, menuY0 = 0;      // posicion del menu en pixeles (para clic)
const int MENU_W = 1000, MENU_H = 640;
void DibujarMenu();
void VolverAlMenu();
std::vector<glm::vec3> puntosMedio; std::vector<std::string> nombresMedio; int pasoMedio = 0;   // recorrido del nivel medio
std::vector<glm::vec3> filasRecarga;                     // donde termina cada fila (experto)
bool enFila = false; float tiempoFila = 0;
const int PERSONAS_FILA = 4; const float TURNO_FILA = 2.5f;     // cada 2.5 s pasa una persona
const float DURACION_FILA = (PERSONAS_FILA + 1) * TURNO_FILA;   // +1: el turno del usuario al frente
struct FilaRecarga { glm::vec3 inicio, dir; std::vector<int> personas; float reloj = 0; };
bool filasEnMovimiento = false;
float zoomLupa = 3.0f;   // aumento de la lupa del mapa
bool mapaAbierto = false; GLuint texturaMapa = 0; glm::vec3 posMapa(0);   // mapa de la red (nivel medio)
const char* NOMBRE_MAPA = "el mapa de la red del metro";
std::string infoLugar; float infoHasta = 0;
float avisoReservadoHasta = 0; bool cartelEnCentro = false;   // alerta en medio de la pantalla
int carrera = 0; float tCarrera = 0; glm::vec3 asientoJugador(0);   // experto: 1 cartel, 2 cuenta 3..0, 5 carrera, 3 sentado, 4 perdio
float tiempoViaje = 0;   // reloj del movimiento del tren (se detiene durante la carrera)
static bool TrenDetenido();   // definida despues de aBordo
std::string dialogoTexto;   // cuadro de dialogo que espera [Enter]; mientras exista el usuario no se mueve
std::string tituloCartel;   // la animacion de entrada solo corre si cambia el titulo              // ficha informativa al llegar a un punto (nivel medio)
float yawNovato = 0, pitchNovato = 0;                     // vista del novato al terminar la cinematica
static std::string InfoDeLugar(const std::string& n)
{
    if (n == "las taquillas") return "TAQUILLAS\nAqui un empleado del Metro vende y recarga la tarjeta de Movilidad Integrada.\nSirven para pagar en efectivo, pedir informes o resolver problemas con tu tarjeta.";
    if (n == "las máquinas de recarga") return "MÁQUINAS DE RECARGA\nPermiten comprar la tarjeta o recargarle saldo tú solo, sin hacer fila en la taquilla.\nLa misma tarjeta sirve en Metro, Metrobús, Tren Ligero, Trolebús, Cablebús y Ecobici.";
    if (n == "el otro lado de los torniquetes") return "TORNIQUETES\nSeparan la zona libre de la zona pagada: al pasar tu tarjeta se cobra el viaje\ny el torniquete se abre. Una vez dentro puedes transbordar sin volver a pagar.";
    if (n == "el elevador") return "ELEVADOR\nComunica la planta alta con los andenes para personas con discapacidad, adultos\nmayores, carriolas o equipaje pesado. Es parte de la accesibilidad universal.";
    if (n == "el mural del elevador") return "MURAL DE LA REMODELACIÓN (2026)\n"
        "Para el Mundial la estación se renovó con obra de Jorge Rosano Gamboa, inspirada en\n"
        "la historia, el paisaje y los símbolos de la UNAM. Él la llama un \"metamural\":\n"
        "murales dentro del mural, con una reflexión sobre el tiempo. En este elevador aparece\n"
        "la diosa Cihuatéotl, a quien el artista busca reivindicar.";
    if (n == "el cibercentro") return "CIBERCENTRO\nEspacio con computadoras y acceso a internet dentro de la estación, pensado para\nque los usuarios hagan trámites, consultas o tareas mientras se trasladan.";
    if (n == "el mural") return "MURAL: LA UNIVERSIDAD EN EL UMBRAL DEL SIGLO XXI\n"
        "Obra del pintor oaxaqueño Arturo García Bustos, inaugurada en 1989 en esta estación.\n"
        "Retrata figuras de la historia cultural y científica de México, y al cóndor y al águila,\n"
        "símbolo del latinoamericanismo que también aparece en el escudo de la UNAM.\n"
        "El Metro es también una galería: en distintas estaciones se exhiben murales y arte.";
    return "";
}
// Medio: en las taquillas y en las maquinas hay que recargar (no basta con llegar)
static bool PasoEsRecarga(bool* enMaquina = nullptr) {
    if (pasoMedio >= (int)nombresMedio.size()) return false;
    bool taq = nombresMedio[pasoMedio] == "las taquillas", maq = nombresMedio[pasoMedio] == "las máquinas de recarga";
    if (enMaquina) *enMaquina = maq;
    return taq || maq;
}
// Torniquetes junto al mural (fila de z = 43.8): paso libre, sin tarjeta ni animacion.
// La otra fila (z = -37.6, junto a taquillas y maquinas) queda a mas de 3.1 m del punto medio.
static bool ZonaTorniquetesMural(const glm::vec3& p) { return p.z > 3.1f; }
static bool PasoEsMapa() { return pasoMedio < (int)nombresMedio.size() && nombresMedio[pasoMedio] == NOMBRE_MAPA; }   // las demas filas avanzan desde que el usuario se forma   // personas: indices en pasajeros.arriba
std::vector<FilaRecarga> filas; int filaJugador = -1, turnosPasados = 0;
std::vector<std::pair<int, glm::vec3>> saliendoFila;              // persona y punto donde desaparece
glm::vec3 taqMin(0), taqMax(0); int numMaquinasRecarga = 0; int rutaPaso = -1;                          // caja del muro de taquillas
std::vector<glm::vec3> rutaGuia; float rutaRecalculo = 0;   // camino sugerido (nivel medio)
bool ayudaVisible = false;   // panel de controles (se alterna con H)
int menuHover = -1, menuElegido = -1; float menuClicT = 0;
GLFWcursor* cursorMano = nullptr;
// Un nodo puede agrupar varias maquinas/ventanillas: se divide a lo largo (~1 m c/u)
static void AgregarPuntosRecarga(const glm::vec3& mn, const glm::vec3& mx)
{
    glm::vec3 t = mx - mn;
    bool largoZ = t.z >= t.x;
    float L = largoZ ? t.z : t.x;
    int n = std::max(1, (int)std::round(L / 1.05f));
    for (int i = 0; i < n; ++i) {
        glm::vec3 c = (mn + mx) * .5f;
        float u = (largoZ ? mn.z : mn.x) + L * (i + .5f) / n;
        (largoZ ? c.z : c.x) = u;
        puntosRecarga.push_back(c);
    }
}
static glm::vec3 DirFila(const glm::vec3& p) { return glm::vec3(p.x > -6.5f ? -1.0f : 1.0f, 0, 0); }

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
static bool TrenDetenido() { return nivel == NIVEL_EXPERTO && aBordo && (carrera != 3 || tCarrera > 0.0f); }   // sentado: arranca tras 2 s
float fundido = 0.0f;              // 0 = sin negro, 1 = pantalla negra
int fundidoDireccion = 0;          // 1 oscureciendo, 2 negro ya presentado, -1 aclarando
bool accionPendienteSubir = false; // que hacer cuando el fundido llegue a negro

// ---- 2.10 Camara cinematografica (K4) ----
PistaKeyframes<glm::vec3> pistaCamPos;
PistaKeyframes<glm::vec3> pistaCamMira;
float tiempoCinematica = 0.0f;

// ---- 2.11 Audio ----
ISoundEngine* motorSonido = nullptr;
ISound* sonidoTren = nullptr;       // para poder cortarlo al detenerse el tren
ISound* sonidoAmbiente = nullptr;
ISound* sonidoTono = nullptr; float relojTono = 10.0f, tonoSonando = 0.0f;
bool cierreSonando = false;   // el aviso de cierre ya se lanzo en esta parada

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
static void DibujarRutaGuia();
static void DibujarLetreroReservado();
void ActualizarInteraccion(float dt);
void IntentarSubirOBajar();
void ActualizarFundido(float dt);
void EntrarEscenaVagon();
void SalirEscenaVagon();
bool ProbarPasajeros();
bool ProbarNiveles();
void IniciarNivel(int elegido);
bool CercaDeRecarga();
bool LadoDeEntrada(const glm::vec3& p);
static void RevisarValidacion();
static void ActualizarValidacion(float dt);
static bool EntrarPorTorniquete();
void IniciarRecarga();
static void MirarParaRecarga();
static HFONT FuenteMetro(int px, int peso, bool condensada, bool italica);
static void Placa(HDC dc, int x0, int y0, int x1, int y1, COLORREF c, int radio);
static void TextoCaja(HDC dc, RECT r, const std::string& t, UINT formato);
bool JuntoAPuerta();
int EscaleraCercana();
void ActualizarCartel();
void PrepararCartel(const std::string& texto);
void ProcesarEntrada(float dt);
void callbackTamano(GLFWwindow* w, int ancho, int alto);
void callbackMouse(GLFWwindow* w, double x, double y);
void callbackScroll(GLFWwindow* w, double dx, double dy);
void callbackBotonMouse(GLFWwindow* w, int boton, int accion, int mods);
void callbackTecla(GLFWwindow* w, int tecla, int scancode, int accion, int mods);
void ActualizarTitulo();
void GuardarCaptura(const std::string& archivo);
void ReproducirSonido(const char* ruta);
void ActualizarAmbiente();
void ActualizarTono(float dt);

// ============================================================================
// BLOQUE 4. main(): ciclo principal
// ============================================================================
int main(int argc, char** argv)
{
    bool probarEscaleras = false, probarTorniquetes = false, probarPasajeros = false, probarNiveles = false, iniciarVagon = false;
    bool camaraInicialIndicada = false;
    int nivelInicial = -1;
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
        else if (a == "--probar-niveles") probarNiveles = true;
        else if (a == "--nivel" && i + 1 < argc) nivelInicial = glm::clamp(atoi(argv[++i]) - 1, 0, 3);
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

    if (!Inicializar(probarEscaleras || probarTorniquetes || probarPasajeros || probarNiveles || !archivoCapturaAuto.empty())) return -1;
    auto cerrar = [] {
        pasajeros.Liberar();
        estacion.reset(); interiorGuardado.reset(); escenaAndenEnEspera.reset();
        if (texturaCartel) { glDeleteTextures(1, &texturaCartel); texturaCartel = 0; }
        if (sonidoTren) sonidoTren->drop();
        if (sonidoAmbiente) sonidoAmbiente->drop();
        if (sonidoTono) sonidoTono->drop();
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
    if (nivelInicial >= 0 && !probarEscaleras && !probarTorniquetes && !probarPasajeros && !probarNiveles)
        IniciarNivel(nivelInicial);
    else if (probarEscaleras || probarTorniquetes || probarPasajeros || probarNiveles || !archivoCapturaAuto.empty()) {
        nivel = NIVEL_LIBRE;
        ActualizarCartel();
    }
    if (probarEscaleras || probarTorniquetes || probarPasajeros || probarNiveles) {
        bool ok = !probarEscaleras || ProbarEscaleras();
        if (ok && probarTorniquetes) ok = (plantaAlta || CargarPlanta(true)) && ProbarTorniquetes();
        if (ok && probarPasajeros) ok = ProbarPasajeros();
        if (ok && probarNiveles) ok = ProbarNiveles();
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
        if (iniciarVagon && plantaAlta && nivel==NIVEL_EXPERTO) {
            tarjetaRecargada=true;
            if (!CargarPlanta(false)) { cerrar(); return 1; }
        }
        if (iniciarVagon && !plantaAlta) {
            const Camera vistaPedida=camara;
            EntrarEscenaVagon();
            if (!aBordo) { cerrar(); return 1; }
            if (camaraInicialIndicada) camara=vistaPedida;
            for (int i=0;i<45;++i) ActualizarAnimaciones(paso);
            if (nivel != NIVEL_LIBRE) misionCumplida=true;
            ActualizarCartel();
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

        const glm::vec3 posAntes = posJugador;
        ProcesarEntrada(deltaTime);
        ActualizarAmbiente();
        ActualizarTono(deltaTime);
        if (nivel < 0) {   // menu: el entorno no se dibuja hasta elegir nivel
            DibujarMenu();
            glfwSwapBuffers(ventana); glfwPollEvents(); continue;
        }
        ActualizarAnimaciones(deltaTime);
        if (!aBordo) ActualizarInteraccion(deltaTime);
        ActualizarCamara(deltaTime);
        // No colarse: se revisa DESPUES de mover al jugador (el movimiento ocurre en ActualizarCamara).
        // Aunque un pasajero abra el torniquete, solo se entra con la animacion de la tarjeta.
        if (plantaAlta && !aBordo && validaAnim < 0.0f && !ZonaTorniquetesMural(posJugador) &&
            LadoDeEntrada(posAntes) && !LadoDeEntrada(posJugador)) {
            bool enTorniquete = false;
            for (const auto& t : torniquetesAlta.carriles)
                if (glm::length(glm::vec2(posJugador.x - t.centro.x, posJugador.z - t.centro.y)) < 2.0f) enTorniquete = true;
            if (enTorniquete) {   // regresa: hay que pasar la tarjeta con saldo
                posJugador = posAntes;
                camara.Position = posJugador + glm::vec3(0, ALTURA_OJOS, 0);
                avisoBloqueoHasta = tiempoTotal + 2.0f;
            }
        }
        if ((nivel == NIVEL_EXPERTO || nivel == NIVEL_LIBRE) && plantaAlta && (!tarjetaRecargada || saldoTarjeta < 5.0f) && LadoDeEntrada(posJugador) && !ZonaTorniquetesMural(posJugador))
            for (const auto& t : torniquetesAlta.carriles)
                if (glm::length(glm::vec2(posJugador.x - t.centro.x, posJugador.z - t.centro.y)) < 2.5f)
                    avisoBloqueoHasta = tiempoTotal + 2.0f;
        ActualizarFundido(deltaTime);
        if (!TrenDetenido()) tiempoViaje += deltaTime;   // el tren solo "avanza" fuera de la carrera o ya sentado
        if (aBordo && carrera == 2 && (tCarrera -= deltaTime) <= 0.0f) { carrera = 5; tCarrera = .8f; }   // 3, 2, 1, 0: arranca
        if (aBordo && carrera == 5) {   // carrera: gana el asiento libre que tenga enfrente
            bool quedan = false;
            for (const auto& a : pasajeros.asientos) if (!a.ocupado && !a.reservado) quedan = true;
            // cada vez llegan mas pasajeros
            if ((tCarrera -= deltaTime) <= 0.0f) { pasajeros.SoltarCorredor(); tCarrera = .8f; }
            if (carrera == 5 && !quedan) { carrera = 4; tCarrera = 2.8f; }   // ya no hay asientos
        } else if (aBordo && carrera == 4 && (tCarrera -= deltaTime) <= 0.0f) {
            SalirEscenaVagon();   // de vuelta al anden para intentarlo de nuevo
        }
        if (aBordo && carrera == 3 && tCarrera > 0.0f) tCarrera -= deltaTime;   // espera antes de arrancar
        if (aBordo && carrera == 3)   // sentado: la vista baja a la altura del asiento
            camara.Position = glm::vec3(asientoJugador.x, 1.15f, asientoJugador.z * .8f);
        if (aBordo && nivel != NIVEL_LIBRE && (nivel != NIVEL_EXPERTO || carrera == 3)) misionCumplida = true;
        if (enFila && filaJugador >= 0 && filaJugador < (int)filas.size()) {
            FilaRecarga& f = filas[filaJugador];
            int turno = (int)((DURACION_FILA - tiempoFila) / TURNO_FILA);
            while (turnosPasados < turno && !f.personas.empty()) {
                // El primero termina de recargar y se va; los demas avanzan un lugar
                int quien = f.personas.front(); f.personas.erase(f.personas.begin());
                glm::vec3 lado(0, 0, (quien % 2) ? 1.0f : -1.0f);
                glm::vec3 a = f.inicio + f.dir * 1.2f + lado * 1.5f, b = a + f.dir * 6.0f + lado * 3.0f;
                pasajeros.Enviar(quien, { a, b });
                saliendoFila.push_back({ quien, b });
                for (size_t k = 0; k < f.personas.size(); ++k)
                    pasajeros.Enviar(f.personas[k], { f.inicio + f.dir * (.75f * k) });
                ++turnosPasados;
            }
            // El jugador avanza suavemente a su lugar en la fila
            glm::vec3 lugar = f.inicio + f.dir * (.75f * f.personas.size());
            glm::vec3 sig = posJugador + (lugar - posJugador) * std::min(1.0f, deltaTime * 2.5f);
            if (colisionPlantaAlta.Libre(sig)) posJugador = sig;   // nunca entra a un muro
            camara.Position = posJugador + glm::vec3(0, ALTURA_OJOS, 0);
        }
        // Las demas filas tambien avanzan desde que el usuario se formo (cada una a su ritmo)
        if (filasEnMovimiento && plantaAlta) {
            for (size_t i = 0; i < filas.size(); ++i) {
                FilaRecarga& f = filas[i];
                if ((int)i == filaJugador && enFila) continue;   // esa la maneja el bloque anterior
                if (f.personas.empty() || (f.reloj -= deltaTime) > 0.0f) continue;
                f.reloj = TURNO_FILA * (1.0f + .25f * float((i * 7 + f.personas.size() * 3) % 5));
                int quien = f.personas.front(); f.personas.erase(f.personas.begin());
                glm::vec3 lado(0, 0, (quien % 2) ? 1.0f : -1.0f);
                glm::vec3 a = f.inicio + f.dir * 1.2f + lado * 1.5f, b = a + f.dir * 6.0f + lado * 3.0f;
                pasajeros.Enviar(quien, { a, b });
                saliendoFila.push_back({ quien, b });
                for (size_t k = 0; k < f.personas.size(); ++k)
                    pasajeros.Enviar(f.personas[k], { f.inicio + f.dir * (.75f * k) });
                filasRecarga[i] = f.inicio + f.dir * (.75f * f.personas.size());   // nuevo final de la fila
            }
        }
        for (size_t k = 0; k < saliendoFila.size();)
            if (pasajeros.Ocultar(saliendoFila[k].first, saliendoFila[k].second)) saliendoFila.erase(saliendoFila.begin() + k);
            else ++k;
        RevisarValidacion();
        ActualizarValidacion(deltaTime);
        if (recargaAnim >= 0.0f && dialogoTexto.empty()) {   // avanza; se detiene en cada explicacion hasta [Enter]
            const float* pausas = recargaEnMaquina ? PAUSAS_MAQUINA : PAUSAS_TAQUILLA;
            const char** textos = recargaEnMaquina ? TEXTOS_MAQUINA : TEXTOS_TAQUILLA;
            int n = recargaEnMaquina ? 4 : 3;
            if (nivel == NIVEL_MEDIO && recargaPaso < n && recargaAnim + deltaTime >= pausas[recargaPaso]) {
                recargaAnim = pausas[recargaPaso]; dialogoTexto = textos[recargaPaso]; ++recargaPaso;
            } else recargaAnim += deltaTime * (nivel == NIVEL_LIBRE ? 1.6f : 1.0f);   // libre: mas rapido
        }
        if (recargaAnim >= DURACION_RECARGA) {
            recargaAnim = -1.0f; saldoTarjeta += MONTO_RECARGA;
            tarjetaRecargada = true;
            bool pideMaquina = false;
            if (nivel == NIVEL_MEDIO && PasoEsRecarga(&pideMaquina) && pideMaquina == recargaEnMaquina) {
                ++pasoMedio; rutaRecalculo = 0;
                char buf[200];
                snprintf(buf, sizeof buf, "¡RECARGA %d DE 2 LISTA!\nSaldo de tu tarjeta: $%.2f", recargaEnMaquina ? 2 : 1, saldoTarjeta);
                dialogoTexto = std::string(buf) + (pasoMedio < (int)nombresMedio.size() ? "\nSiguiente: " + nombresMedio[pasoMedio] + "." : std::string());
            }
        }
        if (enFila && (tiempoFila -= deltaTime) <= 0.0f) {
            enFila = false; IniciarRecarga(); saldoTarjeta = 0.0f;   // llega su turno: recarga con animacion
            if (filaJugador >= 0) filasRecarga[filaJugador] = filas[filaJugador].inicio;   // la fila quedo vacia
            std::cout << "> Tarjeta recargada; torniquetes habilitados." << std::endl;
        }
        if (nivel == NIVEL_MEDIO && plantaAlta && pasoMedio < (int)puntosMedio.size() &&
            !PasoEsMapa() && !PasoEsRecarga() && DistanciaXZ(posJugador, puntosMedio[pasoMedio]) < 6.0f) {
            infoLugar = InfoDeLugar(nombresMedio[pasoMedio]);
            ++pasoMedio; rutaRecalculo = 0;
            if (!infoLugar.empty())   // ficha: el usuario lee a su ritmo y continua con [Enter]
                dialogoTexto = infoLugar + (pasoMedio < (int)nombresMedio.size() ? "\nSiguiente: " + nombresMedio[pasoMedio] + "."
                                                                               : std::string("\nRecorrido completo: busca una escalera a los andenes."));
        }
        ActualizarCartel();
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
    puntosRecarga.clear();
    for (int i : E.BuscarPrefijo("Maquinas_Recarga")) {
        const Nodo& n = E.nodos[i];
        if (n.aabbMax.x > n.aabbMin.x) AgregarPuntosRecarga(n.aabbMin, n.aabbMax);
    }
    size_t numMaquinas = puntosRecarga.size(); numMaquinasRecarga = (int)numMaquinas;
    for (int i : E.BuscarPrefijo("Taquilla_Cuerpo")) {
        const Nodo& n = E.nodos[i];
        if (n.aabbMax.x > n.aabbMin.x) { AgregarPuntosRecarga(n.aabbMin, n.aabbMax); taqMin = n.aabbMin; taqMax = n.aabbMax; }
    }
    centroMaquinas = centroTaquillas = glm::vec3(0);
    for (size_t i = 0; i < puntosRecarga.size(); ++i) (i < numMaquinas ? centroMaquinas : centroTaquillas) += puntosRecarga[i];
    if (numMaquinas) centroMaquinas /= (float)numMaquinas;
    if (puntosRecarga.size() > numMaquinas) centroTaquillas /= (float)(puntosRecarga.size() - numMaquinas);
    else centroTaquillas = centroMaquinas;

    // Los gabinetes y barandales se quedan fijos. Las hojas ya tienen cajas moviles.
    for (size_t i = 0; i < E.nodos.size(); ++i) {
        const Nodo& n = E.nodos[i];
        if (n.mallas.empty() || n.nombre.compare(0, 8, "Esc_Sup_") == 0 ||
            std::find(referencias.begin(), referencias.end(), (int)i) != referencias.end()) continue;
        glm::vec3 tam = n.aabbMax - n.aabbMin;
        if (n.aabbMin.y < 1.5f && n.aabbMax.y > 0.25f && tam.x < 8.0f && tam.z < 11.0f)
            colisionPlantaAlta.obstaculos.push_back({ n.aabbMin, n.aabbMax });
    }
    // Muros de taquillas / mural: son largos y no entran en el filtro anterior
    for (size_t i = 0; i < E.nodos.size(); ++i) {
        const Nodo& n = E.nodos[i];
        if (n.mallas.empty()) continue;
        bool taquilla = n.nombre.compare(0, 8, "Taquilla") == 0 || n.nombre.compare(0, 5, "Mural") == 0;
        if (taquilla && n.aabbMax.x > n.aabbMin.x && n.aabbMin.y < 1.5f)
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
    MaterialesDeSenales(E, { "Letrero_Andenes_", "Letrero_Salida_", "Letrero_Est_Fumar_", "Letrero_Elevador_" }, false);
    RegistrarInteractivos(E, {"Letrero_Andenes_", "Letrero_Salida_", "Mural"}, objetosGuardadosAlta, false);
    for (const auto& objeto : interactivos) referencias.push_back(objeto.nodo);
    pasajeros.PrepararAlta(E,colisionPlantaAlta,torniquetesAlta,EscalerasParaPasajeros(accesosAlta));
    pasajeros.AjustarAfluenciaAlta(nivel==NIVEL_EXPERTO,colisionPlantaAlta);
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
    colisionAnden.zonasCaminables.push_back({ -10.0f, -1.80f, -110.0f, 110.0f });   // borde del anden en -1.535
    colisionAnden.zonasCaminables.push_back({ 1.66f, 10.0f, -110.0f, 110.0f });     // borde del anden en 1.396
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
    pasajeros.AjustarAfluenciaAnden(nivel==NIVEL_EXPERTO,colisionAnden);
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
    if (aBordo) { if (carrera != 1 && carrera != 2) pasajeros.ActualizarVagon(dt); return; }   // pausa: cartel y cuenta regresiva
    if (plantaAlta) {
        torniquetesAlta.Actualizar(*estacion, colisionPlantaAlta, posJugador, dt,
                                  pasajeros.SensoresAlta(), true,
                                  validado || (validaAnim >= 0.0f && validaSalida) || !LadoDeEntrada(posJugador) || ZonaTorniquetesMural(posJugador));
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
    if (nuevo == TREN_LLEGANDO && motorSonido && std::ifstream(RUTA_SONIDO_TREN).good()) {
        if (sonidoTren) { sonidoTren->stop(); sonidoTren->drop(); }
        sonidoTren = motorSonido->play2D(RUTA_SONIDO_TREN, false, false, true);
    }
    if (nuevo == TREN_ABRIENDO && sonidoTren) {   // el tren ya se detuvo: se corta el sonido de llegada
        sonidoTren->stop(); sonidoTren->drop(); sonidoTren = nullptr;
    }
    if (nuevo == TREN_ABRIENDO) { ReproducirSonido(RUTA_SONIDO_PUERTAS); cierreSonando = false; }
    if (nuevo == TREN_CERRANDO && !cierreSonando) { ReproducirSonido(RUTA_SONIDO_CIERRE); cierreSonando = true; }
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
        {   // el audio de cierre se adelanta lo necesario para que acabe cuando las puertas se cierran
            static float adelanto = -1.0f;
            if (adelanto < 0.0f) {
                adelanto = 0.0f;
                if (motorSonido && std::ifstream(RUTA_SONIDO_CIERRE).good())
                    if (ISoundSource* f = motorSonido->addSoundSourceFromFile(RUTA_SONIDO_CIERRE)) {
                        float dur = f->getPlayLength() / 1000.0f;
                        if (dur > 0.0f) adelanto = std::max(0.0f, dur - pistaPuertas.Duracion());
                    adelanto += AJUSTE_CIERRE;
                    }
            }
            if (!cierreSonando && !aBordo && tiempoEstado >= TIEMPO_DETENIDO - adelanto) {
                ReproducirSonido(RUTA_SONIDO_CIERRE); cierreSonando = true;
            }
        }
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
    if (nivel == NIVEL_NOVATO && pasoNovato < 0) {   // introduccion: la cinematica se repite
        tiempoCinematica = std::fmod(tiempoCinematica, pistaCamPos.Duracion());
        return;
    }
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
        float corrimiento = std::fmod(tiempoViaje * 15.0f, 12.0f);   // lamparas del tunel: quietas si el tren esta detenido
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
    // Iluminacion global uniforme en la estacion: las lamparas ya no siguen a la camara
    if (true) {
        if (!plantaAlta && nodoTren >= 0 && estadoTren != TREN_AUSENTE) {
            lucesPos.push_back(glm::vec3(0.0f, 1.0f, -104.5f + desplazamientoTren));
            lucesColor.push_back(glm::vec3(1.0f, 0.95f, 0.8f) * 3.0f);
            lucesRadio.push_back(25.0f);
        }
        return;
    }

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
        float potencia = plantaAlta ? 0.85f : (k == lamparaConFalla ? 1.05f * intensidadFalla : 1.05f);
        // Fuente elevada y de mayor alcance: cubre el piso de forma pareja (sin zonas oscuras)
        lucesPos.push_back(posicionesLamparas[k] + glm::vec3(0.0f, 4.0f, 0.0f));
        lucesColor.push_back(colorLampara * potencia * 1.25f);
        lucesRadio.push_back(22.0f);
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
    if (enFila || recargaAnim >= 0.0f || validaAnim >= 0.0f || mapaAbierto || !dialogoTexto.empty() || (aBordo && carrera >= 1 && carrera <= 4) || (nivel == NIVEL_NOVATO && pasoNovato < 0)) velocidad = 0.0f;   // fila, mapa o introduccion
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
        proyeccion = glm::perspective(glm::radians(camara.Zoom + ((!plantaAlta && !aBordo) ? 0.0f : 15.0f)), aspecto, 0.1f, 400.0f);   // andenes: angulo menor, el tren se ve mas grande
        vista = camara.GetViewMatrix();
        if (aBordo) {
            // Simulacion de movimiento: el vagon se mece y vibra (senoidales)
            float t = tiempoViaje;
            float m = TrenDetenido() ? 0.0f : 1.0f;   // detenido: sin vaiven
            float rebote = m * 0.010f * std::sin(t * 21.0f) + 0.006f * std::sin(t * 6.7f);
            float alabeo = m * glm::radians(0.8f) * std::sin(t * 1.3f);
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

// Variacion de colores: cada pasajero recibe al iniciar la simulacion un tono de piel y
// colores de ropa propios (semilla aleatoria por ejecucion + indice del pasajero).
static unsigned semillaColores = std::random_device{}();
static float AzarColor(unsigned k) { k ^= k >> 16; k *= 0x7feb352dU; k ^= k >> 15; k *= 0x846ca68bU; k ^= k >> 16; return (k & 0xffffff) / 16777215.0f; }
static bool EsPiel(const MaterialEscena& m) {
    std::string n = m.nombre; for (auto& c : n) c = (char)std::tolower((unsigned char)c);
    for (const char* k : { "skin", "piel", "body", "head", "cuerpo", "face", "cara", "hand", "mano" }) if (n.find(k) != std::string::npos) return true;
    for (const char* k : { "hair", "pelo", "cabello", "eye", "ojo", "teeth", "brow", "lash", "shoe", "zapato" }) if (n.find(k) != std::string::npos) return false;
    const glm::vec3& d = m.difuso;   // sin nombre claro: un color calido tipo piel
    return d.r > d.g && d.g > d.b && d.r > .35f && d.r - d.b > .12f && d.r - d.b < .55f;
}
static bool NoTenir(const MaterialEscena& m) {
    std::string n = m.nombre; for (auto& c : n) c = (char)std::tolower((unsigned char)c);
    for (const char* k : { "hair", "pelo", "cabello", "eye", "ojo", "teeth", "diente", "brow", "lash" }) if (n.find(k) != std::string::npos) return true;
    return false;
}
static MaterialEscena VariarMaterial(const MaterialEscena& base, unsigned id, int idxMat) {
    static const glm::vec3 pieles[] = { {.93f,.76f,.62f}, {.87f,.67f,.52f}, {.82f,.60f,.44f}, {.76f,.54f,.38f}, {.70f,.47f,.31f},
                                         {.62f,.42f,.29f}, {.55f,.36f,.23f}, {.47f,.30f,.19f}, {.39f,.25f,.16f}, {.31f,.20f,.13f} };   // de claro a muy oscuro
    static const glm::vec3 ropa[] = { {.12f,.16f,.30f}, {.55f,.10f,.12f}, {.15f,.38f,.22f}, {.85f,.80f,.70f}, {.20f,.20f,.22f}, {.70f,.55f,.20f},
                                      {.35f,.20f,.45f}, {.25f,.45f,.65f}, {.90f,.45f,.30f}, {.50f,.50f,.52f}, {.95f,.95f,.95f}, {.40f,.28f,.18f} };
    MaterialEscena m = base;
    { std::string n = m.nombre; for (auto& c : n) c = (char)std::tolower((unsigned char)c);
      if (n.find("shoe") != std::string::npos) { static const glm::vec3 z[] = { {.08f,.08f,.09f}, {.35f,.22f,.12f}, {.85f,.85f,.85f}, {.15f,.15f,.30f} }; m.difuso = z[(unsigned)(AzarColor(id * 7u + 1u) * 4) % 4]; return m; } }
    if (NoTenir(m)) return m;
    if (EsPiel(m)) m.difuso = pieles[(unsigned)(AzarColor(id * 31u + 7u) * 10) % 10] * (.93f + .14f * AzarColor(id * 53u + 11u));   // 10 tonos con un poco de variacion
    else {
        glm::vec3 c = ropa[(unsigned)(AzarColor(id * 131u + idxMat * 977u + 3u) * 12) % 12];
        m.difuso = c * (.85f + .3f * AzarColor(id * 17u + idxMat));   // un poco mas claro u oscuro
    }
    return m;
}

// ---------------------------------------------------------------------------
// Animacion de recarga en primera persona (espacio de la camara, metros).
//  Maquina : la tarjeta entra a la ranura, se insertan 5 monedas de $10 y el dedo
//            toca "RECARGAR" en la pantalla; despues la tarjeta sale.
//  Taquilla: la tarjeta se pega al lector del vidrio y se entrega un billete de $50.
// Tarjeta: modelo Tarjeta_Metro.fbx. Monedas, billete, dedo y pantalla: mallas
// sencillas generadas aqui.
// ---------------------------------------------------------------------------
struct MallaSimple { GLuint vao = 0; GLsizei n = 0; };
// Modelo FBX que se sostiene con la mano (billete, moneda): se centra, se lleva a su
// tamano real (lado mas largo, en metros) y se gira para que su cara mire a la camara.
struct ModeloMano {
    std::unique_ptr<ModeloEscena> fbx;
    glm::mat4 base{ 1.0f };
    bool Cargar(const char* ruta, float largo, float giroZ) {
        fbx = std::make_unique<ModeloEscena>(ruta);
        if (!fbx->cargado) { fbx.reset(); return false; }
        for (size_t i = 0; i < fbx->nodos.size(); ++i) {
            auto& n = fbx->nodos[i];
            n.global = n.padre >= 0 ? fbx->nodos[n.padre].global * n.local : n.local;
        }
        glm::vec3 mn(1e9f), mx(-1e9f);
        for (const auto& n : fbx->nodos) for (auto mi : n.mallas) {
            glm::vec3 a, b; ModeloEscena::CajaMundo(fbx->mallas[mi], n.global, a, b);
            mn = glm::min(mn, a); mx = glm::max(mx, b);
        }
        glm::vec3 t = mx - mn; float lado = std::max(t.x, std::max(t.y, t.z));
        base = glm::rotate(glm::mat4(1.0f), glm::radians(giroZ), glm::vec3(0, 0, 1)) *
               glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(1, 0, 0)) *
               glm::scale(glm::mat4(1.0f), glm::vec3(largo / lado)) * glm::translate(glm::mat4(1.0f), -(mn + mx) * .5f);
        return true;
    }
    void Dibujar(const glm::mat4& M, const glm::vec3& emision) const {
        for (const auto& n : fbx->nodos) for (auto mi : n.mallas) {
            const auto& m = fbx->mallas[mi];
            DibujarConMaterial(fbx->materiales[m.material], M * base * n.global, emision, m.vao, m.numIndices);
        }
    }
};
static MallaSimple CrearMallaSimple(const std::vector<float>& v, const std::vector<unsigned>& idx)
{
    MallaSimple m; GLuint vbo, ebo;
    glGenVertexArrays(1, &m.vao); glGenBuffers(1, &vbo); glGenBuffers(1, &ebo);
    glBindVertexArray(m.vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo); glBufferData(GL_ARRAY_BUFFER, v.size() * 4, v.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo); glBufferData(GL_ELEMENT_ARRAY_BUFFER, idx.size() * 4, idx.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0); glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 32, (void*)0);
    glEnableVertexAttribArray(1); glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 32, (void*)12);
    glEnableVertexAttribArray(2); glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 32, (void*)24);
    glBindVertexArray(0);
    m.n = (GLsizei)idx.size();
    return m;
}
// Caja centrada en el origen (uv 0..1 en la cara +Z, para la pantalla)
static MallaSimple CrearCaja(glm::vec3 t)
{
    std::vector<float> v; std::vector<unsigned> idx;
    const glm::vec3 N[6] = { {1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1} };
    for (int f = 0; f < 6; ++f) {
        glm::vec3 n = N[f], a = std::fabs(n.y) > .5f ? glm::vec3(1, 0, 0) : glm::vec3(0, 1, 0), b = glm::cross(n, a);
        if (n.z > .5f) { a = glm::vec3(1, 0, 0); b = glm::vec3(0, 1, 0); }
        unsigned base = (unsigned)(v.size() / 8);
        for (int k = 0; k < 4; ++k) {
            float su = (k == 1 || k == 2) ? 1.f : -1.f, sv = (k >= 2) ? 1.f : -1.f;
            glm::vec3 p = (n + a * su + b * sv) * t * .5f;
            v.insert(v.end(), { p.x, p.y, p.z, n.x, n.y, n.z, su * .5f + .5f, 1.0f - (sv * .5f + .5f) });
        }
        if (glm::dot(glm::cross(a, b), n) > 0) idx.insert(idx.end(), { base, base + 1, base + 2, base, base + 2, base + 3 });
        else idx.insert(idx.end(), { base, base + 2, base + 1, base, base + 3, base + 2 });
    }
    return CrearMallaSimple(v, idx);
}
// Cilindro con eje Z (moneda / dedo)
static MallaSimple CrearCilindro(float r, float h, int lados)
{
    std::vector<float> v; std::vector<unsigned> idx;
    for (int i = 0; i <= lados; ++i) {   // costado
        float a = glm::two_pi<float>() * i / lados, c = std::cos(a), s2 = std::sin(a);
        v.insert(v.end(), { r * c, r * s2, -h / 2, c, s2, 0, 0, 0 });
        v.insert(v.end(), { r * c, r * s2,  h / 2, c, s2, 0, 0, 0 });
    }
    for (int i = 0; i < lados; ++i) { unsigned k = i * 2; idx.insert(idx.end(), { k, k + 2, k + 1, k + 1, k + 2, k + 3 }); }
    for (int tapa = -1; tapa <= 1; tapa += 2) {
        unsigned c0 = (unsigned)(v.size() / 8);
        v.insert(v.end(), { 0, 0, tapa * h / 2, 0, 0, (float)tapa, .5f, .5f });
        for (int i = 0; i <= lados; ++i) {
            float a = glm::two_pi<float>() * i / lados;
            v.insert(v.end(), { r * std::cos(a), r * std::sin(a), tapa * h / 2, 0, 0, (float)tapa, 0, 0 });
        }
        for (int i = 0; i < lados; ++i)
            if (tapa > 0) idx.insert(idx.end(), { c0, c0 + 1 + i, c0 + 2 + i });
            else idx.insert(idx.end(), { c0, c0 + 2 + i, c0 + 1 + i });
    }
    return CrearMallaSimple(v, idx);
}
// Pantalla tactil de la maquina: 0 = boton RECARGAR, 1 = boton presionado, 2 = recarga exitosa
static GLuint TexturaPantalla(int estado, const char* monto)
{
    const int W = 512, H = 320;
    HDC dc = CreateCompatibleDC(nullptr);
    BITMAPINFO info{}; info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = W; info.bmiHeader.biHeight = -H; info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32; info.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    HBITMAP bmp = CreateDIBSection(dc, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
    HGDIOBJ ant = SelectObject(dc, bmp);
    RECT todo{ 0, 0, W, H }; HBRUSH f = CreateSolidBrush(RGB(236, 240, 244)); FillRect(dc, &todo, f); DeleteObject(f);
    Placa(dc, 0, 0, W, 56, RGB(232, 78, 15), 0);   // franja naranja del Metro
    HFONT ft = FuenteMetro(30, FW_BOLD, true, false), fb = FuenteMetro(46, FW_BOLD, true, false), fs = FuenteMetro(24, FW_SEMIBOLD, false, false);
    SetBkMode(dc, TRANSPARENT);
    HGDIOBJ fa = SelectObject(dc, ft); SetTextColor(dc, RGB(255, 255, 255));
    TextoCaja(dc, RECT{ 0, 10, W, 50 }, "RECARGA DE TARJETA", DT_SINGLELINE | DT_CENTER);
    SelectObject(dc, fs); SetTextColor(dc, RGB(40, 44, 52));
    if (estado < 2) {
        TextoCaja(dc, RECT{ 0, 74, W, 110 }, std::string("Monto ingresado: ") + monto, DT_SINGLELINE | DT_CENTER);
        Placa(dc, 96, 140, W - 96, 260, estado == 1 ? RGB(20, 110, 50) : RGB(30, 160, 75), 22);
        SelectObject(dc, fb); SetTextColor(dc, RGB(255, 255, 255));
        TextoCaja(dc, RECT{ 96, 172, W - 96, 232 }, "RECARGAR", DT_SINGLELINE | DT_CENTER);
    } else {
        SelectObject(dc, fb); SetTextColor(dc, RGB(20, 130, 60));
        TextoCaja(dc, RECT{ 0, 120, W, 180 }, "¡RECARGA EXITOSA!", DT_SINGLELINE | DT_CENTER);
        SelectObject(dc, fs); SetTextColor(dc, RGB(40, 44, 52));
        TextoCaja(dc, RECT{ 0, 200, W, 240 }, "Retira tu tarjeta", DT_SINGLELINE | DT_CENTER);
    }
    auto* px = static_cast<unsigned char*>(bits);
    for (int i = 0; i < W * H; ++i) px[i * 4 + 3] = 255;
    GLuint tex; glGenTextures(1, &tex); glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, W, H, 0, GL_BGRA, GL_UNSIGNED_BYTE, bits);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    SelectObject(dc, fa); DeleteObject(ft); DeleteObject(fb); DeleteObject(fs);
    SelectObject(dc, ant); DeleteObject(bmp); DeleteDC(dc);
    return tex;
}

// Interpolacion suave entre dos valores en el intervalo [t0, t1]
static float Tramo(float t, float t0, float t1) { float u = glm::clamp((t - t0) / (t1 - t0), 0.0f, 1.0f); return u * u * (3.0f - 2.0f * u); }

static void DibujarTarjetaRecarga(const glm::mat4& vista)
{
    if ((recargaAnim < 0.0f && validaAnim < 0.0f) || !plantaAlta) return;
    static std::unique_ptr<ModeloEscena> tarjeta; static bool listo = false;
    static glm::mat4 normal(1.0f);
    static MallaSimple moneda, billete, dedo, nudillo, pantalla;
    static ModeloMano moneda10, billete50;   // modelos del companero (experto)
    static GLuint texPantalla[2][3] = { { 0, 0, 0 }, { 0, 0, 0 } };
    if (!listo) {
        listo = true;
        tarjeta = std::make_unique<ModeloEscena>("models/Tarjeta_Metro.fbx");
        if (tarjeta->cargado) {
            for (size_t i = 0; i < tarjeta->nodos.size(); ++i) {
                auto& n = tarjeta->nodos[i];
                n.global = n.padre >= 0 ? tarjeta->nodos[n.padre].global * n.local : n.local;
            }
            glm::vec3 mn(1e9f), mx(-1e9f);
            for (const auto& n : tarjeta->nodos) for (auto mi : n.mallas) {
                glm::vec3 a, b; ModeloEscena::CajaMundo(tarjeta->mallas[mi], n.global, a, b);
                mn = glm::min(mn, a); mx = glm::max(mx, b);
            }
            glm::vec3 t = mx - mn; float ancho = std::max(t.x, std::max(t.y, t.z));
            normal = glm::scale(glm::mat4(1.0f), glm::vec3(.086f / ancho)) * glm::translate(glm::mat4(1.0f), -(mn + mx) * .5f);
        } else tarjeta.reset();
        moneda10.Cargar("models/Moneda_10.fbx", .030f, 0.0f);       // 3 cm de diametro
        billete50.Cargar("models/Billete_50.fbx", .125f, 90.0f);    // 12.5 x 6.5 cm, a lo ancho
        moneda = CrearCilindro(.0135f, .0022f, 28);
        billete = CrearCaja(glm::vec3(.135f, .066f, .0006f));
        dedo = CrearCilindro(.0085f, .11f, 14);
        nudillo = CrearCaja(glm::vec3(.075f, .045f, .09f));
        pantalla = CrearCaja(glm::vec3(.30f, .1875f, .004f));
        for (int i = 0; i < 3; ++i) { texPantalla[0][i] = TexturaPantalla(i, "$50.00"); texPantalla[1][i] = TexturaPantalla(i, "$10.00"); }
    }
    const glm::mat4 cam = glm::inverse(vista);
    if (validaAnim >= 0.0f) {
        if (!tarjeta || validaSalida) return;
        float t = validaAnim;
        glm::vec3 lector(.317f, -.099f, -.675f);   // placa blanca del lector derecho (parado a 0.85 m, vista a -20 grados)
        glm::vec3 mano(.10f, -.09f, -.33f), abajo(.16f, -.36f, -.36f);
        glm::vec3 p = abajo; float inc = 10, gir = -25, brillo = 0;
        if (t < .5f) return;                                                     // primero se acomoda
        if (t < 1.0f)      { float u = Tramo(t, .5f, 1.0f); p = glm::mix(abajo, mano, u); gir = glm::mix(-25.f, -12.f, u); inc = 10 * (1 - u); }
        else if (t < 1.6f) { float u = Tramo(t, 1.0f, 1.6f); p = glm::mix(mano, lector, u); inc = -88 * u; gir = -12 * (1 - u); }
        else if (t < 2.4f) { p = lector; inc = -88; gir = 0; }                   // sobre el lector
        else if (t < 3.1f) { float u = Tramo(t, 2.4f, 3.1f); p = glm::mix(lector, abajo, u); inc = glm::mix(-88.f, 10.f, u); gir = -25 * u; }
        else return;
        if (t > 1.6f && t < 2.4f) brillo = .35f * std::sin((t - 1.6f) / .8f * glm::pi<float>());
        glClear(GL_DEPTH_BUFFER_BIT);
        shaderIluminacion->use();
        glDisable(GL_BLEND);
        glm::mat4 M = cam * glm::translate(glm::mat4(1.0f), p) * glm::rotate(glm::mat4(1.0f), glm::radians(gir), glm::vec3(0, 1, 0)) *
                      glm::rotate(glm::mat4(1.0f), glm::radians(inc), glm::vec3(1, 0, 0)) *
                      glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(1, 0, 0)) * normal;
        for (const auto& n : tarjeta->nodos) for (auto mi : n.mallas) {
            const auto& m = tarjeta->mallas[mi];
            DibujarConMaterial(tarjeta->materiales[m.material], M * n.global, glm::vec3(0, brillo, brillo * .3f), m.vao, m.numIndices);
        }
        return;
    }
    const float t = recargaAnim;
    auto Trs = [](glm::vec3 p, float rx, float ry, float esc) {
        return glm::translate(glm::mat4(1.0f), p) * glm::rotate(glm::mat4(1.0f), glm::radians(ry), glm::vec3(0, 1, 0)) *
               glm::rotate(glm::mat4(1.0f), glm::radians(rx), glm::vec3(1, 0, 0)) * glm::scale(glm::mat4(1.0f), glm::vec3(esc));
    };
    auto Mat = [](glm::vec3 c, TipoMaterial tipo, float brillo) { MaterialEscena m; m.difuso = c; m.tipo = tipo; m.brillo = brillo; m.especular = glm::vec3(.5f); return m; };
    static const MaterialEscena oro = [&] { auto m = Mat({ .85f, .68f, .25f }, MAT_METALICO, 90); m.reflectividad = .25f; return m; }();
    static const MaterialEscena papel = Mat({ .78f, .70f, .52f }, MAT_OPACO, 8);
    static const MaterialEscena piel = Mat({ .80f, .60f, .46f }, MAT_OPACO, 12);
    glClear(GL_DEPTH_BUFFER_BIT);   // lo de la mano siempre queda por delante de la escena
    shaderIluminacion->use();
    glDisable(GL_BLEND);

    // ---- tarjeta
    glm::vec3 mano(.10f, -.09f, -.33f), abajo(.16f, -.36f, -.36f);
    glm::vec3 p; float inc = 0, gir = -12, esc = 1, brillo = 0;
    if (recargaEnMaquina) {
        glm::vec3 ranura(-.0965f, -.135f, -.676f), dentro(-.108f, -.151f, -.757f);   // ranura "ENTRADA DE TARJETA"
        if (t < .6f)      { float u = Tramo(t, 0, .6f); p = glm::mix(abajo, mano, u); gir = glm::mix(-25.f, -12.f, u); }
        else if (t < 1.3f){ float u = Tramo(t, .6f, 1.3f); p = glm::mix(mano, ranura, u); inc = -90 * u; gir = glm::mix(-12.f, 90.f, u); }
        else if (t < 1.9f){ float u = Tramo(t, 1.3f, 1.9f); p = glm::mix(ranura, dentro, u); inc = -90; gir = 90; esc = 1 - u; }   // entra a la ranura
        else if (t < 7.0f){ esc = 0; }
        else if (t < 7.6f){ float u = Tramo(t, 7.0f, 7.6f); p = glm::mix(dentro, ranura, u); inc = -90; gir = 90; esc = u; }      // sale
        else              { float u = Tramo(t, 7.6f, 8.6f); p = u < .5f ? glm::mix(ranura, mano, u * 2) : glm::mix(mano, abajo, u * 2 - 1);
                            inc = -90 * std::max(0.f, 1 - u * 2); gir = glm::mix(90.f, -12.f, std::min(1.f, u * 2)); }
        if (t > 6.4f && t < 7.2f) brillo = .3f * std::sin((t - 6.4f) / .8f * glm::pi<float>());
    } else {
        glm::vec3 vidrio(-.04f, .0f, -.47f);
        if (t < .6f)      { float u = Tramo(t, 0, .6f); p = glm::mix(abajo, mano, u); gir = glm::mix(-25.f, -12.f, u); }
        else if (t < 1.5f){ float u = Tramo(t, .6f, 1.5f); p = glm::mix(mano, vidrio, u); gir = -12 * (1 - u); }   // se pega al vidrio
        else if (t < 4.6f){ p = vidrio + glm::vec3(0, .002f * std::sin(t * 8), 0); gir = 0; }
        else if (t < 5.4f){ float u = Tramo(t, 4.6f, 5.4f); p = glm::mix(vidrio, mano, u); gir = -12 * u; }
        else              { float u = Tramo(t, 5.4f, 6.2f); p = glm::mix(mano, abajo, u); gir = glm::mix(-12.f, -25.f, u); }
        if (t > 3.8f && t < 4.6f) brillo = .3f * std::sin((t - 3.8f) / .8f * glm::pi<float>());
    }
    if (tarjeta && esc > .01f) {
        glm::mat4 M = cam * Trs(p, inc, gir, esc) * glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(1, 0, 0)) * normal;
        for (const auto& n : tarjeta->nodos) for (auto mi : n.mallas) {
            const auto& m = tarjeta->mallas[mi];
            DibujarConMaterial(tarjeta->materiales[m.material], M * n.global, glm::vec3(0, brillo, brillo * .3f), m.vao, m.numIndices);
        }
    }

    if (recargaEnMaquina) {
        // ---- monedas: 5 de $10, una cada 0.54 s, de la mano a la ranura de monedas
        glm::vec3 desde(.11f, -.16f, -.30f), hasta(-.035f, -.203f, -.683f);
        for (int i = 0; i < MONEDAS_RECARGA; ++i) {
            float t0 = 1.9f + .54f * i, u = (t - t0) / .5f;
            if (u < 0 || u > 1) continue;
            float e = Tramo(u, 0, .8f);
            glm::vec3 q = glm::mix(desde, hasta, e);
            float s2 = u < .8f ? 1.0f : 1.0f - (u - .8f) / .2f;   // se mete en la ranura
            if (moneda10.fbx) moneda10.Dibujar(cam * Trs(q, 0, 90 * (1 - e) - 10, s2), glm::vec3(0));   // moneda de $10
            else DibujarConMaterial(oro, cam * Trs(q, 0, 90 * (1 - e) - 10, s2 * .75f), glm::vec3(.05f), moneda.vao, moneda.n);   // moneda de $1
        }
        // ---- pantalla tactil + dedo que toca "RECARGAR"
        if (t > 4.6f && t < 7.4f) {
            float a = Tramo(t, 4.6f, 5.0f) * (1 - Tramo(t, 7.0f, 7.4f));
            glm::vec3 centro(-.061f, -.0145f, -.585f);   // sobre la pantalla de la maquina
            int est = t < 5.75f ? 0 : t < 6.4f ? 1 : 2;
            MaterialEscena m = Mat(glm::vec3(1), MAT_SENAL, 16); m.texturaDifusa = texPantalla[nivel == NIVEL_LIBRE ? 1 : 0][est];   // "Monto ingresado: $10.00 / $50.00"
            DibujarConMaterial(m, cam * Trs(centro, 0, 0, .59f * a), glm::vec3(.25f), pantalla.vao, pantalla.n);
            // el dedo sale de abajo a la derecha, toca el boton (5.6 s) y regresa
            glm::vec3 reposo(.14f, -.22f, -.34f), boton(-.061f, -.028f, -.58f);
            float ida = Tramo(t, 4.9f, 5.6f), vuelta = Tramo(t, 5.9f, 6.5f);
            glm::vec3 punta = glm::mix(glm::mix(reposo, boton, ida), reposo, vuelta);
            if (ida > 0 && vuelta < 1) {
                glm::vec3 dir = glm::normalize(glm::vec3(-.35f, .55f, -1.0f));   // el dedo apunta hacia la pantalla
                glm::mat4 orient = glm::mat4(glm::mat3(glm::normalize(glm::cross(glm::vec3(0, 1, 0), -dir)),
                                                        glm::normalize(glm::cross(-dir, glm::normalize(glm::cross(glm::vec3(0, 1, 0), -dir)))), -dir));
                glm::mat4 base = cam * glm::translate(glm::mat4(1.0f), punta) * orient;
                DibujarConMaterial(piel, base * glm::translate(glm::mat4(1.0f), glm::vec3(0, 0, .055f)), glm::vec3(0), dedo.vao, dedo.n);
                DibujarConMaterial(piel, base * glm::translate(glm::mat4(1.0f), glm::vec3(.01f, -.02f, .14f)), glm::vec3(0), nudillo.vao, nudillo.n);
            }
        }
    } else {
        // ---- billete de $50: sale de abajo, se acerca a la ventanilla y pasa por la ranura del vidrio
        if (t > 1.5f && t < 3.9f) {
            glm::vec3 a0(.14f, -.32f, -.34f), a1(.03f, -.15f, -.46f), a2(.03f, -.17f, -.66f);
            float u1 = Tramo(t, 1.5f, 2.4f), u2 = Tramo(t, 2.7f, 3.7f);
            glm::vec3 q = glm::mix(glm::mix(a0, a1, u1), a2, u2);
            float s2 = 1.0f - Tramo(t, 3.3f, 3.85f);   // la persona de la taquilla lo toma
            if (nivel == NIVEL_LIBRE && moneda10.fbx) moneda10.Dibujar(cam * Trs(q, -75.0f * u1, -10 * (1 - u1), s2), glm::vec3(0));   // moneda de $10
            else if (billete50.fbx) billete50.Dibujar(cam * Trs(q, -75.0f * u1, -10 * (1 - u1), s2), glm::vec3(0));   // billete de $50
                else DibujarConMaterial(papel, cam * Trs(q, -75.0f * u1, -10 * (1 - u1), s2), glm::vec3(.06f, .05f, .02f), billete.vao, billete.n);
        }
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
                unsigned id = semillaColores + (unsigned)(&persona - grupo.data()) * 2654435761u + (unsigned)(&grupo == &pasajeros.vagon) * 97u;
                DibujarConMaterial(VariarMaterial(fbx.materiales[m.material], id, (int)m.material),ubicacion*nodo.global,glm::vec3(0),m.vao,m.numIndices);
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
    shaderPaisaje->setFloat("desplazamiento", tiempoViaje * 0.03f);   // se detiene durante la carrera   // velocidad del paisaje
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
    if (aBordo) glUniform3f(U.luzAmbiente, .46f, .47f, .49f);
    else if (plantaAlta) glUniform3f(U.luzAmbiente, 0.80f, 0.80f, 0.80f);   // luz global
    else glUniform3f(U.luzAmbiente, 0.76f, 0.76f, 0.77f);
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
    // Los letreros (imagenes y letras) nunca se descartan por tamano ni por LOD: si se
    // quitara solo una parte de lejos, el cartel se veria incompleto ("bugueado").
    auto visible = [&](const glm::vec3& mn, const glm::vec3& mx, float& distancia, bool senal = false) {
        if (!fr.CajaVisible(mn, mx)) return false;
        glm::vec3 masCercano = glm::clamp(posVista, mn, mx);
        distancia = glm::length(masCercano - posVista);
        return senal || glm::length(mx - mn) >= umbralTamano * distancia;
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
        const bool senal = E.materiales[L.material].tipo == MAT_SENAL;
        if (!visible(mn, mx, dist, senal)) continue;
        // LOD por distancia: el detalle fino lejano no se nota y cuesta mucho
        if (!senal && modoCamara != CAM_AEREA && L.numIndices / 3 > TRIANGULOS_DETALLE && dist > DISTANCIA_DETALLE) continue;
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
            if (!visible(mn, mx, dist, E.materiales[m.material].tipo == MAT_SENAL)) continue;
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

    DibujarTarjetaRecarga(vista);   // animacion de recarga (primera persona)

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
    shaderHUD->setInt("usaTextura", 0);
    shaderHUD->setVec2("desplazamiento", 0.0f, 0.0f);
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
    DibujarRutaGuia();
    if (mapaAbierto) {   // fondo oscuro + mapa debajo del cartel explicativo
        shaderHUD->setInt("usaTextura", 0);
        shaderHUD->setVec4("color", glm::vec4(.02f, .04f, .07f, .78f));
        shaderHUD->setVec2("escala", 1.0f, 1.0f);
        shaderHUD->setVec2("desplazamiento", 0.0f, 0.0f);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        static GLuint texturaRed = 0; static bool intentado = false;
        if (!intentado) {   // imagen del mapa de la red en alta resolucion
            intentado = true;
            int w, h, c;
            unsigned char* d = stbi_load("textures/mapa_red_metro.png", &w, &h, &c, 4);
            if (d) {
                glGenTextures(1, &texturaRed); glBindTexture(GL_TEXTURE_2D, texturaRed);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, d);
                glGenerateMipmap(GL_TEXTURE_2D);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                stbi_image_free(d);
            }
        }
        if (texturaRed) {   // pantalla completa a lo alto, pegado a la derecha (el cartel queda a la izquierda)
            GLint tw = 1, th = 1;
            glBindTexture(GL_TEXTURE_2D, texturaRed);
            glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &tw);
            glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &th);
            float libreX = std::max(200.0f, (float)anchoVentana - 576.0f);   // a la derecha del cartel
            float k = std::min((float)altoVentana / th, libreX / tw);
            float pw = tw * k, ph = th * k;
            float ta = std::min(1.0f, ((float)glfwGetTime() - cartelCambio) / .35f);
            float e = 1.0f - std::pow(2.0f, -10.0f * ta);                  // entrada: sube y aparece
            shaderHUD->setVec4("color", glm::vec4(1, 1, 1, e));
            shaderHUD->setVec2("escala", pw / anchoVentana, ph / altoVentana);
            shaderHUD->setVec2("desplazamiento", 1.0f - 2.0f * (libreX * .5f) / anchoVentana, -(1.0f - e) * .06f);   // centrado en la zona libre
            shaderHUD->setInt("usaTextura", 1);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, texturaRed);
            glDrawArrays(GL_TRIANGLES, 0, 6);
            // Lupa: sigue al cursor sobre el mapa; la rueda cambia el aumento
            double mx, my; glfwGetCursorPos(ventana, &mx, &my);
            float cx = anchoVentana * .5f * (1.0f + (1.0f - 2.0f * (libreX * .5f) / anchoVentana)) - pw * .5f;
            float cy = altoVentana * .5f - ph * .5f;                                      // arriba del mapa (px)
            float u = ((float)mx - cx) / pw, v = ((float)my - cy) / ph;
            if (e > .99f && u >= 0 && u <= 1 && v >= 0 && v <= 1) {
                float R = std::min(170.0f, altoVentana * .22f);                         // radio en pixeles
                float tam = 2.0f * R / (pw * zoomLupa);                                   // region vista (fraccion del mapa)
                float tamV = 2.0f * R / (ph * zoomLupa);
                shaderHUD->setInt("lupa", 1);
                shaderHUD->setVec4("uvLupa", glm::vec4(u - tam * .5f, v - tamV * .5f, tam, tamV));
                shaderHUD->setVec2("escala", 2.0f * R / anchoVentana, 2.0f * R / altoVentana);
                shaderHUD->setVec2("desplazamiento", 2.0f * (float)mx / anchoVentana - 1.0f, 1.0f - 2.0f * (float)my / altoVentana);
                glDrawArrays(GL_TRIANGLES, 0, 6);
                shaderHUD->setInt("lupa", 0);
            }
            shaderHUD->setInt("usaTextura", 0);
        } else if (texturaMapa) {
            GLint tw = 1, th = 1;
            glBindTexture(GL_TEXTURE_2D, texturaMapa);
            glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &tw);
            glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &th);
            float arriba = 24.0f + 36.0f * 8 + 24.0f;                    // alto aproximado del cartel
            float altoLibre = std::max(120.0f, (float)altoVentana - arriba - 20.0f);
            float k = std::min((float)anchoVentana * .94f / tw, altoLibre / th);
            float pw = tw * k, ph = th * k;
            float cy = arriba + altoLibre * .5f;
            shaderHUD->setVec4("color", glm::vec4(1.0f));
            shaderHUD->setVec2("escala", pw / anchoVentana, -ph / altoVentana);   // texturas del modelo vienen invertidas en V
            shaderHUD->setVec2("desplazamiento", 0.0f, 1.0f - 2.0f * cy / altoVentana);
            shaderHUD->setInt("usaTextura", 1);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, texturaMapa);
            glDrawArrays(GL_TRIANGLES, 0, 6);
            shaderHUD->setInt("usaTextura", 0);
        }
    }
    shaderHUD->setVec2("desplazamiento", 0.0f, 0.0f);
    if (texturaCartel && !cartelActual.empty()) {
        GLint modoPoligono[2];
        glGetIntegerv(GL_POLYGON_MODE, modoPoligono);
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        float sx = std::min(mapaAbierto ? 540.0f : (float)anchoCartel, (float)anchoVentana - 24.0f) / anchoVentana;
        float sy = (float)(46 + 36 * (1 + std::count(cartelActual.begin(), cartelActual.end(), '\n'))) / altoVentana;
        shaderHUD->setVec2("escala", sx, sy);
        shaderHUD->setVec2("desplazamiento", -1.0f + sx + 12.0f / anchoVentana,
                           1.0f - sy - 12.0f / altoVentana);
        float tc = glm::clamp(((float)glfwGetTime() - cartelCambio) / .32f, 0.0f, 1.0f);
        float ec = 1.0f - std::pow(2.0f, -10.0f * tc);                     // ease-out exponencial
        shaderHUD->setVec4("color", glm::vec4(1, 1, 1, ec));
        if (cartelEnCentro)   // alerta: centrada, entra creciendo un poco
            shaderHUD->setVec2("escala", sx * (.92f + .08f * ec), sy * (.92f + .08f * ec)), shaderHUD->setVec2("desplazamiento", 0.0f, 0.0f);
        else
            shaderHUD->setVec2("desplazamiento", -1.0f + sx + 12.0f / anchoVentana - (1.0f - ec) * 36.0f / anchoVentana,
                               1.0f - sy - 12.0f / altoVentana);
        shaderHUD->setInt("usaTextura", 1);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texturaCartel);
        shaderHUD->setInt("textoHUD", 0);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        shaderHUD->setInt("usaTextura", 0);
        shaderHUD->setVec2("desplazamiento", 0.0f, 0.0f);
        glPolygonMode(GL_FRONT_AND_BACK, modoPoligono[0]);
    }
    DibujarLetreroReservado();   // alerta centrada, encima del cartel
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


// Recorta esquinas redondeadas en el canal alfa de un bitmap BGRA (r = radio en px)
static void EsquinasRedondeadas(unsigned char* px, int w, int h, int r, unsigned char alfa)
{
    for (int y = 0; y < h; ++y) for (int x = 0; x < w; ++x) {
        float dx = std::max({ 0.0f, (float)(r - x) - .5f, (float)(x - (w - 1 - r)) - .5f });
        float dy = std::max({ 0.0f, (float)(r - y) - .5f, (float)(y - (h - 1 - r)) - .5f });
        float d = std::sqrt(dx * dx + dy * dy) - r;               // >0 fuera de la esquina
        float k = glm::clamp(.5f - d, 0.0f, 1.0f);                 // borde suavizado de 1 px
        px[(y * w + x) * 4 + 3] = (unsigned char)(alfa * k);
    }
}
static COLORREF ColorNivel(int n)
{
    // Colores de lineas del Metro: 3 verde olivo (Universidad), 1 rosa, 2 azul, 8 verde
    return n == NIVEL_NOVATO ? RGB(160, 146, 32) : n == NIVEL_MEDIO ? RGB(226, 72, 140) :
           n == NIVEL_EXPERTO ? RGB(0, 101, 175) : RGB(0, 140, 70);
}

// Texto UTF-8 con GDI (acentos y signos de apertura)
static std::wstring Ancho(const std::string& t)
{
    if (t.empty()) return L"";
    int n = MultiByteToWideChar(CP_UTF8, 0, t.c_str(), (int)t.size(), nullptr, 0);
    std::wstring w(n, L' ');
    MultiByteToWideChar(CP_UTF8, 0, t.c_str(), (int)t.size(), &w[0], n);
    return w;
}
static void TextoU(HDC dc, int x, int y, const std::string& t) { std::wstring w = Ancho(t); TextOutW(dc, x, y, w.c_str(), (int)w.size()); }
static SIZE MedidaU(HDC dc, const std::string& t) { std::wstring w = Ancho(t); SIZE z{0, 0}; GetTextExtentPoint32W(dc, w.c_str(), (int)w.size(), &z); return z; }

// Dibuja una linea; los textos entre [corchetes] se pintan como teclas
static void LineaConTeclas(HDC dc, int x, int y, const std::string& r, COLORREF colorTexto)
{
    size_t i = 0;
    while (i < r.size()) {
        size_t a = r.find('[', i);
        std::string normal = r.substr(i, a == std::string::npos ? std::string::npos : a - i);
        SIZE t = MedidaU(dc, normal);
        SetTextColor(dc, colorTexto);
        TextoU(dc, x, y, normal); x += t.cx;
        if (a == std::string::npos) break;
        size_t b = r.find(']', a);
        if (b == std::string::npos) b = r.size();
        std::string k = r.substr(a + 1, b - a - 1);
        t = MedidaU(dc, k);
        HBRUSH tecla = CreateSolidBrush(RGB(255, 214, 80));
        HPEN borde = CreatePen(PS_SOLID, 2, RGB(170, 120, 20));
        HGDIOBJ pb = SelectObject(dc, tecla), pp = SelectObject(dc, borde);
        RoundRect(dc, x + 2, y - 2, x + t.cx + 18, y + t.cy + 3, 9, 9);
        SelectObject(dc, pb); SelectObject(dc, pp); DeleteObject(tecla); DeleteObject(borde);
        SetTextColor(dc, RGB(25, 25, 30));
        TextoU(dc, x + 10, y, k);
        x += t.cx + 24; i = b + 1;
    }
}

// ---- Estilo "letrero de linea del Metro": placa de color + cuadro con el numero ----
static HFONT FuenteMetro(int px, int peso, bool condensada, bool italica = false)
{
    return CreateFontA(-px, 0, 0, 0, peso, italica, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                       CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH,
                       condensada ? "Bahnschrift SemiBold Condensed" : "Bahnschrift");
}
static void Placa(HDC dc, int x0, int y0, int x1, int y1, COLORREF c, int radio)
{
    HBRUSH b = CreateSolidBrush(c); HPEN p = CreatePen(PS_SOLID, 1, c);
    HGDIOBJ ob = SelectObject(dc, b), op = SelectObject(dc, p);
    RoundRect(dc, x0, y0, x1, y1, radio, radio);
    SelectObject(dc, ob); SelectObject(dc, op); DeleteObject(b); DeleteObject(p);
}
static void TextoCaja(HDC dc, RECT r, const std::string& t, UINT formato)
{
    std::wstring w = Ancho(t);
    DrawTextW(dc, w.c_str(), (int)w.size(), &r, formato);
}

void PrepararCartel(const std::string& textoBase)
{
    const bool tutorial = nivel == NIVEL_NOVATO && !misionCumplida;
    std::string texto = textoBase + (nivel >= 0 ? "\n[Tab] Volver al menú principal" : "");
    std::string clave = texto + (tutorial ? "#" + std::to_string(pasoNovato) : "");
    if (clave == cartelActual) return;
    cartelActual = clave;
    const int lineas = 1 + (int)std::count(texto.begin(), texto.end(), '\n');
    const int w = mapaAbierto ? 540 : anchoCartel, h = 46 + 36 * lineas;
    HDC dc = CreateCompatibleDC(nullptr);
    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = w;
    info.bmiHeader.biHeight = -h;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    HBITMAP bmp = CreateDIBSection(dc, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
    HGDIOBJ anterior = SelectObject(dc, bmp);
    RECT rect{ 0, 0, w, h };
    HBRUSH fondo = CreateSolidBrush(RGB(248, 248, 246));
    FillRect(dc, &rect, fondo); DeleteObject(fondo);
    COLORREF cNivel = ColorNivel(nivel);
    {   // solo anima si cambia el tipo de cuadro (titulo), no cuando se actualizan los metros
        std::string t = texto.substr(0, texto.find('\n'));
        if (t != tituloCartel) { tituloCartel = t; cartelCambio = (float)glfwGetTime(); }
    }
    // Encabezado: placa con el titulo + cuadro con el numero del nivel (como las lineas del Metro)
    Placa(dc, 8, 8, w - 80, 60, cNivel, 16);
    Placa(dc, w - 72, 8, w - 8, 60, cNivel, 16);
    HFONT fTitulo = FuenteMetro(29, FW_SEMIBOLD, true), fNumero = FuenteMetro(44, FW_BOLD, true);
    HFONT fCuerpo = FuenteMetro(23, FW_SEMIBOLD, false);
    HGDIOBJ fuenteAnterior = SelectObject(dc, fTitulo);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(255, 255, 255));
    std::string titulo = texto.substr(0, texto.find('\n'));
    TextoCaja(dc, RECT{ 24, 8, w - 96 - (tutorial ? 150 : 0), 60 }, titulo, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    SelectObject(dc, fNumero);
    std::string num = nivel >= 0 ? std::string(1, char('1' + nivel)) : "M";
    TextoCaja(dc, RECT{ w - 72, 6, w - 8, 62 }, num, DT_SINGLELINE | DT_VCENTER | DT_CENTER);
    if (tutorial) {   // pasos del tutorial dentro de la placa
        for (int i = 0; i < 4; ++i) {
            COLORREF c = i <= pasoNovato ? RGB(255, 255, 255) : RGB(GetRValue(cNivel) * 3 / 4, GetGValue(cNivel) * 3 / 4, GetBValue(cNivel) * 3 / 4);
            Placa(dc, w - 230 + i * 34, 25, w - 210 + i * 34, 45, c, 20);
        }
    }
    // Cuerpo: texto oscuro sobre fondo claro
    SelectObject(dc, fCuerpo);
    int y = 72, inicio = (int)texto.find('\n');
    while (inicio != (int)std::string::npos && inicio < (int)texto.size()) {
        ++inicio;
        size_t fin = texto.find('\n', inicio);
        LineaConTeclas(dc, 22, y, texto.substr(inicio, fin == std::string::npos ? std::string::npos : fin - inicio), RGB(30, 30, 36));
        if (fin == std::string::npos) break;
        inicio = (int)fin; y += 36;
    }
    auto* px = static_cast<unsigned char*>(bits);
    EsquinasRedondeadas(px, w, h, 16, 244);
    if (!texturaCartel) glGenTextures(1, &texturaCartel);
    glBindTexture(GL_TEXTURE_2D, texturaCartel);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_BGRA, GL_UNSIGNED_BYTE, bits);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    SelectObject(dc, fuenteAnterior); DeleteObject(fTitulo); DeleteObject(fNumero); DeleteObject(fCuerpo);
    SelectObject(dc, anterior); DeleteObject(bmp); DeleteDC(dc);
}

// ---- Menu principal (pantalla completa, sin escena detras) ------------------
static void PrepararMenu()
{
    const int w = MENU_W, h = MENU_H;
    HDC dc = CreateCompatibleDC(nullptr);
    BITMAPINFO info{}; info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = w; info.bmiHeader.biHeight = -h; info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32; info.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    HBITMAP bmp = CreateDIBSection(dc, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
    HGDIOBJ anterior = SelectObject(dc, bmp);
    RECT r{ 0, 0, w, h }; HBRUSH f = CreateSolidBrush(RGB(246, 246, 244)); FillRect(dc, &r, f); DeleteObject(f);

    HFONT fT = FuenteMetro(56, FW_SEMIBOLD, true), fS = FuenteMetro(22, FW_NORMAL, false);
    HFONT fP = FuenteMetro(27, FW_SEMIBOLD, true), fN = FuenteMetro(62, FW_BOLD, true);
    HFONT fD = FuenteMetro(21, FW_SEMIBOLD, false, true);
    SetBkMode(dc, TRANSPARENT);
    HGDIOBJ fa = SelectObject(dc, fT);
    SetTextColor(dc, RGB(232, 108, 30)); TextoU(dc, 60, 26, "METRO");
    SIZE tm = MedidaU(dc, "METRO ");
    SetTextColor(dc, RGB(40, 40, 46)); TextoU(dc, 60 + tm.cx, 26, "CDMX");
    SelectObject(dc, fS); SetTextColor(dc, RGB(96, 96, 104));
    TextoU(dc, 62, 96, "Elige tu nivel de experiencia (tecla 1-4 o clic)");
    const char* placa1[4] = { "NOVATO", "MEDIANAMENTE", "EXPERIMENTADO", "MODO LIBRE" };
    const char* placa2[4] = { "GUIADO PASO A PASO", "EXPERIMENTADO", "HORA PICO", "SIN MISIONES" };
    const char* des[4] = {
        "Apareces en el andén. Te guiamos paso a paso hasta subir al vagón.",
        "Apareces junto a los torniquetes. Recorre la estación, baja al andén y sube al vagón.",
        "Apareces en la entrada en hora pico. Recarga tu tarjeta y sube al vagón.",
        "Explora la estación sin misión, con todas las cámaras." };
    for (int i = 0; i < 4; ++i) {
        int y0 = 150 + i * 105;
        COLORREF c = ColorNivel(i);
        Placa(dc, 60, y0 + 4, 470, y0 + 84, c, 18);          // placa con el nombre
        Placa(dc, 480, y0 + 4, 560, y0 + 84, c, 18);         // cuadro con el numero
        SetTextColor(dc, RGB(255, 255, 255));
        SelectObject(dc, fP);
        TextoU(dc, 80, y0 + 12, placa1[i]);
        TextoU(dc, 80, y0 + 44, placa2[i]);
        SelectObject(dc, fN);
        TextoCaja(dc, RECT{ 480, y0 + 2, 560, y0 + 86 }, std::string(1, char('1' + i)), DT_SINGLELINE | DT_VCENTER | DT_CENTER);
        SelectObject(dc, fD); SetTextColor(dc, RGB(30, 30, 36));
        TextoCaja(dc, RECT{ 584, y0 + 14, w - 70, y0 + 84 }, des[i], DT_WORDBREAK);
    }
    SelectObject(dc, fS); SetTextColor(dc, RGB(110, 110, 118));
    TextoU(dc, 62, h - 44, "Durante el recorrido pulsa Tab (o F1) para regresar a este menú.   Esc: salir");
    auto* px = static_cast<unsigned char*>(bits);
    EsquinasRedondeadas(px, w, h, 22, 255);
    glGenTextures(1, &texturaMenu);
    glBindTexture(GL_TEXTURE_2D, texturaMenu);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_BGRA, GL_UNSIGNED_BYTE, bits);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    SelectObject(dc, fa); DeleteObject(fT); DeleteObject(fS); DeleteObject(fP); DeleteObject(fN); DeleteObject(fD);
    SelectObject(dc, anterior); DeleteObject(bmp); DeleteDC(dc);
}

// ---- Ruta guia (A* sobre una rejilla de 0.5 m que respeta las colisiones) ----
static std::vector<glm::vec3> CalcularRuta(glm::vec3 a, glm::vec3 b)
{
    const MundoColision& M = colisionPlantaAlta;
    if (M.zonasCaminables.empty()) return {};
    float x0 = 1e9f, x1 = -1e9f, z0 = 1e9f, z1 = -1e9f;
    for (const auto& r : M.zonasCaminables) { x0 = std::min(x0, r.xMin); x1 = std::max(x1, r.xMax); z0 = std::min(z0, r.zMin); z1 = std::max(z1, r.zMax); }
    const float h = .5f; int W = int((x1 - x0) / h) + 1, H = int((z1 - z0) / h) + 1;
    auto celda = [&](glm::vec3 p) { return glm::ivec2(glm::clamp(int((p.x - x0) / h), 0, W - 1), glm::clamp(int((p.z - z0) / h), 0, H - 1)); };
    auto centro = [&](int x, int z) { return glm::vec3(x0 + (x + .5f) * h, .019f, z0 + (z + .5f) * h); };
    std::vector<signed char> libre(W * H, -1);
    auto esLibre = [&](int x, int z) { int i = z * W + x; if (libre[i] < 0) libre[i] = M.Libre(centro(x, z)) ? 1 : 0; return libre[i] == 1; };
    glm::ivec2 s = celda(a), g = celda(b);
    std::vector<float> costo(W * H, 1e9f); std::vector<int> previo(W * H, -1);
    using Nodo2 = std::pair<float, int>;
    std::priority_queue<Nodo2, std::vector<Nodo2>, std::greater<Nodo2>> abiertos;
    int is = s.y * W + s.x, ig = g.y * W + g.x;
    costo[is] = 0; abiertos.push({ 0, is });
    int mejor = is; float mejorD = 1e9f;
    while (!abiertos.empty()) {
        int i = abiertos.top().second; abiertos.pop();
        float dg = glm::length(glm::vec2(i % W - g.x, i / W - g.y));
        if (dg < mejorD) { mejorD = dg; mejor = i; }
        if (i == ig) break;
        int x = i % W, z = i / W;
        for (int dz = -1; dz <= 1; ++dz) for (int dx = -1; dx <= 1; ++dx) {
            int nx = x + dx, nz = z + dz;
            if ((!dx && !dz) || nx < 0 || nz < 0 || nx >= W || nz >= H || !esLibre(nx, nz)) continue;
            if (dx && dz && (!esLibre(x + dx, z) || !esLibre(x, z + dz))) continue;   // no cortar esquinas
            int j = nz * W + nx; float c = costo[i] + ((dx && dz) ? 1.414f : 1.0f);
            if (c < costo[j]) { costo[j] = c; previo[j] = i; abiertos.push({ c + glm::length(glm::vec2(nx - g.x, nz - g.y)), j }); }
        }
    }
    std::vector<glm::vec3> r;
    if (previo[ig] < 0 && ig != is) ig = mejor;   // destino dentro de un muro: lo mas cerca posible
    for (int i = ig; i >= 0; i = previo[i]) r.push_back(centro(i % W, i / W));
    std::reverse(r.begin(), r.end());
    return r;
}

// Alerta en medio de la pantalla al intentar sentarse en el asiento reservado (2 s)
static void DibujarLetreroReservado()
{
    if (!aBordo || tiempoTotal >= avisoReservadoHasta) return;
    static GLuint tex = 0;
    const int W = 1000, H = 190;
    if (!tex) {
        HDC dc = CreateCompatibleDC(nullptr);
        BITMAPINFO info{}; info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth = W; info.bmiHeader.biHeight = -H; info.bmiHeader.biPlanes = 1;
        info.bmiHeader.biBitCount = 32; info.bmiHeader.biCompression = BI_RGB;
        void* bits = nullptr;
        HBITMAP bmp = CreateDIBSection(dc, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
        HGDIOBJ ant = SelectObject(dc, bmp);
        RECT todo{ 0, 0, W, H }; HBRUSH bl = CreateSolidBrush(RGB(18, 64, 196)); FillRect(dc, &todo, bl); DeleteObject(bl);
        HFONT ft = FuenteMetro(46, FW_BOLD, true), fs = FuenteMetro(28, FW_SEMIBOLD, false);
        SetBkMode(dc, TRANSPARENT); SetTextColor(dc, RGB(255, 255, 255));
        HGDIOBJ fa = SelectObject(dc, ft);
        TextoCaja(dc, RECT{ 0, 16, W, 72 }, "ASIENTO RESERVADO PARA", DT_SINGLELINE | DT_CENTER);
        SelectObject(dc, fs);
        TextoCaja(dc, RECT{ 40, 78, W - 40, H - 10 }, "personas con discapacidad, embarazadas,\ncon bebé en brazos y adultos mayores", DT_CENTER);   // dos renglones para que no se corte
        auto* px = static_cast<unsigned char*>(bits);
        EsquinasRedondeadas(px, W, H, 18, 250);
        glGenTextures(1, &tex); glBindTexture(GL_TEXTURE_2D, tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, W, H, 0, GL_BGRA, GL_UNSIGNED_BYTE, bits);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        SelectObject(dc, fa); DeleteObject(ft); DeleteObject(fs);
        SelectObject(dc, ant); DeleteObject(bmp); DeleteDC(dc);
    }
    float t = glm::clamp((2.0f - (avisoReservadoHasta - tiempoTotal)) / .25f, 0.0f, 1.0f);
    float e = 1.0f - std::pow(2.0f, -10.0f * t);
    float k = std::min(1.0f, (anchoVentana - 40.0f) / W) * (.94f + .06f * e);
    shaderHUD->setVec4("color", glm::vec4(1, 1, 1, e));
    shaderHUD->setVec2("escala", W * k / anchoVentana, H * k / altoVentana);
    shaderHUD->setVec2("desplazamiento", 0.0f, 0.0f);
    shaderHUD->setInt("usaTextura", 1);
    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, tex);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    shaderHUD->setInt("usaTextura", 0);
}

static void DibujarRutaGuia()
{
    if (nivel != NIVEL_MEDIO || !plantaAlta || aBordo || modoCamara != CAM_PRIMERA_PERSONA ||
        pasoMedio >= (int)puntosMedio.size()) return;
    // Camino fijo por punto; se va borrando lo que el jugador ya recorrio
    size_t cerca = 0; float dCerca = 1e9f;
    for (size_t i = 0; i < rutaGuia.size(); ++i) {
        float d = DistanciaXZ(rutaGuia[i], posJugador);
        if (d < dCerca) { dCerca = d; cerca = i; }
    }
    if (rutaPaso != pasoMedio || rutaGuia.empty() || dCerca > 4.0f) {   // nuevo destino o se desvio mucho
        rutaGuia = CalcularRuta(posJugador, puntosMedio[pasoMedio]); rutaPaso = pasoMedio; cerca = 0;
    } else if (cerca > 0) rutaGuia.erase(rutaGuia.begin(), rutaGuia.begin() + cerca);
    glm::mat4 P, V; ObtenerMatrices(P, V);
    glm::mat4 PV = P * V;
    auto marca = [&](glm::vec3 p, float px, glm::vec4 c) {
        glm::vec4 q = PV * glm::vec4(p, 1);
        if (q.w <= .1f) return;
        glm::vec3 n = glm::vec3(q) / q.w;
        if (std::fabs(n.x) > 1.1f || std::fabs(n.y) > 1.1f) return;
        float tam = px / q.w;
        shaderHUD->setVec4("color", c);
        shaderHUD->setVec2("escala", tam / anchoVentana, tam / altoVentana);
        shaderHUD->setVec2("desplazamiento", n.x, n.y);
        glDrawArrays(GL_TRIANGLES, 0, 6);
    };
    float t = (float)glfwGetTime(), recorrido = 0;
    for (size_t i = 1; i < rutaGuia.size() && recorrido < 40.0f; ++i) {
        float d = glm::length(rutaGuia[i] - rutaGuia[i - 1]); recorrido += d;
        if (recorrido < 1.0f || std::fmod(recorrido, 1.0f) > d) continue;   // una marca por metro
        float onda = .5f + .5f * std::sin(t * 5.0f - recorrido * .8f);   // pulso que avanza hacia el destino
        marca(rutaGuia[i] + glm::vec3(0, .05f, 0), 90.0f, glm::vec4(1.0f, .62f, .15f, .45f + .45f * onda));
    }
    glm::vec3 meta = puntosMedio[pasoMedio];
    marca(meta + glm::vec3(0, 2.4f + .15f * std::sin(t * 3.0f), 0), 260.0f, glm::vec4(1.0f, .8f, .2f, .9f));
}

void DibujarMenu()
{
    if (!texturaMenu) PrepararMenu();
    glViewport(0, 0, anchoVentana, altoVentana);
    glClearColor(0.035f, 0.055f, 0.085f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    menuEscala = std::min(anchoVentana * 0.92f / MENU_W, altoVentana * 0.92f / MENU_H);
    float pw = MENU_W * menuEscala, ph = MENU_H * menuEscala;
    static double ultimoMenu = -10, inicioMenu = 0;
    double ahoraM = glfwGetTime();
    if (ahoraM - ultimoMenu > .25) inicioMenu = ahoraM;   // el menu acaba de aparecer
    ultimoMenu = ahoraM;
    float e = 1.0f - std::pow(2.0f, -10.0f * glm::clamp((float)(ahoraM - inicioMenu) / .5f, 0.0f, 1.0f));
    menuX0 = (anchoVentana - pw) * 0.5f; menuY0 = (altoVentana - ph) * 0.5f + (1.0f - e) * 28.0f;
    GLint modoPoligono[2]; glGetIntegerv(GL_POLYGON_MODE, modoPoligono);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glDisable(GL_DEPTH_TEST); glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    shaderHUD->use();
    glBindVertexArray(vaoHUD);
    shaderHUD->setVec4("color", glm::vec4(1, 1, 1, e));
    shaderHUD->setVec2("escala", pw / anchoVentana, ph / altoVentana);
    shaderHUD->setVec2("desplazamiento", 0.0f, 1.0f - (2.0f * menuY0 + ph) / altoVentana);
    shaderHUD->setInt("usaTextura", 1);
    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, texturaMenu);
    shaderHUD->setInt("textoHUD", 0);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    shaderHUD->setInt("usaTextura", 0);

    // Opcion bajo el cursor
    double mx, my; glfwGetCursorPos(ventana, &mx, &my);
    float u = ((float)mx - menuX0) / menuEscala, v = ((float)my - menuY0) / menuEscala;
    menuHover = -1;
    for (int i = 0; i < 4; ++i)
        if (u >= 60 && u <= MENU_W - 60 && v >= 150 + i * 105 && v <= 238 + i * 105) menuHover = i;
    if (!cursorMano) cursorMano = glfwCreateStandardCursor(GLFW_HAND_CURSOR);
    glfwSetCursor(ventana, menuHover >= 0 ? cursorMano : nullptr);
    const glm::vec3 col[4] = { {.63f,.57f,.13f}, {.89f,.28f,.55f}, {0.0f,.40f,.69f}, {0.0f,.55f,.27f} };
    auto rect = [&](int i, float crecer, glm::vec4 c) {   // rectangulo de la opcion i en NDC
        float x0 = menuX0 + (60 - crecer) * menuEscala, x1 = menuX0 + (MENU_W - 60 + crecer) * menuEscala;
        float y0 = menuY0 + (150 + i * 105 - crecer) * menuEscala, y1 = menuY0 + (238 + i * 105 + crecer) * menuEscala;
        shaderHUD->setVec4("color", c);
        shaderHUD->setVec2("escala", (x1 - x0) / anchoVentana, (y1 - y0) / altoVentana);
        shaderHUD->setVec2("desplazamiento", (x0 + x1) / anchoVentana - 1.0f, 1.0f - (y0 + y1) / altoVentana);
        glDrawArrays(GL_TRIANGLES, 0, 6);
    };
    float t = (float)glfwGetTime();
    if (menuHover >= 0 && menuElegido < 0)
        rect(menuHover, 0.0f, glm::vec4(col[menuHover], .14f));   // resaltado estable, sin parpadeo
    if (menuElegido >= 0) {   // animacion de clic: destello que crece y se desvanece
        menuClicT += deltaTime;
        float k = std::min(menuClicT / 0.45f, 1.0f);
        rect(menuElegido, 4.0f + 14.0f * k, glm::vec4(col[menuElegido], .55f * (1.0f - k)));
        rect(menuElegido, 0.0f, glm::vec4(1, 1, 1, .25f * (1.0f - k)));
    }
    shaderHUD->setVec2("desplazamiento", 0.0f, 0.0f);
    glPolygonMode(GL_FRONT_AND_BACK, modoPoligono[0]);
    glDisable(GL_BLEND); glEnable(GL_DEPTH_TEST);
    if (menuElegido >= 0 && menuClicT >= 0.5f) {
        int elegido = menuElegido; menuElegido = -1;
        glfwSetCursor(ventana, nullptr);
        IniciarNivel(elegido);
    }
}

void ActualizarCartel()
{
    cartelEnCentro = false;
    if (nivel >= 0 && !dialogoTexto.empty()) { PrepararCartel(dialogoTexto + "\n[Enter] continuar"); return; }
    if (aBordo && carrera == 2) {
        PrepararCartel("PREPÁRATE...   " + std::to_string((int)std::floor(std::max(tCarrera, 0.0f))) +
                       "\nAl llegar a 0 busca un asiento libre.");
        return;
    }
    if (false) {
        PrepararCartel("LUGAR RESERVADO\nEste asiento es exclusivo para personas con discapacidad, mujeres embarazadas,\npersonas con bebés en brazos y adultos mayores. ¡Busca otro lugar!");
        return;
    }
    if (aBordo && carrera == 5) {
        int libres = 0;
        for (const auto& a : pasajeros.asientos) if (!a.ocupado && !a.reservado) ++libres;
        PrepararCartel("¡ENCUENTRA LUGAR!\nPonte frente a un asiento libre y pulsa [E] para sentarte.\nAsientos libres: " + std::to_string(libres));
        return;
    }
    if (aBordo && carrera == 4) { PrepararCartel("NO ALCANZASTE LUGAR\nRegresas al andén: sube de nuevo con [F] e inténtalo otra vez."); return; }
    if (aBordo && carrera == 3 && misionCumplida && nivel == NIVEL_EXPERTO) {
        PrepararCartel("MISIÓN CUMPLIDA - CONSEGUISTE ASIENTO\nPor fin puedes descansar durante el viaje.\nSuperaste el nivel más difícil. Prueba el modo libre desde el menú.");
        return;
    }
    if (false) {
        PrepararCartel("ELIGE TU MODO\n[1] Novato  [2] Medio experimentado  [3] Experimentado  [4] Libre");
        return;
    }
    if (torniqueteCerca >= 0 && validaAnim < 0.0f && plantaAlta && tiempoTotal >= avisoBloqueoHasta) {
        char buf[200];
        if ((nivel == NIVEL_EXPERTO || nivel == NIVEL_LIBRE) && saldoTarjeta < 5.0f)
            snprintf(buf, sizeof buf, "TORNIQUETE BLOQUEADO\nTu tarjeta no tiene saldo. Recárgala en una máquina o en la taquilla.");
        else if (nivel == NIVEL_EXPERTO || nivel == NIVEL_LIBRE)
            snprintf(buf, sizeof buf, "TORNIQUETE\nPulsa [E] para pasar tu tarjeta y entrar a la estación.  Saldo: $%.2f", saldoTarjeta);
        else snprintf(buf, sizeof buf, "TORNIQUETE\nPulsa [E] para pasar tu tarjeta y entrar a la estación.");
        PrepararCartel(buf);
        return;
    }
    if (validaAnim >= 0.0f && plantaAlta && validaSalida) { PrepararCartel("SALIDA\nCruzas el torniquete hacia la salida."); return; }
    if (validaAnim >= 0.0f && plantaAlta) {   // validacion en el torniquete
        char buf[200];
        if (validaAnim < 1.6f) snprintf(buf, sizeof buf, "TORNIQUETE\nAcerca tu tarjeta al lector que está junto al torniquete.");
        else if (validaAnim < 2.7f) snprintf(buf, sizeof buf, "ACCESO PERMITIDO\nSe cobró el viaje ($5.00). Saldo restante: $%.2f", saldoTarjeta);
        else snprintf(buf, sizeof buf, "ACCESO PERMITIDO - CRUZA EL TORNIQUETE\nSaldo restante: $%.2f", saldoTarjeta);
        PrepararCartel(buf);
        return;
    }
    if (recargaAnim >= 0.0f && plantaAlta && nivel == NIVEL_EXPERTO) {   // experto: solo el monto y el saldo
        char buf[200];
        if (recargaAnim < (recargaEnMaquina ? 6.4f : 3.8f))
            snprintf(buf, sizeof buf, "RECARGA DE TARJETA\nRecargando $%.2f   Saldo actual: $%.2f", MONTO_RECARGA, saldoTarjeta);
        else snprintf(buf, sizeof buf, "¡RECARGA EXITOSA!\nRecargaste $%.2f   Saldo actual: $%.2f", MONTO_RECARGA, saldoTarjeta + MONTO_RECARGA);
        PrepararCartel(buf);
        return;
    }
    if (recargaAnim >= 0.0f && plantaAlta) {   // pasos de la recarga
        char buf[240]; float t = recargaAnim;
        if (recargaEnMaquina) {
            int monedas = (int)glm::clamp((t - 1.9f) / .54f, 0.0f, (float)MONEDAS_RECARGA);
            if (t < 1.9f) snprintf(buf, sizeof buf, "MÁQUINA DE RECARGA - PASO 1 DE 3\nInserta tu tarjeta en la ranura de la máquina.");
            else if (t < 4.6f) snprintf(buf, sizeof buf, "MÁQUINA DE RECARGA - PASO 2 DE 3\nIngresa las monedas: $%d.00 de $%.2f", monedas * 10, MONTO_RECARGA);
            else if (t < 6.4f) snprintf(buf, sizeof buf, "MÁQUINA DE RECARGA - PASO 3 DE 3\nToca en la pantalla el botón \"RECARGAR\".");
            else snprintf(buf, sizeof buf, "¡RECARGA EXITOSA!\nRetira tu tarjeta. Saldo nuevo: $%.2f  (el viaje cuesta $5.00)", saldoTarjeta + MONTO_RECARGA);
        } else {
            if (t < 1.5f) snprintf(buf, sizeof buf, "TAQUILLA - PASO 1 DE 2\nPega tu tarjeta al lector de la ventanilla.");
            else if (t < 3.8f) snprintf(buf, sizeof buf, "TAQUILLA - PASO 2 DE 2\nEntrega $%.2f a la persona de la taquilla.", MONTO_RECARGA);
            else snprintf(buf, sizeof buf, "¡RECARGA EXITOSA!\nSaldo nuevo: $%.2f  (el viaje cuesta $5.00)", saldoTarjeta + MONTO_RECARGA);
        }
        PrepararCartel(buf);
        return;
    }
    if (nivel == NIVEL_LIBRE) {
        if (ayudaVisible) {
            if (aBordo) PrepararCartel("CONTROLES - INTERIOR DEL VAGÓN   ([H] ocultar)\n[W][A][S][D] caminar   [Shift] correr   Mouse: mirar\n[F] bajar al andén   [M] alambre   [N] relleno");
            else if (plantaAlta) PrepararCartel("CONTROLES - PLANTA ALTA   ([H] ocultar)\n[W][A][S][D] caminar   [Shift] correr   Mouse: mirar\n[1] primera persona   [2] cámara libre   [3] aérea\n[X] bajar por la escalera (cerca de ella)   [E] recargar tarjeta (en una máquina)\n[M] alambre   [N] relleno");
            else PrepararCartel("CONTROLES - ANDENES   ([H] ocultar)\n[W][A][S][D] caminar   [Shift] correr   Mouse: mirar\n[1] primera persona   [2] cámara libre   [3] aérea   [C] cinemática\n[F] subir al vagón   [X] subir por la escalera\n[V] conos   [P] pantallas\n[M] alambre   [N] relleno");
            return;
        }
        if (aBordo) PrepararCartel("MODO LIBRE - INTERIOR DEL VAGÓN\nObserva a los pasajeros. [E] sentarte frente a un asiento libre   [F] volver al andén");
        else if (plantaAlta && tiempoTotal < avisoBloqueoHasta) PrepararCartel("TORNIQUETE BLOQUEADO\nTu tarjeta no tiene saldo. Recárgala en una máquina o en la taquilla.");
        else if (plantaAlta) PrepararCartel(CercaDeRecarga() ? "MODO LIBRE - MÁQUINA DE RECARGA\nPulsa [E] para recargar tu tarjeta." : "MODO LIBRE - PLANTA ALTA\nExplora la estación. Pulsa [H] para ver/ocultar los controles.");
        else PrepararCartel("MODO LIBRE - ANDENES\nPulsa [H] para ver/ocultar los controles.");
        return;
    }
    if (misionCumplida) {
        std::string sig = nivel == NIVEL_NOVATO ? "Ya puedes pasar al nivel Medianamente experimentado." :
                          nivel == NIVEL_MEDIO ? "Ya puedes pasar al nivel Experimentado." :
                          "Superaste el nivel más difícil. Prueba el modo libre desde el menú.";
        PrepararCartel(std::string("MISIÓN CUMPLIDA\nEntraste al vagón. ") + (carrera == 3 ? "Ya vas sentado." : "Ponte frente a un asiento libre y pulsa [E] para sentarte.") +
                       "\n" + sig + "\n[F] bajar del vagón");
        return;
    }
    if (nivel == NIVEL_NOVATO) {
        // Tutorial guiado: cada paso se desbloquea al cumplir el anterior
        if (pasoNovato == 0 && DistanciaXZ(posJugador, inicioNovato) > 3.0f) pasoNovato = 1;
        if (pasoNovato == 1 && estadoTren != TREN_AUSENTE) pasoNovato = 2;
        if (pasoNovato >= 2) pasoNovato = JuntoAPuerta() ? 3 : 2;
        if (pasoNovato == -2) PrepararCartel("BIENVENIDO AL METRO DE LA CIUDAD DE MÉXICO\n"
            "El Metro es un tren que corre bajo tierra, a nivel de calle o elevado, y mueve a\n"
            "millones de personas cada día. Tiene 12 líneas y 195 estaciones que conectan\n"
            "toda la ciudad: es una de las formas más rápidas y económicas de viajar.\n"
            "Para usarlo entras con tu tarjeta por los torniquetes, bajas al andén,\n"
            "esperas el tren y subes cuando abre sus puertas.\n"
            "Pulsa [Enter] para continuar.");
        else if (pasoNovato == -1) PrepararCartel("ESTÁS EN LA ESTACIÓN UNIVERSIDAD - LÍNEA 3\n"
            "Universidad es la terminal sur de la Línea 3 (color verde olivo), que recorre la\n"
            "ciudad de sur a norte hasta Indios Verdes. Da servicio a Ciudad Universitaria de\n"
            "la UNAM, por eso la usan muchos estudiantes todos los días.\n"
            "Ahora mismo estás en el andén: el lugar donde se espera y se aborda el tren.\n"
            "Pulsa [Enter] para comenzar.");
        else if (pasoNovato == 0) PrepararCartel("PASO 1 DE 4 - APRENDE A MOVERTE\n"
            "Camina con  [W] adelante  [S] atrás  [A] izquierda  [D] derecha\n"
            "Mueve el MOUSE para mirar a tu alrededor. Mantén [Shift] para correr.\n"
            "Misión: subir al vagón del metro. Camina unos pasos para continuar.");
        else if (pasoNovato == 1) PrepararCartel("PASO 2 DE 4 - ESPERA EL TREN\n"
            "¡Muy bien! Ahora acércate a la orilla del andén, sin cruzar la línea amarilla.\n"
            "El tren llegará solo, o puedes llamarlo pulsando [Enter].");
        else if (pasoNovato == 2) PrepararCartel(estadoTren == TREN_AUSENTE ?
            "PASO 3 DE 4 - ESPERA EL TREN\nEl tren se fue. Pulsa [Enter] para llamar al siguiente." :
            "PASO 3 DE 4 - BUSCA UNA PUERTA\n"
            "El tren está en el andén. Camina hasta una de sus puertas.\n"
            "Espera a que las puertas se abran y deja salir a los pasajeros.");
        else PrepararCartel("PASO 4 DE 4 - SUBE AL VAGÓN\n"
            "Estás frente a una puerta abierta. ¡Pulsa [F] para subir!");
    } else if (nivel == NIVEL_MEDIO) {
        if (mapaAbierto) {
            PrepararCartel("MAPA DE LA RED\n12 líneas, cada una con número y color.\nTransbordo: cambias de línea sin pagar.\nCada dirección lleva el nombre de su terminal.\nEstas en Universidad, terminal sur de la\nLinea 3 (verde olivo), rumbo a Indios Verdes.\nLupa: mueve el mouse sobre el mapa.\nRueda del mouse: más o menos aumento.\n[E] cerrar mapa");
        } else if (plantaAlta && PasoEsMapa() && DistanciaXZ(posJugador, puntosMedio[pasoMedio]) < 3.5f) {
            PrepararCartel("MEDIO - MAPA DE LA RED\nEl mapa muestra todas las líneas y estaciones del Metro y de la red de Movilidad\nIntegrada. Sirve para planear tu viaje. Pulsa [E] para verlo en grande.");
        } else if (bool maq = false; plantaAlta && PasoEsRecarga(&maq) && DistanciaXZ(posJugador, puntosMedio[pasoMedio]) < 6.0f) {
            if (CercaDeRecarga()) PrepararCartel(maq ?
                "MEDIO - RECARGA 2 DE 2: MÁQUINA\nPulsa [E] frente a una máquina para recargar tu tarjeta paso a paso." :
                "MEDIO - RECARGA 1 DE 2: TAQUILLA\nPulsa [E] frente a una ventanilla para recargar tu tarjeta paso a paso.");
            else PrepararCartel(maq ? "MEDIO - RECARGA 2 DE 2: MÁQUINA\nAcércate a una máquina de recarga." :
                                      "MEDIO - RECARGA 1 DE 2: TAQUILLA\nAcércate a una ventanilla de la taquilla.");
        } else if (plantaAlta && CercaDeRecarga()) {
            PrepararCartel("MEDIO - RECARGA DE TU TARJETA\nPulsa [E] para ver paso a paso cómo se recarga la tarjeta del Metro.");
        } else if (plantaAlta && !infoLugar.empty() && tiempoTotal < infoHasta) {
            PrepararCartel(infoLugar + (pasoMedio < (int)nombresMedio.size() ?
                "\nSiguiente: " + nombresMedio[pasoMedio] + "." : std::string("\nRecorrido completo: busca una escalera a los andenes.")));
        } else if (plantaAlta && pasoMedio < (int)puntosMedio.size()) {
            glm::vec3 d = puntosMedio[pasoMedio] - posJugador; d.y = 0;
            glm::vec3 f = glm::normalize(glm::vec3(camara.Front.x, 0, camara.Front.z));
            float frente = glm::dot(glm::normalize(d), f), lado = glm::cross(f, glm::normalize(d)).y;
            const char* dir = frente > .7f ? "adelante" : frente < -.5f ? "detras de ti" : lado < 0 ? "a tu derecha" : "a tu izquierda";
            char buf[256];
            snprintf(buf, sizeof buf, "MEDIO - RECORRE LA ESTACIÓN (punto %d de %d)\nVe hacia %s: está %s, a %.0f m.",
                     pasoMedio + 1, (int)puntosMedio.size(), nombresMedio[pasoMedio].c_str(), dir, glm::length(d));
            PrepararCartel(buf);
        } else if (plantaAlta) PrepararCartel(EscaleraCercana() >= 0 ?
            "MEDIO - BAJA AL ANDÉN\nPulsa [X] para bajar por esta escalera." :
            "MEDIO - RECORRIDO COMPLETO\nAhora busca una escalera hacia los andenes.");
        else PrepararCartel(JuntoAPuerta() ? "MEDIO - ENTRA AL METRO\nPulsa [F] para subir al vagón." :
            "MEDIO - ENTRA AL METRO\nAcercate a una puerta del tren cuando se detenga.");
    } else {
        if (enFila) {
            int delante = (filaJugador >= 0 && filaJugador < (int)filas.size()) ? (int)filas[filaJugador].personas.size() : 0;
            char buf[200];
            snprintf(buf, sizeof buf, "EN LA FILA - ESPERA TU TURNO\nPersonas delante de ti: %d   (%.0f s)\nHora pico: ten paciencia.",
                     std::max(delante, 0), std::ceil(tiempoFila));
            PrepararCartel(buf);
        } else if (!tarjetaRecargada) PrepararCartel(CercaDeRecarga() ?
            "EXPERTO - HAY FILA PARA RECARGAR\nPulsa [E] para formarte y esperar tu turno." :
            tiempoTotal < avisoBloqueoHasta ?
            "TORNIQUETE BLOQUEADO\nPrimero recarga tu tarjeta en una máquina o taquilla." :
            "EXPERTO - RECARGA TU TARJETA Y ENTRA AL METRO\nVe a las máquinas o a las taquillas.");
        else if (!plantaAlta) PrepararCartel(JuntoAPuerta() ? "EXPERTO - SUBE AL VAGÓN\nPulsa [F] para subir al vagón." :
            "EXPERTO - SUBE AL VAGÓN\nEspera el tren en el andén y acércate a una de sus puertas.\nCuando se abran, pulsa [F] para subir.");
        else PrepararCartel("EXPERTO - TARJETA RECARGADA\nCruza los torniquetes y entra al vagón.");
    }
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

void IniciarRecarga()
{
    int mejor = 0; float d = 1e9f;
    for (size_t i = 0; i < puntosRecarga.size(); ++i) {
        float x = DistanciaXZ(posJugador, puntosRecarga[i]);
        if (x < d) { d = x; mejor = (int)i; }
    }
    recargaEnMaquina = mejor < numMaquinasRecarga;
    recargaAnim = 0.0f; recargaPaso = 0;
    if (nivel == NIVEL_MEDIO && !puntosRecarga.empty()) {
        const glm::vec3 r = puntosRecarga[mejor];
        float mejorD = 1e9f; glm::vec3 lugar(0), dir(0);
        for (glm::vec3 d : { glm::vec3(1,0,0), glm::vec3(-1,0,0), glm::vec3(0,0,1), glm::vec3(0,0,-1) })
            for (float dist = .7f; dist <= 3.0f; dist += .1f) {
                glm::vec3 q = r + d * dist; q.y = posJugador.y;
                if (colisionPlantaAlta.Libre(q) && colisionPlantaAlta.Libre(q + d * 2.4f)) {
                    if (dist < mejorD) { mejorD = dist; lugar = q + d * .2f; dir = d; }   // mismo lugar que el primero de la fila
                    break;
                }
            }
        if (mejorD < 1e8f) {
            posJugador = lugar;
            camara.Position = posJugador + glm::vec3(0, ALTURA_OJOS, 0);
            camara.Yaw = glm::degrees(std::atan2(-dir.z, -dir.x));
            camara.ProcessMouseMovement(0, 0);
        }
    }
    if (nivel == NIVEL_LIBRE && !filas.empty()) {
        // Mismo lugar que el primero de la fila en el modo experto
        int k = 0; float dm = 1e9f;
        for (size_t i = 0; i < filas.size(); ++i) {
            float x = DistanciaXZ(posJugador, filas[i].inicio);
            if (x < dm) { dm = x; k = (int)i; }
        }
        recargaEnMaquina = k < numFilasMaquina;
        posJugador = filas[k].inicio; posJugador.y = alturaPiso;
        camara.Position = posJugador + glm::vec3(0, ALTURA_OJOS, 0);
        glm::vec3 d = -filas[k].dir;
        camara.Yaw = glm::degrees(std::atan2(d.z, d.x));
        camara.ProcessMouseMovement(0, 0);
    }
    MirarParaRecarga();
}

static void MirarParaRecarga()
{
    camara.Pitch = recargaEnMaquina ? -25.0f : 14.0f;   // maquina: mirar hacia la ranura y la pantalla
    camara.ProcessMouseMovement(0, 0);
}

// Lado de la entrada (donde estan las maquinas): de ahi hacia el anden hay que validar
bool LadoDeEntrada(const glm::vec3& p)
{
    if (torniquetesAlta.carriles.empty()) return false;
    float zc = 0; for (const auto& t : torniquetesAlta.carriles) zc += t.centro.y;
    zc /= (float)torniquetesAlta.carriles.size();
    // Hay dos filas de torniquetes (una en cada extremo); la entrada (sin pagar) queda
    // del lado de afuera de la fila mas cercana, lejos del centro de la estacion.
    float zb = torniquetesAlta.carriles.front().centro.y;
    for (const auto& t : torniquetesAlta.carriles) if (std::fabs(p.z - t.centro.y) < std::fabs(p.z - zb)) zb = t.centro.y;
    return (p.z - zb) * (zb - zc) > 0.0f;
}

// Revisa si el jugador llego frente a un torniquete y arranca la animacion de validar
// validado: pago y aun no cruza. Se consume al quedar adentro, asi que para volver
// a entrar siempre hay que pasar la tarjeta (y tener al menos $5).
static void RevisarValidacion()
{
    torniqueteCerca = -1;
    if (!plantaAlta || aBordo || validaAnim >= 0.0f || recargaAnim >= 0.0f || enFila || modoCamara != CAM_PRIMERA_PERSONA) return;
    const bool afuera = LadoDeEntrada(posJugador);
    if (!afuera) validado = false;                       // ya entro: el pago se uso
    glm::vec3 vista(camara.Front.x, 0, camara.Front.z);
    for (size_t i = 0; i < torniquetesAlta.carriles.size(); ++i) {
        const auto& t = torniquetesAlta.carriles[i];
        if (ZonaTorniquetesMural(glm::vec3(t.centro.x, 0, t.centro.y))) continue;   // junto al mural: paso libre
        float dx = std::fabs(posJugador.x - t.centro.x), dz = std::fabs(posJugador.z - t.centro.y);
        float sgn = posJugador.z > t.centro.y ? 1.0f : -1.0f;     // lado donde esta el jugador
        glm::vec3 adelante(0, 0, -sgn);
        if (afuera) {
            // casi pegado al torniquete: se ofrece [E] para entrar (no entra solo)
            if (dx <= t.semiancho + .25f && dz < 1.0f && dz > .3f) { torniqueteCerca = (int)i; return; }
            continue;
        }
        // salida: solo si camina de frente hacia el torniquete (no al pasar de lado)
        if (dx > t.semiancho + .2f || dz > 1.3f || dz < .45f) continue;
        if (glm::length(vista) < .1f || glm::dot(glm::normalize(vista), adelante) < .6f) continue;
        validaSalida = true;
        validaParada = glm::vec3(t.centro.x, posJugador.y, t.centro.y + sgn * .85f);
        validaFin = glm::vec3(t.centro.x, posJugador.y, t.centro.y - sgn * 2.8f);   // queda lejos: no se regresa por error
        validaTorn = (int)i; validaAnim = 0.0f;
        validaInicio = posJugador;
        validaYaw = glm::degrees(std::atan2(adelante.z, adelante.x));
        camara.Yaw = validaYaw; camara.ProcessMouseMovement(0, 0);
        return;
    }
}

// [E] frente al torniquete: pasar la tarjeta y entrar (si hay saldo)
static bool EntrarPorTorniquete()
{
    if (torniqueteCerca < 0 || validaAnim >= 0.0f) return false;
    const bool cobra = nivel == NIVEL_EXPERTO || nivel == NIVEL_LIBRE;
    if (cobra && saldoTarjeta < 5.0f) { avisoBloqueoHasta = tiempoTotal + 2.5f; return true; }   // sin saldo: bloqueado
    const auto& t = torniquetesAlta.carriles[torniqueteCerca];
    float sgn = posJugador.z > t.centro.y ? 1.0f : -1.0f;
    glm::vec3 adelante(0, 0, -sgn);
    validaSalida = false;
    validaTorn = torniqueteCerca; validaAnim = 0.0f;
    validaInicio = posJugador;
    validaParada = glm::vec3(t.centro.x, posJugador.y, t.centro.y + sgn * .85f);   // pegado al lector
    validaFin = glm::vec3(t.centro.x, posJugador.y, t.centro.y - sgn * 1.6f);
    validaYaw = glm::degrees(std::atan2(adelante.z, adelante.x));
    camara.Yaw = validaYaw; camara.Pitch = -20.0f; camara.ProcessMouseMovement(0, 0);
    torniqueteCerca = -1;
    return true;
}

static void ActualizarValidacion(float dt)
{
    if (validaAnim < 0.0f) return;
    validaAnim += dt * (nivel == NIVEL_LIBRE ? 1.6f : 1.0f);   // libre: mas rapido
    float t = validaAnim;
    auto suave = [](float u) { u = glm::clamp(u, 0.0f, 1.0f); return u * u * (3.0f - 2.0f * u); };
    if (validaSalida) {   // salida rapida (1.5 s): se alinea y cruza caminando, sin tarjeta
        if (t < .3f) posJugador = glm::mix(validaInicio, validaParada, suave(t / .3f));
        else posJugador = glm::mix(validaParada, validaFin, suave((t - .3f) / 1.2f));
        camara.Pitch = camara.Pitch * (1.0f - std::min(1.0f, dt * 6.0f));
        camara.Yaw = validaYaw; camara.ProcessMouseMovement(0, 0);
        camara.Position = posJugador + glm::vec3(0, ALTURA_OJOS, 0);
        if (t >= DURACION_SALIDA) validaAnim = -1.0f;
        return;
    }
    if (t < .5f) posJugador = glm::mix(validaInicio, validaParada, suave(t / .5f));            // se coloca frente al lector
    else if (t > 2.7f) posJugador = glm::mix(validaParada, validaFin, suave((t - 2.7f) / 1.9f)); // avanza y cruza
    if (t > 1.6f && !validado) {
        validado = true;
        ReproducirSonido(RUTA_SONIDO_BIP);
        if (nivel == NIVEL_EXPERTO || nivel == NIVEL_LIBRE) {
            saldoTarjeta = std::max(0.0f, saldoTarjeta - 5.0f);   // se cobra el viaje
            tarjetaRecargada = saldoTarjeta >= 5.0f;             // sin saldo: hay que recargar otra vez
        }
    }
    camara.Pitch = t < 2.7f ? -20.0f : -20.0f * (1.0f - suave((t - 2.7f) / .6f));
    camara.Yaw = validaYaw; camara.ProcessMouseMovement(0, 0);
    camara.Position = posJugador + glm::vec3(0, ALTURA_OJOS, 0);
    if (t >= DURACION_VALIDA) { validaAnim = -1.0f; if (!LadoDeEntrada(posJugador)) validado = false; }
}

bool CercaDeRecarga()
{
    if (!plantaAlta || modoCamara != CAM_PRIMERA_PERSONA) return false;
    for (const auto& p : puntosRecarga)
        if (glm::length(glm::vec2(posJugador.x-p.x, posJugador.z-p.z)) < 3.0f) return true;
    for (const auto& q : filasRecarga)
        if (DistanciaXZ(posJugador, q) < 1.6f) return true;
    return false;
}

void IniciarNivel(int elegido)
{
    nivel = elegido;
    // Si ya estamos en la planta inicial, CargarPlanta no se ejecuta otra vez.
    pasajeros.AjustarAfluenciaAlta(elegido==NIVEL_EXPERTO,colisionPlantaAlta);
    pasajeros.AjustarAfluenciaAnden(elegido==NIVEL_EXPERTO,colisionAnden);
    tarjetaRecargada = false; recargaAnim = -1.0f; saldoTarjeta = 0.0f; validaAnim = -1.0f; validado = false;
    misionCumplida = false;
    enFila = false; pasoMedio = 0; puntosMedio.clear(); nombresMedio.clear(); filasRecarga.clear(); filas.clear(); saliendoFila.clear(); filaJugador = -1; rutaGuia.clear(); rutaPaso = -1; filasEnMovimiento = false; mapaAbierto = false; infoLugar.clear(); dialogoTexto.clear();
    avisoBloqueoHasta = 0.0f;
    if (elegido == NIVEL_NOVATO) {
        if (plantaAlta && !CargarPlanta(false)) { nivel = -1; return; }
        alturaPiso = 0.0f;
        posJugador = glm::vec3(-4.5f, 0.0f, 30.0f);
        camara.Position = posJugador + glm::vec3(0, ALTURA_OJOS, 0);
        pasoNovato = -2; inicioNovato = posJugador;   // -2 y -1: introduccion
    } else {
        if (!plantaAlta && !CargarPlanta(true)) { nivel = -1; return; }
        // Busca un punto libre cercano a 'p' (en espiral)
        auto libreCerca = [](glm::vec3 p) {
            p.y = .019f;
            if (colisionPlantaAlta.Libre(p)) return p;
            for (float r = .5f; r <= 4.0f; r += .5f)
                for (int k = 0; k < 16; ++k) {
                    glm::vec3 q(p.x + r * std::cos(k * .392699f), .019f, p.z + r * std::sin(k * .392699f));
                    if (colisionPlantaAlta.Libre(q)) return q;
                }
            return p;
        };
        auto mirarA = [](const glm::vec3& o) {
            glm::vec3 d = o - posJugador;
            camara.Yaw = glm::degrees(std::atan2(d.z, d.x)); camara.Pitch = 0;
            camara.ProcessMouseMovement(0, 0);
        };
        glm::vec3 entrada = (centroMaquinas + centroTaquillas) * .5f;   // medio de la entrada
        if (puntosRecarga.empty()) entrada = glm::vec3(-12.0f, 0.019f, 40.0f);
        glm::vec3 torn(0);
        for (const auto& t : torniquetesAlta.carriles) torn += glm::vec3(t.centro.x, 0, t.centro.y);
        if (!torniquetesAlta.carriles.empty()) torn /= (float)torniquetesAlta.carriles.size();
        if (elegido == NIVEL_MEDIO && !torniquetesAlta.carriles.empty()) {
            // Frente al torniquete mas cercano a la entrada, del lado de la entrada
            const Torniquete* mejor = &torniquetesAlta.carriles.front();
            for (const auto& t : torniquetesAlta.carriles)
                if (glm::length(glm::vec2(t.centro.x - entrada.x, t.centro.y - entrada.z)) <
                    glm::length(glm::vec2(mejor->centro.x - entrada.x, mejor->centro.y - entrada.z))) mejor = &t;
            float lado = entrada.z >= mejor->centro.y ? 1.0f : -1.0f;
            posJugador = libreCerca(glm::vec3(mejor->centro.x, 0, mejor->centro.y + 3.2f * lado));
            alturaPiso = .019f;
            camara.Position = posJugador + glm::vec3(0, ALTURA_OJOS, 0);
            mirarA(glm::vec3(mejor->centro.x, 0, mejor->centro.y));
            // Recorrido: taquillas, maquinas y cruzar los torniquetes
            puntosMedio.push_back(libreCerca(centroTaquillas + DirFila(centroTaquillas) * 2.0f));
            puntosMedio.push_back(libreCerca(centroMaquinas + DirFila(centroMaquinas) * 2.0f));
            puntosMedio.push_back(libreCerca(glm::vec3(mejor->centro.x, 0, mejor->centro.y - 3.0f * lado)));
            nombresMedio = { "las taquillas", "las máquinas de recarga", "el otro lado de los torniquetes" };
            {   // Primero: el mapa de la red del metro mas cercano
                texturaMapa = 0; float mejorD = 1e9f;
                for (int i : estacion->BuscarPrefijo("Mapa_CDMX")) {
                    const Nodo& nd = estacion->nodos[i];
                    if (nd.aabbMax.x <= nd.aabbMin.x || nd.mallas.empty()) continue;
                    glm::vec3 c = (nd.aabbMin + nd.aabbMax) * .5f;
                    float d = DistanciaXZ(c, posJugador);
                    if (d >= mejorD) continue;
                    mejorD = d; posMapa = c;
                    const auto& m = estacion->mallas[nd.mallas[0]];
                    if (m.material < estacion->materiales.size()) texturaMapa = estacion->materiales[m.material].texturaDifusa;
                }
                if (mejorD < 1e8f) {
                    puntosMedio.insert(puntosMedio.begin(), libreCerca(posMapa));
                    nombresMedio.insert(nombresMedio.begin(), NOMBRE_MAPA);
                }
            }
            // Despues: cibercentro y murales de la estacion
            auto centroDe = [](std::initializer_list<const char*> prefijos, bool unir) {
                std::vector<glm::vec3> r; glm::vec3 suma(0); int n = 0;
                for (const char* pre : prefijos) for (int i : estacion->BuscarPrefijo(pre)) {
                    const Nodo& nd = estacion->nodos[i];
                    if (nd.aabbMax.x <= nd.aabbMin.x) continue;
                    glm::vec3 c = (nd.aabbMin + nd.aabbMax) * .5f;
                    if (unir) { suma += c; ++n; continue; }
                    bool repetido = false;
                    for (const auto& o : r) if (DistanciaXZ(o, c) < 5.0f) repetido = true;
                    if (!repetido) r.push_back(c);
                }
                if (unir && n) r.push_back(suma / (float)n);
                return r;
            };
            {   // el segundo elevador tiene encima el mural nuevo de la remodelacion
                int n = 0;
                for (const auto& c : centroDe({ "Elevador" }, false)) {
                    puntosMedio.push_back(libreCerca(c)); nombresMedio.push_back(n++ == 0 ? "el elevador" : "el mural del elevador");
                }
            }
            for (const auto& c : centroDe({ "CIBERCENTRO", "cibercentro", "Cibercentro" }, true)) {
                puntosMedio.push_back(libreCerca(c)); nombresMedio.push_back("el cibercentro");
            }
            for (const auto& c : centroDe({ "Mural" }, false)) {
                bool cerca = false;   // no repetir un punto ya visitado (p.ej. mural de taquillas)
                for (const auto& q : puntosMedio) if (DistanciaXZ(q, c) < 4.0f) cerca = true;
                if (!cerca) { puntosMedio.push_back(libreCerca(c)); nombresMedio.push_back("el mural"); }
            }
            posJugador = puntosMedio.empty() ? posJugador : posJugador;
        } else if (elegido == NIVEL_MEDIO) {
            ColocarEnEscalera(0);
        } else {
            // En medio de la entrada, alejado de los torniquetes y mirando hacia ellos:
            // maquinas a un lado y taquillas al otro
            glm::vec3 alejar = entrada - torn; alejar.y = 0;
            if (glm::length(alejar) > .01f) alejar = glm::normalize(alejar);
            posJugador = libreCerca(entrada + alejar * 2.0f);
            alturaPiso = .019f;
            camara.Position = posJugador + glm::vec3(0, ALTURA_OJOS, 0);
            mirarA(torniquetesAlta.carriles.empty() ? centroMaquinas : torn);

            if (elegido == NIVEL_EXPERTO || elegido == NIVEL_LIBRE) {
            // Filas de hora pico frente a cada maquina / taquilla
            // Todas las maquinas y taquillas tienen fila; quietas hasta que el usuario se forme
            filas.clear(); saliendoFila.clear(); filaJugador = -1;
            // Solo 4 filas, en las ventanitas negras del lado derecho del muro de taquillas (mural)
            std::vector<glm::vec3> ventanillas;
            std::vector<std::pair<glm::vec3, glm::vec3>> filasVentanilla;   // inicio y direccion
            {
                glm::vec3 mn = taqMin, mx = taqMax;
                for (int i : estacion->BuscarPrefijo("Mural_Taquilla")) {
                    const Nodo& nd = estacion->nodos[i];
                    if (nd.aabbMax.x > nd.aabbMin.x) { mn = nd.aabbMin; mx = nd.aabbMax; break; }
                }
                if (mx.x > mn.x) {
                    glm::vec3 c = (mn + mx) * .5f, t = mx - mn, frente;
                    // El frente mira hacia el centro de la nave, sobre el eje corto del muro
                    if (t.x < t.z) frente = glm::vec3(c.x > -6.5f ? -1.0f : 1.0f, 0, 0);
                    else frente = glm::vec3(0, 0, c.z > 0.0f ? -1.0f : 1.0f);
                    float grosor = std::fabs(glm::dot(t, frente)) * .5f;
                    glm::vec3 derecha = glm::cross(-frente, glm::vec3(0, 1, 0));   // derecha de quien mira el muro
                    float L = std::fabs(glm::dot(t, derecha));
                    for (float f : { .518f, .648f, .771f, .871f }) {   // frente a 4 ventanitas, separacion pareja
                        glm::vec3 q = c + derecha * ((f - .5f) * L) + frente * (grosor + .9f)
                                     + glm::normalize(glm::cross(frente, glm::vec3(0, 1, 0))) * .25f;   // un poco a la izquierda: justo frente a la ventana
                        q.y = .019f;
                        filasVentanilla.push_back({ q, frente });
                    }
                }
            }
            auto agregar = [&](FilaRecarga f) {
                std::vector<glm::vec3> pos;
                for (int k = 0; k < PERSONAS_FILA; ++k) pos.push_back(f.inicio + f.dir * (.75f * k));
                int base = elegido == NIVEL_EXPERTO ? pasajeros.AgregarFila(pos) : -1;   // en libre: solo el lugar
                if (base >= 0) for (int k = 0; k < PERSONAS_FILA; ++k) f.personas.push_back(base + k);
                filasRecarga.push_back(f.inicio + f.dir * (.75f * PERSONAS_FILA));   // final de la fila
                filas.push_back(f);
            };
            for (int i = numMaquinasRecarga - 1; i >= 0; --i) ventanillas.insert(ventanillas.begin(), puntosRecarga[i]);   // todas las maquinas
            // Ajuste lateral de cada fila de maquina (de izquierda a derecha viendo las maquinas; + = a la izquierda)
            const float ajusteMaquina[] = { .25f, .50f, .0f, .05f, .45f };
            std::vector<float> lateral;
            for (const auto& r : ventanillas) { glm::vec3 d = DirFila(r); lateral.push_back(glm::dot(r, glm::cross(d, glm::vec3(0, 1, 0)))); }
            int iFila = -1;
            for (const auto& r : ventanillas) {
                ++iFila;
                int rango = 0;   // 0 = la de mas a la izquierda
                for (size_t k = 0; k < lateral.size(); ++k) if (lateral[k] > lateral[iFila]) ++rango;
                FilaRecarga f; f.dir = DirFila(r); f.inicio = r + f.dir * 1.1f; f.inicio.y = .019f;
                {   // lado libre (fuera de muros): primer punto caminable mas cercano
                    float mejor = 1e9f;
                    for (glm::vec3 d : { glm::vec3(1,0,0), glm::vec3(-1,0,0), glm::vec3(0,0,1), glm::vec3(0,0,-1) })
                        for (float dist = .7f; dist <= 3.0f; dist += .1f) {
                            glm::vec3 q = r + d * dist; q.y = .019f;
                            if (colisionPlantaAlta.Libre(q) && colisionPlantaAlta.Libre(q + d * 2.4f)) {
                                if (dist < mejor) { mejor = dist; f.dir = d; f.inicio = q + d * .2f; }
                                break;
                            }
                        }
                }
                if (rango < 5) f.inicio += glm::normalize(glm::cross(f.dir, glm::vec3(0, 1, 0))) * ajusteMaquina[rango];
                agregar(f);
            }
            numFilasMaquina = (int)filas.size();
            for (const auto& fv : filasVentanilla) {
                FilaRecarga f; f.inicio = fv.first; f.dir = fv.second;
                agregar(f);
            }
            }
        }
    }
    modoCamara=CAM_PRIMERA_PERSONA;
    if (elegido == NIVEL_EXPERTO)
        dialogoTexto = "EXPERTO - ES HORA PICO\n"
            "Entre semana, de 6 a 9 de la mañana y de 6 a 9 de la noche, millones de personas\n"
            "van o regresan del trabajo y la escuela: los pasillos se llenan y hay filas en\n"
            "taquillas y máquinas. Tu misión: recarga tu tarjeta (haz fila) y entra al vagón.";
    else if (elegido == NIVEL_LIBRE)
        dialogoTexto = "MODO LIBRE\n"
            "Sin misiones ni tiempo: explora la estación a tu gusto, cambia de cámara y de planta.\n"
            "Pulsa [H] en cualquier momento para ver u ocultar los controles.";
    if (elegido == NIVEL_NOVATO) {   // la introduccion se cuenta con la camara cinematografica
        yawNovato = camara.Yaw; pitchNovato = camara.Pitch;
        modoAntesDeCinematica = CAM_PRIMERA_PERSONA;
        modoCamara = CAM_CINEMATICA; tiempoCinematica = 0.0f;
    }
    glfwSetInputMode(ventana,GLFW_CURSOR,GLFW_CURSOR_DISABLED);
    primerMouse=true;
    ActualizarCartel();
}

void VolverAlMenu()
{
    if (aBordo) SalirEscenaVagon();
    nivel = -1;
    misionCumplida = false;
    seleccionado = -1;
    glfwSetInputMode(ventana, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    pasajeros.AjustarAfluenciaAlta(false, colisionPlantaAlta);
    pasajeros.AjustarAfluenciaAnden(false, colisionAnden);
    cartelActual.clear();
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
    if (nivel == NIVEL_EXPERTO && !tarjetaRecargada) return -1;
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
    pasajeros.PrepararVagon(*estacion, nivel == NIVEL_EXPERTO);
    if (nivel == NIVEL_EXPERTO) {
        for (const auto& p : pasajeros.vagon) if (p.accion == AGARRARSE || p.accion == QUIETO)
            colisionVagon.obstaculos.push_back({p.pos+glm::vec3(-.17f,0,-.13f),
                                                p.pos+glm::vec3(.17f,1.8f,.13f)});
    }
    aBordo = true;
    modoCamara = CAM_PRIMERA_PERSONA;
    if (nivel == NIVEL_EXPERTO) {
        // Aparece en el punto del pasillo mas alejado de cualquier asiento
        float mejorX = 72.3f, mejorD = -1.0f;
        for (float x = 65.8f; x <= 78.8f; x += .1f) {
            if (!colisionVagon.Libre(glm::vec3(x, alturaPiso, 0))) continue;
            float d = 1e9f;
            for (const auto& a : pasajeros.asientos) d = std::min(d, std::fabs(a.pos.x - x));
            if (d > mejorD) { mejorD = d; mejorX = x; }
        }
        posJugador = glm::vec3(mejorX, alturaPiso, 0);
        pasajeros.SentarCercanos(mejorX, 6);   // los asientos mas cercanos ya estan ocupados
        camara.Position = posJugador + glm::vec3(0, ALTURA_OJOS, 0);
        carrera = 1;
        dialogoTexto = "ESTÁS MUY CANSADO\n"
            "Fue un día largo y el viaje a casa también lo será: necesitas un asiento.\n"
            "Al pulsar [Enter] habrá una cuenta regresiva para que te prepares. Después camina\n"
            "por el pasillo, ponte frente a un asiento libre y pulsa [E] para sentarte.\n"
            "¡Cada vez subirán más pasajeros que también buscan lugar! Si se ocupan todos,\n"
            "bajarás al andén para intentarlo de nuevo.";
    }
    std::cout << "> Interior del vagon 5: Tren_Completo123.fbx" << std::endl;
}

void SalirEscenaVagon()
{
    if (!aBordo) return;
    carrera = 0;
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
        if (!colisionAnden.Libre(posJugador) || EscaleraCercana() != i) return fallo(i, "sin acceso desde el andén");
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
bool ProbarNiveles()
{
    IniciarNivel(NIVEL_NOVATO);
    if (plantaAlta || aBordo || !colisionAnden.Libre(posJugador) || pasajeros.anden.size()>32) return false;
    IniciarNivel(NIVEL_LIBRE);
    if (plantaAlta || misionCumplida || pasajeros.anden.size()>32) return false;
    IniciarNivel(NIVEL_MEDIO);
    if (!plantaAlta || !colisionPlantaAlta.Libre(posJugador) || EscaleraCercana()<0 || pasajeros.arriba.size()>12) return false;
    IniciarNivel(NIVEL_EXPERTO);
    if (!plantaAlta || puntosRecarga.empty() || !colisionPlantaAlta.Libre(posJugador) || !CercaDeRecarga()) return false;
    if (pasajeros.arriba.size()<24) return false;
    const auto& t=torniquetesAlta.carriles.front();
    glm::vec3 frente(t.centro.x,0,t.centro.y-2.0f);
    for (int i=0;i<60;++i)
        torniquetesAlta.Actualizar(*estacion,colisionPlantaAlta,frente,1.0f/60.0f,{},tarjetaRecargada);
    if (t.avance!=0.0f || colisionPlantaAlta.Libre(glm::vec3(t.centro.x,0,t.centro.y))) return false;
    callbackTecla(ventana,GLFW_KEY_E,0,GLFW_PRESS,0);
    if (!tarjetaRecargada) return false;
    for (int i=0;i<60;++i)
        torniquetesAlta.Actualizar(*estacion,colisionPlantaAlta,frente,1.0f/60.0f,{},tarjetaRecargada);
    if (t.avance<.99f) return false;
    if (!CargarPlanta(false)) return false;
    if (pasajeros.anden.size()<70) return false;
    EntrarEscenaVagon();
    if (!aBordo || pasajeros.vagon.size()<20 || colisionVagon.obstaculos.size()<12 ||
        !colisionVagon.Libre(posJugador)) return false;
    for (float x=66.05f;x<78.5f;x+=.10f)
        if (!colisionVagon.Libre(glm::vec3(x,.012f,0))) return false;
    int agarres=0;
    for (const auto& p: pasajeros.vagon) if (p.accion==AGARRARSE) {
        float distanciaTubo=FLT_MAX;
        for (const auto& n: estacion->nodos) if (n.nombre.compare(0,14,"V05_Pasamanos_")==0 &&
                                                 n.nombre.find("VIP")==std::string::npos) {
            bool positivo=(n.aabbMin.z+n.aabbMax.z)>0.0f;
            float z=positivo?n.aabbMin.z+.115f:n.aabbMax.z-.115f;
            for (float x : {n.aabbMin.x+.04f,n.aabbMax.x-.04f})
                distanciaTubo=std::min(distanciaTubo,glm::length(glm::vec2(p.mano.x-x,p.mano.z-z)));
        }
        const auto& cuerpo=pasajeros.modelos[p.tipo];
        glm::mat4 ubicacion=cuerpo.Ubicar(p);
        auto pose=cuerpo.Pose(p,ubicacion);
        glm::vec3 manoAnimada=glm::vec3(ubicacion*pose[7]*glm::vec4(cuerpo.juntas[7],1));
        if (distanciaTubo>.035f || glm::distance(manoAnimada,p.mano)>.035f) return false;
        ++agarres;
    }
    if (agarres<12) return false;
    for (size_t i=0;i<pasajeros.vagon.size();++i) if (pasajeros.vagon[i].accion==AGARRARSE)
        for (size_t j=i+1;j<pasajeros.vagon.size();++j) if (pasajeros.vagon[j].accion==AGARRARSE &&
            DistanciaXZ(pasajeros.vagon[i].pos,pasajeros.vagon[j].pos)<.65f) return false;
    std::cout << "> Cuatro modos, recarga, bloqueo y hora pico verificados." << std::endl;
    return true;
}

void ProcesarEntrada(float dt)
{
    // El movimiento continuo (WASD, flechas) se lee en los BLOQUES 9 y 11
}

void CambiarModoCamara(ModoCamara nuevo)
{
    if (fundidoDireccion != 0 || (nivel != NIVEL_LIBRE && nuevo != CAM_PRIMERA_PERSONA) ||
        (aBordo && nuevo != CAM_PRIMERA_PERSONA)) return;
    if (modoCamara == CAM_LIBRE && nuevo == CAM_PRIMERA_PERSONA)
        camara.Position = posJugador + glm::vec3(0.0f, ALTURA_OJOS, 0.0f);
    modoCamara = nuevo;
    glfwSetInputMode(ventana, GLFW_CURSOR, nuevo == CAM_AEREA ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED);
    primerMouse = true;
}

void callbackTecla(GLFWwindow* w, int tecla, int scancode, int accion, int mods)
{
    if (accion != GLFW_PRESS || (fundidoDireccion != 0 && tecla != GLFW_KEY_ESCAPE)) return;
    if ((tecla == GLFW_KEY_F1 || tecla == GLFW_KEY_TAB) && nivel >= 0) { VolverAlMenu(); return; }
    if (nivel < 0 && tecla >= GLFW_KEY_1 && tecla <= GLFW_KEY_4) {
        if (menuElegido < 0) { menuElegido = tecla - GLFW_KEY_1; menuClicT = 0; }
        return;
    }
    if (nivel < 0 && tecla != GLFW_KEY_ESCAPE) return;
    switch (tecla) {
    case GLFW_KEY_ESCAPE: glfwSetWindowShouldClose(w, true); break;
    // Camaras
    case GLFW_KEY_1: CambiarModoCamara(CAM_PRIMERA_PERSONA); break;
    case GLFW_KEY_2: CambiarModoCamara(CAM_LIBRE); break;
    case GLFW_KEY_3: CambiarModoCamara(CAM_AEREA); break;
    case GLFW_KEY_C:
        if (nivel == NIVEL_LIBRE && !plantaAlta && !aBordo && modoCamara != CAM_CINEMATICA) {
            modoAntesDeCinematica = (modoCamara == CAM_AEREA) ? CAM_PRIMERA_PERSONA : modoCamara;
            CambiarModoCamara(CAM_PRIMERA_PERSONA);
            modoCamara = CAM_CINEMATICA;
            tiempoCinematica = 0.0f;
        }
        break;
    // Tren y escena
    case GLFW_KEY_ENTER:   // continuar dialogos; si no hay, llamar al tren
        if (!dialogoTexto.empty()) {
            dialogoTexto.clear();
            if (carrera == 1) { carrera = 2; tCarrera = 3.99f; pasajeros.IniciarCarrera(posJugador.x); }   // cuenta 3, 2, 1, 0
            break;
        }
        if (nivel == NIVEL_NOVATO && pasoNovato < 0) { callbackTecla(w, GLFW_KEY_SPACE, scancode, accion, mods); break; }
        if (!plantaAlta && estadoTren == TREN_AUSENTE) CambiarEstadoTren(TREN_LLEGANDO);
        break;
    case GLFW_KEY_F: IntentarSubirOBajar(); break;
    case GLFW_KEY_SPACE:
        if (nivel == NIVEL_NOVATO && pasoNovato < 0) {
            ++pasoNovato; inicioNovato = posJugador;
            if (pasoNovato == 0 && modoCamara == CAM_CINEMATICA) {   // fin de la introduccion: a primera persona
                modoCamara = CAM_PRIMERA_PERSONA;
                camara.Position = posJugador + glm::vec3(0, ALTURA_OJOS, 0);
                camara.Yaw = yawNovato; camara.Pitch = pitchNovato; camara.ProcessMouseMovement(0, 0);
            }
        }
        break;
    case GLFW_KEY_E:
        if (EntrarPorTorniquete()) break;   // frente a un torniquete: pasar la tarjeta y entrar
        if (aBordo && (carrera == 5 || (nivel != NIVEL_EXPERTO && carrera == 0))) {   // sentarse frente a un asiento libre (todos los modos)
            int libre = -1; bool reservado = false;
            for (size_t k = 0; k < pasajeros.asientos.size(); ++k) {
                const auto& a = pasajeros.asientos[k];
                if (a.ocupado || std::fabs(a.pos.x - posJugador.x) > .45f) continue;
                if (a.reservado) reservado = true; else if (libre < 0) libre = (int)k;
            }
            if (libre < 0 && reservado) avisoReservadoHasta = tiempoTotal + 2.0f;   // trampa: asiento reservado
            for (int k = libre; k >= 0; k = -1) {
                const auto& a = pasajeros.asientos[k];
                pasajeros.OcuparAsiento(k);
                asientoJugador = a.pos; carrera = 3; tCarrera = 2.0f;   // el tren arranca en 2 s
                break;
            }
            break;
        }
        if (nivel == NIVEL_MEDIO && plantaAlta) {
            if (mapaAbierto) { mapaAbierto = false; ++pasoMedio; rutaRecalculo = 0; glfwSetInputMode(ventana, GLFW_CURSOR, GLFW_CURSOR_DISABLED); primerMouse = true; break; }
            if (PasoEsMapa() && DistanciaXZ(posJugador, puntosMedio[pasoMedio]) < 3.5f) { mapaAbierto = true; zoomLupa = 3.0f; glfwSetInputMode(ventana, GLFW_CURSOR, GLFW_CURSOR_NORMAL); break; }
        }
        if ((nivel == NIVEL_LIBRE || nivel == NIVEL_MEDIO) && plantaAlta && recargaAnim < 0.0f && CercaDeRecarga()) { IniciarRecarga(); break; }
        if (nivel == NIVEL_EXPERTO && !tarjetaRecargada && !enFila && recargaAnim < 0.0f && CercaDeRecarga()) {
            // Se forma al final de la fila mas cercana
            int k = 0;
            for (size_t i = 1; i < filasRecarga.size(); ++i)
                if (DistanciaXZ(filasRecarga[i], posJugador) < DistanciaXZ(filasRecarga[k], posJugador)) k = (int)i;
            filaJugador = (filasRecarga.empty() || DistanciaXZ(filasRecarga[k], posJugador) > 4.5f) ? -1 : k; turnosPasados = 0;
            if (filaJugador >= 0 && colisionPlantaAlta.Libre(filasRecarga[k])) {
                posJugador = filasRecarga[k];
                camara.Position = posJugador + glm::vec3(0, ALTURA_OJOS, 0);
                glm::vec3 d = filaJugador >= 0 ? -filas[filaJugador].dir : -DirFila(posJugador);
                camara.Yaw = glm::degrees(std::atan2(d.z, d.x)); camara.Pitch = 0;
                camara.ProcessMouseMovement(0, 0);
            }
            enFila = true; tiempoFila = filaJugador >= 0 ? DURACION_FILA : TURNO_FILA;
            if (!filasEnMovimiento) {   // arrancan desfasadas para que no avancen todas a la vez
                filasEnMovimiento = true;
                for (size_t i = 0; i < filas.size(); ++i) filas[i].reloj = .4f + .55f * float((i * 5) % 7);
            }   // la animacion avanza la fila en ActualizarFila()
        }
        break;
    case GLFW_KEY_X:
        if (nivel == NIVEL_MEDIO && plantaAlta && pasoMedio < (int)puntosMedio.size()) break;
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
    case GLFW_KEY_H:   // muestra / oculta el panel de controles
        if (nivel == NIVEL_LIBRE) ayudaVisible = !ayudaVisible;
        break;
    }
}

void callbackBotonMouse(GLFWwindow* w, int boton, int accion, int mods)
{
    if (accion == GLFW_PRESS && nivel < 0 && boton == GLFW_MOUSE_BUTTON_LEFT) {   // clic en el menu
        if (menuHover >= 0 && menuElegido < 0) { menuElegido = menuHover; menuClicT = 0; }
        return;
    }
    if (nivel < 0) return;
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
    if (!mapaAbierto && recargaAnim < 0.0f && validaAnim < 0.0f && fundidoDireccion == 0 && (modoCamara == CAM_PRIMERA_PERSONA || modoCamara == CAM_LIBRE))
        camara.ProcessMouseMovement(dx, dy);
}

void callbackScroll(GLFWwindow* w, double dx, double dy)
{
    if (mapaAbierto) { zoomLupa = glm::clamp(zoomLupa + (float)dy * .5f, 1.5f, 8.0f); return; }
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

    const char* camaras[] = { "Primera persona", "Cámara libre", "Vista aérea", "Cinematica" };
    const char* trenes[] = { "en camino", "llegando", "abriendo puertas", "detenido", "cerrando puertas", "saliendo" };
    std::ostringstream t;
    t << (aBordo ? "Metro CDMX - Vagon 5" : plantaAlta ? "Metro CDMX - Planta alta" : "Metro CDMX - Andenes")
      << "  |  " << (int)(cuadros / acumulado) << " FPS  |  Cámara: " << camaras[modoCamara];
    if (!plantaAlta && !aBordo) t << "  |  Tren: " << trenes[estadoTren];
    if (avisoCargaPlanta) t << "  |  No se pudo cargar la otra planta";
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

// Ambiente en bucle: completo en hora pico (experto) y mas bajo en los demas modos
void ActualizarAmbiente()
{
    if (!motorSonido) return;
    float volumen = nivel < 0 ? 0.0f : nivel == NIVEL_EXPERTO ? 0.85f : (plantaAlta ? 0.0f : 0.06f);
    if (volumen <= 0.0f) {   // menu o planta alta en los modos 1, 2 y 4: en pausa (sin reiniciar el audio)
        if (sonidoAmbiente) sonidoAmbiente->setIsPaused(true);
        return;
    }
    if (!sonidoAmbiente) {
        if (!std::ifstream(RUTA_SONIDO_AMBIENTE).good()) return;
        sonidoAmbiente = motorSonido->play2D(RUTA_SONIDO_AMBIENTE, true, true, true);
        if (!sonidoAmbiente) return;
        sonidoAmbiente->setIsPaused(false);
    }
    sonidoAmbiente->setVolume(volumen);
    if (sonidoAmbiente->getIsPaused()) sonidoAmbiente->setIsPaused(false);
}

void ActualizarTono(float dt)
{
    if (!motorSonido) return;
    if (sonidoTono && (tonoSonando += dt) >= 1.0f) { sonidoTono->stop(); sonidoTono->drop(); sonidoTono = nullptr; }
    if (!aBordo || nivel < 0) { relojTono = 10.0f; return; }   // primer tono a los 10 s de subir
    if ((relojTono -= dt) > 0.0f) return;
    relojTono = 25.0f;
    if (sonidoTono || !std::ifstream(RUTA_SONIDO_TONO).good()) return;
    sonidoTono = motorSonido->play2D(RUTA_SONIDO_TONO, false, false, true);
    tonoSonando = 0.0f;
}

void ReproducirSonido(const char* ruta)
{
    if (!motorSonido) return;
    std::ifstream existe(ruta);
    if (existe.good()) motorSonido->play2D(ruta, false);
}
