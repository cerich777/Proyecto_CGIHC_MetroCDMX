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

    void AjustarAfluenciaAlta(bool horaPico, const MundoColision& mundo) {
        arriba.erase(std::remove_if(arriba.begin(),arriba.end(),[](const Pasajero& p) {
            return p.refuerzoHoraPico;
        }),arriba.end());
        if (!horaPico || !Disponible()) return;
        // Pasillos a ambos lados de los torniquetes, sin ocupar sus carriles.
        // Hora pico CDMX: muchos mas pasajeros en los pasillos
        const float carriles[] = {-16.0f,-11.0f,-6.0f,-1.5f,2.5f};
        // lejos de las dos filas de torniquetes (z = -37.6 y 43.8) para no abrirlos al pasear
        const float tramos[] = {-61.0f,-55.0f,-49.0f,-43.0f,-31.0f,-25.0f,
                                23.0f,29.0f,35.0f,53.0f,59.0f,65.0f};
        for (int carril=0;carril<5;++carril) for (int tramo=0;tramo<12;++tramo) {
            glm::vec3 origen(carriles[carril],.019f,tramos[tramo]+(carril%2?2.0f:0.0f));
            origen=navAlta.Aproximar(origen);
            if (!mundo.Libre(origen)) continue;
            bool separado=true;
            for (const auto& otro:arriba)
                if (otro.visible && DistanciaXZ(origen,otro.pos)<0.9f) { separado=false; break; }
            if (!separado) continue;
            Pasajero p=Nuevo(TipoPasajero((carril+tramo)%3),origen,arriba.size());
            p.refuerzoHoraPico=true;
            glm::vec3 vuelta=navAlta.Aproximar(origen+glm::vec3(0,0,tramo%2?2.0f:-2.0f));
            p.plan={ {vuelta}, {origen} };
            p.espera=.25f*float((carril+tramo)%5);
            arriba.push_back(std::move(p));
        }
    }

    // Personas formadas (quietas) frente a maquinas y taquillas
    // Devuelve el indice del primero (o -1)
    int AgregarFila(const std::vector<glm::vec3>& puntos) {
        if (!Disponible()) return -1;
        int base=(int)arriba.size();
        for (size_t i=0;i<puntos.size();++i) {
            Pasajero p=Nuevo(TipoPasajero(i%3),puntos[i],arriba.size());
            p.refuerzoHoraPico=true;
            arriba.push_back(std::move(p));
        }
        return base;
    }
    // Manda a una persona de la fila a recorrer estos puntos
    void Enviar(int i, const std::vector<glm::vec3>& puntos) {
        if (i<0 || i>=(int)arriba.size()) return;
        Pasajero& p=arriba[i];
        p.plan.clear();
        for (const auto& q:puntos) p.plan.push_back({q});
        p.parada=0; p.paso=0; p.camino.clear(); p.espera=0;
    }
    // Si ya llego a 'destino', la oculta para siempre. Devuelve true al ocultarla.
    bool Ocultar(int i, const glm::vec3& destino) {
        if (i<0 || i>=(int)arriba.size()) return true;
        Pasajero& p=arriba[i];
        if (DistanciaXZ(p.pos,destino)>0.6f) return false;
        p.visible=false; p.espera=1e9f; p.plan.clear();
        return true;
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
                if (p.refuerzoHoraPico) {
                    p.parada=0; p.camino.clear(); p.espera=1.5f;
                    continue;
                }
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

    void AjustarAfluenciaAnden(bool horaPico, const MundoColision& mundo) {
        anden.erase(std::remove_if(anden.begin(),anden.end(),[](const Pasajero& p) {
            return p.refuerzoHoraPico;
        }),anden.end());
        if (!horaPico || !Disponible()) return;
        // Cuatro hileras repartidas por los dos andenes, cerca de las puertas.
        // Filas fijas para abordar: 3 personas perpendiculares a la orilla, en ambos andenes
        for (int lado_i=0;lado_i<2;++lado_i) for (int lugar=0;lugar<14;++lugar) for (int j=0;j<3;++j) {
            float lado=lado_i?1.0f:-1.0f;
            glm::vec3 origen(lado*(3.4f+.7f*j),0,-80.0f+11.0f*float(lugar));
            TipoPasajero tipo=TipoPasajero((lugar+j)%3);
            if (tipo==ADULTO && EsExclusivo(origen)) tipo=MUJER;
            Pasajero p=Nuevo(tipo,origen,anden.size());
            p.pos=p.inicio=NavAnden(p).Aproximar(origen);
            if (!mundo.Libre(p.pos)) continue;
            p.refuerzoHoraPico=true; p.estatico=true;
            anden.push_back(std::move(p));
        }
        // Mas gente caminando, solo en las hileras de atras
        for (int hilera=1;hilera<4;hilera+=2) for (int lugar=0;lugar<16;++lugar) {
            float lado=hilera<2?-1.0f:1.0f;
            float x=lado*(hilera%2?6.45f:3.55f);
            float z=-83.0f+10.8f*float(lugar)+(hilera%2?3.3f:0.0f);
            TipoPasajero tipo=TipoPasajero((hilera+lugar)%3);
            glm::vec3 origen(x,0,z);
            if (tipo==ADULTO && EsExclusivo(origen)) tipo=MUJER;
            Pasajero p=Nuevo(tipo,origen,anden.size());
            p.pos=p.inicio=NavAnden(p).Aproximar(origen);
            if (!mundo.Libre(p.pos)) continue;
            bool separado=true;
            for (const auto& otro:anden)
                if (otro.visible && DistanciaXZ(p.pos,otro.pos)<1.10f) { separado=false; break; }
            if (!separado) continue;
            p.refuerzoHoraPico=true;
            p.paseo=NavAnden(p).Aproximar(p.pos+glm::vec3(0,0,lugar%2?2.1f:-2.1f));
            p.abordar=lugar%4!=0;
            RutaAnden(p,p.paseo);
            anden.push_back(std::move(p));
        }
    }

    void ActualizarAnden(const MundoColision& mundo, bool frenando, bool abiertas, bool ausente,
                         float desplazamiento, float dt) {
        size_t pasajerosHabituales=std::count_if(anden.begin(),anden.end(),[](const Pasajero& p) {
            return !p.refuerzoHoraPico;
        });
        if (!bajadas.empty() && pasajerosHabituales<32 && !escalerasAnden.empty()) {
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
            if (p.estatico) continue;
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

    // Asientos del vagon para la carrera del modo experto
    struct AsientoVagon { glm::vec3 pos; bool ocupado=false; int corredor=-1; std::string nombre; bool reservado=false; };
    std::vector<AsientoVagon> asientos;
    int IndiceAsiento(const std::string& n) const {
        for (size_t k=0;k<asientos.size();++k) if (asientos[k].nombre==n) return (int)k;
        return -1;
    }
    std::vector<int> colaCorredores;   // orden de salida: primero los que van a los asientos mas cercanos al usuario
    void IniciarCarrera(float x) {
        colaCorredores.clear();
        for (size_t k=0;k<asientos.size();++k) {
            int c=asientos[k].corredor;
            if (c>=0 && c<(int)vagon.size() && vagon[c].espera>1e8f && !asientos[k].reservado) colaCorredores.push_back(c);
        }
        auto asientoDe=[&](int c){ for (const auto& a:asientos) if (a.corredor==c) return a.pos.x; return 0.0f; };
        std::sort(colaCorredores.begin(),colaCorredores.end(),[&](int a,int b){ return std::fabs(asientoDe(a)-x)<std::fabs(asientoDe(b)-x); });
        for (int j=0;j<8 && j<(int)colaCorredores.size();++j) { auto& p=vagon[colaCorredores[j]]; p.espera=.2f*float(j); p.visible=true; }
        colaCorredores.erase(colaCorredores.begin(),colaCorredores.begin()+std::min<size_t>(8,colaCorredores.size()));
    }
    // Ocupa con pasajeros ya sentados los n asientos libres mas cercanos a x (deja al menos uno libre)
    void SentarCercanos(float x, int n) {
        std::vector<int> libres;
        for (size_t k=0;k<asientos.size();++k) if (!asientos[k].ocupado) libres.push_back((int)k);
        std::sort(libres.begin(),libres.end(),[&](int a,int b){ return std::fabs(asientos[a].pos.x-x)<std::fabs(asientos[b].pos.x-x); });
        // Trampa: el asiento "pegado a otros" (no apartado) mas cercano queda libre pero es reservado
        auto pegado=[&](int k) {
            for (size_t o=0;o<asientos.size();++o)
                if ((int)o!=k && (asientos[o].pos.z<0)==(asientos[k].pos.z<0) && std::fabs(asientos[o].pos.x-asientos[k].pos.x)<.75f) return true;
            return false;
        };
        for (size_t i=0;i<libres.size();++i) if (pegado(libres[i])) {
            // el reservado es el de enfrente (lado opuesto del pasillo), que ya trae el letrero en el modelo
            int k=libres[i], frente=-1; float mejor=1e9f;
            for (size_t o=0;o<asientos.size();++o)
                if ((asientos[o].pos.z<0)!=(asientos[k].pos.z<0) && !asientos[o].ocupado) {
                    float d=std::fabs(asientos[o].pos.x-asientos[k].pos.x);
                    if (d<mejor) { mejor=d; frente=(int)o; }
                }
            if (frente>=0 && mejor<.8f) {
                auto it=std::find(libres.begin(),libres.end(),frente);
                if (it!=libres.end()) i=size_t(it-libres.begin());
            }
            AsientoVagon& t=asientos[libres[i]];
            t.reservado=true;
            if (t.corredor>=0 && t.corredor<(int)vagon.size()) { auto& c=vagon[t.corredor]; c.visible=false; c.directo=false; c.espera=-1; c.camino.clear(); }
            t.corredor=-1;
            libres.erase(libres.begin()+i);
            break;
        }
        n=std::min(n,(int)libres.size()-1);
        for (int i=0;i<n;++i) {
            AsientoVagon& a=asientos[libres[i]];
            if (a.corredor>=0 && a.corredor<(int)vagon.size()) {   // su corredor ya no participa
                auto& c=vagon[a.corredor]; c.visible=false; c.directo=false; c.espera=-1; c.camino.clear();
            }
            glm::vec3 pos=a.pos; pos.y=.012f; pos.z=std::copysign(std::abs(pos.z)-.035f,pos.z);
            Pasajero p=Nuevo(TipoPasajero(i%3),pos,vagon.size());
            p.accion=SENTARSE; p.giro=pos.z<0?0:glm::pi<float>(); p.alturaAsiento=.37f; p.mezcla=1.0f;
            vagon.push_back(std::move(p));
            a.ocupado=true; a.corredor=-1;
        }
    }
    bool SoltarCorredor() {
        while (!colaCorredores.empty()) {
            int c=colaCorredores.front(); colaCorredores.erase(colaCorredores.begin());
            if (c>=0 && c<(int)vagon.size() && vagon[c].espera>1e8f) { vagon[c].espera=0; vagon[c].visible=true; return true; }
        }
        return false;
    }
    void OcuparAsiento(int k) {   // el usuario gano este asiento: su corredor se queda parado
        if (k<0 || k>=(int)asientos.size()) return;
        asientos[k].ocupado=true;
        int c=asientos[k].corredor;
        if (c>=0 && c<(int)vagon.size()) { auto& p=vagon[c]; p.directo=false; p.accion=QUIETO; p.camino.clear(); p.espera=0; }
    }

    void PrepararVagon(const ModeloEscena& escena, bool horaPico = false) {
        vagon.clear(); asientos.clear();
        if (!Disponible()) return;
        for (int k=1;k<=48;++k) {
            char nm[16]; snprintf(nm,sizeof nm,"Asiento_%02d",k);
            int i=escena.Buscar(nm);
            if (i<0) continue;
            AsientoVagon a; a.pos=Centro(escena.nodos[i]); a.nombre=nm;
            asientos.push_back(a);
        }
        auto sentar=[&](TipoPasajero tipo,const char* nombre,bool yaSentado) {
            int i=escena.Buscar(nombre);
            if (i<0) return;
            int k=IndiceAsiento(nombre);
            if (k>=0) { if (yaSentado) asientos[k].ocupado=true; else asientos[k].corredor=(int)vagon.size(); }
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
        // Cada pasamanos tiene dos tubos verticales en los extremos. Sus
        // cajas del FBX permiten ubicar el eje real sin coordenadas a ojo.
        auto agarrarTubo=[&](TipoPasajero tipo,const char* nombre,bool extremoFinal) {
            int i=escena.Buscar(nombre);
            if (i<0) return;
            const Nodo& n=escena.nodos[i];
            bool ladoPositivo=(n.aabbMin.z+n.aabbMax.z)>0.0f;
            float x=extremoFinal?n.aabbMax.x-.04f:n.aabbMin.x+.04f;
            float z=ladoPositivo?n.aabbMin.z+.115f:n.aabbMax.z-.115f;
            float desplazamiento=(ladoPositivo?-1.0f:1.0f)*(extremoFinal?-1.0f:1.0f)*.35f;
            glm::vec3 pos(x+desplazamiento,.012f,ladoPositivo?.43f:-.43f);
            sujetar(tipo,pos,{x,1.23f,z});
        };
        agarrarTubo(ADULTO,"V05_Pasamanos_06",false);
        agarrarTubo(MUJER,"V05_Pasamanos_01",false);
        if (!horaPico) {
            Pasajero p=Nuevo(ADULTO,{73.8f,.012f,.16f},vagon.size()); p.giro=2.5f;
            vagon.push_back(p);
        }
        const char* extras[]={"Asiento_03","Asiento_13","Asiento_24","Asiento_28","Asiento_33"};
        int i=0;
        for (const auto& a:anden) if (a.viaje==VIAJANDO && i<5) sentar(a.tipo,extras[i++],false);
        if (horaPico) {
            // Diez pasajeros adicionales junto a tubos reales; se alternan
            // ambos lados para dejar un paso estrecho por el centro.
            sentar(MUJER, "Asiento_01",true);
            sentar(ADULTO,"Asiento_17",true);
            sentar(NINO,  "Asiento_20",true);
            sentar(ADULTO,"Asiento_30",true);
            agarrarTubo(ADULTO,"V05_Pasamanos_03",false);
            agarrarTubo(MUJER, "V05_Pasamanos_03",true);

            agarrarTubo(ADULTO,"V05_Pasamanos_01",true);
            agarrarTubo(MUJER, "V05_Pasamanos_02",false);
            agarrarTubo(ADULTO,"V05_Pasamanos_02",true);
            agarrarTubo(MUJER, "V05_Pasamanos_04",false);
            agarrarTubo(ADULTO,"V05_Pasamanos_04",true);
            agarrarTubo(ADULTO,"V05_Pasamanos_01",false);
            agarrarTubo(MUJER, "V05_Pasamanos_06",false);
            agarrarTubo(MUJER, "V05_Pasamanos_06",true);
            agarrarTubo(ADULTO,"V05_Pasamanos_05",false);
            agarrarTubo(MUJER, "V05_Pasamanos_05",true);

            // Corredores: van por todos los asientos libres menos dos (los que puede ganar el usuario)
            int libres=0, j=0;
            for (const auto& a:asientos) if (!a.ocupado && a.corredor<0) ++libres;
            for (size_t k=0;k<asientos.size();++k) {
                if (asientos[k].ocupado || asientos[k].corredor>=0) continue;
                if (libres-- <= 1) break;   // solo queda un asiento sin corredor asignado
                std::string nombre=asientos[k].nombre;
                sentar(TipoPasajero(j%3),nombre.c_str(),false);
                Pasajero& p=vagon.back();
                p.pos.x=(j%2?78.9f:65.7f); p.inicio=p.pos;   // entran por los extremos del pasillo
                p.visible=false;                             // aparecen poco a poco durante la carrera
                p.rapidez=4.5f; p.espera=1e9f;              // esperan la cuenta regresiva (paso tranquilo)
                ++j;
            }
        }
    }

    void ActualizarVagon(float dt) {
        for (size_t idx=0; idx<vagon.size(); ++idx) {
            auto& p=vagon[idx];
            p.tiempo+=dt; p.pulso*=std::max(0.0f,1-8*dt);
            if (p.espera>0) { p.espera-=dt; continue; }
            if (p.directo && AvanzarPasajero(p,nullptr,dt)) {
                p.directo=false; p.accion=SENTARSE; p.tiempo=0;
                for (auto& a:asientos) if (a.corredor==(int)idx) a.ocupado=true;   // asiento ya no disponible
            }
            if (p.accion==SENTARSE) {
                MirarPasajero(p,p.pos+glm::vec3(0,0,p.pos.z<0?1:-1),dt);
                p.mezcla=std::max(p.mezcla,glm::smoothstep(0.0f,.8f,p.tiempo));
            }
        }
    }
};

#endif
