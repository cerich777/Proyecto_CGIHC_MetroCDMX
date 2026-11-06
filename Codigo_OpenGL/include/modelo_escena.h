/*
 * modelo_escena.h
 * ---------------------------------------------------------------------------
 * Carga un archivo FBX con Assimp CONSERVANDO LA JERARQUIA de nodos.
 *
 * Basado en model.h / mesh.h de las practicas del laboratorio
 * (stv-labopengl, Dr. Sergio Teodoro Vite). La diferencia principal es que
 * aqui cada nodo guarda su matriz local y su matriz global, para poder
 * animar partes de la escena (tren, vagones, puertas, ruedas) sin mover
 * todo lo demas. Eso es el "modelado jerarquico" visto en clase:
 *
 *      Global(hijo) = Global(padre) * AnimPadre * Local(hijo) * AnimLocal
 *
 *   - AnimPadre : transformacion extra en el espacio del padre
 *                 (traslaciones de puertas, mover objetos del usuario).
 *   - AnimLocal : transformacion extra en el espacio propio del nodo
 *                 (giro de una rueda sobre su eje).
 *
 * Contenido por bloques:
 *   BLOQUE A. Estructuras de datos (Vertice, Malla, MaterialEscena, Nodo)
 *   BLOQUE B. Carga del archivo con Assimp
 *   BLOQUE C. Materiales y texturas (incluye texturas embebidas en el FBX)
 *   BLOQUE D. Mallas (VAO / VBO / EBO)
 *   BLOQUE E. Jerarquia: matrices globales y cajas envolventes (AABB)
 *   BLOQUE F. Busqueda de nodos por nombre
 *   BLOQUE G. Lotes estaticos (batching) para dibujar rapido
 * ---------------------------------------------------------------------------
 */
#ifndef MODELO_ESCENA_H
#define MODELO_ESCENA_H

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <stb_image.h>

#include <algorithm>
#include <cfloat>
#include <iostream>
#include <map>
#include <string>
#include <tuple>
#include <vector>

// ============================================================================
// BLOQUE A. Estructuras de datos
// ============================================================================

// Mismo orden de atributos que las practicas: 0 = posicion, 1 = normal, 2 = uv
struct VerticeEscena {
    glm::vec3 posicion;
    glm::vec3 normal;
    glm::vec2 uv;
};
struct VerticePiel {
    glm::ivec4 articulaciones{0};
    glm::vec4 pesos{0.0f};
};

inline constexpr const char* ARTICULACIONES[] = {
    "J_Cadera", "J_Cabeza", "J_HombroI", "J_CodoI", "J_ManoI",
    "J_HombroD", "J_CodoD", "J_ManoD", "J_MusloI", "J_RodillaI", "J_MusloD", "J_RodillaD"
};
inline constexpr int NUM_ARTICULACIONES = 12;

// Tipo de material segun la rubrica (opacos, plasticos, traslucidos, metalicos)
enum TipoMaterial { MAT_OPACO = 0, MAT_PLASTICO = 1, MAT_METALICO = 2, MAT_TRASLUCIDO = 3, MAT_EMISIVO = 4, MAT_SENAL = 5, MAT_NEGRO = 6 };

struct MaterialEscena {
    std::string nombre;
    glm::vec3 difuso = glm::vec3(0.8f);
    glm::vec3 especular = glm::vec3(0.3f);
    glm::vec3 emision = glm::vec3(0.0f);
    float brillo = 32.0f;        // exponente especular de Phong
    float opacidad = 1.0f;       // 1 = opaco
    float reflectividad = 0.0f;  // cuanto refleja el cubemap (metales)
    GLuint texturaDifusa = 0;    // 0 = sin textura
    TipoMaterial tipo = MAT_OPACO;
    bool decal = false;          // grafico sobre la cara de un letrero
};

struct Malla {
    GLuint vao = 0, vbo = 0, ebo = 0;
    GLuint vboPiel = 0;
    GLsizei numIndices = 0;
    unsigned int material = 0;
    glm::vec3 aabbMin = glm::vec3(FLT_MAX);  // caja en coordenadas locales de la malla
    glm::vec3 aabbMax = glm::vec3(-FLT_MAX);
    bool translucida = false;
};

