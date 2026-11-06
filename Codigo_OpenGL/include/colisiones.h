/*
 * colisiones.h
 * ---------------------------------------------------------------------------
 * Colisiones sencillas para caminar en primera persona.
 *
 * El jugador es un circulo (vista desde arriba) que solo se mueve en X y Z.
 *   - ZonasCaminables : rectangulos donde SI se puede estar (los andenes).
 *   - Obstaculos      : cajas (AABB) que NO se pueden atravesar
 *                       (escaleras, oficinas, gabinetes, etc.).
 *
 * El movimiento se resuelve por ejes por separado (primero X, luego Z);
 * asi el jugador "se desliza" a lo largo de una pared en vez de quedarse
 * pegado.
 *
 *   BLOQUE A. Estructuras (Rect2D, Caja)
 *   BLOQUE B. Clase MundoColision
 * ---------------------------------------------------------------------------
 */
#ifndef COLISIONES_H
#define COLISIONES_H

#include <glm/glm.hpp>
#include <vector>

// ============================================================================
// BLOQUE A. Estructuras
// ============================================================================
struct Rect2D {  // rectangulo en el plano XZ
    float xMin, xMax, zMin, zMax;
    bool Contiene(float x, float z, float margen) const {
        return x >= xMin + margen && x <= xMax - margen && z >= zMin + margen && z <= zMax - margen;
    }
};

struct Caja {  // caja alineada a los ejes (AABB) en mundo
    glm::vec3 min, max;
};

// ============================================================================
// BLOQUE B. Clase MundoColision
// ============================================================================
class MundoColision {
public:
    std::vector<Rect2D> zonasCaminables;
    std::vector<Caja> obstaculos;
    float radioJugador = 0.30f;

    // Intenta mover de 'actual' a 'deseada'. Devuelve la posicion permitida.
    glm::vec3 Mover(const glm::vec3& actual, const glm::vec3& deseada) const {
        glm::vec3 p = actual;
        glm::vec3 intento = glm::vec3(deseada.x, p.y, p.z);  // eje X
        if (Libre(intento)) p = intento;
        intento = glm::vec3(p.x, p.y, deseada.z);            // eje Z
        if (Libre(intento)) p = intento;
        return p;
    }

    bool Libre(const glm::vec3& p) const {
        bool dentro = false;
        for (const Rect2D& r : zonasCaminables)
            if (r.Contiene(p.x, p.z, radioJugador)) { dentro = true; break; }
        if (!dentro) return false;
        for (const Caja& c : obstaculos)
            if (p.x + radioJugador > c.min.x && p.x - radioJugador < c.max.x &&
                p.z + radioJugador > c.min.z && p.z - radioJugador < c.max.z)
                return false;
        return true;
    }
};

#endif
