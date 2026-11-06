#ifndef FLUJO_PASAJEROS_H
#define FLUJO_PASAJEROS_H

#include "pasajeros.h"
#include "torniquetes.h"

struct PuertaPasajero { glm::vec3 centro; };
struct EscaleraPasajero { glm::vec3 boca, descanso; };

class FlujoPasajeros {
    NavegacionPasajeros navAlta, navAnden, navAdulto;
    glm::vec3 minimoExclusivo{0}, maximoExclusivo{0};
    bool exclusivo = false;
    std::vector<PuertaPasajero> puertas;
    struct Bajada { TipoPasajero tipo; int escalera; };
    std::vector<Bajada> bajadas;
    std::vector<EscaleraPasajero> escalerasAnden;

    static Pasajero Nuevo(TipoPasajero tipo, glm::vec3 p, size_t i) {
        Pasajero a;
        a.tipo=tipo; a.pos=a.inicio=p;
        a.fase=.85f*float(i); a.giro=.42f*float(i);
        a.rapidez=1.15f+.15f*float(i%4);
        return a;
    }
    const NavegacionPasajeros& NavAnden(const Pasajero& p) const { return p.tipo==ADULTO?navAdulto:navAnden; }

    void RutaAnden(Pasajero& p, glm::vec3 destino) {
        p.paso=0;
        NavAnden(p).Trazar(p.pos,destino,p.camino);
    }

    static glm::vec3 Centro(const Nodo& n) { return .5f*(n.aabbMin+n.aabbMax); }

public:
    std::vector<Pasajero> arriba, anden, vagon;
    std::array<ModeloPasajero,3> modelos;
    int recargas=0, compras=0, entradas=0, salidas=0, escaleras=0, subidas=0, esperan=0;

    bool Cargar() {
        bool ok=modelos[ADULTO].Cargar("models/Pasajero_Adulto.fbx");
        ok=modelos[MUJER].Cargar("models/Pasajera_Mujer.fbx")&&ok;
        ok=modelos[NINO].Cargar("models/Pasajero_Nino.fbx")&&ok;
        if (!ok) for (auto& m:modelos) m.fbx.reset();
        return ok;
    }
    bool Disponible() const { return bool(modelos[ADULTO].fbx); }
    void Liberar() { for (auto& m:modelos) m.fbx.reset(); }

    void PrepararAlta(const ModeloEscena& escena, const MundoColision& mundo,
                      const Torniquetes& torniquetes, const std::vector<EscaleraPasajero>& escalerasAlta) {
        if (!Disponible()) return;
        navAlta.Preparar(mundo);
        if (!arriba.empty() || torniquetes.carriles.empty() || escalerasAlta.size()<8) return;
        int maquina=escena.Buscar("Maquinas_Recarga"), taquilla=escena.Buscar("Taquilla_Cuerpo");
        glm::vec3 panel(5.37f,1.26f,-41.22f), ventanilla(-10.0f,1.24f,-62.65f);
        if (maquina>=0) {
            const Nodo& n=escena.nodos[maquina];
            panel.x=n.aabbMin.x+.025f; panel.z=Centro(n).z;
        }
        if (taquilla>=0) ventanilla.z=escena.nodos[taquilla].aabbMax.z+.025f;
        auto carril=[&](float x,float signo) {
            const Torniquete* t=nullptr;
            for (const Torniquete& c:torniquetes.carriles)
                if (c.centro.y*signo>0 && (!t||std::abs(c.centro.x-x)<std::abs(t->centro.x-x))) t=&c;
            return t?t->centro:torniquetes.carriles.front().centro;
        };
        auto preparar=[&](TipoPasajero tipo,glm::vec3 origen,std::vector<ParadaPasajero> plan,float espera,int e) {
            origen=navAlta.Aproximar(origen);
            Pasajero a=Nuevo(tipo,origen,arriba.size());
            a.plan=std::move(plan); a.espera=espera; a.puerta=e;
            arriba.push_back(std::move(a));
        };
        auto entrada=[&](TipoPasajero tipo,glm::vec3 origen,glm::vec2 c,int e,float espera,
                         std::vector<ParadaPasajero> plan) {
            float signo=c.y<0?1.0f:-1.0f;
            plan.push_back({{c.x,.019f,c.y-signo*2.1f}});
            plan.push_back({{c.x,.019f,c.y+signo*1.75f},CAMINAR,{},0,true});
            glm::vec3 boca=escalerasAlta[e].boca, d=escalerasAlta[e].descanso;
            plan.push_back({glm::mix(boca,d,.48f),BAJAR_ESCALERA,boca,.85f});
            preparar(tipo,origen,std::move(plan),espera,e);
        };
        glm::vec3 p1(panel.x-.56f,.019f,panel.z+.23f);
        glm::vec3 p2(panel.x-.56f,.019f,panel.z-1.02f);
        glm::vec3 mano2(panel.x,panel.y,p2.z-.23f);
        entrada(MUJER,p1,carril(3.6f,-1),1,.10f,{{p1,RECARGAR,panel,4.8f}});
        entrada(ADULTO,p2,carril(2.0f,-1),3,1.6f,{{p2,RECARGAR,mano2,4.2f}});
        glm::vec3 p3(ventanilla.x-.22f,.019f,ventanilla.z+.57f);
        entrada(ADULTO,p3,carril(-12.4f,-1),0,.25f,{{p3,TAQUILLA,ventanilla,4.6f}});
        entrada(NINO,{-6.5f,.019f,-45.5f},carril(-10.1f,-1),2,2.6f,{});
        entrada(MUJER,{-10.5f,.019f,54.5f},carril(-8.0f,1),6,1.8f,{});
        entrada(NINO,{-2.8f,.019f,53.8f},carril(2.6f,1),7,4.5f,{});
        auto salida=[&](TipoPasajero tipo,int e,glm::vec2 c,glm::vec3 extremo,float espera) {
            preparar(tipo,escalerasAlta[e].descanso,
                {{{c.x,.019f,c.y-2.1f}},{{c.x,.019f,c.y+1.75f},CAMINAR,{},0,true},
                 {{extremo.x<0?-17.8f:4.7f,.019f,extremo.z}}, {extremo}},espera,-1);
        };
        salida(MUJER,4,carril(-13.0f,1),{-56.9f,.019f,55.4f},.35f);
        salida(ADULTO,5,carril(2.5f,1),{44.0f,.019f,55.4f},2.3f);
    }