struct Nodo {
    std::string nombre;
    int padre = -1;
    std::vector<int> hijos;
    std::vector<unsigned int> mallas;

    glm::mat4 local = glm::mat4(1.0f);      // transformacion original del archivo
    glm::mat4 animPadre = glm::mat4(1.0f);  // animacion en espacio del padre
    glm::mat4 animLocal = glm::mat4(1.0f);  // animacion en espacio propio
    glm::mat4 global = glm::mat4(1.0f);     // resultado final (se recalcula cada cuadro)

    bool visible = true;
    glm::vec3 emisionExtra = glm::vec3(0.0f);  // brillo adicional (alarmas, resaltado)
    int idInteractivo = -1;                    // >= 0 si el usuario puede tomarlo

    // Caja envolvente en coordenadas de mundo (incluye a los hijos)
    glm::vec3 aabbMin = glm::vec3(FLT_MAX);
    glm::vec3 aabbMax = glm::vec3(-FLT_MAX);
};

// Lote: muchas mallas que NO se mueven entre si, unidas en un solo VAO por
// material. Se dibujan con una sola llamada en vez de cientos.
struct Lote {
    GLuint vao = 0, vbo = 0, ebo = 0;
    GLsizei numIndices = 0;
    unsigned int material = 0;
    int nodoReferencia = -1;   // -1 = coordenadas de mundo; si no, se multiplica por global de ese nodo
    glm::vec3 aabbMin = glm::vec3(FLT_MAX);  // en el espacio del nodo de referencia
    glm::vec3 aabbMax = glm::vec3(-FLT_MAX);
    bool translucido = false;
};

class ModeloEscena {
public:
    std::vector<Nodo> nodos;            // nodos[0] es la raiz
    std::vector<Malla> mallas;
    std::vector<MaterialEscena> materiales;
    std::vector<Lote> lotes;
    std::vector<char> enLote;           // 1 si el nodo ya se dibuja dentro de un lote
    std::string directorio;
    bool cargado = false;

    // ========================================================================
    // BLOQUE B. Carga del archivo con Assimp
    // ========================================================================
    explicit ModeloEscena(const std::string& ruta) {
        Assimp::Importer importador;
        // Mismos pasos de post-proceso que las practicas, mas normales suaves si faltan
        const aiScene* escena = importador.ReadFile(ruta,
            aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_GenSmoothNormals |
            aiProcess_JoinIdenticalVertices);

        if (!escena || (escena->mFlags & AI_SCENE_FLAGS_INCOMPLETE) || !escena->mRootNode) {
            std::cout << "ERROR::ASSIMP:: " << importador.GetErrorString() << std::endl;
            return;
        }
        size_t barra = ruta.find_last_of("/\\");
        directorio = (barra == std::string::npos) ? "." : ruta.substr(0, barra);

        for (unsigned int i = 0; i < escena->mNumMaterials; i++)
            materiales.push_back(cargarMaterial(escena, escena->mMaterials[i]));
        for (unsigned int i = 0; i < escena->mNumMeshes; i++)
            mallas.push_back(crearMalla(escena->mMeshes[i]));

        nodos.reserve(4096);
        leerNodo(escena->mRootNode, -1);
        ActualizarJerarquia();
        cargado = true;

        std::cout << "> Modelo: " << ruta << "\n  nodos=" << nodos.size() << " mallas=" << mallas.size()
                  << " materiales=" << materiales.size() << " texturas=" << cacheTexturas.size() << std::endl;
    }

    // ========================================================================
    // BLOQUE E. Recursos, jerarquia: matrices globales y cajas envolventes
    // ========================================================================
    ~ModeloEscena() {
        auto liberar = [](const auto& m) {
            glDeleteVertexArrays(1, &m.vao);
            glDeleteBuffers(1, &m.vbo);
            glDeleteBuffers(1, &m.ebo);
        };
        for (const Malla& m : mallas) {
            liberar(m);
            if (m.vboPiel) glDeleteBuffers(1,&m.vboPiel);
        }
        for (const Lote& l : lotes) liberar(l);
        for (const auto& t : cacheTexturas) if (t.second) glDeleteTextures(1, &t.second);
    }

