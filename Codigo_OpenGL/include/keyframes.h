/*
 * keyframes.h
 * ---------------------------------------------------------------------------
 * Animacion basada en cuadros clave (keyframes).
 *
 * Una "pista" guarda una lista de cuadros clave (tiempo, valor). Para un
 * tiempo t cualquiera se buscan los dos cuadros que lo rodean y se
 * interpola entre ellos:
 *
 *      u = (t - t0) / (t1 - t0)           (0..1 dentro del tramo)
 *      u = suavizar(u)                     (curva de aceleracion opcional)
 *      valor = mix(v0, v1, u)              (interpolacion lineal)
 *
 * Se usa con float (posicion del tren, apertura de puertas) y con vec3
 * (posicion de la camara cinematografica).
 *
 *   BLOQUE A. Curvas de suavizado (easing)
 *   BLOQUE B. Plantilla PistaKeyframes<T>
 * ---------------------------------------------------------------------------
 */
#ifndef KEYFRAMES_H
#define KEYFRAMES_H

#include <glm/glm.hpp>
#include <vector>

// ============================================================================
// BLOQUE A. Curvas de suavizado (easing)
// ============================================================================
enum Suavizado {
    LINEAL,         // velocidad constante
    ENTRADA,        // arranca lento y acelera (salida del tren)
    SALIDA,         // llega rapido y frena (llegada del tren)
    ENTRADA_SALIDA  // lento - rapido - lento (camara, puertas)
};

inline float AplicarSuavizado(float u, Suavizado s) {
    switch (s) {
        case ENTRADA:        return u * u;
        case SALIDA:         return 1.0f - (1.0f - u) * (1.0f - u);
        case ENTRADA_SALIDA: return u * u * (3.0f - 2.0f * u);  // smoothstep
        default:             return u;
    }
}

// ============================================================================
// BLOQUE B. Plantilla PistaKeyframes<T>
// ============================================================================
template <typename T>
class PistaKeyframes {
public:
    struct Cuadro {
        float tiempo;
        T valor;
        Suavizado suavizado;  // curva usada para llegar a este cuadro desde el anterior
    };

    void Agregar(float tiempo, const T& valor, Suavizado s = LINEAL) { cuadros.push_back({tiempo, valor, s}); }

    float Duracion() const { return cuadros.empty() ? 0.0f : cuadros.back().tiempo; }

    T Evaluar(float t) const {
        if (cuadros.empty()) return T();
        if (t <= cuadros.front().tiempo) return cuadros.front().valor;
        if (t >= cuadros.back().tiempo) return cuadros.back().valor;
        for (size_t i = 1; i < cuadros.size(); i++) {
            if (t <= cuadros[i].tiempo) {
                const Cuadro& a = cuadros[i - 1];
                const Cuadro& b = cuadros[i];
                float u = (t - a.tiempo) / (b.tiempo - a.tiempo);
                return glm::mix(a.valor, b.valor, AplicarSuavizado(u, b.suavizado));
            }
        }
        return cuadros.back().valor;
    }

    std::vector<Cuadro> cuadros;
};

// Reproductor sencillo: lleva el tiempo de una pista y sabe si ya termino
template <typename T>
struct ReproductorKeyframes {
    PistaKeyframes<T> pista;
    float tiempo = 0.0f;
    bool activo = false;

    void Iniciar() { tiempo = 0.0f; activo = true; }
    void Avanzar(float dt) {
        if (!activo) return;
        tiempo += dt;
        if (tiempo >= pista.Duracion()) { tiempo = pista.Duracion(); activo = false; }
    }
    T Valor() const { return pista.Evaluar(tiempo); }
    bool Terminado() const { return !activo && tiempo >= pista.Duracion(); }
};

#endif