    std::vector<glm::vec3> SensoresAlta() const {
        std::vector<glm::vec3> p;
        for (const auto& a:arriba) if (a.visible) p.push_back(a.pos);
        return p;
    }

    void ActualizarAlta(const MundoColision& mundo,float dt) {
        for (auto& p:arriba) {
            p.pulso*=std::max(0.0f,1-8*dt);
            if (p.espera>0) { p.espera-=dt; continue; }
            if (!p.visible) {
                p.visible=true; p.pos=p.inicio; p.parada=p.paso=0;
                p.camino.clear(); p.accion=QUIETO; p.tiempo=p.mezcla=0;
            }
            if (p.parada>=p.plan.size()) {
                p.visible=false; p.espera=9.0f+.6f*float(&p-arriba.data());
                if (p.puerta>=0) { ++escaleras; bajadas.push_back({p.tipo,p.puerta}); }
                else ++salidas;
                continue;
            }
            const auto& meta=p.plan[p.parada];
            if (p.camino.empty()) {
                p.paso=0;
                if (meta.cruzar) p.camino.push_back(meta.sitio);
                else if (!navAlta.Trazar(p.pos,meta.sitio,p.camino)) { p.espera=2; continue; }
            }
            if (p.paso<p.camino.size()) {
                p.accion=CAMINAR; p.tiempo+=dt;
                AvanzarPasajero(p,&mundo,dt);
                continue;
            }
            if (meta.accion==CAMINAR) {
                if (meta.cruzar && p.puerta>=0) ++entradas;
                ++p.parada; p.camino.clear(); p.tiempo=p.mezcla=0;
                continue;
            }
            if (p.accion!=meta.accion) { p.tiempo=p.mezcla=0; p.accion=meta.accion; }
            p.mano=meta.mano;
            MirarPasajero(p,meta.mano,dt);
            p.tiempo+=dt;
            p.mezcla=glm::smoothstep(0.0f,.55f,p.tiempo);
            if (meta.accion==BAJAR_ESCALERA) p.mezcla=glm::smoothstep(0.0f,meta.espera,p.tiempo);
            if (p.tiempo>=meta.espera) {
                if (p.accion==RECARGAR) ++recargas;
                if (p.accion==TAQUILLA) ++compras;
                ++p.parada; p.camino.clear(); p.tiempo=0;
            }
        }
    }

    bool EsExclusivo(glm::vec3 p) const {
        return exclusivo && p.x>=minimoExclusivo.x && p.x<=maximoExclusivo.x &&
            p.z>=minimoExclusivo.z && p.z<=maximoExclusivo.z;
    }

