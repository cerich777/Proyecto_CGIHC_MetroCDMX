# Metro CDMX — Escena 1: Andenes (OpenGL)

Recorrido interactivo por los andenes de la estación. Proyecto final de
Computación Gráfica e Interacción Humano-Computadora (FI, UNAM, 2027-1).

Base: prácticas de laboratorio `stv-labopengl` (Dr. Sergio Teodoro Vite):
GLFW, GLAD, GLM, Assimp, stb_image, irrKlang, las clases `Camera`, `Shader` y
`CubeMap`, y los shaders de Phong, Fresnel, cubemap y picking.

---

## 1. Estructura de carpetas

```
ProyectoMetro_OpenGL/
├── ProyectoMetro.sln          ← abrir con Visual Studio
├── ProyectoMetro.vcxproj
├── src/
│   ├── main.cpp               ← programa principal (BLOQUES 0–14)
│   └── stb_image.cpp
├── include/
│   ├── modelo_escena.h        ← carga del FBX con jerarquía + lotes
│   ├── keyframes.h            ← animación por cuadros clave
│   ├── colisiones.h           ← colisiones para caminar
│   ├── camera.h, shader_m.h, cubemap.h, stb_image.h   (del laboratorio)
├── deps/                      ← glad, glfw, glm, assimp, irrklang (del laboratorio)
└── bin/                       ← aquí se genera el .exe y se ejecuta
    ├── models/Andenes_Completo.fbx   ← SE EXPORTA DESDE BLENDER (ver sección 3)
    ├── shaders/               ← metro_iluminacion, metro_cielo, metro_hud, 15_picking
    ├── textures/cubemap_cielo/
    ├── sound/                 ← opcional: tren.mp3 y puertas.mp3
    └── *.dll
```

---

## 2. Compilar y ejecutar

1. Abrir `ProyectoMetro.sln` con Visual Studio 2022 o 2026.
2. Elegir **x64** y **Debug** o **Release** (Release va más rápido).
3. Compilar con **Ctrl + Shift + B** y ejecutar con **F5**.

El ejecutable queda en `bin/` y se ejecuta desde ahí, porque los shaders,
modelos y texturas se buscan con rutas relativas a esa carpeta.

---

## 3. Modelo necesario: exportar desde Blender

El programa necesita **un solo archivo**:

| Desde Blender | Guardar como |
|---|---|
| `Modelos_Completos/Andenes_Completo.blend` (repo) | `bin/models/Andenes_Completo.fbx` |

El tren ya viene dentro de esa escena; no hace falta exportar otro archivo.

### Pasos

1. Abrir `Andenes_Completo.blend`.
2. Verificar que la colección **`99_Fuentes_Importadas`** esté oculta.
3. **File → Export → FBX (.fbx)** con estas opciones:
   - **Include**
     - ☑ Limit to → *Visible Objects*
     - Object Types: solo **Empty** y **Mesh**
   - **Transform**
     - Scale `1.00`, Apply Scalings **FBX All**
     - Forward **-Z**, Up **Y**
     - ☑ Apply Unit, ☑ Use Space Transform
   - **Geometry**
     - ☑ Apply Modifiers
     - Smoothing: **Normals Only**
   - **Path Mode:** **Copy** + botón **Embed Textures** activado
   - **Animation:** ☐ Bake Animation (desmarcado)
4. Guardar como `bin/models/Andenes_Completo.fbx`.

### Nombres que NO se deben cambiar en Blender

El código encuentra las piezas animadas e interactivas por su nombre:

| Para qué | Nombre en Blender |
|---|---|
| Tren completo (llega y se va) | `TREN_RAIZ` |
| Vagón al que se sube (escena 2) | `VAGON_05_RAIZ` |
| Puertas | `Vxx_Puerta_NN_Der` y `Vxx_Puerta_NN_Izq` (en pareja) |
| Ruedas | `Vxx_Llanta_NN` |
| Lámparas (de ahí salen las luces) | `Lampara_Estacion_...` |
| Conos | `Cono_Exclusivo_...` |
| Pantallas | `Pantalla_...` (material de la imagen con **"Imagen"** en el nombre) |
| Luz de alarma | objeto `alarma` dentro de los letreros `Proxima_...` |
| Objetos que se pueden tomar | `Pantalla_`, `Proxima_`, `Terminal_`, `Exclusivo_`, `Direccion_Indios`, `Emergencia_` |
| Piso caminable | `Piso_Anden_1` (x −10…−2) y `Piso_Anden_2` (x 2…10) |

Los **nombres de materiales** deciden el sombreado:

| Si el nombre contiene… | Se dibuja como |
|---|---|
| `vidrio`, `cristal` (o opacidad < 1) | Traslúcido (Fresnel) |
| `metal`, `acero`, `aluminio`, `inox`, `galvan` | Metálico (reflejo del cubemap) |
| `plastic`, `carcasa`, `cono` | Plástico (brillo especular) |
| `luz` (o emisión) | Emisivo |
| cualquier otro | Opaco (Phong) |

Si algo se renombra o se borra, el programa sigue funcionando, pero esa
animación o interacción deja de aparecer.

### Problemas conocidos del `.blend` (se corrigen en Blender)