    ModeloEscena(const ModeloEscena&) = delete;
    ModeloEscena& operator=(const ModeloEscena&) = delete;

    // Geometria auxiliar para cerrar las vistas entre plantas, sin texturas.
    // Se agrega antes de construir lotes para dibujarla con el resto de la escena.
    void AgregarBloqueNegro(const std::string& nombre, const glm::vec3& mn, const glm::vec3& mx) {
        if (nodos.empty()) return;
        if (materiales.empty() || materiales.back().tipo != MAT_NEGRO) {
            MaterialEscena negro;
            negro.nombre = "Fondo negro";
            negro.tipo = MAT_NEGRO;
            materiales.push_back(negro);
        }
        std::vector<VerticeEscena> v;
        for (int i = 0; i < 8; ++i)
            v.push_back({ { (i & 1) ? mx.x : mn.x, (i & 2) ? mx.y : mn.y, (i & 4) ? mx.z : mn.z },
                          { 0, 1, 0 }, { 0, 0 } });
        std::vector<unsigned int> idx = {
            0, 2, 1, 1, 2, 3, 4, 5, 6, 5, 7, 6, 0, 1, 4, 1, 5, 4,
            2, 6, 3, 3, 6, 7, 0, 4, 2, 2, 4, 6, 1, 3, 5, 3, 7, 5
        };
        Malla m;
        m.material = (unsigned int)materiales.size() - 1;
        m.aabbMin = mn; m.aabbMax = mx;
        m.numIndices = (GLsizei)idx.size();
        SubirBuffers(v, idx, m.vao, m.vbo, m.ebo);
        Nodo n;
        n.nombre = nombre;
        n.padre = 0;
        n.local = glm::inverse(nodos[0].global);   // las coordenadas del bloque ya estan en mundo
        n.mallas.push_back((unsigned int)mallas.size());
        nodos[0].hijos.push_back((int)nodos.size());
        nodos.push_back(std::move(n));
        mallas.push_back(m);
        verticesCpu.push_back(std::move(v));
        indicesCpu.push_back(std::move(idx));
    }

    // Las letras de algunos FBX estan pegadas al panel (o dentro de el).
    // Separa los dos juegos de letras hacia sus respectivas caras.
    void SepararLetras(unsigned int mi, const glm::mat4& modelo, float centroZ) {
        Malla& m = mallas[mi];
        auto& vertices = verticesCpu[mi];
        glm::vec3 paso = glm::vec3(glm::inverse(modelo) * glm::vec4(0, 0, 0.006f, 0));
        m.aabbMin = glm::vec3(FLT_MAX);
        m.aabbMax = glm::vec3(-FLT_MAX);
        for (VerticeEscena& v : vertices) {
            const float z = (modelo * glm::vec4(v.posicion, 1.0f)).z;
            v.posicion += z < centroZ ? -paso : paso;
            m.aabbMin = glm::min(m.aabbMin, v.posicion);
            m.aabbMax = glm::max(m.aabbMax, v.posicion);
        }
        glBindBuffer(GL_ARRAY_BUFFER, m.vbo);
        glBufferSubData(GL_ARRAY_BUFFER, 0, vertices.size() * sizeof(VerticeEscena), vertices.data());
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    // FBX asigna el primer material del letrero a las letras sin material.
    // En esas mallas mixtas los iconos son unos pocos triangulos grandes,
    // mientras que la tipografia esta formada por muchos triangulos pequenos.
    void SepararLetrasDeIcono(unsigned int mi, const glm::mat4& modelo, float centroZ, bool claras) {
        const auto& v = verticesCpu[mi];
        std::vector<unsigned int> icono, letras;
        const auto& idx = indicesCpu[mi];
        for (size_t i = 0; i + 2 < idx.size(); i += 3) {
            const auto& a = v[idx[i]];
            const auto& b = v[idx[i + 1]];
            const auto& c = v[idx[i + 2]];
            const glm::vec3 ab = glm::mat3(modelo) * (b.posicion - a.posicion);
            const glm::vec3 ac = glm::mat3(modelo) * (c.posicion - a.posicion);
            const glm::vec2 uv1 = b.uv - a.uv, uv2 = c.uv - a.uv;
            const bool esIcono = glm::length(glm::cross(ab, ac)) > 0.028f &&
                                 std::fabs(uv1.x * uv2.y - uv1.y * uv2.x) > 0.12f;
            auto& destino = esIcono ? icono : letras;
            destino.insert(destino.end(), idx.begin() + i, idx.begin() + i + 3);
        }
        if (icono.empty() || icono.size() > 24 || letras.size() < 600) return;

        MaterialEscena tinta;
        tinta.nombre = "Letras del letrero";
        tinta.difuso = glm::vec3(claras ? 0.92f : 0.025f);
        tinta.tipo = MAT_SENAL;
        Malla texto;
        texto.material = (unsigned int)materiales.size();
        materiales.push_back(tinta);
        texto.numIndices = (GLsizei)letras.size();
        SubirBuffers(v, letras, texto.vao, texto.vbo, texto.ebo);

        Malla& original = mallas[mi];
        original.numIndices = (GLsizei)icono.size();
        glBindVertexArray(original.vao);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, icono.size() * sizeof(unsigned int), icono.data(), GL_STATIC_DRAW);
        glBindVertexArray(0);
        indicesCpu[mi] = std::move(icono);

        const unsigned int nuevo = (unsigned int)mallas.size();
        mallas.push_back(texto);
        // Guardar una copia antes de que crezca el vector que contiene v.
        auto verticesTexto = v;
        verticesCpu.push_back(std::move(verticesTexto));
        indicesCpu.push_back(std::move(letras));
        SepararLetras(nuevo, modelo, centroZ);
        for (Nodo& n : nodos)
            if (std::find(n.mallas.begin(), n.mallas.end(), mi) != n.mallas.end()) n.mallas.push_back(nuevo);
    }

