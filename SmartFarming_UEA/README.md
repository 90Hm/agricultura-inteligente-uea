# Smart Farming con ESP32 y MQTT

Desarrollamos este proyecto para la asignatura Redes Inalámbricas de Sensores de la Universidad Estatal Amazónica. Con nuestra maqueta observamos la humedad estimada del suelo, la temperatura, la humedad del aire y la luz. Utilizamos un ESP32 para obtener las lecturas, controlar una bomba mediante un relé y publicar los datos en Adafruit IO.

Somos Brayan Jacinto Camacho Tapia y William Alexander Camacho Sánchez. En este repositorio reunimos el programa del ESP32 y las instrucciones para abrirlo en Arduino IDE desde Linux o Windows.

## Cómo organizamos el código

Separamos la configuración y el certificado del programa principal. Conservamos la lógica del sketch original, sus trece funciones y la salida del monitor serie. También corregimos los caracteres de formato que aparecieron al copiar el código en Markdown.

| Archivo | Para qué lo utilizamos |
| --- | --- |
| `SmartFarming_ESP32/SmartFarming_ESP32.ino` | Contiene la lectura de sensores, el riego, la conexión MQTT y las publicaciones |
| `SmartFarming_ESP32/configuracion.h` | Define los pines, el umbral, la calibración provisional y los intervalos |
| `SmartFarming_ESP32/internet.h` | Carga las credenciales locales o la plantilla si todavía no existe la copia local |
| `SmartFarming_ESP32/credenciales_ejemplo.h` | Es nuestra plantilla pública, sin contraseñas reales |
| `SmartFarming_ESP32/certificado.h` | Conserva el certificado público utilizado para verificar la conexión TLS |
| `.gitignore` | Excluye las credenciales locales y los archivos temporales cuando utilizamos Git |
| `.gitattributes` | Mantiene finales de línea compatibles entre Linux y Windows |

Creamos `credenciales.h` solamente en nuestra computadora. Ese archivo no viene en el repositorio ni en el ZIP. Abrimos el archivo `.ino` dentro de su carpeta del mismo nombre y mantenemos los archivos `.h` junto a él. Arduino los muestra como pestañas del proyecto.

## Qué hace nuestro programa

Leemos los sensores aproximadamente cada tres segundos. Mostramos cada dato en una línea del monitor serie, junto con el estado de Wi-Fi, MQTT, el nivel de QoS y los resultados de las pruebas de retorno.

| Componente | Pin del ESP32 |
| --- | --- |
| Sensor de suelo | GPIO 35 |
| Sensor de luz | GPIO 34 |
| DHT11 | GPIO 4 |
| Entrada del relé | GPIO 33 |

Usamos un relé activo en LOW. Al iniciar dejamos su salida en HIGH para mantener la bomba apagada. Cuando la humedad estimada baja del 30 %, solicitamos riego. Apagamos la bomba cuando la lectura alcanza al menos el 30 % o cuando se cumple el límite configurado de diez segundos.

Después de un corte por tiempo, el programa espera una lectura de al menos 30 % antes de permitir otro ciclo cuando el suelo vuelva a secarse. No reinicia la bomba cada diez segundos mientras la lectura siga baja. Conservamos ese comportamiento del código original.

El temporizador utiliza `esp_timer` y su callback apaga la salida sin publicar mensajes MQTT. Esto permite atender el corte mientras el bucle principal espera una operación de red. El control por humedad necesita una nueva lectura, por lo que su respuesta puede demorarse durante una operación de conexión. El temporizador tampoco sustituye una protección eléctrica independiente.

La bomba utiliza su circuito de alimentación y el relé conmuta ese circuito. No alimentamos el motor directamente desde GPIO 33. La decisión de riego es local y no necesita que el dashboard esté abierto ni que MQTT permanezca conectado.

## Cómo interpretamos las lecturas