- **Placa de grava sobre la vía central (a media estación).** El objeto
  `Grava_Via_1_17` quedó en la posición (0, 0, 0). El programa lo recoloca
  solo (BLOQUE 6.0). En Blender, su lugar correcto es Location
  **X = −12.5, Y = 55, Z = −0.72**.

- **Pantallas en blanco.** El material `Imagen_Pantalla_Universidad` termina
  en un nodo *Emission* conectado a *Material Output*. El exportador FBX solo
  lee la textura conectada al **Principled BSDF** que llega a *Material Output*.
  Solución: conectar el *Principled BSDF* (que ya tiene la imagen en Base Color
  y Emission Color) a la entrada *Surface* de *Material Output*.
- **"Textura no encontrada: Textura_cubierta_lampara.png".** Una de las 8
  copias de esa imagen no está empaquetada y apunta a una carpeta de otra
  computadora. Solución: **File → External Data → Pack Resources**, guardar y
  volver a exportar. Mientras tanto la pieza se ve con el color de su material.

---

## 4. Controles

| Tecla | Acción |
|---|---|
| **W A S D** + mouse | Caminar y mirar |
| **Shift** | Correr (o volar más rápido en cámara libre) |
| **1** | Cámara en primera persona (con colisiones) |
| **2** | Cámara libre (**E** sube, **Q** baja) |
| **3** | Vista aérea isométrica (sin techo, rueda del mouse = zoom) |
| **C** | Recorrido cinematográfico (para el video) |
| **Enter** | Llamar al tren |
| **F** | Subir / bajar del vagón (tren detenido, junto a una puerta) |
| **X** | Colocar / retirar los conos del espacio exclusivo |
| **P** | Encender / apagar las pantallas |
| **Clic izquierdo** | Seleccionar el objeto bajo la mira |
| **Clic derecho** | Quitar selección |
| **G** | Tomar / soltar el objeto seleccionado |
| **Flechas**, **RePág / AvPág** | Mover el objeto |
| **R / T** | Rotar el objeto |
| **+ / −** | Escalar el objeto |
| **Retroceso** | Regresar el objeto a su lugar |
| **M / N** | Vista de alambre / sólida |
| **F12** | Captura de pantalla (`bin/capturas/`) |
| **Esc** | Salir |

La barra de título muestra FPS, cámara activa, estado del tren y la acción
disponible (por ejemplo “[F] Subir al vagón”).

---

## 5. Qué cubre esta escena de la rúbrica

| Requisito | Implementación (bloque de `main.cpp`) |
|---|---|
| Modelado jerárquico | Árbol de nodos del FBX: tren → vagón → puerta / rueda (`modelo_escena.h`, bloque E) |
| ≥ 2 controles de cámara | Primera persona, libre, aérea isométrica y cinemática (BLOQUE 9) |
| 2 técnicas de iluminación | **Phong** con varias luces puntuales + direccional; **Fresnel (Schlick)** en cristales y metales (`metro_iluminacion.fs`) |
| Textura de ambiente | **Cubemap** como cielo y para reflejos (BLOQUE 10.4) |
| Materiales | Opacos, plásticos, traslúcidos, metálicos y emisivos |
| Interacción con ≥ 15 objetos | 48 objetos: seleccionar (picking), tomar, trasladar, rotar, escalar (BLOQUE 11) |
| Animaciones por keyframes (5) | K1 llegada del tren, K2 salida, K3 puertas, K4 cámara cinematográfica, K5 conos |
| Animaciones procedurales (5) | P1 ruedas (ω = v / r), P2 balanceo del tren, P3 alarmas, P4 lámpara con falla, P5 balanceo de la cabeza al caminar |
| Animaciones simples | S1 pantallas, S2 objeto tomado gira, S3 caída al soltar |

**Pendiente para el proyecto completo:** la rúbrica pide **10 animaciones
simples**; esta escena tiene 3, así que las demás pueden venir de las escenas
2 y 3. Tampoco están aún los sonidos (`bin/sound/tren.mp3` y
`bin/sound/puertas.mp3` son opcionales: si existen, se reproducen).

---

## 6. Conexión con la escena 2 (dentro del vagón)

Con el tren detenido, junto a una puerta, **[F]** hace un fundido a negro y
coloca al usuario dentro del vagón 5 (el que tiene asientos y pasamanos).
Ese cambio está en `EntrarEscenaVagon()` (BLOQUE 12): cuando exista la
escena 2, ahí es donde se cargará. Mientras el usuario está dentro, el tren
no se va.

---

## 7. Rendimiento

Probado en Intel UHD Graphics 620 (tarjeta integrada): ~25 FPS sin
antialiasing. Para acelerar:

- La escena se agrupa en **lotes por material** y se descarta lo que no está
  en la vista (*frustum culling*) — `modelo_escena.h`, bloque G.
- Los detalles muy densos (rejas) solo se dibujan de cerca (LOD).
- Ejecutar con `ProyectoMetro.exe --sinmsaa` quita el antialiasing.

Opciones de línea de comandos (para pruebas y capturas del reporte):
`--modelo ruta.fbx`, `--res ancho alto`, `--sinmsaa`, `--modo n`,
`--cam x y z yaw pitch`, `--captura archivo.bmp --t segundos`.