    void PrepararAnden(const ModeloEscena& escena, const MundoColision& mundo,
                       std::vector<PuertaPasajero> acceso,const std::vector<EscaleraPasajero>& e) {
        if (!Disponible()) return;
        puertas=std::move(acceso); escalerasAnden=e;
        navAnden.Preparar(mundo);
        MundoColision restriccion=mundo;
        auto franjas=escena.BuscarPrefijo("Franja_Rosa_Mujeres");
        exclusivo=!franjas.empty();
        if (exclusivo) {
            minimoExclusivo=glm::vec3(FLT_MAX); maximoExclusivo=glm::vec3(-FLT_MAX);
            for (int i:franjas) {
                minimoExclusivo=glm::min(minimoExclusivo,escena.nodos[i].aabbMin);
                maximoExclusivo=glm::max(maximoExclusivo,escena.nodos[i].aabbMax);
            }
            // Toda la seccion de anden delimitada por las dos franjas, no solo la pintura.
            for (const auto& r:mundo.zonasCaminables) if (r.Contiene(Centro(escena.nodos[franjas.front()]).x,Centro(escena.nodos[franjas.front()]).z,0)) {
                minimoExclusivo.x=r.xMin; maximoExclusivo.x=r.xMax;
                break;
            }
            restriccion.obstaculos.push_back({minimoExclusivo,maximoExclusivo});
        }
        navAdulto.Preparar(restriccion);
        if (!anden.empty()) return;
        const float z[] = { 14, -9, 7, 24, -15, -31, 36, 47, -51, -53, 62, -67, -70, 69, 80, -81, 32, 47, 21, -8, 73, -88, 9, -15 };
        for (size_t i=0;i<24;++i) {
            float lado=i%2==0?-1.0f:1.0f;
            TipoPasajero tipo=TipoPasajero(i%3);
            glm::vec3 pos(lado*(3.8f+.9f*float(i%4)),0,z[i]);
            if (tipo==ADULTO && EsExclusivo(pos)) tipo=i%2?MUJER:NINO;
            Pasajero a=Nuevo(tipo,pos,i);
            a.pos=a.inicio=NavAnden(a).Aproximar(pos);
            a.paseo=NavAnden(a).Aproximar(a.pos+glm::vec3(lado*(i%2?-.55f:.55f),0,(i%4<2?1.0f:-1.0f)*(2.8f+float(i%3))));
            a.abordar=i%4!=1;
            RutaAnden(a,a.paseo);
            anden.push_back(std::move(a));
        }
    }

    void ActualizarAnden(const MundoColision& mundo, bool frenando, bool abiertas, bool ausente,
                         float desplazamiento, float dt) {
        if (!bajadas.empty() && anden.size()<32 && !escalerasAnden.empty()) {
            auto b=bajadas.back(); bajadas.pop_back();
            auto p=Nuevo(b.tipo,escalerasAnden[b.escalera].descanso,anden.size());
            if (p.tipo!=ADULTO || !EsExclusivo(p.pos)) {
                p.abordar=true;
                p.paseo=p.pos+glm::vec3(0,0,glm::sign(p.pos.z-escalerasAnden[b.escalera].boca.z)*4);
                RutaAnden(p,p.paseo);
                anden.push_back(std::move(p));
            }
        }
        MundoColision restringido=mundo;
        if (exclusivo) restringido.obstaculos.push_back({minimoExclusivo,maximoExclusivo});
        for (auto& p:anden) {
            p.tiempo+=dt;
            p.pulso*=std::max(0.0f,1-5*dt);
            if (p.viaje==VIAJANDO) {
                if (!ausente) continue;
                p.pos=p.inicio; p.viaje=PASEO; p.visible=true; p.puerta=-1;
                RutaAnden(p,p.paseo);
            }
            if (p.viaje==PASEO && p.abordar && (frenando || abiertas)) {
                std::vector<std::pair<float,int>> candidatas;
                for (size_t i=0;i<puertas.size();++i) {
                    glm::vec3 d=puertas[i].centro;
                    if (d.x*p.pos.x<=0) continue;
                    glm::vec3 espera(d.x<0?-2.70f:2.70f,0,d.z);
                    if ((p.tipo==ADULTO && EsExclusivo(espera)) || !mundo.Libre(espera)) continue;
                    candidatas.emplace_back(DistanciaXZ(p.pos,espera),int(i));
                }
                std::sort(candidatas.begin(),candidatas.end());
                for (auto [dist,i]:candidatas) {
                    if (dist>24) break;
                    glm::vec3 d=puertas[i].centro;
                    int fila=0;
                    for (const auto& otro:anden)
                        if (otro.puerta==i && (otro.viaje==HACIA_PUERTA || otro.viaje==ESPERA_PUERTA)) ++fila;
                    const float separacion[]={0.0f,-.42f,.42f};
                    glm::vec3 espera(std::copysign(2.70f+.38f*float(fila/3),d.x),0,d.z+separacion[fila%3]);
                    if (!NavAnden(p).Trazar(p.pos,espera,p.camino)) continue;
                    p.puerta=i; p.paso=0; p.viaje=HACIA_PUERTA; ++esperan; break;
                }
            }
            if (p.viaje==SUBIENDO) {
                if (AvanzarPasajero(p,nullptr,dt)) {
                    p.viaje=VIAJANDO; p.visible=false; ++subidas;
                }
                continue;
            }
            const auto& col=p.tipo==ADULTO?restringido:mundo;
            if (p.viaje==HACIA_PUERTA || p.viaje==PASEO) {
                p.accion=CAMINAR;
                if (AvanzarPasajero(p,&col,dt)) {
                    if (p.viaje==HACIA_PUERTA) p.viaje=ESPERA_PUERTA;
                    else {
                        glm::vec3 destino=DistanciaXZ(p.pos,p.inicio)<DistanciaXZ(p.pos,p.paseo)?p.paseo:p.inicio;
                        RutaAnden(p,destino);
                    }
                }
            }
            if ((p.viaje==HACIA_PUERTA || p.viaje==ESPERA_PUERTA) && !frenando && !abiertas) {
                p.viaje=PASEO; p.puerta=-1;
                RutaAnden(p,p.inicio);
            }
            if (p.viaje==ESPERA_PUERTA) {
                glm::vec3 d=puertas[p.puerta].centro;
                d.z+=desplazamiento;
                MirarPasajero(p,d,dt);
                bool ocupada=false;
                for (const auto& otro:anden)
                    if (otro.viaje==SUBIENDO && otro.puerta==p.puerta) { ocupada=true; break; }
                if (abiertas && !ocupada) {
                    p.camino={{d.x*.72f,0,d.z},{d.x*.32f,0,d.z}};
                    p.paso=0; p.viaje=SUBIENDO;
                }
            }
        }
    }