    // Recorre el arbol desde la raiz. Los padres siempre tienen indice menor
    // que sus hijos (se leyeron en ese orden), asi que basta un ciclo.
    void ActualizarJerarquia() {
        for (size_t i = 0; i < nodos.size(); i++) {
            Nodo& n = nodos[i];
            glm::mat4 padre = (n.padre >= 0) ? nodos[n.padre].global : glm::mat4(1.0f);
            n.global = padre * n.animPadre * n.local * n.animLocal;
        }
    }

    // Calcula la caja de cada nodo en mundo, sumando la de sus hijos.
    // Se llama al cargar y despues de mover objetos estaticos.
    void CalcularCajas() {
        for (Nodo& n : nodos) { n.aabbMin = glm::vec3(FLT_MAX); n.aabbMax = glm::vec3(-FLT_MAX); }
        for (int i = (int)nodos.size() - 1; i >= 0; i--) {
            Nodo& n = nodos[i];
            for (unsigned int m : n.mallas) {
                glm::vec3 mn, mx;
                CajaMundo(mallas[m], n.global, mn, mx);
                n.aabbMin = glm::min(n.aabbMin, mn);
                n.aabbMax = glm::max(n.aabbMax, mx);
            }
            if (n.padre >= 0) {
                Nodo& p = nodos[n.padre];
                p.aabbMin = glm::min(p.aabbMin, n.aabbMin);
                p.aabbMax = glm::max(p.aabbMax, n.aabbMax);
            }
        }
    }

    // Transforma las 8 esquinas de la caja local de una malla
    static void CajaMundo(const Malla& m, const glm::mat4& M, glm::vec3& mn, glm::vec3& mx) {
        mn = glm::vec3(FLT_MAX); mx = glm::vec3(-FLT_MAX);
        for (int c = 0; c < 8; c++) {
            glm::vec3 p((c & 1) ? m.aabbMax.x : m.aabbMin.x,
                        (c & 2) ? m.aabbMax.y : m.aabbMin.y,
                        (c & 4) ? m.aabbMax.z : m.aabbMin.z);
            glm::vec3 w = glm::vec3(M * glm::vec4(p, 1.0f));
            mn = glm::min(mn, w); mx = glm::max(mx, w);
        }
    }

    // Recorre un sub-arbol (nodo y todos sus descendientes)
    template <typename F> void RecorrerSubarbol(int raiz, F funcion) {
        funcion(raiz);
        for (int h : nodos[raiz].hijos) RecorrerSubarbol(h, funcion);
    }

