#ifndef PASAJEROS_H
#define PASAJEROS_H

#include "modelo_escena.h"
#include "colisiones.h"
#include <glm/gtx/quaternion.hpp>
#include <array>
#include <queue>
#include <memory>

enum TipoPasajero { ADULTO, MUJER, NINO };
enum AccionPasajero { QUIETO, CAMINAR, RECARGAR, TAQUILLA, SENTARSE, AGARRARSE, BAJAR_ESCALERA };
enum ViajePasajero { PASEO, HACIA_PUERTA, ESPERA_PUERTA, SUBIENDO, VIAJANDO };

struct ParadaPasajero {
    glm::vec3 sitio;
    AccionPasajero accion = CAMINAR;
    glm::vec3 mano{0};
    float espera = 0;
    bool cruzar = false; // tramo recto por el centro del torniquete
};

struct Pasajero {
    TipoPasajero tipo = ADULTO;
    glm::vec3 pos{0}, inicio{0}, mano{0};
    float giro = 0, fase = 0, tiempo = 0, mezcla = 0, pulso = 0;
    float espera = 0, rapidez = 1.35f;
    float alturaAsiento = .45f;
    AccionPasajero accion = QUIETO;
    ViajePasajero viaje = PASEO;
    std::vector<ParadaPasajero> plan;
    std::vector<glm::vec3> camino;
    size_t parada = 0, paso = 0;
    int puerta = -1;
    bool visible = true, abordar = false, directo = false;
    glm::vec3 paseo{0};
};

inline float DistanciaXZ(glm::vec3 a, glm::vec3 b) {
    return glm::length(glm::vec2(a.x-b.x, a.z-b.z));
}

inline void MirarPasajero(Pasajero& p, const glm::vec3& destino, float dt) {
    if (DistanciaXZ(p.pos, destino) < .015f) return;
    float ang = std::atan2(destino.x-p.pos.x, destino.z-p.pos.z);
    float dif = std::remainder(ang-p.giro, glm::two_pi<float>());
    p.giro += glm::clamp(dif, -7.0f*dt, 7.0f*dt);
}

// Rejilla A* sobre las mismas zonas y cajas que usa el jugador. Los torniquetes
// se cruzan por un tramo recto separado para no redondear el centro del carril.
class NavegacionPasajeros {
    static constexpr float paso = .48f;
    MundoColision mundo;
    glm::vec2 origen{0};
    int nx = 0, nz = 0;
    std::vector<char> libre;

    glm::vec3 Punto(int i, float y) const {
        return {origen.x+(i%nx)*paso, y, origen.y+(i/nx)*paso};
    }
    bool Recta(glm::vec3 a, glm::vec3 b) const {
        int n = std::max(1, int(std::ceil(DistanciaXZ(a,b)/.16f)));
        for (int j=0; j<=n; ++j) if (!mundo.Libre(glm::mix(a,b,float(j)/n))) return false;
        return true;
    }
    int NodoCercano(glm::vec3 p) const {
        if (libre.empty()) return -1;
        int cx = int(std::round((p.x-origen.x)/paso)), cz = int(std::round((p.z-origen.y)/paso));
        int mejor = -1;
        float d = 1000;
        for (int z=std::max(0,cz-4); z<=std::min(nz-1,cz+4); ++z)
            for (int x=std::max(0,cx-4); x<=std::min(nx-1,cx+4); ++x) {
                int i=z*nx+x;
                float c=DistanciaXZ(p,Punto(i,p.y));
                if (libre[i] && c<d && (!mundo.Libre(p) || Recta(p,Punto(i,p.y)))) { mejor=i; d=c; }
            }
        return mejor;
    }
public:
    void Preparar(MundoColision colision) {
        mundo = std::move(colision);
        mundo.radioJugador = .23f;
        origen = glm::vec2(FLT_MAX);
        glm::vec2 maximo(-FLT_MAX);
        for (const auto& r : mundo.zonasCaminables) {
            origen=glm::min(origen,glm::vec2(r.xMin,r.zMin));
            maximo=glm::max(maximo,glm::vec2(r.xMax,r.zMax));
        }
        libre.clear();
        if (mundo.zonasCaminables.empty()) return;
        nx=int((maximo.x-origen.x)/paso)+1;
        nz=int((maximo.y-origen.y)/paso)+1;
        libre.resize(nx*nz);
        for (int i=0; i<nx*nz; ++i) libre[i]=mundo.Libre(Punto(i,0));
    }

