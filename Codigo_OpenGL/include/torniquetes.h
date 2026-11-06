/*
 * torniquetes.h - hojas de la planta alta, apertura por proximidad y colision.
 * Cada raiz TORNIQUETE_ agrupa dos hojas. El eje de cada hoja se toma de su
 * cilindro de acero en el FBX; los gabinetes se quedan estaticos.
 */
#ifndef TORNIQUETES_H
#define TORNIQUETES_H

#include <array>
#include <colisiones.h>
#include <modelo_escena.h>

struct HojaTorniquete {
    int nodo = -1;
    glm::vec3 bisagra{0.0f};
    Caja cerrada{};
    size_t colision = 0;
};

struct Torniquete {
    std::array<HojaTorniquete, 2> hojas;  // izquierda y derecha en el eje X
    glm::vec2 centro{0.0f};             // XZ del carril
    float semiancho = 0.0f;
    float avance = 0.0f;                // 0 = cerrado, 1 = abierto
    float pausa = 0.0f;
    float sentido = 1.0f;
};

class Torniquetes {
public:
    std::vector<Torniquete> carriles;

    // Devuelve los nodos de hojas: tienen su propio lote y su propia colision.
    std::vector<int> Preparar(ModeloEscena& escena, MundoColision& mundo) {
        carriles.clear();
        std::vector<int> referencias;
        for (int raiz : escena.BuscarPrefijo("TORNIQUETE_")) {
            std::array<int, 2> nodos{ -1, -1 };
            for (int i : escena.nodos[raiz].hijos) {
                const auto& nombre = escena.nodos[i].nombre;
                if (nombre.compare(0, 9, "Puerta_1_") == 0) nodos[0] = i;
                if (nombre.compare(0, 9, "Puerta_2_") == 0) nodos[1] = i;
            }
            if (nodos[0] < 0 || nodos[1] < 0) continue;
            if (escena.nodos[nodos[0]].aabbMin.x > escena.nodos[nodos[1]].aabbMin.x)
                std::swap(nodos[0], nodos[1]);
            const Nodo& izq = escena.nodos[nodos[0]];
            const Nodo& der = escena.nodos[nodos[1]];
            if (izq.aabbMax.x <= izq.aabbMin.x || der.aabbMax.x <= der.aabbMin.x) continue;

            Torniquete t;
            t.centro = { (izq.aabbMin.x + der.aabbMax.x) * 0.5f,
                         (izq.aabbMin.z + der.aabbMax.z) * 0.5f };
            t.semiancho = (der.aabbMax.x - izq.aabbMin.x) * 0.5f;
            for (size_t h = 0; h < nodos.size(); ++h) {
                HojaTorniquete& hoja = t.hojas[h];
                hoja.nodo = nodos[h];
                const Nodo& n = escena.nodos[hoja.nodo];
                hoja.cerrada = { n.aabbMin, n.aabbMax };
                hoja.bisagra = { h == 0 ? n.aabbMin.x + 0.025f : n.aabbMax.x - 0.025f,
                                 n.aabbMin.y, (n.aabbMin.z + n.aabbMax.z) * 0.5f };
                for (unsigned int mi : n.mallas) {
                    const Malla& m = escena.mallas[mi];
                    if (escena.materiales[m.material].nombre.find("Acero_Superior") != 0) continue;
                    glm::vec3 mn, mx;
                    ModeloEscena::CajaMundo(m, n.global, mn, mx);
                    if (mx.x - mn.x < 0.09f && mx.z - mn.z < 0.09f) {
                        hoja.bisagra.x = (mn.x + mx.x) * 0.5f;
                        hoja.bisagra.z = (mn.z + mx.z) * 0.5f;
                        break;
                    }
                }
                hoja.colision = mundo.obstaculos.size();
                mundo.obstaculos.push_back(hoja.cerrada);
                referencias.push_back(hoja.nodo);
            }
            carriles.push_back(t);
        }
        return referencias;
    }

    void Actualizar(ModeloEscena& escena, MundoColision& mundo, const glm::vec3& jugador, float dt,
                    const std::vector<glm::vec3>& pasajeros = {}) {
        for (Torniquete& t : carriles) {
            bool detectado = false;
            float dz = 1000.0f;
            auto detectar = [&](const glm::vec3& persona) {
                const float dx = std::fabs(persona.x - t.centro.x);
                const float distancia = persona.z - t.centro.y;
                const bool cerca = dx <= t.semiancho + 0.12f && std::fabs(distancia) <= 2.60f;
                const bool cruzando = t.avance > 0.0f && dx <= t.semiancho + mundo.radioJugador &&
                                      std::fabs(distancia) <= 0.90f;
                if (cerca || cruzando) {
                    detectado = true;
                    if (std::fabs(distancia) < std::fabs(dz)) dz = distancia;
                }
            };
            detectar(jugador);
            for (const auto& p : pasajeros) detectar(p);
            if (detectado) {
                if (t.avance == 0.0f) t.sentido = dz <= 0.0f ? 1.0f : -1.0f;
                t.pausa = 0.45f;
            } else {
                t.pausa = std::max(0.0f, t.pausa - dt);
            }

            const float anterior = t.avance;
            const bool abrir = detectado || t.pausa > 0.0f;
            t.avance = glm::clamp(t.avance + (abrir ? dt / 0.36f : -dt / 0.52f), 0.0f, 1.0f);
            if (t.avance == anterior) continue;
            const float progreso = t.avance * t.avance * (3.0f - 2.0f * t.avance);
            for (size_t h = 0; h < t.hojas.size(); ++h) {
                const HojaTorniquete& hoja = t.hojas[h];
                const float giro = glm::radians((h == 0 ? -90.0f : 90.0f) * t.sentido * progreso);
                glm::mat4 transformacion = glm::translate(glm::mat4(1.0f), hoja.bisagra);
                transformacion = glm::rotate(transformacion, giro, glm::vec3(0, 1, 0));
                transformacion = glm::translate(transformacion, -hoja.bisagra);
                Nodo& n = escena.nodos[hoja.nodo];
                const glm::mat4& padre = escena.nodos[n.padre].global;
                n.animPadre = glm::inverse(padre) * transformacion * padre;

                // La caja sigue a la hoja durante todo el giro, tambien al cerrar.
                Caja& caja = mundo.obstaculos[hoja.colision];
                caja = { glm::vec3(FLT_MAX), glm::vec3(-FLT_MAX) };
                for (int e = 0; e < 8; ++e) {
                    const Caja& c = hoja.cerrada;
                    glm::vec4 p((e & 1) ? c.max.x : c.min.x, (e & 2) ? c.max.y : c.min.y,
                                (e & 4) ? c.max.z : c.min.z, 1.0f);
                    glm::vec3 q = glm::vec3(transformacion * p);
                    caja.min = glm::min(caja.min, q);
                    caja.max = glm::max(caja.max, q);
                }
            }
        }
    }
};

#endif