    void PrepararVagon(const ModeloEscena& escena) {
        vagon.clear();
        if (!Disponible()) return;
        auto sentar=[&](TipoPasajero tipo,const char* nombre,bool yaSentado) {
            int i=escena.Buscar(nombre);
            if (i<0) return;
            const Nodo& n=escena.nodos[i];
            auto pos=Centro(n); pos.y=.012f;
            pos.z=std::copysign(std::abs(pos.z)-.035f,pos.z);
            Pasajero p=Nuevo(tipo,pos,vagon.size());
            p.accion=SENTARSE; p.giro=pos.z<0?0:glm::pi<float>();
            p.alturaAsiento=.37f; p.mezcla=yaSentado?1.0f:0.0f;
            if (!yaSentado) {
                p.pos.z=std::copysign(.12f,pos.z);
                p.inicio=p.pos;
                p.camino={pos};
                p.accion=CAMINAR; p.directo=true; p.paseo=pos;
            }
            vagon.push_back(std::move(p));
        };
        sentar(MUJER,"Asiento_05",true);
        sentar(NINO,"Asiento_22",true);
        sentar(ADULTO,"Asiento_31",true);
        sentar(NINO,"Asiento_10",true);
        auto sujetar=[&](TipoPasajero tipo,glm::vec3 pos,glm::vec3 mano) {
            Pasajero p=Nuevo(tipo,pos,vagon.size()); p.accion=AGARRARSE; p.mano=mano;
            p.giro=std::atan2(mano.x-pos.x,mano.z-pos.z)-.29f; p.mezcla=1;
            vagon.push_back(std::move(p));
        };
        sujetar(ADULTO,{67.76f,.012f,.33f},{67.36f,1.24f,.60f});
        sujetar(MUJER,{75.39f,.012f,-.24f},{74.91f,1.25f,-.60f});
        Pasajero p=Nuevo(ADULTO,{73.8f,.012f,.16f},vagon.size()); p.giro=2.5f;
        vagon.push_back(p);
        const char* extras[]={"Asiento_03","Asiento_13","Asiento_24","Asiento_28","Asiento_33"};
        int i=0;
        for (const auto& a:anden) if (a.viaje==VIAJANDO && i<5) sentar(a.tipo,extras[i++],false);
    }

    void ActualizarVagon(float dt) {
        for (auto& p:vagon) {
            p.tiempo+=dt; p.pulso*=std::max(0.0f,1-8*dt);
            if (p.directo && AvanzarPasajero(p,nullptr,dt)) {
                p.directo=false; p.accion=SENTARSE; p.tiempo=0;
            }
            if (p.accion==SENTARSE) {
                MirarPasajero(p,p.pos+glm::vec3(0,0,p.pos.z<0?1:-1),dt);
                p.mezcla=std::max(p.mezcla,glm::smoothstep(0.0f,.8f,p.tiempo));
            }
        }
    }
};

#endif