    glm::vec3 Aproximar(glm::vec3 p) const {
        if (mundo.Libre(p)) return p;
        int n=NodoCercano(p);
        return n<0 ? p : Punto(n,p.y);
    }

    bool Trazar(glm::vec3 a, glm::vec3 b, std::vector<glm::vec3>& resultado) const {
        resultado.clear();
        if (Recta(a,b)) { resultado.push_back(b); return true; }
        int inicio=NodoCercano(a), fin=NodoCercano(b);
        if (inicio<0 || fin<0) return false;
        std::vector<int> padre(libre.size(),-1);
        std::vector<float> coste(libre.size(),FLT_MAX);
        using Entrada=std::pair<float,int>;
        std::priority_queue<Entrada,std::vector<Entrada>,std::greater<Entrada>> pendientes;
        coste[inicio]=0;
        pendientes.emplace(0.0f,inicio);
        while (!pendientes.empty()) {
            auto [prioridad,i]=pendientes.top(); pendientes.pop();
            if (i==fin) break;
            if (prioridad>coste[i]+DistanciaXZ(Punto(i,0),Punto(fin,0))+.001f) continue;
            int x=i%nx,z=i/nx;
            for (int dz=-1; dz<=1; ++dz) for (int dx=-1; dx<=1; ++dx) {
                if ((!dx&&!dz)||x+dx<0||x+dx>=nx||z+dz<0||z+dz>=nz) continue;
                int k=(z+dz)*nx+x+dx;
                if (!libre[k] || (dx && dz && (!libre[i+dx] || !libre[i+dz*nx]))) continue;
                float c=coste[i]+paso*(dx&&dz?1.414214f:1.0f);
                if (c>=coste[k]) continue;
                // Una celda libre no basta para evitar una caja delgada.
                if (!Recta(Punto(i,0),Punto(k,0))) continue;
                coste[k]=c; padre[k]=i;
                pendientes.emplace(c+DistanciaXZ(Punto(k,0),Punto(fin,0)),k);
            }
        }
        if (inicio!=fin && padre[fin]<0) return false;
        std::vector<glm::vec3> puntos;
        for (int i=fin; i!=-1; i=padre[i]) puntos.push_back(Punto(i,a.y));
        std::reverse(puntos.begin(),puntos.end());
        if (mundo.Libre(b)) puntos.push_back(b);
        for (size_t i=0; i<puntos.size();) {
            size_t j=i;
            while (j+1<puntos.size() && Recta(a,puntos[j+1])) ++j;
            a=puntos[j]; resultado.push_back(a); i=j+1;
        }
        return true;
    }
};

inline bool AvanzarPasajero(Pasajero& p, const MundoColision* mundo, float dt) {
    if (p.paso>=p.camino.size()) return true;
    glm::vec3 meta=p.camino[p.paso], d=meta-p.pos;
    float distancia=glm::length(d);
    if (distancia<.065f) { ++p.paso; return p.paso>=p.camino.size(); }
    MirarPasajero(p,meta,dt);
    auto deseada=p.pos+d*(std::min(distancia,p.rapidez*dt)/distancia);
    auto nueva=mundo?mundo->Mover(p.pos,deseada):deseada;
    float mov=glm::distance(nueva,p.pos);
    p.pos=nueva;
    p.fase+=mov*(p.tipo==NINO?9.1f:6.9f);
    p.pulso=glm::mix(p.pulso,mov>.0001f?1.0f:0.0f,std::min(1.0f,dt*10.0f));
    return false;
}