En `configuracion.h` conservamos `SUELO_SECO = 4095` y `SUELO_HUMEDO = 0` como referencias provisionales. Con ellas convertimos el valor crudo a un índice entre 0 y 100. Una lectura de 2046 equivale aproximadamente al 50 %. Antes de utilizar el sistema para decisiones agrícolas reales necesitamos ajustar esas referencias con nuestro sensor y sustrato.

El porcentaje del suelo no es una medición calibrada de contenido volumétrico de agua. La luz se muestra como lectura ADC, no en lux. Si el DHT11 devuelve una lectura inválida, publicamos el estado en `dht-valido` y omitimos el nuevo valor de temperatura y humedad del aire. Por ello, el dashboard puede conservar una lectura válida anterior.

## Qué instalamos

Utilizamos [Arduino IDE](https://www.arduino.cc/en/software/) y el paquete **esp32 by Espressif Systems** desde el gestor de placas. Para reproducir nuestro montaje seleccionamos **ESP32 Dev Module**.

Si necesitamos agregar el índice de Espressif, abrimos las preferencias de Arduino IDE y colocamos esta dirección en el campo de URLs adicionales del gestor de placas.

```text
https://espressif.github.io/arduino-esp32/package_esp32_index.json
```

Desde el gestor de bibliotecas instalamos las siguientes dependencias.

| Biblioteca | Autor o responsable |
| --- | --- |
| DHT sensor library | Adafruit |
| Adafruit Unified Sensor | Adafruit |
| MQTT | Joel Gaehwiler, proyecto 256dpi |

La biblioteca MQTT que necesitamos incluye `MQTT.h`. No la sustituimos por PubSubClient. Las bibliotecas Wi-Fi, TLS y el temporizador vienen con el paquete ESP32. No necesitamos instalar Mosquitto ni Node-RED porque utilizamos el servicio de Adafruit IO.

## Cómo configuramos nuestras credenciales

1. Abrimos la carpeta `SmartFarming_ESP32`.
2. Copiamos `credenciales_ejemplo.h` y llamamos a la copia `credenciales.h`.
3. Editamos la copia con el nombre y la contraseña de nuestra red Wi-Fi de 2,4 GHz, el usuario de Adafruit IO y su clave AIO.
4. Guardamos la plantilla original con sus valores de ejemplo.

Podemos hacer la copia desde el explorador de archivos. También podemos utilizar uno de estos comandos desde la carpeta principal del proyecto.

En Linux utilizamos la terminal.

```bash
cp SmartFarming_ESP32/credenciales_ejemplo.h SmartFarming_ESP32/credenciales.h
```

En Windows utilizamos PowerShell.

```powershell
Copy-Item .\SmartFarming_ESP32\credenciales_ejemplo.h .\SmartFarming_ESP32\credenciales.h
```

Si aún no creamos la copia local, `internet.h` carga la plantilla. Esto permite verificar el sketch, pero los datos de ejemplo no conectarán el dispositivo a nuestra cuenta. En Windows comprobamos que el archivo no termine accidentalmente en `.h.txt`.

El certificado de `certificado.h` es público. No es nuestra clave de Adafruit IO. Conservamos la verificación TLS y no utilizamos `setInsecure`. Si la cadena del servidor cambia, necesitamos revisar el certificado con información oficial.

## Cómo lo cargamos en Windows

1. Instalamos Arduino IDE desde su sitio oficial y las dependencias indicadas arriba.
2. Extraemos el ZIP completo y abrimos `SmartFarming_ESP32.ino` desde su carpeta. No lo ejecutamos dentro del archivo comprimido.
3. Configuramos nuestra copia local de credenciales.
4. Conectamos el ESP32 con un cable USB de datos.
5. Seleccionamos **ESP32 Dev Module** y el puerto que corresponda a la placa, por ejemplo `COM3`. El número puede ser diferente.
6. Pulsamos **Verificar** y luego **Subir**.
7. Abrimos el monitor serie a **115200 baudios**.

Si Windows no reconoce el puerto, revisamos el cable y el dispositivo en el Administrador de dispositivos. Solo instalamos el controlador correspondiente al puente USB de nuestra placa, obtenido de su fabricante. No elegimos un controlador únicamente por el nombre ESP32.

## Cómo lo cargamos en Linux

1. Descargamos Arduino IDE para Linux desde su sitio oficial. Si utilizamos AppImage, permitimos su ejecución desde las propiedades del archivo.
2. Instalamos el paquete ESP32 y las bibliotecas del proyecto.
3. Extraemos el ZIP, creamos `credenciales.h` y abrimos el `.ino`.
4. Conectamos la placa y seleccionamos **ESP32 Dev Module**.
5. Elegimos el puerto detectado, que puede aparecer como `/dev/ttyUSB0` o `/dev/ttyACM0`.
6. Verificamos, subimos el programa y abrimos el monitor a **115200 baudios**.

Si recibimos un error de permisos, comprobamos el grupo propietario del puerto real. En distribuciones que utilizan `dialout`, agregamos nuestro usuario a ese grupo y reiniciamos la sesión o la computadora.

```bash
ls -l /dev/ttyUSB0
sudo usermod -a -G dialout "$USER"
```

Si el puerto está ocupado, cerramos otros monitores serie o instancias del IDE. Si desaparece o muestra un error de entrada y salida, revisamos el cable, la conexión USB y la selección del puerto. Un error de carga no significa que el programa nuevo haya quedado instalado en la placa.

## Cómo conectamos los feeds y el dashboard

Utilizamos Adafruit IO como broker y plataforma de visualización. Si ya tenemos los feeds creados, reutilizamos sus claves y el dashboard existente. La separación de archivos no cambia los temas MQTT.

| Clave del feed | Dato | Bloque que utilizamos o podemos asociar |
| --- | --- | --- |
| `suelo` | Humedad estimada del suelo | Gauge y Line Chart |
| `temperatura` | Temperatura válida en °C | Gauge y Line Chart |
| `humedad-aire` | Humedad relativa en % | Gauge y Line Chart |
| `luz` | Valor ADC | Gauge y Line Chart |
| `orden-riego` | 1 para orden activa y 0 para apagada | Indicator |
| `senal-wifi` | RSSI en dBm | Gauge |
| `dht-valido` | 1 para lectura válida y 0 para inválida | Indicator |
| `prueba` | Identificador de un mensaje | Feed de diagnóstico |
| `rtt` | Tiempo de ida y vuelta en ms | Gauge y Line Chart |

Comprobamos la **clave** del feed, que puede diferir del nombre visible. El programa forma cada tema con `usuario/feeds/clave`. Una clave con prefijo de grupo también tendría que coincidir con la definida en el programa. Un nombre de bloque del dashboard no modifica esa ruta.

En Adafruit IO abrimos **Dashboards**, elegimos nuestro panel y añadimos bloques mediante **New Block**. Asociamos cada bloque con su feed y configuramos su unidad. El certificado y los archivos del ESP32 no crean ni cambian automáticamente la distribución del dashboard.

El código prepara un ciclo de datos cada treinta segundos y separa las publicaciones al menos 2,5 segundos. Da prioridad a los cambios de la orden de riego y al RTT. Por eso las variables no aparecen todas al mismo instante. Las publicaciones fallidas de sensores no se guardan en una cola permanente para reenviarlas después.

El indicador `orden-riego` muestra la orden del programa. No tenemos un sensor de caudal que confirme el movimiento de agua. Este sketch tampoco se suscribe a ese feed para recibir órdenes manuales. Cambiar un interruptor en el dashboard no controla la bomba con esta versión.

## Cómo observamos MQTT y sus tiempos

Conservamos `MQTT_QOS = 0`. Para un ensayo separado podemos cambiarlo a 1 en `configuracion.h`, verificar el sketch y volver a cargarlo. Adafruit IO admite ambos niveles.

El cliente publica un identificador en `prueba` y reconoce su retorno mediante una suscripción. Calcula un tiempo de ida y vuelta y lo envía a `rtt`. Los contadores describen las pruebas de la sesión actual y se reinician al reiniciar el ESP32. El contador de enviadas registra intentos de publicación, no confirma por sí mismo que el broker haya recibido todos los mensajes.

Si activamos `MOSTRAR_CSV`, aparecen líneas adicionales con RTT y resúmenes para guardar registros. Conservamos esa opción desactivada para una lectura más limpia del monitor. Una captura aislada no permite calcular la confiabilidad de toda la red ni comparar de forma válida QoS 0 y QoS 1.

## Cómo subimos el proyecto a GitHub

Para nuestra primera publicación utilizamos una copia recién extraída del ZIP, sin credenciales reales.

1. Entramos a [GitHub](https://github.com/new) y creamos un repositorio llamado `smart-farming-esp32-uea`.
2. Elegimos la visibilidad según la entrega. Si debe revisarlo alguien sin invitación, utilizamos público.
3. Dejamos desmarcada la creación de README, licencia y gitignore porque ya tenemos los archivos necesarios para la carga.
4. Abrimos **uploading an existing file** o **Add file**, después **Upload files**.
5. Arrastramos el contenido de `SmartFarming_UEA`, conservando la subcarpeta `SmartFarming_ESP32`.
6. Revisamos la lista y confirmamos los cambios. El README debe quedar en la raíz del repositorio.

No subimos solamente el ZIP, porque queremos consultar los archivos y el README directamente. Tampoco subimos `credenciales.h`. El archivo `.gitignore` protege frente a incorporaciones habituales mediante Git, pero no filtra los archivos que seleccionamos manualmente en la web. Si trabajamos desde Linux, podemos mostrar los archivos ocultos con Ctrl+H para incluir `.gitignore` y `.gitattributes`.

No publicamos contraseñas en capturas ni en el informe. Si una clave ya se publicó o compartió, la sustituimos por una nueva antes de seguir utilizándola. Borrar el archivo de la última versión no elimina una clave que haya quedado en el historial.

## Alcance de esta versión

Conservamos el cuerpo funcional del programa original al separar los archivos. Esta reorganización no incluye una nueva prueba física ni una compilación certificada para todas las versiones del paquete ESP32. Verificamos el sketch en nuestro Arduino IDE antes de cargarlo.

El repositorio contiene el firmware del ESP32. El proyecto de CupCarbon, el informe y las evidencias de la exposición se documentan por separado. No usamos este README para atribuir resultados de pruebas que no estén registrados.

## Fuentes y créditos

Nos apoyamos en estas fuentes para las bibliotecas y la instalación.

- [Espressif y la instalación del paquete Arduino ESP32](https://docs.espressif.com/projects/arduino-esp32/en/latest/installing.html)
- [Arduino y la instalación del IDE](https://support.arduino.cc/hc/en-us/articles/360019833020-Download-and-install-Arduino-IDE)
- [Arduino y el acceso al puerto en Linux](https://support.arduino.cc/hc/en-us/articles/360016495679-Fix-port-access-on-Linux)
- [Biblioteca MQTT de Joel Gaehwiler](https://github.com/256dpi/arduino-mqtt)
- [Biblioteca DHT de Adafruit](https://github.com/adafruit/DHT-sensor-library)
- [Adafruit IO y su API MQTT](https://io.adafruit.com/api/docs/mqtt.html)
- [Adafruit IO y sus dashboards](https://learn.adafruit.com/adafruit-io-basics-dashboards)
- [GitHub y la creación de repositorios](https://docs.github.com/en/repositories/creating-and-managing-repositories/creating-a-new-repository)
- [GitHub y la carga de archivos](https://docs.github.com/en/repositories/working-with-files/managing-files/adding-a-file-to-a-repository)
- [DigiCert y sus certificados raíz](https://www.digicert.com/kb/digicert-root-certificates.htm)

Las bibliotecas conservan sus autores y licencias. El certificado público pertenece a DigiCert. Nuestro repositorio contiene la configuración y el programa de la maqueta, sin incluir las bibliotecas de terceros dentro del ZIP.
