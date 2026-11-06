# Metro CDMX — Andenes y planta alta (OpenGL)

Recorrido interactivo por los andenes y la planta alta de la estación Universidad. Proyecto final de
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
    ├── models/                ← FBX de estaciones, tren interior y pasajeros
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

El ejecutable queda en `bin/`. Para abrir la versión ya compilada, ejecuta
`Ejecutar_Metro.cmd`: inicia el programa desde `bin/`, donde están los shaders,
modelos y texturas.

---

## 3. Modelo necesario: exportar desde Blender

Los modelos se cargan en OpenGL con Assimp:

| Desde Blender | Guardar como |
|---|---|
| `Modelos_Completos/Andenes_Completo.blend` | `bin/models/Andenes_Completo.fbx` |
| `Modelos_Completos/Estacion_Planta_Alta.blend` | `bin/models/Estacion_Planta_Alta.fbx` |
| `Tren_Completo123.fbx` entregado | `bin/models/Tren_Completo123.fbx` |
| `Modelos_Blender/Pasajero_Adulto.blend` | `bin/models/Pasajero_Adulto.fbx` |
| `Modelos_Blender/Pasajera_Mujer.blend` | `bin/models/Pasajera_Mujer.fbx` |
| `Modelos_Blender/Pasajero_Nino.blend` | `bin/models/Pasajero_Nino.fbx` |

El tren que se ve desde el andén forma parte del FBX de la estación.
El interior usa el vagón 5 del FBX entregado y sus imágenes de
`bin/models/Tren_Completo123.fbm/`. Los FBX de pasajeros incluyen la malla,
los materiales y las articulaciones `J_*`; OpenGL aplica las poses.
Los tres editables se derivan de `Modelos_Blender/niño.blend` e incluyen
las imágenes empaquetadas.

### Pasos para exportar ambas plantas

1. Abrir en Blender el `.blend` de la tabla. Para los andenes, mantener
   oculta la colección **`99_Fuentes_Importadas`**. Para la planta alta,
   revisar los materiales indicados abajo antes de exportar.
2. Ir a **File → Export → FBX (.fbx)** con las mismas opciones para ambas:
   - **Include:** *Visible Objects*, solo **Empty** y **Mesh**.
   - **Transform:** escala `1.00`, **FBX All**, Forward **−Z**, Up **Y**;
     activar *Apply Unit* y *Use Space Transform*.
   - **Geometry:** activar *Apply Modifiers*; *Smoothing*: **Normals Only**.
   - **Path Mode:** **Copy** y activar **Embed Textures**.
   - **Animation:** desactivar *Bake Animation*. OpenGL anima las piezas
     por nombre.
3. Guardar en `bin/models/` con el nombre de la tabla. Las imágenes sueltas
   de la planta alta están en `Texturas/Planta_Alta` del proyecto.

### Materiales de planta alta antes de volver a exportar

En el FBX incluido ya están aplicadas estas correcciones. Si se parte del
`.blend` original, prepararlas en Blender antes de volver a exportar:

- **`Estacion_Base`:** asignar un material mate crema a la ranura vacía de
  las dos caras del techo (RGB lineal: `0.83, 0.77, 0.65`). Conservar el
  material oscuro de las vigas.
- **`Letrero_Est_Fumar_01` y `Letrero_Est_Fumar_02`:** asignar un fondo mate
  claro a la ranura vacía detrás de «Universidad» (RGB lineal:
  `0.94, 0.92, 0.85`). Dejar `nofum.png` únicamente en los cuadros del
  símbolo. En el editor UV, ajustar los UV de esos cuadros al rango `0–1`
  para que la orilla del icono no se repita.

### Nombres que NO se deben cambiar en Blender

El código encuentra las piezas animadas e interactivas por su nombre:

| Para qué | Nombre en Blender |
|---|---|
| Tren completo (llega y se va) | `TREN_RAIZ` |
| Vagón al que se sube (escena 2) | `VAGON_05_RAIZ` |
| Puertas | `Vxx_Puerta_NN_Der` y `Vxx_Puerta_NN_Izq` (en pareja) |
| Ruedas | `Vxx_Llanta_NN` |
| Escaleras y huecos | `Escalera_01` a `Escalera_08` y `Esc_Sup_01` a `Esc_Sup_08` |
| Torniquetes de planta alta | Raíces `TORNIQUETE_…` y sus hojas `Puerta_1_…` / `Puerta_2_…` |
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
| **X** | Subir a planta alta o bajar al andén, cerca de la boca de una escalera en primera persona |
| **V** | Colocar / retirar los conos del espacio exclusivo |
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
disponible (por ejemplo «[X] Subir a planta alta (Escalera 1)»).
Las ocho rutas permiten subir y regresar por el mismo acceso. Se numeran
por hileras desde −Z, izquierda y derecha; los barandales de arriba se
nombran por lado en Blender:

| Escaleras en andenes (izquierda / derecha) | Barandales de planta alta (izquierda / derecha) |
|---|---|
| 01 / 02 | Esc_Sup_05 / Esc_Sup_01 |
| 03 / 04 | Esc_Sup_06 / Esc_Sup_02 |
| 05 / 06 | Esc_Sup_07 / Esc_Sup_03 |
| 07 / 08 | Esc_Sup_08 / Esc_Sup_04 |

Con la pantalla en negro se libera el modelo actual y se carga el del
destino. Ambas plantas usan el mismo cubemap. Los fondos negros de los
huecos y los zócalos de las vías se generan en OpenGL.

En planta alta los torniquetes se abren al acercarse al centro de un carril,
desde cualquiera de los lados. Las hojas giran sobre sus bisagras, permanecen
abiertas mientras se cruza y se cierran al alejarse. Sus cajas de colisión
acompañan el giro; los gabinetes y barandales siguen siendo sólidos.

El mural y los letreros de andenes y salidas de la planta alta se seleccionan
con clic izquierdo. Usan los mismos controles de toma, traslado, rotación,
escala y restauración de los objetos del andén. Cada planta conserva sus
propios cambios mientras dura la sesión.

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
abre el interior equipado del vagón 5. **[F]** de nuevo regresa al punto de
embarque. El tren permanece detenido mientras el jugador está dentro.

En planta alta los pasajeros tocan la máquina de recarga o la ventanilla,
activan los torniquetes en los dos sentidos y siguen hacia una escalera o
un corredor. Los que bajan pueden incorporarse al andén por la escalera
correspondiente. En los andenes, parte de los pasajeros se acerca a una
puerta al frenar el tren y sube cuando abre; el resto sigue paseando.
Las franjas rosas delimitan la zona por donde solo pasan mujeres y niños.
En el interior hay pasajeros que se sientan, esperan de pie o sujetan
un pasamanos.

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
`--planta-alta 1` inicia frente al hueco indicado (del 1 al 8);
`--probar-escaleras` ejecuta las 16 transiciones de subida y bajada;
`--probar-torniquetes` recorre los 20 carriles en ambos sentidos y las cuatro salidas laterales.
`--probar-pasajeros` verifica el recorrido de los pasajeros en las tres escenas.
Con `--captura`, `--interior-tren` permite tomar una imagen dentro del vagón 5.