    // ========================================================================
    // BLOQUE F. Busqueda de nodos por nombre
    // ========================================================================
    int Buscar(const std::string& nombre) const {
        for (size_t i = 0; i < nodos.size(); i++)
            if (nodos[i].nombre == nombre) return (int)i;
        return -1;
    }

    // Todos los nodos cuyo nombre empieza con un prefijo (opcional: solo hijos directos de la raiz)
    std::vector<int> BuscarPrefijo(const std::string& prefijo, bool soloNivelSuperior = false) const {
        std::vector<int> r;
        for (size_t i = 1; i < nodos.size(); i++) {
            if (soloNivelSuperior && nodos[i].padre != 0) continue;
            if (nodos[i].nombre.compare(0, prefijo.size(), prefijo) == 0) r.push_back((int)i);
        }
        return r;
    }

    // Nodos cuyo nombre contiene un texto
    std::vector<int> BuscarContiene(const std::string& texto) const {
        std::vector<int> r;
        for (size_t i = 1; i < nodos.size(); i++)
            if (nodos[i].nombre.find(texto) != std::string::npos) r.push_back((int)i);
        return r;
    }

    // ========================================================================
    // BLOQUE G. Lotes estaticos (batching)
    // ------------------------------------------------------------------------
    //  dinamico[i] = 1  -> el nodo i (y sus hijos) se mueve por su cuenta:
    //                      NO se agrupa y se dibuja suelto.
    //  referencias      -> nodos que se mueven "en bloque" (ej. TREN_RAIZ):
    //                      lo que cuelga de ellos se agrupa en su espacio.
    //  tamSeccion       -> se corta en secciones a lo largo de Z para poder
    //                      descartar las que no se ven (frustum culling).
    // ========================================================================
    void ConstruirLotes(const std::vector<char>& dinamico, const std::vector<int>& referencias, float tamSeccion)
    {
        // 1) Materiales equivalentes (mismo color, textura y tipo) se unen
        std::vector<unsigned int> canonico(materiales.size());
        for (size_t i = 0; i < materiales.size(); i++) {
            canonico[i] = (unsigned int)i;
            for (size_t j = 0; j < i; j++)
                if (MismoMaterial(materiales[i], materiales[j])) { canonico[i] = canonico[j]; break; }
        }

        // 2) A que referencia pertenece cada nodo y si queda excluido
        std::vector<int> ref(nodos.size(), -1);
        std::vector<char> excluido(nodos.size(), 0);
        for (size_t i = 0; i < nodos.size(); i++) {
            int p = nodos[i].padre;
            ref[i] = (p >= 0) ? ref[p] : -1;
            excluido[i] = ((p >= 0 && excluido[p]) || dinamico[i]) ? 1 : 0;
            if (std::find(referencias.begin(), referencias.end(), (int)i) != referencias.end()) ref[i] = (int)i;
        }

        // 3) Juntar vertices ya transformados por (referencia, material, seccion)
        struct Acumulado { std::vector<VerticeEscena> v; std::vector<unsigned int> idx; Lote lote; };
        std::map<std::tuple<int, unsigned int, int>, Acumulado> grupos;
        enLote.assign(nodos.size(), 0);
        for (size_t i = 0; i < nodos.size(); i++) {
            if (excluido[i] || nodos[i].mallas.empty()) continue;
            glm::mat4 M = nodos[i].global;
            if (ref[i] >= 0) M = glm::inverse(nodos[ref[i]].global) * M;
            glm::mat3 N = glm::transpose(glm::inverse(glm::mat3(M)));
            int seccion = (int)std::floor(glm::vec3(M[3]).z / tamSeccion);
            for (unsigned int mi : nodos[i].mallas) {
                const Malla& m = mallas[mi];
                Acumulado& a = grupos[std::make_tuple(ref[i], canonico[m.material], seccion)];
                a.lote.material = canonico[m.material];
                a.lote.nodoReferencia = ref[i];
                a.lote.translucido = m.translucida;
                unsigned int base = (unsigned int)a.v.size();
                for (const VerticeEscena& vo : verticesCpu[mi]) {
                    VerticeEscena v;
                    v.posicion = glm::vec3(M * glm::vec4(vo.posicion, 1.0f));
                    v.normal = glm::normalize(N * vo.normal);
                    v.uv = vo.uv;
                    a.lote.aabbMin = glm::min(a.lote.aabbMin, v.posicion);
                    a.lote.aabbMax = glm::max(a.lote.aabbMax, v.posicion);
                    a.v.push_back(v);
                }
                for (unsigned int k : indicesCpu[mi]) a.idx.push_back(base + k);
            }
            enLote[i] = 1;
        }

        // 4) Subir cada grupo a la tarjeta de video
        size_t sueltas = 0;
        for (const Nodo& nd : nodos) sueltas += nd.mallas.size();
        for (auto& g : grupos) {
            Lote L = g.second.lote;
            L.numIndices = (GLsizei)g.second.idx.size();
            SubirBuffers(g.second.v, g.second.idx, L.vao, L.vbo, L.ebo);
            lotes.push_back(L);
        }
        std::cout << "> Lotes estaticos: " << lotes.size() << " (de " << sueltas << " mallas)" << std::endl;
    }

private:
    std::map<std::string, GLuint> cacheTexturas;
    std::vector<std::vector<VerticeEscena>> verticesCpu;   // copia en RAM para armar los lotes
    std::vector<std::vector<unsigned int>> indicesCpu;