struct ModeloPasajero {
    std::unique_ptr<ModeloEscena> fbx;
    std::array<glm::vec3,NUM_ARTICULACIONES> juntas{};
    bool Cargar(const char* ruta) {
        fbx=std::make_unique<ModeloEscena>(ruta);
        if (!fbx->cargado) return false;
        for (int i=0; i<NUM_ARTICULACIONES; ++i) {
            int n=fbx->Buscar(ARTICULACIONES[i]);
            if (n<0) return false;
            juntas[i]=glm::vec3(fbx->nodos[n].global[3]);
        }
        return true;
    }
    glm::mat4 Ubicar(const Pasajero& p) const {
        float sentar=p.accion==SENTARSE?p.mezcla:0.0f;
        float descenso=(p.alturaAsiento+.07f-juntas[0].y)*sentar;
        glm::vec3 avance(0);
        if (p.accion==BAJAR_ESCALERA) {
            descenso-=p.mezcla*1.25f;
            avance=glm::vec3(std::sin(p.giro),0,std::cos(p.giro))*(p.mezcla*.95f);
        }
        float rebote=std::abs(std::sin(p.fase))*.037f*p.pulso;
        return glm::rotate(glm::translate(glm::mat4(1),p.pos+avance+glm::vec3(0,descenso+rebote,0)),p.giro,glm::vec3(0,1,0));
    }
    std::array<glm::mat4,NUM_ARTICULACIONES> Pose(const Pasajero& p,const glm::mat4& modelo) const {
        std::array<glm::mat4,NUM_ARTICULACIONES> d;
        for (auto& m:d) m=glm::mat4(1);
        auto giro=[](glm::vec3 punto,const glm::quat& q) {
            return glm::translate(glm::mat4(1),punto)*glm::mat4_cast(q)*glm::translate(glm::mat4(1),-punto);
        };
        auto rx=[&](int i,float a) { return giro(juntas[i],glm::angleAxis(a,glm::vec3(1,0,0))); };
        auto mover=[&](const glm::mat4& m,int j) { return glm::vec3(m*glm::vec4(juntas[j],1)); };
        float sentar=p.accion==SENTARSE?p.mezcla:0.0f;
        for (int l=0;l<2;++l) {
            float onda=p.accion==BAJAR_ESCALERA?std::sin(p.tiempo*13.0f+l*glm::pi<float>())*.7f:
                std::sin(p.fase+l*glm::pi<float>())*p.pulso;
            int muslo=8+l*2, hombro=2+l*3;
            float incl=-1.48f*sentar+.41f*onda;
            d[muslo]=rx(muslo,incl);
            float rodilla=.48f*std::max(0.0f,-onda);
            if (sentar > 0.0f) {
                // Con las nuevas proporciones, adelantar el pie al sentarse
                // evita que la suela de un adulto atraviese el piso del vagon.
                const float descenso = (p.alturaAsiento + .07f - juntas[0].y) * sentar;
                const float alturaRodilla = mover(d[muslo], muslo + 1).y + descenso;
                const float largoPantorrilla = juntas[muslo + 1].y;
                const float pendiente = std::acos(glm::clamp((alturaRodilla - .025f) / largoPantorrilla, 0.0f, 1.0f));
                rodilla += (1.48f - pendiente) * sentar;
            }
            d[muslo+1]=d[muslo]*rx(muslo+1,rodilla);
            d[hombro]=rx(hombro,-.34f*onda-.19f*sentar);
            d[hombro+1]=d[hombro]*rx(hombro+1,-.65f*sentar-.19f*std::max(0.0f,onda));
            d[hombro+2]=d[hombro+1];
        }
        if (p.accion==RECARGAR || p.accion==TAQUILLA || p.accion==AGARRARSE) {
            const int a=5,b=6,c=7;
            glm::vec3 objetivo=glm::vec3(glm::inverse(modelo)*glm::vec4(p.mano,1));
            // Toques repetidos de la mano; en el pasamanos el punto permanece fijo.
            if (p.accion!=AGARRARSE) objetivo.z-=.10f*std::pow(std::abs(std::sin(p.tiempo*5.1f)),2.0f);
            objetivo=glm::mix(mover(d[b],c),objetivo,p.mezcla);
            glm::vec3 dir=objetivo-juntas[a];
            float sup=glm::distance(juntas[a],juntas[b]), inf=glm::distance(juntas[b],juntas[c]);
            float r=glm::clamp(glm::length(dir),.065f,sup+inf-.002f);
            dir=glm::normalize(dir+glm::vec3(0,0,.00001f));
            glm::vec3 costado=glm::vec3(.38f,-1.0f,-.08f);
            glm::vec3 plano=glm::normalize(costado-dir*glm::dot(costado,dir)+glm::vec3(.00001f,0,0));
            float x=(sup*sup+r*r-inf*inf)/(2*r);
            glm::vec3 codo=juntas[a]+dir*x+plano*std::sqrt(std::max(0.0f,sup*sup-x*x));
            glm::quat q=glm::rotation(glm::normalize(juntas[b]-juntas[a]),glm::normalize(codo-juntas[a]));
            d[a]=giro(juntas[a],q);
            glm::vec3 v=glm::normalize(juntas[a]+dir*r-codo);
            glm::quat q2=glm::rotation(glm::normalize(q*(juntas[c]-juntas[b])),v);
            d[b]=giro(codo,q2)*d[a]; d[c]=d[b];
        }
        d[1]=rx(1,.035f*std::sin(p.tiempo*1.3f)+.12f*(p.accion==RECARGAR?p.mezcla:0));
        return d;
    }
};

#endif