    static bool MismoMaterial(const MaterialEscena& a, const MaterialEscena& b) {
        auto igual = [](const glm::vec3& x, const glm::vec3& y) { return glm::length(x - y) < 0.01f; };
        return a.tipo == b.tipo && a.decal == b.decal && a.texturaDifusa == b.texturaDifusa && igual(a.difuso, b.difuso) &&
               igual(a.especular, b.especular) && igual(a.emision, b.emision) &&
               std::fabs(a.brillo - b.brillo) < 1.0f && std::fabs(a.opacidad - b.opacidad) < 0.01f;
    }

    static void SubirBuffers(const std::vector<VerticeEscena>& v, const std::vector<unsigned int>& idx,
                             GLuint& vao, GLuint& vbo, GLuint& ebo) {
        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);
        glGenBuffers(1, &ebo);
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, v.size() * sizeof(VerticeEscena), v.data(), GL_STATIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, idx.size() * sizeof(unsigned int), idx.data(), GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(VerticeEscena), (void*)offsetof(VerticeEscena, posicion));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(VerticeEscena), (void*)offsetof(VerticeEscena, normal));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(VerticeEscena), (void*)offsetof(VerticeEscena, uv));
        glBindVertexArray(0);
    }  // evita cargar la misma imagen dos veces

    void leerNodo(const aiNode* nodoAi, int padre) {
        Nodo n;
        n.nombre = nodoAi->mName.C_Str();
        n.padre = padre;
        n.local = aConvertir(nodoAi->mTransformation);
        for (unsigned int i = 0; i < nodoAi->mNumMeshes; i++) n.mallas.push_back(nodoAi->mMeshes[i]);
        int indice = (int)nodos.size();
        nodos.push_back(n);
        if (padre >= 0) nodos[padre].hijos.push_back(indice);
        for (unsigned int i = 0; i < nodoAi->mNumChildren; i++) leerNodo(nodoAi->mChildren[i], indice);
    }

    static glm::mat4 aConvertir(const aiMatrix4x4& m) {
        // Assimp guarda por filas y GLM por columnas
        return glm::transpose(glm::make_mat4(&m.a1));
    }

    // ========================================================================
    // BLOQUE C. Materiales y texturas
    // ========================================================================
    MaterialEscena cargarMaterial(const aiScene* escena, aiMaterial* matAi) {
        MaterialEscena mat;
        aiString nombre;
        matAi->Get(AI_MATKEY_NAME, nombre);
        mat.nombre = nombre.C_Str();

        aiColor3D color;
        if (matAi->Get(AI_MATKEY_COLOR_DIFFUSE, color) == AI_SUCCESS) mat.difuso = glm::vec3(color.r, color.g, color.b);
        if (matAi->Get(AI_MATKEY_COLOR_SPECULAR, color) == AI_SUCCESS) mat.especular = glm::vec3(color.r, color.g, color.b);
        if (matAi->Get(AI_MATKEY_COLOR_EMISSIVE, color) == AI_SUCCESS) mat.emision = glm::vec3(color.r, color.g, color.b);
        matAi->Get(AI_MATKEY_SHININESS, mat.brillo);
        matAi->Get(AI_MATKEY_OPACITY, mat.opacidad);
        mat.brillo = glm::clamp(mat.brillo, 4.0f, 256.0f);

        if (matAi->GetTextureCount(aiTextureType_DIFFUSE) > 0) {
            aiString ruta;
            matAi->GetTexture(aiTextureType_DIFFUSE, 0, &ruta);
            mat.texturaDifusa = cargarTextura(escena, ruta.C_Str());
            if (mat.texturaDifusa) mat.difuso = glm::vec3(1.0f);  // la textura ya trae el color
        }

        // Clasificacion por nombre y propiedades (se usa para elegir el sombreado)
        std::string n = minusculas(mat.nombre);
        auto tiene = [&](const char* t) { return n.find(t) != std::string::npos; };
        if (mat.opacidad < 0.99f || tiene("vidrio") || tiene("cristal")) {
            mat.tipo = MAT_TRASLUCIDO;
            mat.opacidad = std::min(mat.opacidad, 0.45f);
            mat.reflectividad = 0.6f;
        } else if (glm::length(mat.emision) > 0.5f || tiene("luz")) {
            mat.tipo = MAT_EMISIVO;
        } else if (tiene("metal") || tiene("acero") || tiene("aluminio") || tiene("inox") ||
                   tiene("galvan") || tiene("soporte_letrero") || tiene("riel")) {
            mat.tipo = MAT_METALICO;
            mat.reflectividad = 0.35f;
            mat.brillo = std::max(mat.brillo, 64.0f);
        } else if (tiene("plastic") || tiene("carcasa") || tiene("cono")) {
            mat.tipo = MAT_PLASTICO;
            mat.especular = glm::max(mat.especular, glm::vec3(0.5f));
            mat.brillo = std::max(mat.brillo, 48.0f);
        }
        return mat;
    }

    GLuint cargarTextura(const aiScene* escena, const std::string& ruta) {
        auto it = cacheTexturas.find(ruta);
        if (it != cacheTexturas.end()) return it->second;

        int ancho = 0, alto = 0, canales = 0;
        unsigned char* datos = nullptr;
        bool liberarConStb = true;

        // 1) Textura embebida dentro del FBX (opcion "Embed Textures" de Blender)
        const aiTexture* emb = escena->GetEmbeddedTexture(ruta.c_str());
        if (!emb) {
            // Blender incrusta una sola copia cuando varias imagenes tienen el mismo
            // nombre de archivo: se busca solo por el nombre, sin la carpeta
            std::string nombre = ruta.substr(ruta.find_last_of("/\\") + 1);
            for (unsigned int i = 0; i < escena->mNumTextures && !emb; i++) {
                std::string r = escena->mTextures[i]->mFilename.C_Str();
                if (r.substr(r.find_last_of("/\\") + 1) == nombre) emb = escena->mTextures[i];
            }
        }
        if (emb) {
            if (emb->mHeight == 0) {  // comprimida (png / jpg)
                datos = stbi_load_from_memory(reinterpret_cast<unsigned char*>(emb->pcData), (int)emb->mWidth,
                                              &ancho, &alto, &canales, 4);
            } else {  // sin comprimir (ARGB8888)
                ancho = emb->mWidth; alto = emb->mHeight; canales = 4; liberarConStb = false;
                datos = new unsigned char[ancho * alto * 4];
                for (int i = 0; i < ancho * alto; i++) {
                    datos[i * 4 + 0] = emb->pcData[i].r; datos[i * 4 + 1] = emb->pcData[i].g;
                    datos[i * 4 + 2] = emb->pcData[i].b; datos[i * 4 + 3] = emb->pcData[i].a;
                }
            }
        }
        // 2) Archivo externo junto al modelo
        if (!datos) {
            std::string completa = directorio + "/" + ruta;
            datos = stbi_load(completa.c_str(), &ancho, &alto, &canales, 4);
        }
        if (!datos) {
            std::cout << "  Textura no encontrada: " << ruta << std::endl;
            cacheTexturas[ruta] = 0;
            return 0;
        }

        GLuint id;
        glGenTextures(1, &id);
        glBindTexture(GL_TEXTURE_2D, id);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, ancho, alto, 0, GL_RGBA, GL_UNSIGNED_BYTE, datos);
        glGenerateMipmap(GL_TEXTURE_2D);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        if (liberarConStb) stbi_image_free(datos); else delete[] datos;
        cacheTexturas[ruta] = id;
        return id;
    }

    // ========================================================================
    // BLOQUE D. Mallas (VAO / VBO / EBO), igual que mesh.h del laboratorio
    // ========================================================================
    Malla crearMalla(const aiMesh* m) {
        Malla malla;
        malla.material = m->mMaterialIndex;
        malla.translucida = materiales[m->mMaterialIndex].tipo == MAT_TRASLUCIDO;

        std::vector<VerticeEscena> vertices(m->mNumVertices);
        for (unsigned int i = 0; i < m->mNumVertices; i++) {
            VerticeEscena& v = vertices[i];
            v.posicion = glm::vec3(m->mVertices[i].x, m->mVertices[i].y, m->mVertices[i].z);
            v.normal = m->HasNormals() ? glm::vec3(m->mNormals[i].x, m->mNormals[i].y, m->mNormals[i].z) : glm::vec3(0, 1, 0);
            v.uv = m->mTextureCoords[0] ? glm::vec2(m->mTextureCoords[0][i].x, m->mTextureCoords[0][i].y) : glm::vec2(0.0f);
            malla.aabbMin = glm::min(malla.aabbMin, v.posicion);
            malla.aabbMax = glm::max(malla.aabbMax, v.posicion);
        }
        std::vector<unsigned int> indices;
        indices.reserve(m->mNumFaces * 3);
        for (unsigned int i = 0; i < m->mNumFaces; i++)
            for (unsigned int j = 0; j < m->mFaces[i].mNumIndices; j++) indices.push_back(m->mFaces[i].mIndices[j]);
        malla.numIndices = (GLsizei)indices.size();
        SubirBuffers(vertices, indices, malla.vao, malla.vbo, malla.ebo);
        // Mantener el formato compacto de andenes, edificios y tren.
        std::vector<VerticePiel> piel(m->mNumBones ? m->mNumVertices : 0);
        for (unsigned int i = 0; i < m->mNumBones; ++i) {
            const aiBone& hueso = *m->mBones[i];
            int j = 0;
            while (j < NUM_ARTICULACIONES && std::string(hueso.mName.C_Str()) != ARTICULACIONES[j]) ++j;
            if (j == NUM_ARTICULACIONES) continue;
            for (unsigned int k = 0; k < hueso.mNumWeights; ++k) {
                const aiVertexWeight& w = hueso.mWeights[k];
                VerticePiel& v = piel[w.mVertexId];
                for (int canal = 0; canal < 4; ++canal) if (v.pesos[canal] == 0.0f) {
                    v.articulaciones[canal] = j;
                    v.pesos[canal] = w.mWeight;
                    break;
                }
            }
        }
        if (!piel.empty()) {
            glBindVertexArray(malla.vao);
            glGenBuffers(1,&malla.vboPiel);
            glBindBuffer(GL_ARRAY_BUFFER,malla.vboPiel);
            glBufferData(GL_ARRAY_BUFFER,piel.size()*sizeof(VerticePiel),piel.data(),GL_STATIC_DRAW);
            glEnableVertexAttribArray(3);
            glVertexAttribIPointer(3,4,GL_INT,sizeof(VerticePiel),(void*)offsetof(VerticePiel,articulaciones));
            glEnableVertexAttribArray(4);
            glVertexAttribPointer(4,4,GL_FLOAT,GL_FALSE,sizeof(VerticePiel),(void*)offsetof(VerticePiel,pesos));
            glBindVertexArray(0);
        }
        verticesCpu.push_back(std::move(vertices));
        indicesCpu.push_back(std::move(indices));
        return malla;
    }

    static std::string minusculas(std::string s) {
        std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return (char)std::tolower(c); });
        return s;
    }
};

#endif
